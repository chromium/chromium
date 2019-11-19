// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_smart_card_manager_bridge.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "crypto/sha2.h"

namespace arc {

namespace {

constexpr char kArcCaCerts[] = "caCerts";
constexpr char kPolicyCompliantJson[] = "{ \"policyCompliant\": true }";
constexpr char kArcRequiredKeyPairs[] = "requiredKeyPairs";

// invert_bool_value: If the Chrome policy and the ARC policy with boolean value
// have opposite semantics, set this to true so the bool is inverted before
// being added. Otherwise, set it to false.
void MapBoolToBool(const std::string& arc_policy_name,
                   const std::string& policy_name,
                   const policy::PolicyMap& policy_map,
                   bool invert_bool_value,
                   base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (!policy_value)
    return;
  if (!policy_value->is_bool()) {
    NOTREACHED() << "Policy " << policy_name << " is not a boolean.";
    return;
  }
  bool bool_value;
  policy_value->GetAsBoolean(&bool_value);
  filtered_policies->SetBoolean(arc_policy_name,
                                bool_value != invert_bool_value);
}

// int_true: value of Chrome OS policy for which arc policy is set to true.
// It is set to false for all other values.
void MapIntToBool(const std::string& arc_policy_name,
                  const std::string& policy_name,
                  const policy::PolicyMap& policy_map,
                  int int_true,
                  base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (!policy_value)
    return;
  if (!policy_value->is_int()) {
    NOTREACHED() << "Policy " << policy_name << " is not an integer.";
    return;
  }
  int int_value;
  policy_value->GetAsInteger(&int_value);
  filtered_policies->SetBoolean(arc_policy_name, int_value == int_true);
}

// |arc_policy_name| is only set if the |pref_name| pref is managed.
// int_true: value of Chrome OS pref for which arc policy is set to true.
// It is set to false for all other values.
void MapManagedIntPrefToBool(const std::string& arc_policy_name,
                             const std::string& pref_name,
                             const PrefService* profile_prefs,
                             int int_true,
                             base::DictionaryValue* filtered_policies) {
  if (!profile_prefs->IsManagedPreference(pref_name))
    return;

  int int_value = profile_prefs->GetInteger(pref_name);
  filtered_policies->SetBoolean(arc_policy_name, int_value == int_true);
}

// Checks whether |policy_name| is present as an object and has all |fields|,
// Sets |arc_policy_name| to true only if the condition above is satisfied.
void MapObjectToPresenceBool(const std::string& arc_policy_name,
                             const std::string& policy_name,
                             const policy::PolicyMap& policy_map,
                             base::DictionaryValue* filtered_policies,
                             const std::vector<std::string>& fields) {
  const base::Value* const policy_value = policy_map.GetValue(policy_name);
  if (!policy_value)
    return;
  const base::DictionaryValue* dict = nullptr;
  if (!policy_value->GetAsDictionary(&dict)) {
    NOTREACHED() << "Policy " << policy_name << " is not an object.";
    return;
  }
  for (const auto& field : fields) {
    if (!dict->HasKey(field))
      return;
  }
  filtered_policies->SetBoolean(arc_policy_name, true);
}

void AddOncCaCertsToPolicies(const policy::PolicyMap& policy_map,
                             base::DictionaryValue* filtered_policies) {
  const base::Value* const policy_value =
      policy_map.GetValue(policy::key::kArcCertificatesSyncMode);
  int32_t mode = ArcCertsSyncMode::SYNC_DISABLED;

  // Old certs should be uninstalled if the sync is disabled or policy is not
  // set.
  if (!policy_value || !policy_value->GetAsInteger(&mode) ||
      mode != ArcCertsSyncMode::COPY_CA_CERTS) {
    return;
  }

  // Importing CA certificates from device policy is not allowed.
  // Import only from user policy.
  const base::Value* onc_policy_value =
      policy_map.GetValue(policy::key::kOpenNetworkConfiguration);
  if (!onc_policy_value) {
    VLOG(1) << "onc policy is not set.";
    return;
  }
  std::string onc_blob;
  if (!onc_policy_value->GetAsString(&onc_blob)) {
    LOG(ERROR) << "Value of onc policy has invalid format.";
    return;
  }

  base::ListValue certificates;
  {
    base::ListValue unused_network_configs;
    base::DictionaryValue unused_global_network_config;
    if (!chromeos::onc::ParseAndValidateOncForImport(
            onc_blob, onc::ONCSource::ONC_SOURCE_USER_POLICY,
            "" /* no passphrase */, &unused_network_configs,
            &unused_global_network_config, &certificates)) {
      LOG(ERROR) << "Value of onc policy has invalid format =" << onc_blob;
    }
  }

  std::unique_ptr<base::ListValue> ca_certs(
      std::make_unique<base::ListValue>());
  for (const auto& entry : certificates) {
    const base::DictionaryValue* certificate = nullptr;
    if (!entry.GetAsDictionary(&certificate)) {
      DLOG(FATAL) << "Value of a certificate entry is not a dictionary "
                  << "value.";
      continue;
    }

    std::string cert_type;
    certificate->GetStringWithoutPathExpansion(::onc::certificate::kType,
                                               &cert_type);
    if (cert_type != ::onc::certificate::kAuthority)
      continue;

    const base::ListValue* trust_list = nullptr;
    if (!certificate->GetListWithoutPathExpansion(
            ::onc::certificate::kTrustBits, &trust_list)) {
      continue;
    }

    bool web_trust_flag = false;
    for (const auto& list_val : *trust_list) {
      std::string trust_type;
      if (!list_val.GetAsString(&trust_type))
        NOTREACHED();

      if (trust_type == ::onc::certificate::kWeb) {
        // "Web" implies that the certificate is to be trusted for SSL
        // identification.
        web_trust_flag = true;
        break;
      }
    }
    if (!web_trust_flag)
      continue;

    std::string x509_data;
    if (!certificate->GetStringWithoutPathExpansion(::onc::certificate::kX509,
                                                    &x509_data)) {
      continue;
    }

    base::DictionaryValue data;
    data.SetString("X509", x509_data);
    ca_certs->Append(data.CreateDeepCopy());
  }
  if (!ca_certs->GetList().empty())
    filtered_policies->SetKey("credentialsConfigDisabled", base::Value(true));
  filtered_policies->Set(kArcCaCerts, std::move(ca_certs));
}

void AddRequiredKeyPairs(const ArcSmartCardManagerBridge* smart_card_manager,
                         base::DictionaryValue* filtered_policies) {
  if (!smart_card_manager)
    return;
  std::unique_ptr<base::ListValue> cert_names(
      std::make_unique<base::ListValue>());
  for (const auto& name : smart_card_manager->get_required_cert_names()) {
    cert_names->Append(name);
  }
  filtered_policies->Set(kArcRequiredKeyPairs, std::move(cert_names));
}

std::string GetFilteredJSONPolicies(
    const policy::PolicyMap& policy_map,
    const PrefService* profile_prefs,
    const std::string& guid,
    bool is_affiliated,
    const ArcSmartCardManagerBridge* smart_card_manager) {
  base::DictionaryValue filtered_policies;
  // Parse ArcPolicy as JSON string before adding other policies to the
  // dictionary.
  const base::Value* const app_policy_value =
      policy_map.GetValue(policy::key::kArcPolicy);
  if (app_policy_value) {
    std::string app_policy_string;
    app_policy_value->GetAsString(&app_policy_string);
    std::unique_ptr<base::DictionaryValue> app_policy_dict =
        base::DictionaryValue::From(
            base::JSONReader::ReadDeprecated(app_policy_string));
    if (app_policy_dict) {
      // Need a deep copy of all values here instead of doing a swap, because
      // JSONReader::Read constructs a dictionary whose StringValues are
      // JSONStringValues which are based on StringPiece instead of string.
      filtered_policies.MergeDictionary(app_policy_dict.get());
    } else {
      LOG(ERROR) << "Value of ArcPolicy has invalid format: "
                 << app_policy_string;
    }
  }

  // Keep them sorted by the ARC policy names.
  MapBoolToBool("cameraDisabled", policy::key::kVideoCaptureAllowed, policy_map,
                /* invert_bool_value */ true, &filtered_policies);
  // Use the pref for "debuggingFeaturesDisabled" to avoid duplicating the logic
  // of handling DeveloperToolsDisabled / DeveloperToolsAvailability policies.
  MapManagedIntPrefToBool(
      "debuggingFeaturesDisabled", ::prefs::kDevToolsAvailability,
      profile_prefs,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed),
      &filtered_policies);
  MapBoolToBool("printingDisabled", policy::key::kPrintingEnabled, policy_map,
                /* invert_bool_value */ true, &filtered_policies);
  MapBoolToBool("screenCaptureDisabled", policy::key::kDisableScreenshots,
                policy_map, false, &filtered_policies);
  MapIntToBool("shareLocationDisabled", policy::key::kDefaultGeolocationSetting,
               policy_map, 2 /*BlockGeolocation*/, &filtered_policies);
  MapBoolToBool("unmuteMicrophoneDisabled", policy::key::kAudioCaptureAllowed,
                policy_map, /* invert_bool_value */ true, &filtered_policies);
  MapBoolToBool("mountPhysicalMediaDisabled",
                policy::key::kExternalStorageDisabled, policy_map,
                /* invert_bool_value */ false, &filtered_policies);
  MapObjectToPresenceBool("setWallpaperDisabled", policy::key::kWallpaperImage,
                          policy_map, &filtered_policies, {"url", "hash"});
  MapBoolToBool("vpnConfigDisabled", policy::key::kVpnConfigAllowed, policy_map,
                /* invert_bool_value */ true, &filtered_policies);

