// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/extension_force_install_mixin.h"

#include <stdint.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/crx_verifier.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#endif

namespace {

// Name of the directory whose contents are served by the embedded test
// server.
constexpr char kServedDirName[] = "served";
// Hardcoded string value expected from the extension after extension is
// installed and it started executing.
constexpr char kReadyMessage[] = "ready";
// Template for the file name of a served CRX file.
constexpr char kCrxFileNameTemplate[] = "%s-%s.crx";
// Template for the file name of a served update manifest file.
constexpr char kUpdateManifestFileNameTemplate[] = "%s.xml";

// Implements waiting until the given extension appears in the
// force-installation pref.
class ForceInstallPrefObserver final {
 public:
  ForceInstallPrefObserver(Profile* profile,
                           const extensions::ExtensionId& extension_id);
  ForceInstallPrefObserver(const ForceInstallPrefObserver&) = delete;
  ForceInstallPrefObserver& operator=(const ForceInstallPrefObserver&) = delete;
  ~ForceInstallPrefObserver();

  void Wait();

 private:
  void OnPrefChanged();
  bool IsForceInstallPrefSet() const;

  const raw_ptr<PrefService> pref_service_;
  const std::string pref_name_;
  const extensions::ExtensionId extension_id_;
  PrefChangeRegistrar pref_change_registrar_;
  base::RunLoop run_loop_;
  base::WeakPtrFactory<ForceInstallPrefObserver> weak_ptr_factory_{this};
};

ForceInstallPrefObserver::ForceInstallPrefObserver(
    Profile* profile,
    const extensions::ExtensionId& extension_id)
    : pref_service_(profile->GetPrefs()),
      pref_name_(extensions::pref_names::kInstallForceList),
      extension_id_(extension_id) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_name_, base::BindRepeating(&ForceInstallPrefObserver::OnPrefChanged,
                                      weak_ptr_factory_.GetWeakPtr()));
}

ForceInstallPrefObserver::~ForceInstallPrefObserver() = default;

void ForceInstallPrefObserver::Wait() {
  if (IsForceInstallPrefSet())
    return;
  run_loop_.Run();
}

void ForceInstallPrefObserver::OnPrefChanged() {
  if (IsForceInstallPrefSet())
    run_loop_.Quit();
}

bool ForceInstallPrefObserver::IsForceInstallPrefSet() const {
  const PrefService::Preference* const pref =
      pref_service_->FindPreference(pref_name_);
  if (!pref || !pref->IsManaged()) {
    // Note that we intentionally ignore the pref if it's set but isn't managed,
    // mimicking the real behavior of the Extensions system that only respects
    // trusted (policy-set) values. Normally there's no "untrusted" value in
    // these prefs, but in theory this is an attack that the Extensions system
    // protects against, and there might be tests that simulate this scenario.
    return false;
  }
  DCHECK_EQ(pref->GetType(), base::Value::Type::DICT);
  return pref->GetValue()->GetDict().contains(extension_id_);
}

// Implements waiting for the mixin's specified event.
class ForceInstallWaiter final {
 public:
  ForceInstallWaiter(ExtensionForceInstallMixin::WaitMode wait_mode,
                     const extensions::ExtensionId& extension_id,
                     Profile* profile);
  ForceInstallWaiter(const ForceInstallWaiter&) = delete;
  ForceInstallWaiter& operator=(const ForceInstallWaiter&) = delete;
  ~ForceInstallWaiter();

  // Waits until the event specified |wait_mode| gets satisfied. Returns false
  // if the waiting timed out.
  bool Wait();

 private:
  // Implementation of Wait(). Returns the result via |success| in order to be
  // able to use ASSERT* macros inside.
  void WaitImpl(bool* success);

  const ExtensionForceInstallMixin::WaitMode wait_mode_;
  const extensions::ExtensionId extension_id_;
  const raw_ptr<Profile> profile_;
  std::unique_ptr<ForceInstallPrefObserver> force_install_pref_observer_;
  std::unique_ptr<extensions::TestExtensionRegistryObserver> registry_observer_;
  std::unique_ptr<extensions::ExtensionHostTestHelper>
      background_page_first_load_observer_;
  std::unique_ptr<ExtensionTestMessageListener> extension_message_listener_;
};

