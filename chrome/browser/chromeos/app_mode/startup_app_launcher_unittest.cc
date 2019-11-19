// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/version.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_external_loader.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/chromeos/extensions/test_external_cache.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/version_info/channel.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest.h"
#include "url/gurl.h"

using extensions::ExternalInstallInfoFile;
using extensions::ExternalInstallInfoUpdateUrl;
using extensions::Manifest;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

namespace chromeos {

namespace {

constexpr char kTestPrimaryAppId[] = "abcdefghabcdefghabcdefghabcdefgh";

constexpr char kSecondaryAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";

constexpr char kExtraSecondaryAppId[] = "aaaaccccaaaaccccaaaaccccaaaacccc";

constexpr char kTestUserAccount[] = "user@test";

enum class LaunchState {
  kNotStarted,
  kInitializingNetwork,
  kInstallingApp,
  kReadyToLaunch,
  kLaunchSucceeded,
  kLaunchFailed
};

class TestAppLaunchDelegate : public StartupAppLauncher::Delegate {
 public:
  TestAppLaunchDelegate() = default;
  ~TestAppLaunchDelegate() override = default;

  const std::vector<LaunchState>& launch_state_changes() const {
    return launch_state_changes_;
  }
  KioskAppLaunchError::Error launch_error() const { return launch_error_; }

  void set_network_ready(bool network_ready) { network_ready_ = network_ready; }
  void set_should_skip_app_installation(bool skip_app_installation) {
    should_skip_app_installation_ = skip_app_installation;
  }
  void set_showing_network_config_screen(bool showing) {
    showing_network_config_screen_ = showing;
  }

  void ClearLaunchStateChanges() { launch_state_changes_.clear(); }

  void WaitForLaunchStates(const std::set<LaunchState>& states) {
    if (states.count(launch_state_))
      return;

    ASSERT_FALSE(run_loop_.get());

    waiting_for_launch_states_ = states;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // StartupAppLauncher::Delegate:
  void InitializeNetwork() override {
    SetLaunchState(LaunchState::kInitializingNetwork);
  }
  bool IsNetworkReady() override { return network_ready_; }
  bool ShouldSkipAppInstallation() override {
    return should_skip_app_installation_;
  }
  void OnInstallingApp() override {
    SetLaunchState(LaunchState::kInstallingApp);
  }
  void OnReadyToLaunch() override {
    SetLaunchState(LaunchState::kReadyToLaunch);
  }
  void OnLaunchSucceeded() override {
    SetLaunchState(LaunchState::kLaunchSucceeded);
  }
  void OnLaunchFailed(KioskAppLaunchError::Error error) override {
    launch_error_ = error;
    SetLaunchState(LaunchState::kLaunchFailed);
  }
  bool IsShowingNetworkConfigScreen() override {
    return showing_network_config_screen_;
  }

 private:
  void SetLaunchState(LaunchState state) {
    launch_state_changes_.push_back(state);
    launch_state_ = state;

    if (run_loop_ && waiting_for_launch_states_.count(state)) {
      waiting_for_launch_states_.clear();
      std::move(run_loop_)->Quit();
    }
  }

  LaunchState launch_state_ = LaunchState::kNotStarted;
  std::vector<LaunchState> launch_state_changes_;
  KioskAppLaunchError::Error launch_error_ = KioskAppLaunchError::NONE;

  bool network_ready_ = false;
  bool showing_network_config_screen_ = false;

  bool should_skip_app_installation_ = false;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::set<LaunchState> waiting_for_launch_states_;

  DISALLOW_COPY_AND_ASSIGN(TestAppLaunchDelegate);
};

class AppLaunchTracker : public extensions::TestEventRouter::EventObserver {
 public:
  AppLaunchTracker(const std::string& app_id,
                   extensions::TestEventRouter* event_router)
      : app_id_(app_id), event_router_(event_router) {
    event_router->AddEventObserver(this);
  }
  ~AppLaunchTracker() override { event_router_->RemoveEventObserver(this); }

