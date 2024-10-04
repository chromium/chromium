// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/infobars/simple_alert_infobar_creator.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/local_test_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/proxy_policy_provider.h"
#include "components/policy/core/common/schema_registry_tracking_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/restricted_mgs_policy_provider.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_provider.h"
#include "chrome/browser/ash/policy/login/login_profile_policy_provider.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#endif

namespace policy {

namespace internal {
#if BUILDFLAG(IS_CHROMEOS_ASH)
// This class allows observing a |device_wide_policy_service| for policy updates
// during which the |source_policy_provider| has already been initialized.
// It is used to know when propagation of primary user policies proxied to the
// device-wide PolicyService has finished.
class ProxiedPoliciesPropagatedWatcher : PolicyService::ProviderUpdateObserver {
 public:
  ProxiedPoliciesPropagatedWatcher(
      PolicyService* device_wide_policy_service,
      ProxyPolicyProvider* proxy_policy_provider,
      ConfigurationPolicyProvider* source_policy_provider,
      base::OnceClosure proxied_policies_propagated_callback)
      : device_wide_policy_service_(device_wide_policy_service),
        proxy_policy_provider_(proxy_policy_provider),
        source_policy_provider_(source_policy_provider),
        proxied_policies_propagated_callback_(
            std::move(proxied_policies_propagated_callback)) {
    device_wide_policy_service->AddProviderUpdateObserver(this);

    timeout_timer_.Start(
        FROM_HERE, base::Seconds(kProxiedPoliciesPropagationTimeoutInSeconds),
        this,
        &ProxiedPoliciesPropagatedWatcher::OnProviderUpdatePropagationTimedOut);
  }

  ProxiedPoliciesPropagatedWatcher(const ProxiedPoliciesPropagatedWatcher&) =
      delete;
  ProxiedPoliciesPropagatedWatcher& operator=(
      const ProxiedPoliciesPropagatedWatcher&) = delete;
  ~ProxiedPoliciesPropagatedWatcher() override {
    device_wide_policy_service_->RemoveProviderUpdateObserver(this);
  }

  // PolicyService::Observer:
  void OnProviderUpdatePropagated(
      ConfigurationPolicyProvider* provider) override {
    if (!proxied_policies_propagated_callback_)
      return;
    if (provider != proxy_policy_provider_)
      return;

    if (!source_policy_provider_->IsInitializationComplete(
            POLICY_DOMAIN_CHROME)) {
      return;
    }

    ReportTimeUma();
    std::move(proxied_policies_propagated_callback_).Run();
  }

  void OnProviderUpdatePropagationTimedOut() {
    if (!proxied_policies_propagated_callback_)
      return;
    LOG(WARNING) << "Waiting for proxied policies to propagate timed out.";
    ReportTimeUma();
    std::move(proxied_policies_propagated_callback_).Run();
  }

 private:
  static constexpr int kProxiedPoliciesPropagationTimeoutInSeconds = 5;

  void ReportTimeUma() const {
    UmaHistogramTimes("Enterprise.TimeToUnthrottlePolicyInit",
                      base::TimeTicks::Now() - construction_time_);
  }

  const raw_ptr<PolicyService> device_wide_policy_service_;
  const raw_ptr<const ProxyPolicyProvider> proxy_policy_provider_;
  const raw_ptr<const ConfigurationPolicyProvider> source_policy_provider_;
  const base::TimeTicks construction_time_ = base::TimeTicks::Now();
  base::OnceClosure proxied_policies_propagated_callback_;
  base::OneShotTimer timeout_timer_;
};
#endif
// Class responsible for showing infobar when test policies are set from
// the chrome://policy/test page
class LocalTestInfoBarVisibilityManager :
#if BUILDFLAG(IS_ANDROID)
    public TabModelObserver
#else
    public BrowserListObserver,
    public TabStripModelObserver
#endif  // BUILDFLAG(IS_ANDROID)
{
 public:
  LocalTestInfoBarVisibilityManager() = default;

  LocalTestInfoBarVisibilityManager(const LocalTestInfoBarVisibilityManager&) =
      delete;
  LocalTestInfoBarVisibilityManager& operator=(
      const LocalTestInfoBarVisibilityManager&) = delete;

  ~LocalTestInfoBarVisibilityManager() override {
    if (infobar_active_) {
      DismissInfobarsForActiveLocalTestPoliciesAllTabs();
    }
  }

#if BUILDFLAG(IS_ANDROID)
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (tab) {
      AddInfobarForActiveLocalTestPolicies(tab->web_contents());
    }
  }
#else
  void OnBrowserAdded(Browser* browser) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(browser);