ForceInstallWaiter::ForceInstallWaiter(
    ExtensionForceInstallMixin::WaitMode wait_mode,
    const extensions::ExtensionId& extension_id,
    Profile* profile)
    : wait_mode_(wait_mode), extension_id_(extension_id), profile_(profile) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id_));
  if (!profile_ && wait_mode_ != ExtensionForceInstallMixin::WaitMode::kNone) {
    ADD_FAILURE() << "No profile passed to the Init method";
    return;
  }
  switch (wait_mode_) {
    case ExtensionForceInstallMixin::WaitMode::kNone:
      break;
    case ExtensionForceInstallMixin::WaitMode::kPrefSet:
      force_install_pref_observer_ =
          std::make_unique<ForceInstallPrefObserver>(profile_, extension_id_);
      break;
    case ExtensionForceInstallMixin::WaitMode::kLoad:
      registry_observer_ =
          std::make_unique<extensions::TestExtensionRegistryObserver>(
              extensions::ExtensionRegistry::Get(profile_), extension_id_);
      break;
    case ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad:
      background_page_first_load_observer_ =
          std::make_unique<extensions::ExtensionHostTestHelper>(profile_,
                                                                extension_id_);
      background_page_first_load_observer_->RestrictToType(
          extensions::mojom::ViewType::kExtensionBackgroundPage);
      break;
    case ExtensionForceInstallMixin::WaitMode::kReadyMessageReceived:
      extension_message_listener_ =
          std::make_unique<ExtensionTestMessageListener>(kReadyMessage);
      extension_message_listener_->set_extension_id(extension_id_);
      break;
  }
}

ForceInstallWaiter::~ForceInstallWaiter() = default;

bool ForceInstallWaiter::Wait() {
  bool success = false;
  WaitImpl(&success);
  return success;
}

void ForceInstallWaiter::WaitImpl(bool* success) {
  switch (wait_mode_) {
    case ExtensionForceInstallMixin::WaitMode::kNone:
      // No waiting needed.
      *success = true;
      break;
    case ExtensionForceInstallMixin::WaitMode::kPrefSet:
      // Wait and assert that the waiting run loop didn't time out.
      ASSERT_NO_FATAL_FAILURE(force_install_pref_observer_->Wait());
      *success = true;
      break;
    case ExtensionForceInstallMixin::WaitMode::kLoad:
      *success = registry_observer_->WaitForExtensionLoaded() != nullptr;
      break;
    case ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad:
      // Wait and assert that the waiting run loop didn't time out.
      ASSERT_NO_FATAL_FAILURE(background_page_first_load_observer_
                                  ->WaitForHostCompletedFirstLoad());
      *success = true;
      break;
    case ExtensionForceInstallMixin::WaitMode::kReadyMessageReceived:
      ASSERT_NO_FATAL_FAILURE(*success = extension_message_listener_
                                             ->WaitUntilSatisfied());
      break;
  }
}

std::string GetServedUpdateManifestFileName(
    const extensions::ExtensionId& extension_id) {
  return base::StringPrintf(kUpdateManifestFileNameTemplate,
                            extension_id.c_str());
}

std::string GetServedCrxFileName(const extensions::ExtensionId& extension_id,
                                 const base::Version& extension_version) {
  return base::StringPrintf(kCrxFileNameTemplate, extension_id.c_str(),
                            extension_version.GetString().c_str());
}

std::string GenerateUpdateManifest(const extensions::ExtensionId& extension_id,
                                   const base::Version& extension_version,
                                   const GURL& crx_url) {
  return extensions::CreateUpdateManifest(
      {extensions::UpdateManifestItem(extension_id)
           .codebase(crx_url.spec())
           .version(extension_version.GetString())});
}