  // Add CA certificates.
  AddOncCaCertsToPolicies(policy_map, &filtered_policies);

  if (!is_affiliated)
    filtered_policies.RemoveKey("apkCacheEnabled");

  filtered_policies.SetString("guid", guid);

  AddRequiredKeyPairs(smart_card_manager, &filtered_policies);

  std::string policy_json;
  JSONStringValueSerializer serializer(&policy_json);
  serializer.Serialize(filtered_policies);
  return policy_json;
}

void UpdateFirstComplianceSinceSignInTiming(
    const base::TimeDelta& elapsed_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Arc.FirstComplianceReportTime.SinceSignIn",
                             elapsed_time, base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromMinutes(10), 50);
}

void UpdateFirstComplianceSinceStartupTiming(
    const base::TimeDelta& elapsed_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Arc.FirstComplianceReportTime.SinceStartup",
                             elapsed_time, base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromMinutes(10), 50);
}

void UpdateComplianceSinceUpdateTiming(const base::TimeDelta& elapsed_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Arc.ComplianceReportSinceUpdateNotificationTime",
                             elapsed_time,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10), 50);
}

// Returns the SHA-256 hash of the JSON dump of the ARC policies, in the textual
// hex dump format.  Note that no specific JSON normalization is performed, as
// the spurious hash mismatches, even if they occur (which is unlikely), would
// only result in some UMA metrics not being sent.
std::string GetPoliciesHash(const std::string& json_policies) {
  const std::string hash_bits = crypto::SHA256HashString(json_policies);
  return base::ToLowerASCII(
      base::HexEncode(hash_bits.c_str(), hash_bits.length()));
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
      instance_guid_(base::GenerateGUID()) {
  VLOG(2) << "ArcPolicyBridge::ArcPolicyBridge";
  arc_bridge_service_->policy()->SetHost(this);
  arc_bridge_service_->policy()->AddObserver(this);
}

ArcPolicyBridge::~ArcPolicyBridge() {
  VLOG(2) << "ArcPolicyBridge::~ArcPolicyBridge";
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
  if (policy_service_ == nullptr) {
    InitializePolicyService();
  }
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  initial_policies_hash_ = GetPoliciesHash(GetCurrentJSONPolicies());

  if (!on_arc_instance_ready_callback_.is_null()) {
    std::move(on_arc_instance_ready_callback_).Run();
  }
}

void ArcPolicyBridge::OnConnectionClosed() {
  VLOG(1) << "ArcPolicyBridge::OnConnectionClosed";
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy_service_ = nullptr;
  initial_policies_hash_.clear();
}

void ArcPolicyBridge::GetPolicies(GetPoliciesCallback callback) {
  VLOG(1) << "ArcPolicyBridge::GetPolicies";
  const std::string policy = GetCurrentJSONPolicies();
  for (Observer& observer : observers_) {
    observer.OnPolicySent(policy);
  }
  std::move(callback).Run(policy);
}

void ArcPolicyBridge::ReportCompliance(const std::string& request,
                                       ReportComplianceCallback callback) {
  VLOG(1) << "ArcPolicyBridge::ReportCompliance";
  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  data_decoder::DataDecoder::ParseJsonIsolated(
      request,
      base::BindOnce(&ArcPolicyBridge::OnReportComplianceParse,
                     weak_ptr_factory_.GetWeakPtr(), repeating_callback));
}

void ArcPolicyBridge::ReportCloudDpsRequested(
    base::Time time,
    const std::vector<std::string>& package_names) {
  const std::set<std::string> packages_set(package_names.begin(),
                                           package_names.end());
  for (Observer& observer : observers_)
    observer.OnCloudDpsRequested(time, packages_set);
}

void ArcPolicyBridge::ReportCloudDpsSucceeded(
    base::Time time,
    const std::vector<std::string>& package_names) {
  const std::set<std::string> packages_set(package_names.begin(),
                                           package_names.end());
  for (Observer& observer : observers_)
    observer.OnCloudDpsSucceeded(time, packages_set);
}

void ArcPolicyBridge::ReportCloudDpsFailed(base::Time time,
                                           const std::string& package_name,
                                           mojom::InstallErrorReason reason) {
  for (Observer& observer : observers_)
    observer.OnCloudDpsFailed(time, package_name, reason);
}

void ArcPolicyBridge::ReportDirectInstall(
    base::Time time,
    const std::vector<std::string>& package_names) {
  const std::set<std::string> packages_set(package_names.begin(),
                                           package_names.end());
  for (Observer& observer : observers_)
    observer.OnReportDirectInstall(time, packages_set);
}

void ArcPolicyBridge::ReportForceInstallMainLoopFailed(
    base::Time time,
    const std::vector<std::string>& package_names) {
  const std::set<std::string> packages_set(package_names.begin(),
                                           package_names.end());
  for (Observer& observer : observers_)
    observer.OnReportForceInstallMainLoopFailed(time, packages_set);
}

void ArcPolicyBridge::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  VLOG(1) << "ArcPolicyBridge::OnPolicyUpdated";
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->policy(),
                                               OnPolicyUpdated);
  if (!instance)
    return;

  const std::string policies_hash = GetPoliciesHash(GetCurrentJSONPolicies());
  if (policies_hash != update_notification_policies_hash_) {
    update_notification_policies_hash_ = policies_hash;
    update_notification_time_ = base::TimeTicks::Now();
    compliance_since_update_timing_reported_ = false;
  }

  instance->OnPolicyUpdated();
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
  auto* profile_policy_connector =
      Profile::FromBrowserContext(context_)->GetProfilePolicyConnector();
  policy_service_ = profile_policy_connector->policy_service();
  is_managed_ = profile_policy_connector->IsManaged();
}