    browser->tab_strip_model()->AddObserver(this);
  }

  void OnBrowserRemoved(Browser* browser) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(browser);

    if (BrowserList::GetInstance()->empty()) {
      BrowserList::GetInstance()->RemoveObserver(this);
    }
    browser->tab_strip_model()->RemoveObserver(this);
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        AddInfobarForActiveLocalTestPolicies(contents.contents);
      }
    } else if (change.type() == TabStripModelChange::kRemoved &&
               tab_strip_model->empty()) {
      tab_strip_model->RemoveObserver(this);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  void AddInfobarsForActiveLocalTestPoliciesAllTabs() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
    for (TabModel* model : TabModelList::models()) {
      for (int index = 0; index < model->GetTabCount(); ++index) {
        TabAndroid* tab = model->GetTabAt(index);
        if (tab) {
          AddInfobarForActiveLocalTestPolicies(tab->web_contents());
        }
      }
      model->AddObserver(this);
    }
#else
    for (Browser* browser : *BrowserList::GetInstance()) {
      CHECK(browser);

      OnBrowserAdded(browser);

      TabStripModel* tab_strip_model = browser->tab_strip_model();
      for (int i = 0; i < tab_strip_model->count(); i++) {
        AddInfobarForActiveLocalTestPolicies(
            tab_strip_model->GetWebContentsAt(i));
      }
    }
    BrowserList::GetInstance()->AddObserver(this);
#endif  // BUILDFLAG(IS_ANDROID)
    infobar_active_ = true;
  }

  void AddInfobarForActiveLocalTestPolicies(
      content::WebContents* web_contents) {
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents);
    CreateSimpleAlertInfoBar(
        infobars::ContentInfoBarManager::FromWebContents(web_contents),
        infobars::InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR, nullptr,
        l10n_util::GetStringUTF16(IDS_LOCAL_TEST_POLICIES_ENABLED),
        /*auto_expire=*/false, /*should_animate=*/false, /*closeable=*/false);
  }

  void DismissInfobarsForActiveLocalTestPoliciesAllTabs() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
    for (TabModel* model : TabModelList::models()) {
      for (int index = 0; index < model->GetTabCount(); ++index) {
        TabAndroid* tab = model->GetTabAt(index);
        if (tab) {
          DismissInfobarForActiveLocalTestPolicies(tab->web_contents());
        }
      }
      model->RemoveObserver(this);
    }
#else
    for (Browser* browser : *BrowserList::GetInstance()) {
      CHECK(browser);

      browser->tab_strip_model()->RemoveObserver(this);

      TabStripModel* tab_strip_model = browser->tab_strip_model();
      for (int i = 0; i < tab_strip_model->count(); i++) {
        DismissInfobarForActiveLocalTestPolicies(
            tab_strip_model->GetWebContentsAt(i));
      }
    }
    BrowserList::GetInstance()->RemoveObserver(this);
#endif  // BUILDFLAG(IS_ANDROID)
    infobar_active_ = false;
  }

  void DismissInfobarForActiveLocalTestPolicies(
      content::WebContents* web_contents) {
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents);
    auto* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    const auto it = base::ranges::find(
        infobar_manager->infobars(),
        infobars::InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR,
        &infobars::InfoBar::GetIdentifier);
    if (it != infobar_manager->infobars().cend()) {
      infobar_manager->RemoveInfoBar(*it);
    }
  }

  bool infobar_active() { return infobar_active_; }

 private:
  bool infobar_active_ = false;
};
}  // namespace internal

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {
// Returns the PolicyService that holds device-wide policies.
PolicyService* GetDeviceWidePolicyService() {
  BrowserPolicyConnectorAsh* browser_policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return browser_policy_connector->GetPolicyService();
}

// Returns the ProxyPolicyProvider which is used to forward primary Profile
// policies into the device-wide PolicyService.
ProxyPolicyProvider* GetProxyPolicyProvider() {
  BrowserPolicyConnectorAsh* browser_policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return browser_policy_connector->GetGlobalUserCloudPolicyProvider();
}
}  // namespace

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ProfilePolicyConnector::ProfilePolicyConnector() = default;