  int kiosk_launch_count() const { return kiosk_launch_count_; }

  // TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    ADD_FAILURE() << "Unexpected broadcast " << event.event_name;
  }

  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const extensions::Event& event) override {
    ASSERT_EQ(extension_id, app_id_);

    ASSERT_EQ(event.event_name,
              extensions::api::app_runtime::OnLaunched::kEventName);
    ASSERT_TRUE(event.event_args);
    ASSERT_EQ(1u, event.event_args->GetList().size());

    const base::Value& launch_data = event.event_args->GetList()[0];
    const base::Value* is_kiosk_session =
        launch_data.FindKeyOfType("isKioskSession", base::Value::Type::BOOLEAN);
    ASSERT_TRUE(is_kiosk_session);
    EXPECT_TRUE(is_kiosk_session->GetBool());
    ++kiosk_launch_count_;
  }

 private:
  const std::string app_id_;
  extensions::TestEventRouter* event_router_;
  int kiosk_launch_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchTracker);
};

// Simulates extension service behavior related to external extensions loading,
// but does not initiate found extension's CRX installation - instead, it keeps
// track of pending extension installations, and expect the test code to finish
// the pending extension installations.
class TestKioskLoaderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  TestKioskLoaderVisitor(content::BrowserContext* browser_context,
                         extensions::ExtensionRegistry* extension_registry,
                         extensions::ExtensionService* extension_service)
      : browser_context_(browser_context),
        extension_registry_(extension_registry),
        extension_service_(extension_service) {}

  ~TestKioskLoaderVisitor() override = default;

  const std::set<std::string>& pending_crx_files() const {
    return pending_crx_files_;
  }
  const std::set<std::string>& pending_update_urls() const {
    return pending_update_urls_;
  }

  bool FinishPendingInstall(const extensions::Extension* extension) {
    if (!pending_crx_files_.count(extension->id()) &&
        !pending_update_urls_.count(extension->id())) {
      return false;
    }

    if (!extension_service_->pending_extension_manager()->IsIdPending(
            extension->id())) {
      return false;
    }

    pending_crx_files_.erase(extension->id());
    pending_update_urls_.erase(extension->id());
    extension_service_->OnExtensionInstalled(
        extension, syncer::StringOrdinal::CreateInitialOrdinal(),
        extensions::kInstallFlagInstallImmediately);
    extensions::InstallTracker::Get(browser_context_)
        ->OnFinishCrxInstall(extension->id(), true);
    return true;
  }

  bool FailPendingInstall(const std::string& extension_id) {
    if (!pending_crx_files_.count(extension_id) &&
        !pending_update_urls_.count(extension_id)) {
      return false;
    }

    if (!extension_service_->pending_extension_manager()->IsIdPending(
            extension_id)) {
      return false;
    }

    pending_crx_files_.erase(extension_id);
    pending_update_urls_.erase(extension_id);
    extensions::InstallTracker::Get(browser_context_)
        ->OnFinishCrxInstall(extension_id, false);
    return true;
  }

  // extensions::ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override {
    const extensions::Extension* existing =
        extension_registry_->GetExtensionById(
            info.extension_id, extensions::ExtensionRegistry::EVERYTHING);
    // Alredy exists, and does not require update.
    if (existing && existing->version().CompareTo(info.version) >= 0)
      return false;

    if (!extension_service_->pending_extension_manager()->AddFromExternalFile(
            info.extension_id, info.crx_location, info.version,
            info.creation_flags, info.mark_acknowledged)) {
      return false;
    }

    pending_crx_files_.insert(info.extension_id);
    extensions::InstallTracker::Get(browser_context_)
        ->OnBeginCrxInstall(info.extension_id);
    return true;
  }
  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool is_initial_load) override {
    if (extension_registry_->GetExtensionById(
            info.extension_id, extensions::ExtensionRegistry::EVERYTHING))
      return false;

    if (!extension_service_->pending_extension_manager()
             ->AddFromExternalUpdateUrl(
                 info.extension_id, info.install_parameter, info.update_url,
                 info.download_location, info.creation_flags,
                 info.mark_acknowledged)) {
      return false;
    }

    pending_update_urls_.insert(info.extension_id);
    extensions::InstallTracker::Get(browser_context_)
        ->OnBeginCrxInstall(info.extension_id);
    return true;
  }
  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override {}
  void OnExternalProviderUpdateComplete(
      const extensions::ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    for (const auto& extension : update_url_extensions)
      OnExternalExtensionUpdateUrlFound(extension, false);

    for (const auto& extension : file_extensions)
      OnExternalExtensionFileFound(extension);

    for (const auto& extension_id : removed_extensions) {
      extension_service_->UninstallExtension(
          extension_id,
          extensions::UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, nullptr);
    }
  }

 private:
  content::BrowserContext* const browser_context_;
  extensions::ExtensionRegistry* const extension_registry_;
  extensions::ExtensionService* const extension_service_;

  std::set<std::string> pending_crx_files_;
  std::set<std::string> pending_update_urls_;

  DISALLOW_COPY_AND_ASSIGN(TestKioskLoaderVisitor);
};

}  // namespace

