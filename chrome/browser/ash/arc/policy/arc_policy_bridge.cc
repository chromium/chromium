// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "arc_policy_util.h"
#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service_factory.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

constexpr char kPolicyCompliantJson[] = "{ \"policyCompliant\": true }";
constexpr char kPolicyAppInstallType[] = "installType";
constexpr char kPolicyAppInstallTypeForceInstalled[] = "FORCE_INSTALLED";
constexpr char kPolicyCaCertType[] = "X509";
constexpr char kPolicyRequiredKeyAlias[] = "alias";
constexpr char kPolicyPrivateKeyAlias[] = "privateKeyAlias";
constexpr char kPolicyPrivateKeyPackageNames[] = "packageNames";
constexpr char kPolicyPlayStoreModeSupervised[] = "SUPERVISED";
constexpr char kPolicyPlayStoreModeAllowList[] = "WHITELIST"; // nocheck

// invert_bool_value: If the Chrome policy and the ARC policy with boolean value
// have opposite semantics, set this to true so the bool is inverted before
// being added. Otherwise, set it to false.
void MapBoolToBool(const std::string& arc_policy_name,
                   const std::string& policy_name,
                   const policy::PolicyMap& policy_map,
                   bool invert_bool_value,
                   base::Value::Dict* filtered_policies) {
  if (!policy_map.IsPolicySet(policy_name)) {
    return;
  }

  const base::Value* const policy_value =
      policy_map.GetValue(policy_name, base::Value::Type::BOOLEAN);
  if (!policy_value) {
    NOTREACHED_IN_MIGRATION()
        << "Policy " << policy_name << " is not a boolean.";
    return;
  }
  filtered_policies->Set(arc_policy_name,
                         policy_value->GetBool() != invert_bool_value);
}

// int_true: value of Chrome OS policy for which arc policy is set to true.
// It is set to false for all other values.
void MapIntToBool(const std::string& arc_policy_name,
                  const std::string& policy_name,
                  const policy::PolicyMap& policy_map,
                  int int_true,
                  base::Value::Dict* filtered_policies) {
  if (!policy_map.IsPolicySet(policy_name)) {
    return;
  }

  const base::Value* const policy_value =
      policy_map.GetValue(policy_name, base::Value::Type::INTEGER);
  if (!policy_value) {
    NOTREACHED_IN_MIGRATION()
        << "Policy " << policy_name << " is not an integer.";
    return;
  }
  filtered_policies->Set(arc_policy_name, policy_value->GetInt() == int_true);
}

// |arc_policy_name| is only set if the |pref_name| pref is managed.
// int_true: value of Chrome OS pref for which arc policy is set to true.
// It is set to false for all other values.
void MapManagedIntPrefToBool(const std::string& arc_policy_name,
                             const std::string& pref_name,
                             const PrefService* profile_prefs,
                             int int_true,
                             base::Value::Dict* filtered_policies) {
  if (!profile_prefs->IsManagedPreference(pref_name)) {
    return;
  }

  filtered_policies->Set(arc_policy_name,
                         profile_prefs->GetInteger(pref_name) == int_true);
}

// Checks whether |policy_name| is present as an object and has all |fields|,
// Sets |arc_policy_name| to true only if the condition above is satisfied.
void MapObjectToPresenceBool(const std::string& arc_policy_name,
                             const std::string& policy_name,
                             const policy::PolicyMap& policy_map,
                             base::Value::Dict* filtered_policies,
                             const std::vector<std::string>& fields) {
  if (!policy_map.IsPolicySet(policy_name)) {
    return;
  }

  const base::Value* const policy_value =
      policy_map.GetValue(policy_name, base::Value::Type::DICT);
  if (!policy_value) {
    NOTREACHED_IN_MIGRATION()
        << "Policy " << policy_name << " is not an object.";
    return;
  }
  for (const auto& field : fields) {
    if (!policy_value->GetDict().contains(field)) {
      return;
    }
  }
  filtered_policies->Set(arc_policy_name, true);
}