ProfilePolicyConnector::~ProfilePolicyConnector() {
  if (policy_service_) {
    // We should've subscribed by this point, but in case not that's a no-op.
    policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  }
}

void ProfilePolicyConnector::Init(
    const user_manager::User* user,
    SchemaRegistry* schema_registry,
    ConfigurationPolicyProvider* configuration_policy_provider,
    const CloudPolicyStore* policy_store,
    policy::ChromeBrowserPolicyConnector* connector,
    bool force_immediate_load) {
  DCHECK(!configuration_policy_provider_);
  DCHECK(!policy_store_);
  DCHECK(!policy_service_);

  configuration_policy_provider_ = configuration_policy_provider;
  policy_store_ = policy_store;
  local_test_infobar_visibility_manager_ =
      std::make_unique<internal::LocalTestInfoBarVisibilityManager>();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_ = user;
  auto* browser_policy_connector =
      static_cast<BrowserPolicyConnectorAsh*>(connector);
#else
  DCHECK_EQ(nullptr, user);
#endif

  ConfigurationPolicyProvider* platform_provider =
      connector->GetPlatformProvider();
  if (platform_provider) {
    AppendPolicyProviderWithSchemaTracking(platform_provider, schema_registry);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  browser_policy_connector_ = connector;
  if (connector->ash_policy_provider()) {
    policy_providers_.push_back(connector->ash_policy_provider());
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (browser_policy_connector->GetDeviceCloudPolicyManager()) {
    policy_providers_.push_back(
        browser_policy_connector->GetDeviceCloudPolicyManager());
  }
#else
  ConfigurationPolicyProvider* machine_level_user_cloud_policy_provider =
      connector->proxy_policy_provider();
  if (machine_level_user_cloud_policy_provider) {
    policy_providers_.push_back(machine_level_user_cloud_policy_provider);
  }

  if (connector->command_line_policy_provider()) {
    policy_providers_.push_back(connector->command_line_policy_provider());
  }
#endif

    local_test_policy_provider_ = connector->local_test_policy_provider();

  if (configuration_policy_provider) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    AppendPolicyProviderWithSchemaTracking(configuration_policy_provider,
                                           schema_registry);
    configuration_policy_provider_ = wrapped_policy_providers_.back().get();
#else
    policy_providers_.push_back(configuration_policy_provider);
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!user) {
    DCHECK(schema_registry);
    // This case occurs for the signin and the lock screen app profiles.
    special_user_policy_provider_ =
        std::make_unique<LoginProfilePolicyProvider>(
            browser_policy_connector->GetPolicyService());
  } else {
    auto* const manager = user_manager::UserManager::Get();
    // |user| should never be nullptr except for the signin and the lock screen
    // app profile.
    is_primary_user_ = user == manager->GetPrimaryUser();
    is_user_new_ =
        user == manager->GetActiveUser() && manager->IsCurrentUserNew();
    // Note that |DeviceLocalAccountPolicyProvider::Create| returns nullptr when
    // the user supplied is not a device-local account user.
    special_user_policy_provider_ = DeviceLocalAccountPolicyProvider::Create(
        user->GetAccountId().GetUserEmail(),
        browser_policy_connector->GetDeviceLocalAccountPolicyService(),
        force_immediate_load);
  }
  if (special_user_policy_provider_) {
    special_user_policy_provider_->Init(schema_registry);
    policy_providers_.push_back(special_user_policy_provider_.get());
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // `RestrictedMGSPolicyProvider::Create()` returns a nullptr when we are not
  // in a Managed Guest Session.
  restricted_mgs_policy_provider = RestrictedMGSPolicyProvider::Create();
  if (restricted_mgs_policy_provider) {
    restricted_mgs_policy_provider->Init(schema_registry);
    policy_providers_.push_back(restricted_mgs_policy_provider.get());
  }
#endif

  std::vector<std::unique_ptr<PolicyMigrator>> migrators;
#if BUILDFLAG(IS_WIN)
  migrators.push_back(
      std::make_unique<browser_switcher::BrowserSwitcherPolicyMigrator>());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ConfigurationPolicyProvider* user_policy_delegate_candidate =
      configuration_policy_provider ? configuration_policy_provider
                                    : special_user_policy_provider_.get();

  // Only proxy primary user policies to the device_wide policy service if all
  // of the following are true:
  // (*) This ProfilePolicyConnector has been created for the primary user.
  // (*) There is a policy provider for this profile. Note that for unmanaged
  //     users, |user_policy_delegate_candidate| will be nullptr.
  // (*) The ProxyPolicyProvider is actually used by the device-wide policy
  //     service. This may not be the case  e.g. in tests that use
  //     BrowserPolicyConnectorBase::SetPolicyProviderForTesting.
  if (is_primary_user_ && user_policy_delegate_candidate &&
      GetDeviceWidePolicyService()->HasProvider(GetProxyPolicyProvider())) {
    GetProxyPolicyProvider()->SetUnownedDelegate(
        user_policy_delegate_candidate);

    // When proxying primary user policies to the device-wide PolicyService,
    // delay signaling that initialization is complete until the policies have
    // propagated. See CreatePolicyServiceWithInitializationThrottled for
    // details.
    policy_service_ = CreatePolicyServiceWithInitializationThrottled(
        policy_providers_, std::move(migrators),
        user_policy_delegate_candidate);
  } else {
    policy_service_ = std::make_unique<PolicyServiceImpl>(
        policy_providers_, PolicyServiceImpl::ScopeForMetrics::kUser,
        std::move(migrators));
  }
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  policy_service_ = std::make_unique<PolicyServiceImpl>(
      policy_providers_, PolicyServiceImpl::ScopeForMetrics::kUser,
      std::move(migrators));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (local_test_policy_provider_ && local_test_policy_provider_->is_active()) {
    UseLocalTestPolicyProvider();
  }
  DoPostInit();
}

void ProfilePolicyConnector::InitForTesting(
    std::unique_ptr<PolicyService> service) {
  DCHECK(!policy_service_);
  policy_service_ = std::move(service);
  DoPostInit();
}

void ProfilePolicyConnector::OverrideIsManagedForTesting(bool is_managed) {
  is_managed_override_ = std::make_unique<bool>(is_managed);
}

void ProfilePolicyConnector::Shutdown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_primary_user_)
    GetProxyPolicyProvider()->SetUnownedDelegate(nullptr);

  if (special_user_policy_provider_)
    special_user_policy_provider_->Shutdown();
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (restricted_mgs_policy_provider)
    restricted_mgs_policy_provider->Shutdown();
#endif

  for (auto& wrapped_policy_provider : wrapped_policy_providers_) {
    wrapped_policy_provider->Shutdown();
  }
}