bool ParseExtensionManifestData(const base::FilePath& extension_dir_path,
                                base::Version* extension_version) {
  std::string error_message;
  std::optional<base::Value::Dict> extension_manifest;
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    extension_manifest =
        extensions::file_util::LoadManifest(extension_dir_path, &error_message);
  }
  if (!extension_manifest) {
    ADD_FAILURE() << "Failed to load extension manifest from "
                  << extension_dir_path.value() << ": " << error_message;
    return false;
  }
  const std::string* version_string =
      extension_manifest->FindString(extensions::manifest_keys::kVersion);
  if (!version_string) {
    ADD_FAILURE() << "Failed to load extension version from "
                  << extension_dir_path.value()
                  << ": manifest key missing or has wrong type";
    return false;
  }
  *extension_version = base::Version(*version_string);
  if (!extension_version->IsValid()) {
    ADD_FAILURE() << "Failed to load extension version from "
                  << extension_dir_path.value() << ": bad format";
    return false;
  }
  return true;
}

bool ParseCrxOuterData(const base::FilePath& crx_path,
                       extensions::ExtensionId* extension_id) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  std::string public_key;
  const crx_file::VerifierResult crx_verifier_result = crx_file::Verify(
      crx_path, crx_file::VerifierFormat::CRX3,
      /*required_key_hashes=*/std::vector<std::vector<uint8_t>>(),
      /*required_file_hash=*/std::vector<uint8_t>(), &public_key, extension_id,
      /*compressed_verified_contents=*/nullptr);
  if (crx_verifier_result != crx_file::VerifierResult::OK_FULL) {
    ADD_FAILURE() << "Failed to read created CRX: verifier result "
                  << static_cast<int>(crx_verifier_result);
    return false;
  }
  return true;
}

bool ParseCrxInnerData(const base::FilePath& crx_path,
                       base::Version* extension_version) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    ADD_FAILURE() << "Failed to create temp directory";
    return false;
  }
  if (!zip::Unzip(crx_path, temp_dir.GetPath())) {
    ADD_FAILURE() << "Failed to unpack CRX from " << crx_path.value();
    return false;
  }
  return ParseExtensionManifestData(temp_dir.GetPath(), extension_version);
}

std::string MakeForceInstallPolicyItemValue(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url) {
  if (update_manifest_url.is_empty())
    return extension_id;
  return base::StringPrintf("%s;%s", extension_id.c_str(),
                            update_manifest_url.spec().c_str());
}

void UpdatePolicyViaMockPolicyProvider(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url,
    policy::MockConfigurationPolicyProvider* mock_policy_provider) {
  const std::string policy_item_value =
      MakeForceInstallPolicyItemValue(extension_id, update_manifest_url);
  policy::PolicyMap policy_map =
      mock_policy_provider->policies()
          .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                       /*component_id=*/std::string()))
          .Clone();
  policy::PolicyMap::Entry* const existing_entry =
      policy_map.GetMutable(policy::key::kExtensionInstallForcelist);
  if (existing_entry && existing_entry->value(base::Value::Type::LIST)) {
    // Append to the existing policy.
    existing_entry->value(base::Value::Type::LIST)
        ->GetList()
        .Append(policy_item_value);
  } else {
    // Set the new policy value.
    base::Value::List policy_value;
    policy_value.Append(policy_item_value);
    policy_map.Set(policy::key::kExtensionInstallForcelist,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(policy_value)),
                   /*external_data_fetcher=*/nullptr);
  }
  mock_policy_provider->UpdateChromePolicy(policy_map);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void UpdatePolicyViaDeviceStateMixin(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url,
    ash::DeviceStateMixin* device_state_mixin) {
  device_state_mixin->RequestDevicePolicyUpdate()
      ->policy_payload()
      ->mutable_device_login_screen_extensions()
      ->add_device_login_screen_extensions(
          MakeForceInstallPolicyItemValue(extension_id, update_manifest_url));
}

void UpdatePolicyViaDevicePolicyCrosTestHelper(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url,
    policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper) {
  device_policy_cros_test_helper->device_policy()
      ->payload()
      .mutable_device_login_screen_extensions()
      ->add_device_login_screen_extensions(
          MakeForceInstallPolicyItemValue(extension_id, update_manifest_url));
  device_policy_cros_test_helper->RefreshDevicePolicy();
}