void AddOncCaCertsToPolicies(const policy::PolicyMap& policy_map,
                             base::Value::Dict* filtered_policies) {
  const base::Value* const policy_value = policy_map.GetValue(
      policy::key::kArcCertificatesSyncMode, base::Value::Type::INTEGER);
  // Old certs should be uninstalled if the sync is disabled or policy is not
  // set.
  if (!policy_value ||
      policy_value->GetInt() != ArcCertsSyncMode::COPY_CA_CERTS) {
    return;
  }

  if (!policy_map.IsPolicySet(policy::key::kOpenNetworkConfiguration)) {
    VLOG(1) << "onc policy is not set.";
    return;
  }

  // Importing CA certificates from device policy is not allowed.
  // Import only from user policy.
  const base::Value* onc_policy_value = policy_map.GetValue(
      policy::key::kOpenNetworkConfiguration, base::Value::Type::STRING);
  if (!onc_policy_value) {
    LOG(ERROR) << "Value of onc policy has invalid format.";
    return;
  }

  const std::string& onc_blob = onc_policy_value->GetString();
  base::Value::List certificates;
  {
    base::Value::List unused_network_configs;
    base::Value::Dict unused_global_network_config;
    if (!chromeos::onc::ParseAndValidateOncForImport(
            onc_blob, onc::ONCSource::ONC_SOURCE_USER_POLICY,
            "" /* no passphrase */, &unused_network_configs,
            &unused_global_network_config, &certificates)) {
      LOG(ERROR) << "Value of onc policy has invalid format =" << onc_blob;
    }
  }

  base::Value::List ca_certs;
  for (const auto& certificate : certificates) {
    if (!certificate.is_dict()) {
      DLOG(FATAL) << "Value of a certificate entry is not a dictionary "
                  << "value.";
      continue;
    }

    const base::Value::Dict& cert_dict = certificate.GetDict();
    const std::string* const cert_type =
        cert_dict.FindString(::onc::certificate::kType);
    if (!cert_type || *cert_type != ::onc::certificate::kAuthority) {
      continue;
    }

    const base::Value::List* const trust_list =
        cert_dict.FindList(::onc::certificate::kTrustBits);
    if (!trust_list) {
      continue;
    }

    bool web_trust_flag = false;
    for (const auto& list_val : *trust_list) {
      if (!list_val.is_string()) {
        NOTREACHED_IN_MIGRATION();
      }

      if (list_val.GetString() == ::onc::certificate::kWeb) {
        // "Web" implies that the certificate is to be trusted for SSL
        // identification.
        web_trust_flag = true;
        break;
      }
    }
    if (!web_trust_flag) {
      continue;
    }

    const std::string* const x509_data =
        cert_dict.FindString(::onc::certificate::kX509);
    if (!x509_data) {
      continue;
    }

    base::Value::Dict data;
    data.Set(kPolicyCaCertType, *x509_data);
    ca_certs.Append(std::move(data));
  }
  if (!ca_certs.empty()) {
    filtered_policies->Set(policy_util::kArcPolicyKeyCredentialsConfigDisabled,
                           base::Value(true));
  }
  filtered_policies->Set(policy_util::kArcPolicyKeyCaCerts,
                         std::move(ca_certs));
}

void AddRequiredKeyPairs(const CertStoreService* cert_store_service,
                         base::Value::Dict* filtered_policies) {
  if (!cert_store_service) {
    return;
  }
  base::Value::List cert_names;
  for (const auto& name : cert_store_service->get_required_cert_names()) {
    base::Value::Dict value;
    value.Set(kPolicyRequiredKeyAlias, name);
    cert_names.Append(std::move(value));
  }
  filtered_policies->Set(policy_util::kArcPolicyKeyRequiredKeyPairs,
                         std::move(cert_names));
}

bool LooksLikeAndroidPackageName(const std::string& name) {
  return name.find(".") != std::string::npos;
}