bool ProfilePolicyConnector::IsManaged() const {
  if (is_managed_override_)
    return *is_managed_override_;
  const CloudPolicyStore* actual_policy_store = GetActualPolicyStore();
  if (actual_policy_store)
    return actual_policy_store->is_managed();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // As Lacros uses different ways to handle the main and the secondary
  // profiles, these profiles need to be handled differently:
  // ChromeOS's way is using mirror and we need to check with Ash using the
  // device account (via IsManagedDeviceAccount).
  // Desktop's way is used for secondary profiles and is using dice, which
  // can be read directly from the profile.
  // TODO(crbug.com/40788404): Remove this once Lacros only uses mirror.
  if (browser_policy_connector_ && IsMainProfile())
    return browser_policy_connector_->IsMainUserManaged();
#endif
  return false;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ProfilePolicyConnector::IsMainProfile() const {
  // If there is only a single profile or this connector object is owned by the
  // main profile, it must be the main profile.
  // TODO(crbug.com/40788404): Remove this once Lacros only uses mirror.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager->GetNumberOfProfiles() <= 1)
    return true;

  auto profiles = profile_manager->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(profiles, &Profile::IsMainProfile);
  if (main_it == profiles.end())
    return false;
  return (*main_it)->GetProfilePolicyConnector() == this;
}
#endif

bool ProfilePolicyConnector::IsProfilePolicy(const char* policy_key) const {
  const ConfigurationPolicyProvider* const provider =
      DeterminePolicyProviderForPolicy(policy_key);
  return provider == configuration_policy_provider_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ProfilePolicyConnector::TriggerProxiedPoliciesWaitTimeoutForTesting() {
  CHECK(proxied_policies_propagated_watcher_);
  proxied_policies_propagated_watcher_->OnProviderUpdatePropagationTimedOut();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::flat_set<std::string> ProfilePolicyConnector::user_affiliation_ids()
    const {
  if (!user_affiliation_ids_for_testing_.empty()) {
    return user_affiliation_ids_for_testing_;
  }
  auto* store = GetActualPolicyStore();
  if (!store || !store->has_policy())
    return {};
  const auto& ids = store->policy()->user_affiliation_ids();
  return {ids.begin(), ids.end()};
}

void ProfilePolicyConnector::SetUserAffiliationIdsForTesting(
    const base::flat_set<std::string>& user_affiliation_ids) {
  user_affiliation_ids_for_testing_ = user_affiliation_ids;
}

void ProfilePolicyConnector::OnPolicyServiceInitialized(PolicyDomain domain) {
  DCHECK_EQ(domain, POLICY_DOMAIN_CHROME);
  RecordAffiliationMetrics();
}

void ProfilePolicyConnector::DoPostInit() {
  DCHECK(policy_service_);
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
}

const CloudPolicyStore* ProfilePolicyConnector::GetActualPolicyStore() const {
  if (policy_store_)
    return policy_store_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (special_user_policy_provider_) {
    // |special_user_policy_provider_| is non-null for device-local accounts,
    // for the login profile, and the lock screen app profile.
    const DeviceCloudPolicyManagerAsh* const device_cloud_policy_manager =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceCloudPolicyManager();
    // The device_cloud_policy_manager can be a nullptr in unit tests.
    if (device_cloud_policy_manager)
      return device_cloud_policy_manager->core()->store();
  }
#endif
  return nullptr;
}

const ConfigurationPolicyProvider*
ProfilePolicyConnector::DeterminePolicyProviderForPolicy(
    const char* policy_key) const {
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  for (const ConfigurationPolicyProvider* provider : policy_providers_) {
    if (provider->policies().Get(chrome_ns).Get(policy_key))
      return provider;
  }
  return nullptr;
}

void ProfilePolicyConnector::AppendPolicyProviderWithSchemaTracking(
    ConfigurationPolicyProvider* policy_provider,
    SchemaRegistry* schema_registry) {
  auto wrapped_policy_provider =
      std::make_unique<SchemaRegistryTrackingPolicyProvider>(policy_provider);
  wrapped_policy_provider->Init(schema_registry);
  policy_providers_.push_back(wrapped_policy_provider.get());
  wrapped_policy_providers_.push_back(std::move(wrapped_policy_provider));
}

std::string ProfilePolicyConnector::GetTimeToFirstPolicyLoadMetricSuffix()
    const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!is_primary_user_) {
    // Don't report the metric for secondary users: we're only interested in the
    // delay during the initial load as it blocks any other UX.
    return "";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!IsManaged()) {
    return "Unmanaged";
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(user_);
  switch (user_->GetType()) {
    case user_manager::UserType::kRegular:
      if (user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
              user_->GetAccountId())) {
        return "Managed.Ephemeral";
      }
      return is_user_new_ ? "Managed.NewPersistent" : "Managed.Existing";
    case user_manager::UserType::kChild:
      return "Child";
    case user_manager::UserType::kPublicAccount:
      return "ManagedGuestSession";
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return "Kiosk";
    case user_manager::UserType::kGuest:
      // Don't report the metric in uninteresting or unreachable cases.
      return "";
  }
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return "Managed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ProfilePolicyConnector::UseLocalTestPolicyProvider() {
  if (IsManaged()) {
    return;
  }
  local_test_policy_provider_->set_active(true);
  policy_service_->UseLocalTestPolicyProvider(local_test_policy_provider_);
  policy_service()->RefreshPolicies(base::DoNothing(),
                                    PolicyFetchReason::kTest);
  if (!local_test_infobar_visibility_manager_->infobar_active()) {
    local_test_infobar_visibility_manager_
        ->AddInfobarsForActiveLocalTestPoliciesAllTabs();
  }
}

void ProfilePolicyConnector::RevertUseLocalTestPolicyProvider() {
  local_test_policy_provider_->set_active(false);
  policy_service_->UseLocalTestPolicyProvider(nullptr);
  static_cast<LocalTestPolicyProvider*>(local_test_policy_provider_)
      ->ClearPolicies();
  policy_service()->RefreshPolicies(base::DoNothing(),
                                    PolicyFetchReason::kTest);
  if (local_test_infobar_visibility_manager_->infobar_active()) {
    local_test_infobar_visibility_manager_
        ->DismissInfobarsForActiveLocalTestPoliciesAllTabs();
  }
}

bool ProfilePolicyConnector::IsUsingLocalTestPolicyProvider() const {
  return local_test_policy_provider_ &&
         local_test_policy_provider_->is_active();
}

void ProfilePolicyConnector::RecordAffiliationMetrics() {
  const PolicyMap& chrome_policies = policy_service()->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  base::UmaHistogramBoolean("Enterprise.ProfileAffiliation.IsAffiliated",
                            chrome_policies.IsUserAffiliated());

  if (!chrome_policies.IsUserAffiliated()) {
    const auto reason = enterprise_util::GetUnaffiliatedReason(this);
    base::UmaHistogramEnumeration(
        "Enterprise.ProfileAffiliation.UnaffiliatedReason", reason);
  }

  // base::Unretained is safe because `this` owns the timer.
  management_status_metrics_timer_.Start(
      FROM_HERE, base::Days(7),
      base::BindRepeating(&ProfilePolicyConnector::RecordAffiliationMetrics,
                          base::Unretained(this)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<PolicyService>
ProfilePolicyConnector::CreatePolicyServiceWithInitializationThrottled(
    const std::vector<raw_ptr<ConfigurationPolicyProvider, VectorExperimental>>&
        policy_providers,
    std::vector<std::unique_ptr<PolicyMigrator>> migrators,
    ConfigurationPolicyProvider* user_policy_delegate) {
  DCHECK(user_policy_delegate);

  auto policy_service = PolicyServiceImpl::CreateWithThrottledInitialization(
      policy_providers, PolicyServiceImpl::ScopeForMetrics::kUser,
      std::move(migrators));

  // base::Unretained is OK for |this| because
  // |proxied_policies_propagated_watcher_| is guaranteed not to call its
  // callback after it has been destroyed. base::Unretained is also OK for
  // |policy_service.get()| because it will be owned by |*this| and is never
  // explicitly destroyed.
  proxied_policies_propagated_watcher_ =
      std::make_unique<internal::ProxiedPoliciesPropagatedWatcher>(
          GetDeviceWidePolicyService(), GetProxyPolicyProvider(),
          user_policy_delegate,
          base::BindOnce(&ProfilePolicyConnector::OnProxiedPoliciesPropagated,
                         base::Unretained(this),
                         base::Unretained(policy_service.get())));
  return std::move(policy_service);
}

void ProfilePolicyConnector::OnProxiedPoliciesPropagated(
    PolicyServiceImpl* policy_service) {
  policy_service->UnthrottleInitialization();
  // Do not delete |proxied_policies_propagated_watcher_| synchronously, as the
  // PolicyService it is observing is expected to be iterating its observer
  // list.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(proxied_policies_propagated_watcher_));
}
#endif

}  // namespace policy