class StartupAppLauncherTest : public extensions::ExtensionServiceTestBase,
                               public KioskAppManager::Overrides {
 public:
  StartupAppLauncherTest() = default;
  ~StartupAppLauncherTest() override = default;

  // testing::Test:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kForceAppMode);
    command_line_.GetProcessCommandLine()->AppendSwitch(switches::kAppId);

    KioskAppManager::InitializeForTesting(this);

    InitializePrimaryAppState();

    extensions::ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    external_apps_loader_handler_ = std::make_unique<TestKioskLoaderVisitor>(
        browser_context(), registry(), service());
    CreateAndInitializeKioskAppsProviders(external_apps_loader_handler_.get());

    extensions::TestEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter(browser_context());
    app_launch_tracker_ =
        std::make_unique<AppLaunchTracker>(kTestPrimaryAppId, event_router);

    startup_app_launcher_ = std::make_unique<StartupAppLauncher>(
        profile(), kTestPrimaryAppId, false /*diagnostic_mode*/,
        &startup_launch_delegate_);
  }

  void TearDown() override {
    startup_app_launcher_.reset();
    external_cache_ = nullptr;

    primary_app_provider_->ServiceShutdown();
    secondary_apps_provider_->ServiceShutdown();
    external_apps_loader_handler_.reset();

    app_launch_tracker_.reset();

    KioskAppManager::Shutdown();

    accounts_settings_helper_.reset();

    extensions::ExtensionServiceTestBase::TearDown();
  }

  // KioskAppManager::Overrides:
  std::unique_ptr<ExternalCache> CreateExternalCache(
      ExternalCacheDelegate* delegate,
      bool always_check_updates) override {
    auto cache =
        std::make_unique<TestExternalCache>(delegate, always_check_updates);
    external_cache_ = cache.get();
    return cache;
  }

  std::unique_ptr<AppSession> CreateAppSession() override {
    EXPECT_FALSE(kiosk_app_session_initialized_);
    kiosk_app_session_initialized_ = true;
    return nullptr;
  }

 protected:
  // Note: These tests should not actually create files, so the actual returned
  // path is not too important. Still, putting it under the test's temp dir, in
  // case something unexpectedly tries to do file I/O with the file paths
  // returned here.
  std::string GetExtensionPath(const std::string& app_id) {
    return temp_dir()
        .GetPath()
        .AppendASCII("test_crx_file")
        .AppendASCII(app_id)
        .value();
  }

  void InitializeLauncherWithNetworkReady() {
    startup_app_launcher_->Initialize();
    EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInitializingNetwork}),
              startup_launch_delegate_.launch_state_changes());
    startup_launch_delegate_.ClearLaunchStateChanges();

    startup_launch_delegate_.set_network_ready(true);
    startup_app_launcher_->ContinueWithNetworkReady();
    EXPECT_TRUE(startup_launch_delegate_.launch_state_changes().empty());
    startup_launch_delegate_.ClearLaunchStateChanges();
  }

  AssertionResult DownloadPrimaryApp(
      const TestKioskExtensionBuilder& app_builder) WARN_UNUSED_RESULT {
    return DownloadPrimaryApp(app_builder.extension_id(),
                              app_builder.version());
  }

  AssertionResult DownloadPrimaryApp(const std::string& app_id,
                                     const std::string& version)
      WARN_UNUSED_RESULT {
    if (!external_cache_)
      return AssertionFailure() << "External cache not initialized";

    if (!external_cache_->pending_downloads().count(app_id))
      return AssertionFailure() << "Download not pending: " << app_id;

    if (!external_cache_->SimulateExtensionDownloadFinished(
            app_id, GetExtensionPath(app_id), version)) {
      return AssertionFailure() << " Finish download attempt failed";
    }

    return AssertionSuccess();
  }

  AssertionResult FinishPrimaryAppInstall(
      const TestKioskExtensionBuilder& app_builder) WARN_UNUSED_RESULT {
    const std::string& id = app_builder.extension_id();
    if (!external_apps_loader_handler_->pending_crx_files().count(id))
      return AssertionFailure() << "App install not peding: " << id;

    scoped_refptr<const extensions::Extension> app = app_builder.Build();
    if (!app)
      return AssertionFailure() << "App builder failed: " << id;

    if (!external_apps_loader_handler_->FinishPendingInstall(app.get()))
      return AssertionFailure() << "Finish install attempt failed: " << id;

    return AssertionSuccess();
  }

  AssertionResult DownloadAndInstallPrimaryApp(
      const TestKioskExtensionBuilder& app_builder) WARN_UNUSED_RESULT {
    AssertionResult download_result = DownloadPrimaryApp(app_builder);
    if (!download_result)
      return download_result;

    AssertionResult install_result = FinishPrimaryAppInstall(app_builder);
    if (!install_result)
      return install_result;

    return AssertionSuccess();
  }

  AssertionResult FinishSecondaryExtensionInstall(
      const TestKioskExtensionBuilder& builder) WARN_UNUSED_RESULT {
    const std::string& id = builder.extension_id();
    if (!external_apps_loader_handler_->pending_update_urls().count(id)) {
      return AssertionFailure()
             << "Secondary extension install not pending: " << id;
    }

    scoped_refptr<const extensions::Extension> extension = builder.Build();
    if (!extension)
      return AssertionFailure() << "Extension builder failed: " << id;

    if (!external_apps_loader_handler_->FinishPendingInstall(extension.get()))
      return AssertionFailure() << "Finish install attempt failed: " << id;

    return AssertionSuccess();
  }

 private:
  void InitializePrimaryAppState() {
    // Inject test kiosk app data to prevent KioskAppManager from attempting to
    // load it.
    // TODO(tbarzic): Introducing a test KioskAppData class that overrides app
    //     data load logic, and injecting a KioskAppData object factory to
    //     KioskAppManager would be a cleaner solution here.
    KioskAppManager::Get()->AddAppForTest(
        kTestPrimaryAppId, AccountId::FromUserEmail(kTestUserAccount),
        GURL("http://cws/"), "");

    accounts_settings_helper_ = std::make_unique<ScopedCrosSettingsTestHelper>(
        false /*create_service*/);
    accounts_settings_helper_->ReplaceDeviceSettingsProviderWithStub();

    auto account = std::make_unique<base::DictionaryValue>();
    account->SetKey(kAccountsPrefDeviceLocalAccountsKeyId,
                    base::Value(kTestUserAccount));
    account->SetKey(kAccountsPrefDeviceLocalAccountsKeyType,
                    base::Value(policy::DeviceLocalAccount::TYPE_KIOSK_APP));
    account->SetKey(kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                    base::Value(kTestPrimaryAppId));
    base::ListValue accounts;
    accounts.Append(std::move(account));

    accounts_settings_helper_->Set(kAccountsPrefDeviceLocalAccounts, accounts);
    accounts_settings_helper_->SetString(
        kAccountsPrefDeviceLocalAccountAutoLoginId, kTestUserAccount);
    accounts_settings_helper_->SetInteger(
        kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
  }

  void CreateAndInitializeKioskAppsProviders(TestKioskLoaderVisitor* visitor) {
    primary_app_provider_ = std::make_unique<extensions::ExternalProviderImpl>(
        visitor,
        base::MakeRefCounted<KioskAppExternalLoader>(
            KioskAppExternalLoader::AppClass::kPrimary),
        profile(), extensions::Manifest::EXTERNAL_POLICY,
        extensions::Manifest::INVALID_LOCATION,
        extensions::Extension::NO_FLAGS);
    InitializeKioskAppsProvider(primary_app_provider_.get());

    secondary_apps_provider_ =
        std::make_unique<extensions::ExternalProviderImpl>(
            visitor,
            base::MakeRefCounted<KioskAppExternalLoader>(
                KioskAppExternalLoader::AppClass::kSecondary),
            profile(), extensions::Manifest::EXTERNAL_PREF,
            extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
            extensions::Extension::NO_FLAGS);
    InitializeKioskAppsProvider(secondary_apps_provider_.get());
  }

  void InitializeKioskAppsProvider(extensions::ExternalProviderImpl* provider) {
    provider->set_auto_acknowledge(true);
    provider->set_install_immediately(true);
    provider->set_allow_updates(true);
    provider->VisitRegisteredExtension();
  }

 protected:
  TestAppLaunchDelegate startup_launch_delegate_;

  std::unique_ptr<StartupAppLauncher> startup_app_launcher_;
  std::unique_ptr<AppLaunchTracker> app_launch_tracker_;
  std::unique_ptr<TestKioskLoaderVisitor> external_apps_loader_handler_;

  TestExternalCache* external_cache_ = nullptr;

  bool kiosk_app_session_initialized_ = false;
  session_manager::SessionManager session_manager_;

 private:
  base::test::ScopedCommandLine command_line_;

  std::unique_ptr<ScopedCrosSettingsTestHelper> accounts_settings_helper_;

  std::unique_ptr<extensions::ExternalProviderImpl> primary_app_provider_;
  std::unique_ptr<extensions::ExternalProviderImpl> secondary_apps_provider_;

  DISALLOW_COPY_AND_ASSIGN(StartupAppLauncherTest);
};