void AddChoosePrivateKeyRuleToPolicy(
    policy::PolicyService* const policy_service,
    const CertStoreService* cert_store_service,
    base::Value::Dict* filtered_policies) {
  if (!cert_store_service) {
    return;
  }

  auto app_ids = chromeos::platform_keys::ExtensionKeyPermissionsService::
      GetCorporateKeyUsageAllowedAppIds(policy_service);
  base::Value::List arc_app_ids;
  for (const auto& app_id : app_ids) {
    if (LooksLikeAndroidPackageName(app_id)) {
      arc_app_ids.Append(app_id);
    }
  }
  if (arc_app_ids.empty() ||
      cert_store_service->get_required_cert_names().empty()) {
    return;
  }

  base::Value::List rules;
  for (const auto& name : cert_store_service->get_required_cert_names()) {
    base::Value::Dict value;
    value.Set(kPolicyPrivateKeyAlias, name);
    value.Set(kPolicyPrivateKeyPackageNames, arc_app_ids.Clone());
    rules.Append(std::move(value));
  }

  filtered_policies->Set(policy_util::kArcPolicyKeyPrivateKeySelectionEnabled,
                         true);
  filtered_policies->Set(policy_util::kArcPolicyKeyChoosePrivateKeyRules,
                         std::move(rules));
}

// Finds managed configurations of applications in |arc_policy| and replace
// string values that refer to template variables.
void ReplaceManagedConfigurationVariables(const Profile* profile,
                                          base::Value::Dict* arc_policy) {
  // Replace template variables in application managed configuration.
  base::Value::List* applications =
      arc_policy->FindList(policy_util::kArcPolicyKeyApplications);
  if (applications) {
    for (base::Value& entry : *applications) {
      base::Value::Dict* config =
          entry.GetDict().FindDict(ArcPolicyBridge::kManagedConfiguration);
      if (config) {
        RecursivelyReplaceManagedConfigurationVariables(profile, *config);
      }
    }
  }
}

void FilterApps(base::Value::Dict* arc_policy,
                const std::unordered_set<std::string>& allowed_packages) {
  base::Value::List* applications =
      arc_policy->FindList(policy_util::kArcPolicyKeyApplications);

  if (!applications) {
    return;
  }

  applications->EraseIf([&allowed_packages](const base::Value& val) {
    const base::Value::Dict& application = val.GetDict();
    const std::string* package_name =
        application.FindString(ArcPolicyBridge::kPackageName);
    return package_name && !base::Contains(allowed_packages, *package_name);
  });
}

void ConfigureRevenPolicies(base::Value::Dict* arc_policy) {
  // The policy value is used to restrict the user from being able to
  // toggle between different accounts in ARC++.
  arc_policy->Set(policy_util::kArcPolicyKeyPlayStoreMode,
                  kPolicyPlayStoreModeAllowList);

  // Define a set of certified package names for Android VPN apps on Reven.
  const std::unordered_set<std::string> allowed_packages = {
      "com.paloaltonetworks.globalprotect",
      "com.cisco.anyconnect.vpn.android.avf",
      "zscaler.com.zschromeosapp",
      "com.f5.edge.client_ics",
      "com.netskope.netskopeclient",
      "com.zimperium.zips",
      "com.fortinet.forticlient_vpn",
      "com.forcepoint.sslvpn"};

  FilterApps(arc_policy, allowed_packages);
}