std::string ArcPolicyBridge::GetCurrentJSONPolicies() const {
  if (!is_managed_)
    return std::string();
  const policy::PolicyNamespace policy_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  const policy::PolicyMap& policy_map =
      policy_service_->GetPolicies(policy_namespace);

  const Profile* const profile = Profile::FromBrowserContext(context_);
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const ArcSmartCardManagerBridge* smart_card_manager =
      ArcSmartCardManagerBridge::GetForBrowserContext(context_);

  return GetFilteredJSONPolicies(policy_map, profile->GetPrefs(),
                                 instance_guid_, user->IsAffiliated(),
                                 smart_card_manager);
}

void ArcPolicyBridge::OnReportComplianceParse(
    base::OnceCallback<void(const std::string&)> callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    // TODO(poromov@): Report to histogram.
    DLOG(ERROR) << "Can't parse policy compliance report";
    std::move(callback).Run(kPolicyCompliantJson);
    return;
  }

  // Always returns "compliant".
  std::move(callback).Run(kPolicyCompliantJson);
  Profile::FromBrowserContext(context_)->GetPrefs()->SetBoolean(
      prefs::kArcPolicyComplianceReported, true);

  const base::DictionaryValue* dict = nullptr;
  if (result.value->GetAsDictionary(&dict)) {
    UpdateComplianceReportMetrics(dict);
    for (Observer& observer : observers_) {
      observer.OnComplianceReportReceived(&result.value.value());
    }
  }
}