TEST_F(StartupAppLauncherTest, PrimaryAppLaunchFlow) {
  InitializeLauncherWithNetworkReady();

  ASSERT_TRUE(external_cache_);
  EXPECT_EQ(std::set<std::string>({kTestPrimaryAppId}),
            external_cache_->pending_downloads());

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  ASSERT_TRUE(DownloadPrimaryApp(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_builder));

  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, OfflineLaunchWithPrimaryAppPreInstalled) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  startup_app_launcher_->Initialize();

  // Given that the app is offline enabled and installed, the app should be
  // launched immediately, without waiting for network or checking for updates.
  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());

  // Primary app cache checks finished after the startup app launcher reports
  // it's ready should be ignored - i.e. startup app launcher should not attempt
  // to relaunch the app, nor request the update installation.
  startup_app_launcher_->ContinueWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(kTestPrimaryAppId, "1.1"));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(startup_launch_delegate_.launch_state_changes().empty());

  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest,
       OfflineLaunchWithPrimaryAppPreInstalled_UpdateFoundAfterLaunch) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  startup_app_launcher_->Initialize();

  // Given that the app is offline enabled and installed, the app should be
  // launched immediately, without waiting for network or checking for updates.
  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());

  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());

  // Primary app cache checks finished after the app launch
  // it's ready should be ignored - i.e. startup app launcher should not attempt
  // to relaunch the app, nor request the update installation.
  startup_app_launcher_->ContinueWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(kTestPrimaryAppId, "1.1"));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(startup_launch_delegate_.launch_state_changes().empty());
}

