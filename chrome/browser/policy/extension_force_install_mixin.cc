// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/extension_force_install_mixin.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/crx_verifier.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/test_background_page_first_load_observer.h"
#include "extensions/test/test_background_page_ready_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#endif

namespace {

// Name of the directory whose contents are served by the embedded test
// server.
constexpr char kServedDirName[] = "served";
// Template for the file name of a served CRX file.
constexpr char kCrxFileNameTemplate[] = "%s-%s.crx";
// Template for the file name of a served update manifest file.
constexpr char kUpdateManifestFileNameTemplate[] = "%s.xml";
// Template for the update manifest contents.
constexpr char kUpdateManifestTemplate[] =
    R"(<?xml version='1.0' encoding='UTF-8'?>
       <gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>
         <app appid='$1'>
           <updatecheck codebase='$2' version='$3' />
         </app>
       </gupdate>)";

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

  PrefService* const pref_service_;
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
  DCHECK_EQ(pref->GetType(), base::Value::Type::DICTIONARY);
  return pref->GetValue()->FindKey(extension_id_) != nullptr;
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
  Profile* const profile_;
  std::unique_ptr<ForceInstallPrefObserver> force_install_pref_observer_;
  std::unique_ptr<extensions::TestExtensionRegistryObserver> registry_observer_;
  std::unique_ptr<extensions::ExtensionBackgroundPageReadyObserver>
      background_page_ready_observer_;
  std::unique_ptr<extensions::TestBackgroundPageFirstLoadObserver>
      background_page_first_load_observer_;
};

ForceInstallWaiter::ForceInstallWaiter(
    ExtensionForceInstallMixin::WaitMode wait_mode,
    const extensions::ExtensionId& extension_id,
    Profile* profile)
    : wait_mode_(wait_mode), extension_id_(extension_id), profile_(profile) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id_));
  DCHECK(profile_);
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
    case ExtensionForceInstallMixin::WaitMode::kBackgroundPageReady:
      background_page_ready_observer_ =
          std::make_unique<extensions::ExtensionBackgroundPageReadyObserver>(
              profile_, extension_id_);
      break;
    case ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad:
      background_page_first_load_observer_ =
          std::make_unique<extensions::TestBackgroundPageFirstLoadObserver>(
              profile_, extension_id_);
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
    case ExtensionForceInstallMixin::WaitMode::kBackgroundPageReady:
      // Wait and assert that the waiting run loop didn't time out.
      ASSERT_NO_FATAL_FAILURE(background_page_ready_observer_->Wait());
      *success = true;
      break;
    case ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad:
      // Wait and assert that the waiting run loop didn't time out.
      ASSERT_NO_FATAL_FAILURE(background_page_first_load_observer_->Wait());
      *success = true;
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
  return base::ReplaceStringPlaceholders(
      kUpdateManifestTemplate,
      {extension_id, crx_url.spec(), extension_version.GetString()},
      /*offsets=*/nullptr);
}