base::Value::Dict ParseArcPoliciesToDict(const policy::PolicyMap& policy_map) {
  base::Value::Dict filtered_policies;
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  // Parse ArcPolicy as JSON string before adding other policies to the
  // dictionary.
  const base::Value* const app_policy_value =
      policy_map.GetValueUnsafe(policy::key::kArcPolicy);
  if (app_policy_value) {
    std::optional<base::Value> app_policy_dict;
    if (app_policy_value->is_string()) {
      app_policy_dict = base::JSONReader::Read(
          app_policy_value->GetString(),
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    }
    if (app_policy_dict.has_value() && app_policy_dict.value().is_dict()) {
      // Need a deep copy of all values here instead of doing a swap, because
      // JSONReader::Read constructs a dictionary whose StringValues are
      // JSONStringValues which are based on std::string_view instead of string.
      filtered_policies.Merge(std::move(app_policy_dict.value().GetDict()));
    } else {
      std::string app_policy_string =
          app_policy_value->is_string() ? app_policy_value->GetString() : "";
      LOG(ERROR) << "Value of ArcPolicy has invalid format: "
                 << app_policy_string;
    }
  }
  return filtered_policies;
}

void MapChromeToArcPolicies(base::Value::Dict& filtered_policies,
                            const Profile* profile,
                            const policy::PolicyMap& policy_map) {
  const PrefService* profile_prefs = profile->GetPrefs();

  // Keep them sorted by the ARC policy names.
  MapBoolToBool(policy_util::kArcPolicyKeyCameraDisabled,
                policy::key::kVideoCaptureAllowed, policy_map,
                /* invert_bool_value */ true, &filtered_policies);
  // Use the pref for "debuggingFeaturesDisabled" to avoid duplicating the
  // logic of handling DeveloperToolsDisabled / DeveloperToolsAvailability
  // policies.
  MapManagedIntPrefToBool(
      policy_util::kArcPolicyKeyDebuggingFeaturesDisabled,
      ::prefs::kDevToolsAvailability, profile_prefs,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed),
      &filtered_policies);
  MapBoolToBool(policy_util::kArcPolicyKeyPrintingDisabled,
                policy::key::kPrintingEnabled, policy_map,
                /* invert_bool_value */ true, &filtered_policies);
  MapBoolToBool(policy_util::kArcPolicyKeyScreenCaptureDisabled,
                policy::key::kDisableScreenshots, policy_map, false,
                &filtered_policies);
  MapIntToBool(policy_util::kArcPolicyKeyShareLocationDisabled,
               policy::key::kDefaultGeolocationSetting, policy_map,
               2 /*BlockGeolocation*/, &filtered_policies);
  MapBoolToBool(policy_util::kArcPolicyKeyUnmuteMicrophoneDisabled,
                policy::key::kAudioCaptureAllowed, policy_map,
                /* invert_bool_value */ true, &filtered_policies);
  MapObjectToPresenceBool(policy_util::kArcPolicyKeySetWallpaperDisabled,
                          policy::key::kWallpaperImage, policy_map,
                          &filtered_policies, {"url", "hash"});
  MapBoolToBool(policy_util::kArcPolicyKeyVpnConfigDisabled,
                policy::key::kVpnConfigAllowed, policy_map,
                /* invert_bool_value */ true, &filtered_policies);
}

void OverrideArcPolicies(base::Value::Dict& filtered_policies,
                         const policy::PolicyMap& policy_map,
                         const std::string& guid,
                         bool is_affiliated,
                         const Profile* profile) {
  MapChromeToArcPolicies(filtered_policies, profile, policy_map);

  // If kForceDevToolsAvailable is set, then force debugging features to be
  // available for ARC as well. This must be after the initial writing of
  // "debuggingFeaturesDisabled".
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDevToolsAvailable)) {
    filtered_policies.Set(policy_util::kArcPolicyKeyDebuggingFeaturesDisabled,
                          false);
  }

  // Always enable APK Cache for affiliated users, and always disable it for
  // not affiliated ones.
  filtered_policies.Set(policy_util::kArcPolicyKeyApkCacheEnabled,
                        is_affiliated);

  filtered_policies.Set(policy_util::kArcPolicyKeyGuid, guid);

  // Always allow mounting physical media because mounts are controlled
  // outside of ARC based on policy in file_manager::VolumeManager. Since this
  // Android policy used to be mapped from Chrome-side policy
  // policy::key::kExternalStoragePolicy before, we hard-code it to false to
  // ensure that the old policy setting does not remain on the ARC side.
  // See b/217531658 for details.
  filtered_policies.Set(policy_util::kArcPolicyKeyMountPhysicalMediaDisabled,
                        false);

  if (profile->IsChild() &&
      ash::ProfileHelper::Get()->IsPrimaryProfile(profile)) {
    // Adds "playStoreMode" policy. The policy value is used to restrict the
    // user from being able to toggle between different accounts in ARC++.
    filtered_policies.Set(policy_util::kArcPolicyKeyPlayStoreMode,
                          kPolicyPlayStoreModeSupervised);
  }

  if (ash::switches::IsRevenBranding()) {
    ConfigureRevenPolicies(&filtered_policies);
  }
}