TEST_F(StartupAppLauncherTest, PrimaryAppDownloadFailure) {
  InitializeLauncherWithNetworkReady();

  ASSERT_TRUE(external_cache_);
  EXPECT_EQ(std::set<std::string>({kTestPrimaryAppId}),
            external_cache_->pending_downloads());
  ASSERT_TRUE(
      external_cache_->SimulateExtensionDownloadFailed(kTestPrimaryAppId));

  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchFailed}),
            startup_launch_delegate_.launch_state_changes());

  EXPECT_EQ(KioskAppLaunchError::UNABLE_TO_DOWNLOAD,
            startup_launch_delegate_.launch_error());

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, PrimaryAppCrxInstallFailure) {
  InitializeLauncherWithNetworkReady();

  ASSERT_TRUE(DownloadPrimaryApp(kTestPrimaryAppId, "1.0"));
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(
      external_apps_loader_handler_->FailPendingInstall(kTestPrimaryAppId));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchFailed}),
            startup_launch_delegate_.launch_state_changes());

  EXPECT_EQ(KioskAppLaunchError::UNABLE_TO_INSTALL,
            startup_launch_delegate_.launch_error());

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, PrimaryAppNotKioskEnabled) {
  InitializeLauncherWithNetworkReady();

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_kiosk_enabled(false);
  ASSERT_TRUE(DownloadPrimaryApp(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchFailed}),
            startup_launch_delegate_.launch_state_changes());

  EXPECT_EQ(KioskAppLaunchError::NOT_KIOSK_ENABLED,
            startup_launch_delegate_.launch_error());

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, PrimaryAppIsExtension) {
  InitializeLauncherWithNetworkReady();

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_EXTENSION,
                                                kTestPrimaryAppId);
  ASSERT_TRUE(DownloadPrimaryApp(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchFailed}),
            startup_launch_delegate_.launch_state_changes());

  EXPECT_EQ(KioskAppLaunchError::NOT_KIOSK_ENABLED,
            startup_launch_delegate_.launch_error());

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, LaunchWithSecondaryApps) {
  InitializeLauncherWithNetworkReady();

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(
      kExtraSecondaryAppId, false);

  ASSERT_TRUE(DownloadPrimaryApp(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_builder));

  // Installing primary app with a non-installed secondary app should notify
  // delegate about pending app installation - in this case for secondary app.
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  TestKioskExtensionBuilder secondary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                  kSecondaryAppId);
  secondary_app_builder.set_kiosk_enabled(false);
  ASSERT_TRUE(FinishSecondaryExtensionInstall(secondary_app_builder));

  TestKioskExtensionBuilder disabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kExtraSecondaryAppId);
  ASSERT_TRUE(FinishSecondaryExtensionInstall(disabled_secondary_app_builder));

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kExtraSecondaryAppId));

  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, LaunchWithSecondaryExtension) {
  InitializeLauncherWithNetworkReady();

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);

  ASSERT_TRUE(DownloadPrimaryApp(primary_app_builder));

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_builder));

  // Installing primary app with a non-installed secondary app should notify
  // delegate about pending app installation - in this case for secondary app.
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kInstallingApp}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  TestKioskExtensionBuilder secondary_extension_builder(
      Manifest::TYPE_EXTENSION, kSecondaryAppId);
  secondary_extension_builder.set_kiosk_enabled(false);
  ASSERT_TRUE(FinishSecondaryExtensionInstall(secondary_extension_builder));

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, OfflineWithPrimaryAndSecondaryAppInstalled) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  TestKioskExtensionBuilder secondary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                  kSecondaryAppId);
  secondary_app_builder.set_kiosk_enabled(false);
  scoped_refptr<const extensions::Extension> secondary_app =
      secondary_app_builder.Build();
  service()->AddExtension(secondary_app.get());

  startup_app_launcher_->Initialize();

  // Given that the app is offline enabled and installed, the app should be
  // launched immediately, without waiting for network or checking for updates.
  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());

  // Primary app cache checks finished after the startup app launcher reports
  // it's ready should be ignored - i.e. startup app launcher should not attempt
  // to relaunch the app, nor request the update installation.
  startup_app_launcher_->ContinueWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(kTestPrimaryAppId, "1.1"));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(startup_launch_delegate_.launch_state_changes().empty());

  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, IgnoreSecondaryAppsSecondaryApps) {
  InitializeLauncherWithNetworkReady();

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);

  ASSERT_TRUE(DownloadAndInstallPrimaryApp(primary_app_builder));

  startup_launch_delegate_.ClearLaunchStateChanges();

  TestKioskExtensionBuilder secondary_extension_builder(
      Manifest::TYPE_PLATFORM_APP, kSecondaryAppId);
  secondary_extension_builder.set_kiosk_enabled(true);
  secondary_extension_builder.AddSecondaryExtension(kExtraSecondaryAppId);

  ASSERT_TRUE(FinishSecondaryExtensionInstall(secondary_extension_builder));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());
  startup_launch_delegate_.ClearLaunchStateChanges();

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kLaunchSucceeded}),
            startup_launch_delegate_.launch_state_changes());
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_FALSE(registry()->GetInstalledExtension(kExtraSecondaryAppId));

  EXPECT_TRUE(kiosk_app_session_initialized_);
  EXPECT_TRUE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest, SecondaryAppCrxInstallFailure) {
  InitializeLauncherWithNetworkReady();

  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);

  ASSERT_TRUE(DownloadAndInstallPrimaryApp(primary_app_builder));
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_EQ(std::set<std::string>({kSecondaryAppId}),
            external_apps_loader_handler_->pending_update_urls());
  ASSERT_TRUE(
      external_apps_loader_handler_->FailPendingInstall(kSecondaryAppId));

  EXPECT_EQ(KioskAppLaunchError::UNABLE_TO_INSTALL,
            startup_launch_delegate_.launch_error());

  EXPECT_FALSE(kiosk_app_session_initialized_);
  EXPECT_FALSE(session_manager_.IsSessionStarted());
}