void UpdatePolicyViaEmbeddedPolicyMixin(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url,
    ash::EmbeddedPolicyTestServerMixin* policy_test_server_mixin,
    policy::UserPolicyBuilder* user_policy_builder,
    const std::string& account_id,
    const std::string& policy_type,
    bool* success) {
  user_policy_builder->payload()
      .mutable_extensioninstallforcelist()
      ->mutable_value()
      ->add_entries(
          MakeForceInstallPolicyItemValue(extension_id, update_manifest_url));
  user_policy_builder->Build();

  policy_test_server_mixin->UpdatePolicy(
      policy_type, account_id,
      user_policy_builder->payload().SerializeAsString());

  base::RunLoop run_loop;
  g_browser_process->policy_service()->RefreshPolicies(
      run_loop.QuitClosure(), policy::PolicyFetchReason::kTest);
  ASSERT_NO_FATAL_FAILURE(run_loop.Run());

  // Report the outcome via an output argument instead of the return value,
  // since ASSERT_NO_FATAL_FAILURE() only works in void functions.
  *success = true;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Simulates a server error according to the current error mode, or returns no
// response when no error is configured. Note that this function is called on
// the IO thread.
std::unique_ptr<net::test_server::HttpResponse> ErrorSimulatingRequestHandler(
    std::atomic<ExtensionForceInstallMixin::ServerErrorMode>* server_error_mode,
    const net::test_server::HttpRequest& /*request*/) {
  switch (server_error_mode->load()) {
    case ExtensionForceInstallMixin::ServerErrorMode::kNone: {
      return nullptr;
    }
    case ExtensionForceInstallMixin::ServerErrorMode::kHung: {
      return std::make_unique<net::test_server::HungResponse>();
    }
    case ExtensionForceInstallMixin::ServerErrorMode::kInternalError: {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
      return response;
    }
  }
}

}  // namespace

ExtensionForceInstallMixin::ExtensionForceInstallMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

ExtensionForceInstallMixin::~ExtensionForceInstallMixin() = default;