base::Value::Dict GetFilteredDictPolicies(
    policy::PolicyService* const policy_service,
    const std::string& guid,
    bool is_affiliated,
    const CertStoreService* cert_store_service,
    const Profile* profile) {
  const policy::PolicyNamespace policy_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  const policy::PolicyMap& policy_map =
      policy_service->GetPolicies(policy_namespace);

  base::Value::Dict filtered_policies = ParseArcPoliciesToDict(policy_map);

  // Add CA certificates.
  AddOncCaCertsToPolicies(policy_map, &filtered_policies);

  AddRequiredKeyPairs(cert_store_service, &filtered_policies);
  AddChoosePrivateKeyRuleToPolicy(policy_service, cert_store_service,
                                  &filtered_policies);

  ReplaceManagedConfigurationVariables(profile, &filtered_policies);

  OverrideArcPolicies(filtered_policies, policy_map, guid, is_affiliated,
                      profile);
  return filtered_policies;
}

std::string GetFilteredJSONPolicies(policy::PolicyService* const policy_service,
                                    const std::string& guid,
                                    bool is_affiliated,
                                    const CertStoreService* cert_store_service,
                                    const Profile* profile) {
  base::Value::Dict filtered_policies = GetFilteredDictPolicies(
      policy_service, guid, is_affiliated, cert_store_service, profile);

  std::string policy_json;
  JSONStringValueSerializer serializer(&policy_json);
  serializer.Serialize(filtered_policies);
  return policy_json;
}

void RecordPolicyMetrics(const policy::PolicyMap& policy) {
  const base::Value* const arc_enabled =
      policy.GetValue(policy::key::kArcEnabled, base::Value::Type::BOOLEAN);
  if (!arc_enabled || !arc_enabled->GetBool()) {
    return;
  }

  const base::Value* const arc_policy =
      policy.GetValue(policy::key::kArcPolicy, base::Value::Type::STRING);
  if (arc_policy) {
    policy_util::RecordPolicyMetrics(arc_policy->GetString());
  }
}

// Singleton factory for ArcPolicyBridge.
class ArcPolicyBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPolicyBridge,
          ArcPolicyBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPolicyBridgeFactory";

  static ArcPolicyBridgeFactory* GetInstance() {
    return base::Singleton<ArcPolicyBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPolicyBridgeFactory>;

  ArcPolicyBridgeFactory() {}
  ~ArcPolicyBridgeFactory() override = default;
};

}  // namespace

// static
const char ArcPolicyBridge::kPackageName[] = "packageName";

// static
const char ArcPolicyBridge::kManagedConfiguration[] = "managedConfiguration";

// static
ArcPolicyBridge* ArcPolicyBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPolicyBridgeFactory::GetForBrowserContext(context);
}

// static
ArcPolicyBridge* ArcPolicyBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcPolicyBridgeFactory::GetForBrowserContextForTesting(context);
}

// static
BrowserContextKeyedServiceFactory* ArcPolicyBridge::GetFactory() {
  return ArcPolicyBridgeFactory::GetInstance();
}

base::WeakPtr<ArcPolicyBridge> ArcPolicyBridge::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ArcPolicyBridge::ArcPolicyBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : ArcPolicyBridge(context, bridge_service, nullptr /* policy_service */) {}