TEST_F(StartupAppLauncherTest,
       SecondaryAppEnabledOnLaunchOverridesInstalledAppState) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId,
                                                               false);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(
      kExtraSecondaryAppId, true);
  primary_app_builder.set_version("1.0");

  // Add the secondary app that should be disabled on startup - make it enabled
  // initially, so the test can verify the app gets disabled regardless of the
  // initial state.
  TestKioskExtensionBuilder disabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kSecondaryAppId);
  scoped_refptr<const extensions::Extension> disabled_secondary_app =
      disabled_secondary_app_builder.Build();
  service()->AddExtension(disabled_secondary_app.get());

  // Add the secondary app that should be enabled on startup - make it disabled
  // initially, so the test can verify the app gets enabled regardless of the
  // initial state.
  TestKioskExtensionBuilder enabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kExtraSecondaryAppId);

  scoped_refptr<const extensions::Extension> enabled_secondary_app =
      enabled_secondary_app_builder.Build();
  service()->AddExtension(enabled_secondary_app.get());
  service()->DisableExtension(enabled_secondary_app->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadAndInstallPrimaryApp(primary_app_builder));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest,
       KeepInstalledAppStateWithNoEnabledOnLaunchProperty) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);
  primary_app_builder.AddSecondaryExtension(kExtraSecondaryAppId);
  primary_app_builder.set_version("1.0");

  TestKioskExtensionBuilder enabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kSecondaryAppId);
  scoped_refptr<const extensions::Extension> enabled_secondary_app =
      enabled_secondary_app_builder.Build();
  service()->AddExtension(enabled_secondary_app.get());

  TestKioskExtensionBuilder disabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kExtraSecondaryAppId);

  scoped_refptr<const extensions::Extension> disabled_secondary_app =
      disabled_secondary_app_builder.Build();
  service()->AddExtension(disabled_secondary_app.get());
  service()->DisableExtension(disabled_secondary_app->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadAndInstallPrimaryApp(primary_app_builder));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest,
       DoNotEnableSecondayAppsDisabledForNonUserActionReason) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId,
                                                               true);
  primary_app_builder.set_version("1.0");

  // Add the secondary app that should be enabled on startup - make it disabled
  // initially, so the test can verify the app gets enabled regardless of the
  // initial state.
  TestKioskExtensionBuilder secondary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                  kSecondaryAppId);

  // Disable the secodnary app for a reason different than user action - that
  // disable reason should not be overriden during the kiosk launch.
  scoped_refptr<const extensions::Extension> secondary_app =
      secondary_app_builder.Build();
  service()->AddExtension(secondary_app.get());
  service()->DisableExtension(
      secondary_app->id(),
      extensions::disable_reason::DISABLE_USER_ACTION |
          extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadAndInstallPrimaryApp(primary_app_builder));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, PrimaryAppUpdatesToDisabledOnLaunch) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);
  primary_app_builder.set_version("1.0");
  primary_app_builder.set_offline_enabled(false);
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  TestKioskExtensionBuilder secondary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                  kSecondaryAppId);
  scoped_refptr<const extensions::Extension> secondary_app =
      secondary_app_builder.Build();
  service()->AddExtension(secondary_app.get());

  TestKioskExtensionBuilder primary_app_update(Manifest::TYPE_PLATFORM_APP,
                                               kTestPrimaryAppId);
  primary_app_update.AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId,
                                                              false);
  primary_app_update.set_version("1.1");

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(primary_app_update));
  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_update));

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  startup_app_launcher_->LaunchApp();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, PrimaryAppUpdatesToEnabledOnLaunch) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId,
                                                               false);
  primary_app_builder.set_version("1.0");
  primary_app_builder.set_offline_enabled(false);
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  TestKioskExtensionBuilder secondary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                  kSecondaryAppId);
  scoped_refptr<const extensions::Extension> secondary_app =
      secondary_app_builder.Build();
  service()->AddExtension(secondary_app.get());
  service()->DisableExtension(secondary_app->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);

  TestKioskExtensionBuilder primary_app_update(Manifest::TYPE_PLATFORM_APP,
                                               kTestPrimaryAppId);
  primary_app_update.AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId,
                                                              true);
  primary_app_update.set_version("1.1");

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(primary_app_update));
  ASSERT_TRUE(FinishPrimaryAppInstall(primary_app_update));

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  startup_app_launcher_->LaunchApp();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, SecondaryExtensionStateOnSessionRestore) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId,
                                                               false);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(
      kExtraSecondaryAppId, true);
  primary_app_builder.set_version("1.0");
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  // Add the secondary app that should be disabled on launch - make it enabled
  // initially, and let test verify it remains enabled during the launch.
  TestKioskExtensionBuilder disabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kSecondaryAppId);
  scoped_refptr<const extensions::Extension> disabled_secondary_app =
      disabled_secondary_app_builder.Build();
  service()->AddExtension(disabled_secondary_app.get());

  // Add the secondary app that should be enabled on launch - make it disabled
  // initially, and let test verify the app remains disabled during the launch.
  TestKioskExtensionBuilder enabled_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kExtraSecondaryAppId);

  scoped_refptr<const extensions::Extension> enabled_secondary_app =
      enabled_secondary_app_builder.Build();
  service()->AddExtension(enabled_secondary_app.get());
  service()->DisableExtension(enabled_secondary_app->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);

  // This matches the delegate settings during session restart (e.g. after a
  // browser process crash).
  startup_launch_delegate_.set_should_skip_app_installation(true);
  startup_launch_delegate_.set_network_ready(true);
  startup_app_launcher_->Initialize();

  startup_launch_delegate_.WaitForLaunchStates({LaunchState::kReadyToLaunch});
  EXPECT_EQ(std::vector<LaunchState>({LaunchState::kReadyToLaunch}),
            startup_launch_delegate_.launch_state_changes());

  startup_app_launcher_->LaunchApp();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtraSecondaryAppId));
}

}  // namespace chromeos