void ExtensionForceInstallMixin::InitWithMockPolicyProvider(
    Profile* profile,
    policy::MockConfigurationPolicyProvider* mock_policy_provider) {
  DCHECK(mock_policy_provider);
  DCHECK(!initialized_) << "Init already called";
  DCHECK(!profile_);
  DCHECK(!mock_policy_provider_);
  initialized_ = true;
  profile_ = profile;
  mock_policy_provider_ = mock_policy_provider;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void ExtensionForceInstallMixin::InitWithDeviceStateMixin(
    Profile* profile,
    ash::DeviceStateMixin* device_state_mixin) {
  DCHECK(device_state_mixin);
  DCHECK(!initialized_) << "Init already called";
  DCHECK(!profile_);
  DCHECK(!device_state_mixin_);
  initialized_ = true;
  profile_ = profile;
  device_state_mixin_ = device_state_mixin;
}

void ExtensionForceInstallMixin::InitWithDevicePolicyCrosTestHelper(
    Profile* profile,
    policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper) {
  DCHECK(device_policy_cros_test_helper);
  DCHECK(!initialized_) << "Init already called";
  DCHECK(!profile_);
  DCHECK(!device_policy_cros_test_helper_);
  initialized_ = true;
  profile_ = profile;
  device_policy_cros_test_helper_ = device_policy_cros_test_helper;
}

void ExtensionForceInstallMixin::InitWithEmbeddedPolicyMixin(
    Profile* profile,
    ash::EmbeddedPolicyTestServerMixin* policy_test_server_mixin,
    policy::UserPolicyBuilder* user_policy_builder,
    const std::string& account_id,
    const std::string& policy_type) {
  DCHECK(policy_test_server_mixin);
  DCHECK(user_policy_builder);
  DCHECK(!account_id.empty());
  DCHECK(!policy_type.empty());
  DCHECK(!initialized_) << "Init already called";
  DCHECK(!profile_);
  DCHECK(!policy_test_server_mixin_);
  DCHECK(!user_policy_builder_);
  DCHECK(account_id_.empty());
  DCHECK(policy_type_.empty());
  initialized_ = true;
  profile_ = profile;
  policy_test_server_mixin_ = policy_test_server_mixin;
  user_policy_builder_ = user_policy_builder;
  account_id_ = account_id;
  policy_type_ = policy_type;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool ExtensionForceInstallMixin::ForceInstallFromCrx(
    const base::FilePath& crx_path,
    WaitMode wait_mode,
    extensions::ExtensionId* extension_id,
    base::Version* extension_version) {
  DCHECK(initialized_) << "Init not called";
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  extensions::ExtensionId local_extension_id;
  if (!ParseCrxOuterData(crx_path, &local_extension_id))
    return false;
  if (extension_id)
    *extension_id = local_extension_id;
  base::Version local_extension_version;
  if (!ParseCrxInnerData(crx_path, &local_extension_version))
    return false;
  if (extension_version)
    *extension_version = local_extension_version;
  return ServeExistingCrx(crx_path, local_extension_id,
                          local_extension_version) &&
         ForceInstallFromServedCrx(local_extension_id, local_extension_version,
                                   wait_mode);
}

bool ExtensionForceInstallMixin::ForceInstallFromSourceDir(
    const base::FilePath& extension_dir_path,
    const std::optional<base::FilePath>& pem_path,
    WaitMode wait_mode,
    extensions::ExtensionId* extension_id,
    base::Version* extension_version) {
  DCHECK(initialized_) << "Init not called";
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  base::Version local_extension_version;
  if (!ParseExtensionManifestData(extension_dir_path, &local_extension_version))
    return false;
  if (extension_version)
    *extension_version = local_extension_version;
  extensions::ExtensionId local_extension_id;
  if (!CreateAndServeCrx(extension_dir_path, pem_path, local_extension_version,
                         &local_extension_id)) {
    return false;
  }
  if (extension_id)
    *extension_id = local_extension_id;
  return ForceInstallFromServedCrx(local_extension_id, local_extension_version,
                                   wait_mode);
}

bool ExtensionForceInstallMixin::UpdateFromCrx(
    const base::FilePath& crx_path,
    UpdateWaitMode wait_mode,
    base::Version* extension_version) {
  DCHECK(initialized_) << "Init not called";
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  extensions::ExtensionId extension_id;
  if (!ParseCrxOuterData(crx_path, &extension_id))
    return false;
  base::Version local_extension_version;
  if (!ParseCrxInnerData(crx_path, &local_extension_version))
    return false;
  if (extension_version)
    *extension_version = local_extension_version;
  return ServeExistingCrx(crx_path, extension_id, local_extension_version) &&
         CreateAndServeUpdateManifestFile(extension_id,
                                          local_extension_version) &&
         WaitForExtensionUpdate(extension_id, local_extension_version,
                                wait_mode);
}

bool ExtensionForceInstallMixin::UpdateFromSourceDir(
    const base::FilePath& extension_dir_path,
    const extensions::ExtensionId& extension_id,
    UpdateWaitMode wait_mode,
    base::Version* extension_version) {
  DCHECK(initialized_) << "Init not called";
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  // Get the PEM path that was used for packing the extension last time, so that
  // packing results in the same extension ID.
  auto pem_path_iter = extension_id_to_pem_path_.find(extension_id);
  if (pem_path_iter == extension_id_to_pem_path_.end()) {
    ADD_FAILURE() << "Requested update of extension that wasn't installed via "
                     "ForceInstallFromSourceDir()";
    return false;
  }
  const base::FilePath pem_path = pem_path_iter->second;

  base::Version local_extension_version;
  if (!ParseExtensionManifestData(extension_dir_path, &local_extension_version))
    return false;
  if (extension_version)
    *extension_version = local_extension_version;
  extensions::ExtensionId packed_extension_id;
  if (!CreateAndServeCrx(extension_dir_path, pem_path, local_extension_version,
                         &packed_extension_id)) {
    return false;
  }
  if (packed_extension_id != extension_id) {
    ADD_FAILURE() << "Unexpected extension ID after packing: "
                  << packed_extension_id << ", expected: " << extension_id;
    return false;
  }
  return CreateAndServeUpdateManifestFile(extension_id,
                                          local_extension_version) &&
         WaitForExtensionUpdate(extension_id, local_extension_version,
                                wait_mode);
}

const extensions::Extension* ExtensionForceInstallMixin::GetInstalledExtension(
    const extensions::ExtensionId& extension_id) const {
  DCHECK(initialized_) << "Init not called";
  if (!profile_) {
    ADD_FAILURE() << "No profile passed to the Init method";
    return nullptr;
  }

  const auto* const registry = extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  return registry->GetInstalledExtension(extension_id);
}

const extensions::Extension* ExtensionForceInstallMixin::GetEnabledExtension(
    const extensions::ExtensionId& extension_id) const {
  DCHECK(initialized_) << "Init not called";
  if (!profile_) {
    ADD_FAILURE() << "No profile passed to the Init method";
    return nullptr;
  }

  const auto* const registry = extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  return registry->enabled_extensions().GetByID(extension_id);
}

void ExtensionForceInstallMixin::SetServerErrorMode(
    ServerErrorMode server_error_mode) {
  server_error_mode_.store(server_error_mode);
}

void ExtensionForceInstallMixin::SetUpOnMainThread() {
  // Create a temporary directory for keeping served and auxiliary files.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath served_dir_path =
      temp_dir_.GetPath().AppendASCII(kServedDirName);
  ASSERT_TRUE(base::CreateDirectory(served_dir_path));

  // Start the embedded test server. The first request handler is a handler for
  // simulating errors as configured (note that the error mode is shared via an
  // atomic variable, so that its changes on the main thread are correctly
  // picked up by the handler on the IO thread). The default handler is serving
  // files from the directory created above.
  embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
      &ErrorSimulatingRequestHandler, base::Unretained(&server_error_mode_)));
  embedded_test_server_.ServeFilesFromDirectory(served_dir_path);
  ASSERT_TRUE(embedded_test_server_.Start());
}