ArcPolicyBridge::ArcPolicyBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service,
                                 policy::PolicyService* policy_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      policy_service_(policy_service),
      instance_guid_(base::Uuid::GenerateRandomV4().AsLowercaseString()) {
  VLOG(2) << "ArcPolicyBridge::ArcPolicyBridge";
  arc_bridge_service_->policy()->SetHost(this);
  arc_bridge_service_->policy()->AddObserver(this);
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  arc_session_manager->AddObserver(this);
}

ArcPolicyBridge::~ArcPolicyBridge() {
  VLOG(2) << "ArcPolicyBridge::~ArcPolicyBridge";
  if (is_policy_service_observed) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    is_policy_service_observed = false;
    policy_service_ = nullptr;
  }
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // It can be null in unittests
  if (arc_session_manager) {
    arc_session_manager->RemoveObserver(this);
  }
  arc_bridge_service_->policy()->RemoveObserver(this);
  arc_bridge_service_->policy()->SetHost(nullptr);
}

const std::string& ArcPolicyBridge::GetInstanceGuidForTesting() {
  return instance_guid_;
}

void ArcPolicyBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcPolicyBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcPolicyBridge::OverrideIsManagedForTesting(bool is_managed) {
  is_managed_ = is_managed;
}

void ArcPolicyBridge::OnConnectionReady() {
  VLOG(1) << "ArcPolicyBridge::OnConnectionReady";
  InitializePolicyService();
  policy_util::RecordPolicyMetrics(GetCurrentJSONPolicies());

  if (!on_arc_instance_ready_callback_.is_null()) {
    std::move(on_arc_instance_ready_callback_).Run();
  }
}

void ArcPolicyBridge::OnConnectionClosed() {
  VLOG(1) << "ArcPolicyBridge::OnConnectionClosed";
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  is_policy_service_observed = false;
  policy_service_ = nullptr;
}

void ArcPolicyBridge::GetPolicies(GetPoliciesCallback callback) {
  VLOG(1) << "ArcPolicyBridge::GetPolicies";
  std::string new_policy = GetCurrentJSONPolicies();
  if (arc_policy_for_reporting_ != new_policy) {
    arc_policy_for_reporting_ = new_policy;
    VLOG(1) << "Policy updated. Current policy: " << new_policy;
  }
  for (Observer& observer : observers_) {
    observer.OnPolicySent(arc_policy_for_reporting_);
  }
  std::move(callback).Run(arc_policy_for_reporting_);
}

void ArcPolicyBridge::ReportCompliance(const std::string& request,
                                       ReportComplianceCallback callback) {
  VLOG(1) << "ArcPolicyBridge::ReportCompliance";
  data_decoder::DataDecoder::ParseJsonIsolated(
      request,
      base::BindOnce(&ArcPolicyBridge::OnReportComplianceParse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcPolicyBridge::ReportDPCVersion(const std::string& version) {
  arc_dpc_version_ = version;

  for (Observer& observer : observers_) {
    observer.OnReportDPCVersion(version);
  }
}

void ArcPolicyBridge::ReportPlayStoreLocalPolicySet(
    base::Time time,
    const std::vector<std::string>& package_names) {
  const std::set<std::string> packages_set(package_names.begin(),
                                           package_names.end());
  for (Observer& observer : observers_) {
    observer.OnPlayStoreLocalPolicySet(time, packages_set);
  }
}

void ArcPolicyBridge::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  VLOG(1) << "ArcPolicyBridge::OnPolicyUpdated";

  // Allow ARC activation if any app needs to be force installed when ARC on
  // demand is enabled. As ARC on demand will only be enabled if there are no
  // apps being installed, only current is checked here instead of the delta
  // between previous and current.
  ActivateArcIfRequiredByPolicy(current);

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->policy(),
                                               OnPolicyUpdated);
  if (!instance) {
    return;
  }

  instance->OnPolicyUpdated();
  RecordPolicyMetrics(current);
}

void ArcPolicyBridge::OnArcStartDelayed() {
  InitializePolicyService();
  const policy::PolicyNamespace policy_namespace(
      policy::POLICY_DOMAIN_CHROME,
      /*component_id=*/std::string());
  const policy::PolicyMap& policy_map =
      policy_service_->GetPolicies(policy_namespace);
  ActivateArcIfRequiredByPolicy(policy_map);
}

void ArcPolicyBridge::OnCommandReceived(
    const std::string& command,
    mojom::PolicyInstance::OnCommandReceivedCallback callback) {
  VLOG(1) << "ArcPolicyBridge::OnCommandReceived";
  auto* const instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->policy(), OnCommandReceived);

  if (!instance) {
    VLOG(1) << "ARC not ready yet, will retry remote command once it is ready.";
    DCHECK(on_arc_instance_ready_callback_.is_null());

    // base::Unretained is safe here since this class owns the callback's
    // lifetime.
    on_arc_instance_ready_callback_ =
        base::BindOnce(&ArcPolicyBridge::OnCommandReceived,
                       base::Unretained(this), command, std::move(callback));

    return;
  }

  instance->OnCommandReceived(command, std::move(callback));
}

