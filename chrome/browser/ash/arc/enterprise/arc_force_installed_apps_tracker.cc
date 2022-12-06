// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_force_installed_apps_tracker.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"

namespace arc {
namespace data_snapshotd {

namespace {

// Gets the profile from ArcSessionManager.
Profile* GetProfile() {
  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  return session_manager->profile();
}

}  // namespace

// This class starts observing force-installed/required ARC apps installation
// steps on creation and stops on destruction.
// It notifies back the caller via passed |update_callback| if the number of
// installed observing apps changes.
class ArcForceInstalledAppsObserver : public ArcAppListPrefs::Observer,
                                      public policy::PolicyService::Observer {
 public:
  ArcForceInstalledAppsObserver(
      ArcAppListPrefs* prefs,
      policy::PolicyService* policy_service,
      base::RepeatingCallback<void(int)> update_callback);
  ArcForceInstalledAppsObserver(const ArcForceInstalledAppsObserver&) = delete;
  ArcForceInstalledAppsObserver& operator=(
      const ArcForceInstalledAppsObserver&) = delete;
  ~ArcForceInstalledAppsObserver() override;

  // ArcAppListPrefs::Observer overrides:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;

  // PolicyService::Observer overrides.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

 private:
  // Update the list of installed packages if the list of tracking packages is
  // changed.
  void UpdateInstalledPackages();

  // Returns a [0..100] number - the percentage of installed packages among
  // tracking packages.
  // If there are no tracking packages, returns 100%, since no packages are
  // awaited to be installed.
  int CalculateInstallationProgress();

  // Not owned singleton. Initialized in ctor.
  ArcAppListPrefs* prefs_ = nullptr;
  // Not owned singleton. Initialized in ctor.
  policy::PolicyService* policy_service_ = nullptr;
  // This callback is invoked when the number of installed tracking packages is
  // changed. The floored percentage of installed tracking packages is passed to
  // the callback.
  base::RepeatingCallback<void(int)> update_callback_;

  // Maps tracking package names into whether the package is installed.
  base::flat_map<std::string, bool> tracking_packages_;
  // Number of installed packages among tracking packages.
  int installed_packages_num_ = 0;
};

class PolicyComplianceObserver : public arc::ArcPolicyBridge::Observer {
 public:
  PolicyComplianceObserver(arc::ArcPolicyBridge* arc_policy_bridge,
                           base::OnceClosure finish_callback);
  PolicyComplianceObserver& operator=(const PolicyComplianceObserver&) = delete;
  PolicyComplianceObserver(const PolicyComplianceObserver&) = delete;

  ~PolicyComplianceObserver() override;

  // arc::ArcPolicyBridge::Observer override:
  void OnComplianceReportReceived(
      const base::Value* compliance_report) override;

 private:
  // Parses initial compliance report JSON.
  void ProcessInitialComplianceReport(std::string last_report);

  // Not owned singleton. Initialized in ctor.
  arc::ArcPolicyBridge* arc_policy_bridge_ = nullptr;

  // This callback is invoked once ARC is compliant with ARC policy.
  base::OnceClosure finish_callback_;

  base::WeakPtrFactory<PolicyComplianceObserver> weak_ptr_factory_{this};
};

ArcForceInstalledAppsObserver::ArcForceInstalledAppsObserver(
    ArcAppListPrefs* prefs,
    policy::PolicyService* policy_service,
    base::RepeatingCallback<void(int)> update_callback)
    : prefs_(prefs),
      policy_service_(policy_service),
      update_callback_(std::move(update_callback)) {
  DCHECK(prefs_);
  DCHECK(policy_service_);
  prefs_->AddObserver(this);
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  // Initialize the tracking and installed packages lists.
  const policy::PolicyNamespace policy_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  OnPolicyUpdated(policy_namespace, policy::PolicyMap(),
                  policy_service_->GetPolicies(policy_namespace));
}

ArcForceInstalledAppsObserver::~ArcForceInstalledAppsObserver() {
  prefs_->RemoveObserver(this);
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

void ArcForceInstalledAppsObserver::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  auto iter = tracking_packages_.find(package_info.package_name);
  if (iter == tracking_packages_.end()) {
    // Installed non-required/force-installed ARC system package.
    VLOG(1) << "Installed not tracking package " << package_info.package_name;
    return;
  }
  bool& installed = iter->second;
  if (!installed) {
    // If installed package is among tracking packages and not yet installed,
    // mark it as installed.
    installed = true;
    installed_packages_num_++;
    update_callback_.Run(CalculateInstallationProgress());
  }
}

void ArcForceInstalledAppsObserver::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  auto iter = tracking_packages_.find(package_name);
  if (uninstalled && iter != tracking_packages_.end() && iter->second) {
    // If an installed package is removed, proceed.
    iter->second = false;
    installed_packages_num_--;
    update_callback_.Run(CalculateInstallationProgress());
  }
}

void ArcForceInstalledAppsObserver::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {
  if (ns.domain != policy::POLICY_DOMAIN_CHROME)
    return;
  const base::Value* const arc_policy =
      current.GetValue(policy::key::kArcPolicy, base::Value::Type::STRING);
  tracking_packages_.clear();

  // Track packages only if ArcPolicy is set.
  if (arc_policy) {
    // Get the required packages from ArcPolicy.
    auto required_packages =
        arc::policy_util::GetRequestedPackagesFromArcPolicy(
            arc_policy->GetString());
    // Mark all required packages not yet installed in |tracking_packages_|.
    tracking_packages_ = base::MakeFlatMap<std::string, bool>(
        required_packages, {},
        [](const std::string& v) { return std::make_pair(v, false); });
  }
  UpdateInstalledPackages();
}

void ArcForceInstalledAppsObserver::UpdateInstalledPackages() {
  DCHECK(prefs_);
  installed_packages_num_ = 0;
  auto all_installed_packages = prefs_->GetPackagesFromPrefs();
  for (const auto& package_name : all_installed_packages) {
    auto package = tracking_packages_.find(package_name);
    if (package != tracking_packages_.end() && !package->second) {
      // If tracking this package, mark it as installed.
      package->second = true;
      installed_packages_num_++;
    }
  }
  if (!update_callback_.is_null())
    update_callback_.Run(CalculateInstallationProgress());
}

int ArcForceInstalledAppsObserver::CalculateInstallationProgress() {
  return tracking_packages_.empty()
             ? 100
             : installed_packages_num_ * 100 / tracking_packages_.size();
}

PolicyComplianceObserver::PolicyComplianceObserver(
    arc::ArcPolicyBridge* arc_policy_bridge,
    base::OnceClosure finish_callback)
    : arc_policy_bridge_(arc_policy_bridge),
      finish_callback_(std::move(finish_callback)) {
  DCHECK(arc_policy_bridge);
  arc_policy_bridge_->AddObserver(this);

  // Check last compliance report.
  auto last_report = arc_policy_bridge->get_arc_policy_compliance_report();
  if (last_report.empty())
    return;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PolicyComplianceObserver::ProcessInitialComplianceReport,
                     weak_ptr_factory_.GetWeakPtr(), std::move(last_report)));
}