bool ParseExtensionManifestData(const base::FilePath& extension_dir_path,
                                base::Version* extension_version) {
  std::string error_message;
  std::unique_ptr<base::DictionaryValue> extension_manifest;
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
  std::string version_string;
  if (!extension_manifest->GetString(extensions::manifest_keys::kVersion,
                                     &version_string)) {
    ADD_FAILURE() << "Failed to load extension version from "
                  << extension_dir_path.value()
                  << ": manifest key missing or has wrong type";
    return false;
  }
  *extension_version = base::Version(version_string);
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
  policy::PolicyMap policy_map;
  policy_map.CopyFrom(
      mock_policy_provider->policies().Get(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_CHROME, /*component_id=*/std::string())));
  policy::PolicyMap::Entry* const existing_entry =
      policy_map.GetMutable(policy::key::kExtensionInstallForcelist);
  if (existing_entry) {
    // Append to the existing policy.
    existing_entry->value()->Append(policy_item_value);
  } else {
    // Set the new policy value.
    base::Value policy_value(base::Value::Type::LIST);
    policy_value.Append(policy_item_value);
    policy_map.Set(policy::key::kExtensionInstallForcelist,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, std::move(policy_value),
                   /*external_data_fetcher=*/nullptr);
  }
  mock_policy_provider->UpdateChromePolicy(policy_map);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void UpdatePolicyViaDeviceStateMixin(
    const extensions::ExtensionId& extension_id,
    const GURL& update_manifest_url,
    chromeos::DeviceStateMixin* device_state_mixin) {
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ExtensionForceInstallMixin::ExtensionForceInstallMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

ExtensionForceInstallMixin::~ExtensionForceInstallMixin() = default;

void ExtensionForceInstallMixin::InitWithMockPolicyProvider(
    Profile* profile,
    policy::MockConfigurationPolicyProvider* mock_policy_provider) {
  DCHECK(profile);
  DCHECK(mock_policy_provider);
  DCHECK(!profile_) << "Init already called";
  DCHECK(!mock_policy_provider_);
  profile_ = profile;
  mock_policy_provider_ = mock_policy_provider;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void ExtensionForceInstallMixin::InitWithDeviceStateMixin(
    Profile* profile,
    chromeos::DeviceStateMixin* device_state_mixin) {
  DCHECK(profile);
  DCHECK(device_state_mixin);
  DCHECK(!profile_) << "Init already called";
  DCHECK(!device_state_mixin_);
  profile_ = profile;
  device_state_mixin_ = device_state_mixin;
}

void ExtensionForceInstallMixin::InitWithDevicePolicyCrosTestHelper(
    Profile* profile,
    policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper) {
  DCHECK(profile);
  DCHECK(device_policy_cros_test_helper);
  DCHECK(!profile_) << "Init already called";
  DCHECK(!device_policy_cros_test_helper_);
  profile_ = profile;
  device_policy_cros_test_helper_ = device_policy_cros_test_helper;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool ExtensionForceInstallMixin::ForceInstallFromCrx(
    const base::FilePath& crx_path,
    WaitMode wait_mode,
    extensions::ExtensionId* extension_id,
    base::Version* extension_version) {
  DCHECK(profile_) << "Init not called";
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
    const base::Optional<base::FilePath>& pem_path,
    WaitMode wait_mode,
    extensions::ExtensionId* extension_id,
    base::Version* extension_version) {
  DCHECK(profile_) << "Init not called";
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

const extensions::Extension* ExtensionForceInstallMixin::GetInstalledExtension(
    const extensions::ExtensionId& extension_id) const {
  DCHECK(profile_) << "Init not called";

  const auto* const registry = extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  return registry->GetInstalledExtension(extension_id);
}

const extensions::Extension* ExtensionForceInstallMixin::GetEnabledExtension(
    const extensions::ExtensionId& extension_id) const {
  DCHECK(profile_) << "Init not called";

  const auto* const registry = extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  return registry->enabled_extensions().GetByID(extension_id);
}

bool ExtensionForceInstallMixin::IsExtensionBackgroundPageReady(
    const extensions::ExtensionId& extension_id) const {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  DCHECK(profile_) << "Init not called";

  const auto* const extension = GetInstalledExtension(extension_id);
  if (!extension) {
    ADD_FAILURE() << "Extension " << extension_id << " not installed";
    return false;
  }
  auto* const extension_system = extensions::ExtensionSystem::Get(profile_);
  DCHECK(extension_system);
  return extension_system->runtime_data()->IsBackgroundPageReady(extension);
}

void ExtensionForceInstallMixin::SetUpOnMainThread() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath served_dir_path =
      temp_dir_.GetPath().AppendASCII(kServedDirName);
  ASSERT_TRUE(base::CreateDirectory(served_dir_path));
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

  const base::FilePath served_crx_path =
      GetPathInServedDir(GetServedCrxFileName(extension_id, extension_version));
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  if (!base::CopyFile(source_crx_path, served_crx_path)) {
    ADD_FAILURE() << "Failed to copy CRX from " << source_crx_path.value()
                  << " to " << served_crx_path.value();
    return false;
  }
  return true;
}

bool ExtensionForceInstallMixin::CreateAndServeCrx(
    const base::FilePath& extension_dir_path,
    const base::Optional<base::FilePath>& pem_path,
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
  extensions::ExtensionCreator extension_creator;
  if (!extension_creator.Run(extension_dir_path, temp_crx_path,
                             pem_path.value_or(base::FilePath()),
                             /*private_key_output_path=*/base::FilePath(),
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

  return true;
}

bool ExtensionForceInstallMixin::ForceInstallFromServedCrx(
    const extensions::ExtensionId& extension_id,
    const base::Version& extension_version,
    WaitMode wait_mode) {
  DCHECK(profile_) << "Init not called";
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
  DCHECK(profile_) << "Init not called";

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED() << "Init not called";
  return false;
}