void ArcPolicyBridge::InitializePolicyService() {
  if (policy_service_ == nullptr) {
    auto* profile_policy_connector =
        Profile::FromBrowserContext(context_)->GetProfilePolicyConnector();
    policy_service_ = profile_policy_connector->policy_service();
    is_managed_ = profile_policy_connector->IsManaged();
  }
  if (!is_policy_service_observed) {
    policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
    is_policy_service_observed = true;
  }
}

std::string ArcPolicyBridge::GetCurrentJSONPolicies() const {
  if (!is_managed_) {
    return std::string();
  }
  const Profile* const profile = Profile::FromBrowserContext(context_);
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  const CertStoreService* cert_store_service =
      CertStoreServiceFactory::GetForBrowserContext(context_);

  return GetFilteredJSONPolicies(policy_service_, instance_guid_,
                                 user->IsAffiliated(), cert_store_service,
                                 profile);
}

void ArcPolicyBridge::OnReportComplianceParse(
    base::OnceCallback<void(const std::string&)> callback,
    data_decoder::DataDecoder::ValueOrError result) {
  std::move(callback).Run(kPolicyCompliantJson);
  if (!result.has_value()) {
    DLOG(ERROR) << "Can't parse policy compliance report";
    return;
  }

  Profile::FromBrowserContext(context_)->GetPrefs()->SetBoolean(
      prefs::kArcPolicyComplianceReported, true);

  if (result->is_dict()) {
    JSONStringValueSerializer serializer(&arc_policy_compliance_report_);
    serializer.Serialize(*result);
    for (Observer& observer : observers_) {
      observer.OnComplianceReportReceived(&*result);
    }
  }
}

// static
void ArcPolicyBridge::ActivateArcIfRequiredByPolicy(
    const policy::PolicyMap& policy_map) {
  base::Value::Dict filtered_policies = ParseArcPoliciesToDict(policy_map);
  base::Value::List* apps =
      filtered_policies.FindList(policy_util::kArcPolicyKeyApplications);
  if (apps == nullptr) {
    return;
  }
  bool hasForceInstallApps =
      std::any_of(apps->cbegin(), apps->cbegin(), [](const auto& app) {
        return *app.GetDict().FindString(kPolicyAppInstallType) ==
               kPolicyAppInstallTypeForceInstalled;
      });
  if (hasForceInstallApps) {
    arc::ArcSessionManager::Get()->AllowActivation(
        arc::ArcSessionManager::AllowActivationReason::kForcedByPolicy);
  }
}

// static
void ArcPolicyBridge::EnsureFactoryBuilt() {
  ArcPolicyBridgeFactory::GetInstance();
}

}  // namespace arc