base::FilePath ExtensionForceInstallMixin::GetPathInServedDir(
    const std::string& file_name) const {
  return temp_dir_.GetPath().AppendASCII(kServedDirName).AppendASCII(file_name);
}

GURL ExtensionForceInstallMixin::GetServedUpdateManifestUrl(
    const extensions::ExtensionId& extension_id) const {
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  return embedded_test_server_.GetURL(
      "/" + GetServedUpdateManifestFileName(extension_id));
}

GURL ExtensionForceInstallMixin::GetServedCrxUrl(
    const extensions::ExtensionId& extension_id,
    const base::Version& extension_version) const {
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  return embedded_test_server_.GetURL(
      "/" + GetServedCrxFileName(extension_id, extension_version));
}

bool ExtensionForceInstallMixin::ServeExistingCrx(
    const base::FilePath& source_crx_path,
    const extensions::ExtensionId& extension_id,
    const base::Version& extension_version) {
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // First copy the CRX into a temporary location.
  base::FilePath temp_crx_path;
  if (!base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_crx_path)) {
    ADD_FAILURE() << "Failed to create a temporary file.";
    return false;
  }
  if (!base::CopyFile(source_crx_path, temp_crx_path)) {
    ADD_FAILURE() << "Failed to copy CRX from " << source_crx_path.value()
                  << " to " << temp_crx_path.value();
    return false;
  }

  // Then atomically move the created file into the served directory. This is
  // important as the embedded test server is reading files on a different
  // thread (IO) and, for example, we can be asked to re-serve the same version
  // again.
  const base::FilePath served_crx_path =
      GetPathInServedDir(GetServedCrxFileName(extension_id, extension_version));
  if (!base::Move(temp_crx_path, served_crx_path)) {
    ADD_FAILURE() << "Failed to move CRX from " << temp_crx_path.value()
                  << " to " << served_crx_path.value();
    return false;
  }
  return true;
}