void ArcPolicyBridge::UpdateComplianceReportMetrics(
    const base::DictionaryValue* report) {
  bool is_arc_plus_plus_report_successful = false;
  report->GetBoolean("isArcPlusPlusReportSuccessful",
                     &is_arc_plus_plus_report_successful);
  std::string reported_policies_hash;
  report->GetString("policyHash", &reported_policies_hash);
  if (!is_arc_plus_plus_report_successful || reported_policies_hash.empty())
    return;

  const base::TimeTicks now = base::TimeTicks::Now();
  ArcSessionManager* const session_manager = ArcSessionManager::Get();

  if (reported_policies_hash == initial_policies_hash_ &&
      !first_compliance_timing_reported_) {
    const base::TimeTicks sign_in_start_time =
        session_manager->sign_in_start_time();
    if (!sign_in_start_time.is_null()) {
      UpdateFirstComplianceSinceSignInTiming(now - sign_in_start_time);
    } else {
      UpdateFirstComplianceSinceStartupTiming(
          now - session_manager->arc_start_time());
    }
    first_compliance_timing_reported_ = true;
  }

  if (reported_policies_hash == update_notification_policies_hash_ &&
      !compliance_since_update_timing_reported_) {
    UpdateComplianceSinceUpdateTiming(now - update_notification_time_);
    compliance_since_update_timing_reported_ = true;
  }
}

}  // namespace arc