PolicyComplianceObserver::~PolicyComplianceObserver() {
  arc_policy_bridge_->RemoveObserver(this);
}

void PolicyComplianceObserver::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  const base::Value* const details = compliance_report->FindKeyOfType(
      "nonComplianceDetails", base::Value::Type::LIST);
  if (!details) {
    // ARC policy compliant.
    if (!finish_callback_.is_null())
      std::move(finish_callback_).Run();
    return;
  }

  bool is_android_id_reset = true;
  for (const auto& detail : details->GetList()) {
    const base::Value* const reason =
        detail.FindKeyOfType("nonComplianceReason", base::Value::Type::INTEGER);
    const std::string* const settingName = detail.FindStringKey("settingName");
    if (!reason || !settingName)
      continue;
    // Not compliant with ARC applications policy.
    if (*settingName == ArcPolicyBridge::kApplications)
      return;
    // android_id is expected to be reset, but still not reset by clouddpc.
    if (*settingName == ArcPolicyBridge::kResetAndroidIdEnabled) {
      is_android_id_reset = false;
      continue;
    }
  }
  // If compliant with ARC applications policy, android ID is expected to be
  // reset shortly.
  // Force a compliance report.
  if (!is_android_id_reset) {
    arc_policy_bridge_->OnPolicyUpdated(
        policy::PolicyNamespace(), policy::PolicyMap(), policy::PolicyMap());
    return;
  }
  if (!finish_callback_.is_null())
    std::move(finish_callback_).Run();
}

void PolicyComplianceObserver::ProcessInitialComplianceReport(
    std::string last_report) {
  absl::optional<base::Value> last_report_value =
      base::JSONReader::Read(last_report);
  if (!last_report_value.has_value())
    return;
  OnComplianceReportReceived(&last_report_value.value());
}

ArcForceInstalledAppsTracker::ArcForceInstalledAppsTracker() = default;

ArcForceInstalledAppsTracker::~ArcForceInstalledAppsTracker() = default;

// static
std::unique_ptr<ArcForceInstalledAppsTracker>
ArcForceInstalledAppsTracker::CreateForTesting(
    ArcAppListPrefs* prefs,
    policy::PolicyService* policy_service,
    arc::ArcPolicyBridge* arc_policy_bridge) {
  return base::WrapUnique(new ArcForceInstalledAppsTracker(
      prefs, policy_service, arc_policy_bridge));
}

void ArcForceInstalledAppsTracker::StartTracking(
    base::RepeatingCallback<void(int)> update_callback,
    base::OnceClosure finish_callback) {
  DCHECK(!apps_observer_);
  DCHECK(!policy_compliance_observer_);

  Initialize();
  apps_observer_ = std::make_unique<ArcForceInstalledAppsObserver>(
      prefs_, policy_service_, std::move(update_callback));
  policy_compliance_observer_ = std::make_unique<PolicyComplianceObserver>(
      arc_policy_bridge_,
      base::BindOnce(&ArcForceInstalledAppsTracker::OnTrackingFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(finish_callback)));
}

ArcForceInstalledAppsTracker::ArcForceInstalledAppsTracker(
    ArcAppListPrefs* prefs,
    policy::PolicyService* policy_service,
    arc::ArcPolicyBridge* arc_policy_bridge)
    : prefs_(prefs),
      policy_service_(policy_service),
      arc_policy_bridge_(arc_policy_bridge) {}

void ArcForceInstalledAppsTracker::Initialize() {
  if (prefs_ && policy_service_ && arc_policy_bridge_)
    return;
  auto* profile = GetProfile();
  DCHECK(profile);

  prefs_ = ArcAppListPrefs::Get(profile);

  auto* profile_policy_connector = profile->GetProfilePolicyConnector();
  DCHECK(profile_policy_connector);

  policy_service_ = profile_policy_connector->policy_service();

  arc_policy_bridge_ = arc::ArcPolicyBridge::GetForBrowserContext(profile);
}

void ArcForceInstalledAppsTracker::OnTrackingFinished(
    base::OnceClosure finish_callback) {
  apps_observer_.reset();
  policy_compliance_observer_.reset();

  std::move(finish_callback).Run();
}

}  // namespace data_snapshotd
}  // namespace arc