bool ExtensionForceInstallMixin::CreateAndServeCrx(
    const base::FilePath& extension_dir_path,
    const std::optional<base::FilePath>& pem_path,
    const base::Version& extension_version,
    extensions::ExtensionId* extension_id) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // Use a temporary CRX file name, since the ID is yet unknown if |pem_path| is
  // empty. Delete the file first in case the previous operation failed in the
  // middle.
  const std::string kTempCrxFileName = "temp.crx";
  const base::FilePath temp_crx_path =
      temp_dir_.GetPath().AppendASCII(kTempCrxFileName);
  base::DeleteFile(temp_crx_path);

  // Use the specified PEM file, if any. Otherwise, create a file in the temp
  // dir and let the extension creator populate it with a random key.
  base::FilePath final_pem_path;
  if (pem_path) {
    final_pem_path = *pem_path;
  } else if (!base::CreateTemporaryFileInDir(temp_dir_.GetPath(),
                                             &final_pem_path)) {
    ADD_FAILURE() << "Failed to create a temp PEM file";
    return false;
  }

  extensions::ExtensionCreator extension_creator;
  if (!extension_creator.Run(extension_dir_path, temp_crx_path,
                             pem_path.value_or(base::FilePath()),
                             /*private_key_output_path=*/
                             pem_path ? base::FilePath() : final_pem_path,
                             /*run_flags=*/0)) {
    ADD_FAILURE() << "Failed to pack extension: "
                  << extension_creator.error_message();
    return false;
  }

  if (!ParseCrxOuterData(temp_crx_path, extension_id))
    return false;
  const base::FilePath served_crx_path = GetPathInServedDir(
      GetServedCrxFileName(*extension_id, extension_version));
  if (!base::Move(temp_crx_path, served_crx_path)) {
    ADD_FAILURE() << "Failed to move the created CRX file to "
                  << served_crx_path.value();
    return false;
  }

  extension_id_to_pem_path_[*extension_id] = final_pem_path;
  return true;
}

bool ExtensionForceInstallMixin::ForceInstallFromServedCrx(
    const extensions::ExtensionId& extension_id,
    const base::Version& extension_version,
    WaitMode wait_mode) {
  DCHECK(initialized_) << "Init not called";
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  if (!CreateAndServeUpdateManifestFile(extension_id, extension_version))
    return false;
  // Prepare the waiter's observers before setting the policy, so that we don't
  // miss synchronous operations triggered by the policy update.
  ForceInstallWaiter waiter(wait_mode, extension_id, profile_);
  if (!UpdatePolicy(extension_id, GetServedUpdateManifestUrl(extension_id)))
    return false;
  return waiter.Wait();
}

bool ExtensionForceInstallMixin::CreateAndServeUpdateManifestFile(
    const extensions::ExtensionId& extension_id,
    const base::Version& extension_version) {
  DCHECK(embedded_test_server_.Started()) << "Called before setup";

  const GURL crx_url = GetServedCrxUrl(extension_id, extension_version);
  const std::string update_manifest =
      GenerateUpdateManifest(extension_id, extension_version, crx_url);
  const base::FilePath update_manifest_path =
      GetPathInServedDir(GetServedUpdateManifestFileName(extension_id));
  // Note: Doing an atomic write, since the embedded test server might
  // concurrently try to access this file from another thread.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  if (!base::ImportantFileWriter::WriteFileAtomically(update_manifest_path,
                                                      update_manifest)) {
    ADD_FAILURE() << "Failed to write update manifest file";
    return false;
  }
  return true;
}

bool ExtensionForceInstallMixin::UpdatePolicy(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url) {
  DCHECK(initialized_) << "Init not called";

  if (mock_policy_provider_) {
    UpdatePolicyViaMockPolicyProvider(extension_id, update_manifest_url,
                                      mock_policy_provider_);
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (device_state_mixin_) {
    UpdatePolicyViaDeviceStateMixin(extension_id, update_manifest_url,
                                    device_state_mixin_);
    return true;
  }
  if (device_policy_cros_test_helper_) {
    UpdatePolicyViaDevicePolicyCrosTestHelper(extension_id, update_manifest_url,
                                              device_policy_cros_test_helper_);
    return true;
  }
  if (policy_test_server_mixin_) {
    bool success = false;
    UpdatePolicyViaEmbeddedPolicyMixin(
        extension_id, update_manifest_url, policy_test_server_mixin_,
        user_policy_builder_, account_id_, policy_type_, &success);
    return success;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED_IN_MIGRATION() << "Init not called";
  return false;
}

bool ExtensionForceInstallMixin::WaitForExtensionUpdate(
    const extensions::ExtensionId& extension_id,
    const base::Version& extension_version,
    UpdateWaitMode wait_mode) {
  switch (wait_mode) {
    case UpdateWaitMode::kNone:
      return true;
  }
}
