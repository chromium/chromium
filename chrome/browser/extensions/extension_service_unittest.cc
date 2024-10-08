// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/extension_service.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/chrome_zipfile_installer.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_testing_loader.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/plugin_manager.h"
#include "chrome/browser/extensions/preinstalled_apps.h"
#include "chrome/browser/extensions/scoped_database_manager_for_test.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_waiter.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/verifier_formats.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

// The blocklist tests rely on the safe-browsing database.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#define ENABLE_BLOCKLIST_TESTS
#endif

using base::ScopedObservation;
using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;
using extensions::mojom::APIPermissionID;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace keys = manifest_keys;

namespace {

// Extension ids used during testing.
const char good0[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
const char good1[] = "hpiknbiabeeppbpihjehijgoemciehgk";
const char good2[] = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
const char all_zero[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char good2048[] = "dfhpodpjggiioolfhoimofdbfjibmedp";
const char good_crx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char minimal_platform_app_crx[] = "jjeoclcdfjddkdjokiejckgcildcflpp";
const char hosted_app[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char page_action[] = "dpfmafkdlbmopmcepgpjkpldjbghdibm";
const char theme_crx[] = "idlfhncioikpdnlhnmcjogambnefbbfp";
const char theme2_crx[] = "ibcijncamhmjjdodjamgiipcgnnaeagd";
const char permissions_crx[] = "eagpmdpfmaekmmcejjbmjoecnejeiiin";
const char updates_from_webstore[] = "akjooamlhcgeopfifcmlggaebeocgokj";
const char updates_from_webstore2[] = "oolblhbomdbcpmafphaodhjfcgbihcdg";
const char updates_from_webstore3[] = "bmfoocgfinpmkmlbjhcbofejhkhlbchk";
const char permissions_blocklist[] = "noffkehfcaggllbcojjbopcmlhcnhcdn";
const char video_player_app[] = "jcgeabjmjgoblfofpppfkcoakmfobdko";
const char kPrefBlocklistState[] = "blacklist_state";

// A helper value to cast the malware blocklist state to an integer.
static constexpr int kBlocklistedMalwareInteger =
    static_cast<int>(BitMapBlocklistState::BLOCKLISTED_MALWARE);

struct BubbleErrorsTestData {
  BubbleErrorsTestData(const std::string& id,
                       const std::string& version,
                       const base::FilePath& crx_path,
                       size_t expected_bubble_error_count)
      : id(id),
        version(version),
        crx_path(crx_path),
        expected_bubble_error_count(expected_bubble_error_count) {}
  std::string id;
  std::string version;
  base::FilePath crx_path;
  size_t expected_bubble_error_count;
  bool expect_has_shown_bubble_view;
};

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

base::FilePath GetTemporaryFile() {
  base::FilePath temp_file;
  CHECK(base::CreateTemporaryFile(&temp_file));
  return temp_file;
}

bool HasExternalInstallErrors(ExtensionService* service) {
  return !service->external_install_manager()->GetErrorsForTesting().empty();
}

bool HasExternalInstallBubble(ExtensionService* service) {
  return base::Contains(
      service->external_install_manager()->GetErrorsForTesting(),
      ExternalInstallError::BUBBLE_ALERT, &ExternalInstallError::alert_type);
}

size_t GetExternalInstallBubbleCount(ExtensionService* service) {
  size_t bubble_count = 0u;
  std::vector<ExternalInstallError*> errors =
      service->external_install_manager()->GetErrorsForTesting();
  for (auto* error : errors)
    bubble_count += error->alert_type() == ExternalInstallError::BUBBLE_ALERT;
  return bubble_count;
}

scoped_refptr<const Extension> CreateExtension(const std::string& name,
                                               const base::FilePath& path,
                                               ManifestLocation location) {
  return ExtensionBuilder(name).SetPath(path).SetLocation(location).Build();
}

std::unique_ptr<ExternalInstallInfoFile> CreateExternalExtension(
    const ExtensionId& extension_id,
    const std::string& version_str,
    const base::FilePath& path,
    ManifestLocation location,
    Extension::InitFromValueFlags flags) {
  return std::make_unique<ExternalInstallInfoFile>(
      extension_id, base::Version(version_str), path, location, flags, false,
      false);
}

// Helper function to persist the passed directories and file paths in
// |extension_dir|. Also, writes a generic manifest file.
void PersistExtensionWithPaths(
    const base::FilePath& extension_dir,
    const std::vector<base::FilePath>& directory_paths,
    const std::vector<base::FilePath>& file_paths) {
  for (const auto& directory : directory_paths)
    EXPECT_TRUE(base::CreateDirectory(directory));

  std::string data = "file_data";
  for (const auto& file : file_paths) {
    EXPECT_TRUE(base::WriteFile(file, data));
  }

  base::Value::Dict manifest = base::Value::Dict()
                                   .Set(keys::kName, "Test extension")
                                   .Set(keys::kVersion, "1.0")
                                   .Set(keys::kManifestVersion, 2);

  // Persist manifest file.
  base::FilePath manifest_path = extension_dir.Append(kManifestFilename);
  JSONFileValueSerializer(manifest_path).Serialize(manifest);
  EXPECT_TRUE(base::PathExists(manifest_path));
}

}  // namespace

class MockProviderVisitor : public ExternalProviderInterface::VisitorInterface {
 public:
  // The provider will return |fake_base_path| from
  // GetBaseCrxFilePath().  User can test the behavior with
  // and without an empty path using this parameter.
  explicit MockProviderVisitor(base::FilePath fake_base_path)
      : ids_found_(0),
        fake_base_path_(fake_base_path),
        expected_creation_flags_(Extension::NO_FLAGS) {
    profile_ = std::make_unique<TestingProfile>();
  }

  MockProviderVisitor(base::FilePath fake_base_path,
                      int expected_creation_flags)
      : ids_found_(0),
        fake_base_path_(fake_base_path),
        expected_creation_flags_(expected_creation_flags) {
    profile_ = std::make_unique<TestingProfile>();
  }

  MockProviderVisitor(const MockProviderVisitor&) = delete;
  MockProviderVisitor& operator=(const MockProviderVisitor&) = delete;

  int Visit(const std::string& json_data) {
    return Visit(json_data, ManifestLocation::kExternalPref,
                 ManifestLocation::kExternalPrefDownload);
  }

  int Visit(const std::string& json_data,
            ManifestLocation crx_location,
            ManifestLocation download_location) {
    SetUp(json_data, crx_location, download_location);

    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    prefs_ = GetDictionaryFromJSON(json_data);

    // Reset our counter.
    ids_found_ = 0;
    // Ask the provider to look up all extensions and return them.
    provider_->VisitRegisteredExtension();

    return ids_found_;
  }

  int ReinstallProviderExtensions(const std::string& json_data) {
    return ReinstallProviderExtensions(json_data,
                                       ManifestLocation::kExternalPref,
                                       ManifestLocation::kExternalPrefDownload);
  }

  int ReinstallProviderExtensions(const std::string& json_data,
                                  ManifestLocation crx_location,
                                  ManifestLocation download_location) {
    // Don't recreate the provider_ because that way we will never have any
    // saved prefs inside.
    if (!provider_)
      SetUp(json_data, crx_location, download_location);

    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    prefs_ = GetDictionaryFromJSON(json_data);

    // Reset our counter.
    ids_found_ = 0;
    // Ask the provider to look up all extensions and notify the visitor.
    provider_->TriggerOnExternalExtensionFound();

    return ids_found_;
  }

  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override {
    EXPECT_EQ(expected_creation_flags_, info.creation_flags);

    ++ids_found_;
    base::Value::Dict* pref = prefs_->FindDict(info.extension_id);
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(pref) << "Got back ID (" << info.extension_id
                      << ") we weren't expecting";

    EXPECT_TRUE(info.path.IsAbsolute());
    if (!fake_base_path_.empty())
      EXPECT_TRUE(fake_base_path_.IsParent(info.path));

    if (pref) {
      EXPECT_TRUE(provider_->HasExtension(info.extension_id));

      // Ask provider if the extension we got back is registered.
      ManifestLocation location = ManifestLocation::kInvalidLocation;
      std::unique_ptr<base::Version> v1;
      base::FilePath crx_path;

      EXPECT_TRUE(
          provider_->GetExtensionDetails(info.extension_id, nullptr, &v1));
      EXPECT_EQ(info.version.GetString(), v1->GetString());

      std::unique_ptr<base::Version> v2;
      EXPECT_TRUE(
          provider_->GetExtensionDetails(info.extension_id, &location, &v2));
      EXPECT_EQ(info.version.GetString(), v1->GetString());
      EXPECT_EQ(info.version.GetString(), v2->GetString());
      EXPECT_EQ(crx_location_, location);

      // Remove it so we won't count it ever again.
      prefs_->Remove(info.extension_id);
    }
    return true;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool force_update) override {
    ++ids_found_;
    base::Value::Dict* pref = prefs_->FindDict(info.extension_id);
    // This tests is to make sure that the provider only notifies us of the
    // values we gave it. So if the id we doesn't exist in our internal
    // dictionary then something is wrong.
    EXPECT_TRUE(pref) << "Got back ID (" << info.extension_id
                      << ") we weren't expecting";
    EXPECT_EQ(ManifestLocation::kExternalPrefDownload, info.download_location);

    if (pref) {
      EXPECT_TRUE(provider_->HasExtension(info.extension_id));

      // External extensions with update URLs do not have versions.
      std::unique_ptr<base::Version> v1;
      ManifestLocation location1 = ManifestLocation::kInvalidLocation;
      EXPECT_TRUE(
          provider_->GetExtensionDetails(info.extension_id, &location1, &v1));
      EXPECT_FALSE(v1.get());
      EXPECT_EQ(ManifestLocation::kExternalPrefDownload, location1);

      const std::string* parsed_install_parameter =
          pref->FindString("install_parameter");
      EXPECT_TRUE(parsed_install_parameter);
      EXPECT_EQ(*parsed_install_parameter, info.install_parameter);

      // Remove it so we won't count it again.
      prefs_->Remove(info.extension_id);
    }
    return true;
  }

  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "MockProviderVisitor does not provide incremental updates,"
                     " use MockUpdateProviderVisitor instead.";
  }

  void OnExternalProviderReady(
      const ExternalProviderInterface* provider) override {
    EXPECT_EQ(provider, provider_.get());
    EXPECT_TRUE(provider->IsReady());
  }

  Profile* profile() { return profile_.get(); }
  ExternalProviderImpl* provider() { return provider_.get(); }

 protected:
  std::unique_ptr<ExternalProviderImpl> provider_;

  void SetUp(const std::string& json_data,
             ManifestLocation crx_location,
             ManifestLocation download_location) {
    crx_location_ = crx_location;
    // Give the test json file to the provider for parsing.
    provider_ = std::make_unique<ExternalProviderImpl>(
        this, new ExternalTestingLoader(json_data, fake_base_path_),
        profile_.get(), crx_location, download_location, Extension::NO_FLAGS);
    if (crx_location == ManifestLocation::kExternalRegistry)
      provider_->set_allow_updates(true);
  }

  std::optional<base::Value::Dict> GetDictionaryFromJSON(
      const std::string& json_data) {
    // We also parse the file into a dictionary to compare what we get back
    // from the provider.
    JSONStringValueDeserializer deserializer(json_data);
    auto json_value = deserializer.Deserialize(nullptr, nullptr);

    if (!json_value || !json_value->is_dict()) {
      ADD_FAILURE() << "Unable to deserialize json data";
      return std::nullopt;
    }
    return std::move(*json_value).TakeDict();
  }

 private:
  int ids_found_;
  base::FilePath fake_base_path_;
  int expected_creation_flags_;
  ManifestLocation crx_location_;
  std::optional<base::Value::Dict> prefs_;
  std::unique_ptr<TestingProfile> profile_;
};

// Mock provider that can simulate incremental update like
// ExternalRegistryLoader.
class MockUpdateProviderVisitor : public MockProviderVisitor {
 public:
  // The provider will return |fake_base_path| from
  // GetBaseCrxFilePath().  User can test the behavior with
  // and without an empty path using this parameter.
  explicit MockUpdateProviderVisitor(base::FilePath fake_base_path)
      : MockProviderVisitor(fake_base_path) {}

  MockUpdateProviderVisitor(const MockUpdateProviderVisitor&) = delete;
  MockUpdateProviderVisitor& operator=(const MockUpdateProviderVisitor&) =
      delete;

  void VisitDueToUpdate(const std::string& json_data) {
    update_url_extension_ids_.clear();
    file_extension_ids_.clear();
    removed_extension_ids_.clear();

    auto new_prefs = GetDictionaryFromJSON(json_data);
    if (!new_prefs)
      return;
    provider_->UpdatePrefs(std::move(*new_prefs));
  }

  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    for (const auto& extension_info : update_url_extensions)
      update_url_extension_ids_.insert(extension_info.extension_id);
    EXPECT_EQ(update_url_extension_ids_.size(), update_url_extensions.size());

    for (const auto& extension_info : file_extensions)
      file_extension_ids_.insert(extension_info.extension_id);
    EXPECT_EQ(file_extension_ids_.size(), file_extensions.size());

    for (const auto& extension_id : removed_extensions)
      removed_extension_ids_.insert(extension_id);
  }

  size_t GetUpdateURLExtensionCount() {
    return update_url_extension_ids_.size();
  }
  size_t GetFileExtensionCount() { return file_extension_ids_.size(); }
  size_t GetRemovedExtensionCount() { return removed_extension_ids_.size(); }

  bool HasSeenUpdateWithUpdateUrl(const std::string& extension_id) {
    return update_url_extension_ids_.count(extension_id) > 0u;
  }
  bool HasSeenUpdateWithFile(const std::string& extension_id) {
    return file_extension_ids_.count(extension_id) > 0u;
  }
  bool HasSeenRemoval(const std::string& extension_id) {
    return removed_extension_ids_.count(extension_id) > 0u;
  }

 private:
  std::set<std::string> update_url_extension_ids_;
  std::set<std::string> file_extension_ids_;
  std::set<std::string> removed_extension_ids_;
};

struct MockExtensionRegistryObserver : public ExtensionRegistryObserver {
  MockExtensionRegistryObserver() = default;

  MockExtensionRegistryObserver(const MockExtensionRegistryObserver&) = delete;
  MockExtensionRegistryObserver& operator=(
      const MockExtensionRegistryObserver&) = delete;

  ~MockExtensionRegistryObserver() override = default;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    last_extension_loaded = extension->id();
  }
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    last_extension_unloaded = extension->id();
  }
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    last_extension_installed = extension->id();
  }
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override {
    last_extension_uninstalled = extension->id();
    last_extension_uninstalled_path = extension->path();
  }

  std::string last_extension_loaded;
  std::string last_extension_unloaded;
  std::string last_extension_installed;
  std::string last_extension_uninstalled;
  base::FilePath last_extension_uninstalled_path;
};

class ExtensionLoadedObserver : public ExtensionRegistryObserver {
 public:
  ExtensionLoadedObserver(ExtensionRegistry* registry, const std::string& id)
      : id_(id) {
    observation_.Observe(registry);
  }

  void Wait() { run_loop_.Run(); }

 protected:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    if (extension->id() == id_) {
      run_loop_.Quit();
    }
  }

 private:
  const std::string id_;
  ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver> observation_{
      this};
  base::RunLoop run_loop_;
};

class PendingRemovalObserver : public PendingExtensionManager::Observer {
 public:
  PendingRemovalObserver(PendingExtensionManager* pending,
                         const std::string& id)
      : observed_id_(id) {
    observation_.Observe(pending);
  }

  void WaitForRemoval() { run_loop_.Run(); }

 protected:
  void OnExtensionRemoved(const std::string& id) override {
    if (id == observed_id_) {
      run_loop_.Quit();
    }
  }

 private:
  const std::string observed_id_;
  ScopedObservation<PendingExtensionManager, PendingExtensionManager::Observer>
      observation_{this};
  base::RunLoop run_loop_;
};

class ExtensionServiceTest : public ExtensionServiceTestWithInstall {
 public:
  ExtensionServiceTest() = default;

  MockExternalProvider* AddMockExternalProvider(ManifestLocation location) {
    auto provider = std::make_unique<MockExternalProvider>(service(), location);
    MockExternalProvider* provider_ptr = provider.get();
    service()->AddProviderForTesting(std::move(provider));
    return provider_ptr;
  }

  // Checks for external extensions and waits for the one with the given `id` to
  // successfully complete installing.
  void WaitForExternalExtensionInstalled(const std::string& id) {
    ExtensionLoadedObserver observer(registry(), id);
    service()->CheckForExternalUpdates();
    observer.Wait();
  }

  // Waits for an installation attempt of the extension with the given id
  // to complete (successfully or not).
  void WaitForInstallationAttemptToComplete(const std::string& id) {
    service()->CheckForExternalUpdates();
    PendingRemovalObserver observer(service()->pending_extension_manager(), id);
    observer.WaitForRemoval();
  }

 protected:
  void TestExternalProvider(MockExternalProvider* provider,
                            ManifestLocation location);

  // Grants all optional permissions stated in manifest to active permission
  // set for extension |id|.
  void GrantAllOptionalPermissions(const std::string& id) {
    const Extension* extension = registry()->GetInstalledExtension(id);
    const PermissionSet& all_optional_permissions =
        PermissionsParser::GetOptionalPermissions(extension);
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension, all_optional_permissions);
  }

  testing::AssertionResult IsBlocked(const std::string& id) {
    const ExtensionSet all_unblocked_extensions =
        registry()->GenerateInstalledExtensionsSet(
            ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::BLOCKED);
    if (all_unblocked_extensions.Contains(id)) {
      return testing::AssertionFailure() << id << " is still unblocked!";
    }
    if (!registry()->blocked_extensions().Contains(id)) {
      return testing::AssertionFailure() << id << " is not blocked!";
    }
    return testing::AssertionSuccess();
  }

  // Helper method to test that an extension moves through being blocked and
  // unblocked as appropriate for its type.
  void AssertExtensionBlocksAndUnblocks(bool should_block,
                                        const std::string& extension_id) {
    // Assume we start in an unblocked state.
    EXPECT_FALSE(IsBlocked(extension_id));

    // Block the extensions.
    service()->BlockAllExtensions();
    task_environment()->RunUntilIdle();

    if (should_block)
      ASSERT_TRUE(IsBlocked(extension_id));
    else
      ASSERT_FALSE(IsBlocked(extension_id));

    service()->UnblockAllExtensions();
    task_environment()->RunUntilIdle();

    ASSERT_FALSE(IsBlocked(extension_id));
  }

  // Test that certain histograms are emitted for user and non-user profiles
  // (users for ChromeOS Ash).
  void RunEmitUserHistogramsTest(int nonuser_expected_total_count,
                                 int user_expected_total_count) {
    base::HistogramTester histograms;
    TestExtensionDir good_extension_dir;
    good_extension_dir.WriteManifest(
        R"({
           "name": "Good Extension",
           "version": "0.1",
           "manifest_version": 2
         })");

    ChromeTestExtensionLoader loader(testing_profile());
    loader.set_pack_extension(false);
    loader.LoadExtension(good_extension_dir.UnpackedPath());

    histograms.ExpectTotalCount("Extensions.InstallType", 1);
    histograms.ExpectTotalCount("Extensions.InstallSource", 1);
    histograms.ExpectTotalCount("Extensions.InstallType.NonUser",
                                nonuser_expected_total_count);
    histograms.ExpectTotalCount("Extensions.InstallType.User",
                                user_expected_total_count);
    histograms.ExpectTotalCount("Extensions.InstallSource.NonUser",
                                nonuser_expected_total_count);
    histograms.ExpectTotalCount("Extensions.InstallSource.User",
                                user_expected_total_count);
  }

  const base::Value::Dict* GetExtensionPref(const std::string& extension_id) {
    const base::Value::Dict& dict =
        profile()->GetPrefs()->GetDict(pref_names::kExtensions);
    const base::Value::Dict* pref = dict.FindDict(extension_id);
    if (!pref) {
      return nullptr;
    }
    return pref;
  }

  bool IsPrefExist(const std::string& extension_id,
                   const std::string& pref_path) {
    const base::Value::Dict* pref = GetExtensionPref(extension_id);
    return pref && pref->FindBoolByDottedPath(pref_path).has_value();
  }

  bool DoesIntegerPrefExist(const std::string& extension_id,
                            const std::string& pref_path) {
    const base::Value::Dict* pref = GetExtensionPref(extension_id);
    if (!pref) {
      return false;
    }
    return pref->FindIntByDottedPath(pref_path).has_value();
  }

  void SetPref(const std::string& extension_id,
               const std::string& pref_path,
               std::unique_ptr<base::Value> value,
               const std::string& msg) {
    ScopedDictPrefUpdate update(profile()->GetPrefs(), pref_names::kExtensions);
    base::Value::Dict& dict = update.Get();
    base::Value::Dict* pref = dict.FindDict(extension_id);
    ASSERT_TRUE(pref) << msg;
    pref->SetByDottedPath(pref_path,
                          base::Value::FromUniquePtrValue(std::move(value)));
  }

  void SetPrefList(const std::string& extension_id,
                   const std::string& pref_path,
                   base::Value::List& value,
                   const std::string& msg) {
    ScopedDictPrefUpdate update(profile()->GetPrefs(), pref_names::kExtensions);
    base::Value::Dict& dict = update.Get();
    base::Value::Dict* pref = dict.FindDict(extension_id);
    ASSERT_TRUE(pref) << msg;
    pref->SetByDottedPath(pref_path, std::move(value));
  }

  void SetPrefInteg(const std::string& extension_id,
                    const std::string& pref_path,
                    int value) {
    std::string msg = " while setting: ";
    msg += extension_id;
    msg += " ";
    msg += pref_path;
    msg += " = ";
    msg += base::NumberToString(value);

    SetPref(extension_id, pref_path, std::make_unique<base::Value>(value), msg);
  }

  void SetPrefBool(const std::string& extension_id,
                   const std::string& pref_path,
                   bool value) {
    std::string msg = " while setting: ";
    msg += extension_id + " " + pref_path;
    msg += " = ";
    msg += (value ? "true" : "false");

    SetPref(extension_id, pref_path, std::make_unique<base::Value>(value), msg);
  }

  void ClearPref(const std::string& extension_id,
                 const std::string& pref_path) {
    std::string msg = " while clearing: ";
    msg += extension_id + " " + pref_path;

    ScopedDictPrefUpdate update(profile()->GetPrefs(), pref_names::kExtensions);
    base::Value::Dict& dict = update.Get();
    base::Value::Dict* pref = dict.FindDict(extension_id);
    ASSERT_TRUE(pref) << msg;
    pref->RemoveByDottedPath(pref_path);
  }

  void SetPrefStringSet(const std::string& extension_id,
                        const std::string& pref_path,
                        const std::set<std::string>& value) {
    std::string msg = " while setting: ";
    msg += extension_id + " " + pref_path;

    base::Value::List list_value;
    for (const auto& item : value) {
      list_value.Append(item);
    }

    SetPrefList(extension_id, pref_path, list_value, msg);
  }

  void InitPluginService() {
#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();
#endif
  }

  void InitializeEmptyExtensionServiceWithTestingPrefs() {
    InitializeExtensionService(ExtensionServiceInitParams());
  }

  ManagementPolicy* GetManagementPolicy() {
    return ExtensionSystem::Get(browser_context())->management_policy();
  }

  ExternalInstallError* GetError(const std::string& extension_id) {
    std::vector<ExternalInstallError*> errors =
        service_->external_install_manager()->GetErrorsForTesting();
    auto found = base::ranges::find(errors, extension_id,
                                    &ExternalInstallError::extension_id);
    return found == errors.end() ? nullptr : *found;
  }

  storage::QuotaErrorOr<storage::BucketLocator> GetStorageBucket(
      const blink::StorageKey& storage_key) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    profile()
        ->GetDefaultStoragePartition()
        ->GetQuotaManager()
        ->proxy()
        ->UpdateOrCreateBucket(
            storage::BucketInitParams::ForDefaultBucket(storage_key),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  typedef ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>
      ManagementPrefUpdater;
};

// Receives notifications from a PackExtensionJob, indicating either that
// packing succeeded or that there was some error.
class PackExtensionTestClient : public PackExtensionJob::Client {
 public:
  PackExtensionTestClient(const base::FilePath& expected_crx_path,
                          const base::FilePath& expected_private_key_path,
                          base::OnceClosure quit_closure);

  PackExtensionTestClient(const PackExtensionTestClient&) = delete;
  PackExtensionTestClient& operator=(const PackExtensionTestClient&) = delete;

  void OnPackSuccess(const base::FilePath& crx_path,
                     const base::FilePath& private_key_path) override;
  void OnPackFailure(const std::string& error_message,
                     ExtensionCreator::ErrorType type) override;

 private:
  const base::FilePath expected_crx_path_;
  const base::FilePath expected_private_key_path_;
  base::OnceClosure quit_closure_;
};

PackExtensionTestClient::PackExtensionTestClient(
    const base::FilePath& expected_crx_path,
    const base::FilePath& expected_private_key_path,
    base::OnceClosure quit_closure)
    : expected_crx_path_(expected_crx_path),
      expected_private_key_path_(expected_private_key_path),
      quit_closure_(std::move(quit_closure)) {}

// If packing succeeded, we make sure that the package names match our
// expectations.
void PackExtensionTestClient::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& private_key_path) {
  // We got the notification and processed it; we don't expect any further tasks
  // to be posted to the current thread, so we should stop blocking and continue
  // on with the rest of the test.
  // This call to |Quit()| matches the call to |Run()| in the
  // |PackPunctuatedExtension| test.
  std::move(quit_closure_).Run();
  EXPECT_EQ(expected_crx_path_.value(), crx_path.value());
  EXPECT_EQ(expected_private_key_path_.value(), private_key_path.value());
  ASSERT_TRUE(base::PathExists(private_key_path));
}

// The tests are designed so that we never expect to see a packing error.
void PackExtensionTestClient::OnPackFailure(const std::string& error_message,
                                            ExtensionCreator::ErrorType type) {
  if (type == ExtensionCreator::kCRXExists)
     FAIL() << "Packing should not fail.";
  else
     FAIL() << "Existing CRX should have been overwritten.";
}

// Test loading good extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAllExtensionsFromDirectorySuccess) {
  InitPluginService();
  InitializeGoodInstalledExtensionService();
  service()->Init();

  uint32_t expected_num_extensions = 3u;
  ASSERT_EQ(expected_num_extensions, loaded_extensions().size());

  EXPECT_EQ(std::string(good0), loaded_extensions()[0]->id());
  EXPECT_EQ(std::string("My extension 1"), loaded_extensions()[0]->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            loaded_extensions()[0]->description());
  EXPECT_EQ(ManifestLocation::kInternal, loaded_extensions()[0]->location());
  EXPECT_TRUE(
      registry()->enabled_extensions().GetByID(loaded_extensions()[0]->id()));
  EXPECT_EQ(expected_num_extensions, registry()->enabled_extensions().size());

  ValidatePrefKeyCount(4);
  ValidateIntegerPref(good0, "state", Extension::ENABLED);
  ValidateIntegerPref(good0, "location",
                      static_cast<int>(ManifestLocation::kInternal));
  ValidateIntegerPref(good1, "state", Extension::ENABLED);
  ValidateIntegerPref(good1, "location",
                      static_cast<int>(ManifestLocation::kInternal));
  ValidateIntegerPref(good2, "state", Extension::ENABLED);
  ValidateIntegerPref(good2, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "file:///*");
  AddPattern(&expected_patterns, "http://*.google.com/*");
  AddPattern(&expected_patterns, "https://*.google.com/*");
  const Extension* extension = loaded_extensions()[0].get();
  const UserScriptList& scripts =
      ContentScriptsInfo::GetContentScripts(extension);
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(expected_patterns, scripts[0]->url_patterns());
  EXPECT_EQ(2u, scripts[0]->js_scripts().size());
  ExtensionResource resource00(extension->id(),
                               scripts[0]->js_scripts()[0]->extension_root(),
                               scripts[0]->js_scripts()[0]->relative_path());
  base::FilePath expected_path =
      base::MakeAbsoluteFilePath(extension->path().AppendASCII("script1.js"));

  EXPECT_EQ(expected_path.NormalizePathSeparators(),
            resource00.GetFilePath().NormalizePathSeparators());
  ExtensionResource resource01(extension->id(),
                               scripts[0]->js_scripts()[1]->extension_root(),
                               scripts[0]->js_scripts()[1]->relative_path());
  expected_path =
      base::MakeAbsoluteFilePath(extension->path().AppendASCII("script2.js"));
  EXPECT_EQ(expected_path.NormalizePathSeparators(),
            resource01.GetFilePath().NormalizePathSeparators());
  EXPECT_EQ(1u, scripts[1]->url_patterns().patterns().size());
  EXPECT_EQ("http://*.news.com/*",
            scripts[1]->url_patterns().begin()->GetAsString());
  ExtensionResource resource10(extension->id(),
                               scripts[1]->js_scripts()[0]->extension_root(),
                               scripts[1]->js_scripts()[0]->relative_path());
  expected_path =
      extension->path().AppendASCII("js_files").AppendASCII("script3.js");
  expected_path = base::MakeAbsoluteFilePath(expected_path);
  EXPECT_EQ(expected_path.NormalizePathSeparators(),
            resource10.GetFilePath().NormalizePathSeparators());

  expected_patterns.ClearPatterns();
  AddPattern(&expected_patterns, "http://*.google.com/*");
  AddPattern(&expected_patterns, "https://*.google.com/*");
  EXPECT_EQ(
      expected_patterns,
      extension->permissions_data()->active_permissions().explicit_hosts());

  EXPECT_EQ(std::string(good1), loaded_extensions()[1]->id());
  EXPECT_EQ(std::string("My extension 2"), loaded_extensions()[1]->name());
  EXPECT_EQ(std::string(), loaded_extensions()[1]->description());
  EXPECT_EQ(loaded_extensions()[1]->GetResourceURL("background.html"),
            BackgroundInfo::GetBackgroundURL(loaded_extensions()[1].get()));
  EXPECT_TRUE(
      ContentScriptsInfo::GetContentScripts(loaded_extensions()[1].get())
          .empty());
  EXPECT_EQ(ManifestLocation::kInternal, loaded_extensions()[1]->location());

  int index = expected_num_extensions - 1;
  EXPECT_EQ(std::string(good2), loaded_extensions()[index]->id());
  EXPECT_EQ(std::string("My extension 3"), loaded_extensions()[index]->name());
  EXPECT_EQ(std::string(), loaded_extensions()[index]->description());
  EXPECT_TRUE(
      ContentScriptsInfo::GetContentScripts(loaded_extensions()[index].get())
          .empty());
  EXPECT_EQ(ManifestLocation::kInternal,
            loaded_extensions()[index]->location());
}

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAllExtensionsFromDirectoryFail) {
  // Initialize the test dir with a bad Preferences/extensions.
  ExtensionServiceInitParams params;
  ASSERT_TRUE(
      params.ConfigureByTestDataDirectory(data_dir().AppendASCII("bad")));
  InitializeExtensionService(std::move(params));

  service()->Init();

  ASSERT_EQ(4u, GetErrors().size());
  ASSERT_EQ(0u, loaded_extensions().size());

  EXPECT_TRUE(base::MatchPattern(
      base::UTF16ToUTF8(GetErrors()[0]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
          manifest_errors::kManifestUnreadable))
      << base::UTF16ToUTF8(GetErrors()[0]);

  EXPECT_TRUE(base::MatchPattern(
      base::UTF16ToUTF8(GetErrors()[1]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
          manifest_errors::kManifestUnreadable))
      << base::UTF16ToUTF8(GetErrors()[1]);

  EXPECT_TRUE(base::MatchPattern(
      base::UTF16ToUTF8(GetErrors()[2]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
          manifest_errors::kMissingFile))
      << base::UTF16ToUTF8(GetErrors()[2]);

  EXPECT_TRUE(base::MatchPattern(
      base::UTF16ToUTF8(GetErrors()[3]),
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + " *. " +
          manifest_errors::kManifestUnreadable))
      << base::UTF16ToUTF8(GetErrors()[3]);
}

// Test various cases for delayed install because of missing imports.
TEST_F(ExtensionServiceTest, PendingImports) {
  InitPluginService();

  ExtensionServiceInitParams params;
  ASSERT_TRUE(params.ConfigureByTestDataDirectory(
      data_dir().AppendASCII("pending_updates_with_imports")));
  InitializeExtensionService(std::move(params));

  // Verify there are no pending extensions initially.
  EXPECT_FALSE(service()->pending_extension_manager()->HasPendingExtensions());

  service()->Init();
  // Wait for GarbageCollectExtensions task to complete.
  task_environment()->RunUntilIdle();

  // These extensions are used by the extensions we test below, they must be
  // installed.
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "bjafgdebaacbbbecmhlhpofkepfkgcpa/1.0")));
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/2")));

  // Each of these extensions should have been rejected because of dependencies
  // that cannot be satisfied.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(
      prefs->GetInstalledExtensionInfo("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  EXPECT_FALSE(
      prefs->GetInstalledExtensionInfo("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("cccccccccccccccccccccccccccccccc"));
  EXPECT_FALSE(
      prefs->GetInstalledExtensionInfo("cccccccccccccccccccccccccccccccc"));

  // Make sure the import started for the extension with a dependency.
  EXPECT_TRUE(
      prefs->GetDelayedInstallInfo("behllobkkfkfnphdnhnkndlbkcpglgmj"));
  EXPECT_EQ(ExtensionPrefs::DelayReason::kWaitForImports,
            prefs->GetDelayedInstallReason("behllobkkfkfnphdnhnkndlbkcpglgmj"));

  EXPECT_FALSE(base::PathExists(extensions_install_dir().AppendASCII(
      "behllobkkfkfnphdnhnkndlbkcpglgmj/1.0.0.0")));

  EXPECT_TRUE(service()->pending_extension_manager()->HasPendingExtensions());
  std::string pending_id("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(pending_id));
  // Remove it because we are not testing the pending extension manager's
  // ability to download and install extensions.
  EXPECT_TRUE(service()->pending_extension_manager()->Remove(pending_id));
}

// Tests that reloading extension with a install delayed due to pending imports
// reloads currently installed extension version, rather than installing the
// delayed install.
TEST_F(ExtensionServiceTest, ReloadExtensionWithPendingImports) {
  InitializeEmptyExtensionService();

  // Wait for GarbageCollectExtensions task to complete.
  task_environment()->RunUntilIdle();

  const base::FilePath base_path =
      data_dir()
          .AppendASCII("pending_updates_with_imports")
          .AppendASCII("updated_with_imports");

  const base::FilePath pem_path = base_path.AppendASCII("update.pem");

  // Initially installed version - the version with no imports.
  const base::FilePath installed_path = base_path.AppendASCII("1.0.0");

  // The updated version - has import that is not satisfied (due to the imported
  // extension not being installed).
  const base::FilePath updated_path = base_path.AppendASCII("2.0.0");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(installed_path));
  ASSERT_TRUE(base::PathExists(updated_path));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Install version 1.
  const Extension* extension = PackAndInstallCRX(
      installed_path, pem_path, INSTALL_NEW, Extension::FROM_WEBSTORE,
      mojom::ManifestLocation::kInternal);
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(extension);
  const std::string id = extension->id();

  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));
  EXPECT_EQ("1.0.0", extension->VersionString());

  // No pending extensions at this point.
  EXPECT_FALSE(service()->pending_extension_manager()->HasPendingExtensions());

  // Update to version 2 that adds an unsatisfied import.
  PackCRXAndUpdateExtension(id, updated_path, pem_path, ENABLED);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);

  // The extension update should be delayed at this point - the old version
  // should still be installed.
  EXPECT_EQ("1.0.0", extension->VersionString());

  // Make sure the import started for the extension with a dependency.
  EXPECT_TRUE(prefs->GetDelayedInstallInfo(id));
  EXPECT_EQ(ExtensionPrefs::DelayReason::kWaitForImports,
            prefs->GetDelayedInstallReason(id));

  const std::string pending_id(32, 'e');
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(pending_id));

  MockExtensionRegistryObserver reload_observer;
  registry()->AddObserver(&reload_observer);

  // Reload the extension, and verify that the installed version does not
  // change.
  service()->ReloadExtension(id);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
  EXPECT_EQ(id, reload_observer.last_extension_loaded);
  EXPECT_EQ(id, reload_observer.last_extension_unloaded);
  registry()->RemoveObserver(&reload_observer);

  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_EQ("1.0.0", extension->VersionString());

  // The update should remain delayed, with the import pending.
  EXPECT_TRUE(prefs->GetDelayedInstallInfo(id));
  EXPECT_EQ(ExtensionPrefs::DelayReason::kWaitForImports,
            prefs->GetDelayedInstallReason(id));

  // Attempt delayed installed - similar to reloading the extension, the update
  // should remain delayed.
  EXPECT_FALSE(service()->FinishDelayedInstallationIfReady(id, true));

  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_EQ("1.0.0", extension->VersionString());
  EXPECT_EQ(ExtensionPrefs::DelayReason::kWaitForImports,
            prefs->GetDelayedInstallReason(id));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(pending_id));

  // Remove the pending install because the pending extension manager's
  // ability to download and install extensions is not important for this test.
  EXPECT_TRUE(service()->pending_extension_manager()->Remove(pending_id));
}

// Tests that installation fails with extensions disabled.
TEST_F(ExtensionServiceTest, InstallExtensionsWithExtensionsDisabled) {
  InitializeExtensionServiceWithExtensionsDisabled();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_FAILED);
}

// Test installing extensions. This test tries to install few extensions using
// crx files. If you need to change those crx files, feel free to repackage
// them, throw away the key used and change the id's above.
TEST_F(ExtensionServiceTest, InstallExtension) {
  InitializeEmptyExtensionService();
  ValidatePrefKeyCount(0);

  // A simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  // TODO(erikkay): verify the contents of the installed extension.

  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // An extension with page actions.
  path = data_dir().AppendASCII("page_action.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(page_action, "state", Extension::ENABLED);
  ValidateIntegerPref(page_action, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // Bad signature.
  path = data_dir().AppendASCII("bad_signature.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);

  // 0-length extension file.
  path = data_dir().AppendASCII("not_an_extension.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);

  // Bad magic number.
  path = data_dir().AppendASCII("bad_magic.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);

  // Packed extensions may have folders or files that have underscores.
  // This will only cause a warning, rather than a fatal error.
  path = data_dir().AppendASCII("bad_underscore.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);

  // A test for an extension with a 2048-bit public key.
  path = data_dir().AppendASCII("good2048.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(good2048, "state", Extension::ENABLED);
  ValidateIntegerPref(good2048, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

TEST_F(ExtensionServiceTest, InstallExtension_EmitUserHistograms) {
  InitializeEmptyExtensionService();

  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(
      /*is_guest=*/false));
  RunEmitUserHistogramsTest(
      /*nonuser_expected_total_count=*/0,
      /*user_expected_total_count=*/1);
}

TEST_F(ExtensionServiceTest, InstallExtension_NonUserEmitHistograms) {
  InitializeEmptyExtensionService();

  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(
      /*is_guest=*/true));
  RunEmitUserHistogramsTest(
      /*nonuser_expected_total_count=*/1,
      /*user_expected_total_count=*/0);
}

// Test that correct notifications are sent to ExtensionRegistryObserver on
// extension install and uninstall.
TEST_F(ExtensionServiceTest, InstallObserverNotified) {
  InitializeEmptyExtensionService();

  ExtensionRegistry* registry(ExtensionRegistry::Get(profile()));
  MockExtensionRegistryObserver observer;
  registry->AddObserver(&observer);

  // A simple extension that should install without error.
  ASSERT_TRUE(observer.last_extension_installed.empty());
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, observer.last_extension_installed);

  // Uninstall the extension.
  ASSERT_TRUE(observer.last_extension_uninstalled.empty());
  UninstallExtension(good_crx);
  ASSERT_EQ(good_crx, observer.last_extension_uninstalled);

  registry->RemoveObserver(&observer);
}

// Test the handling of uninstalling external extensions.
TEST_F(ExtensionServiceTest, UninstallingExternalExtensions) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  std::string version_str = "1.0.0.0";
  // Install an external extension.
  std::unique_ptr<ExternalInstallInfoFile> info = CreateExternalExtension(
      good_crx, version_str, path, ManifestLocation::kExternalPref,
      Extension::NO_FLAGS);
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(std::move(info));

  WaitForExternalExtensionInstalled(good_crx);

  ASSERT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  // Uninstall it and check that its killbit gets set.
  UninstallExtension(good_crx);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));

  // Try to re-install it externally. This should fail because of the killbit.
  info = CreateExternalExtension(good_crx, version_str, path,
                                 ManifestLocation::kExternalPref,
                                 Extension::NO_FLAGS);
  provider->UpdateOrAddExtension(std::move(info));
  task_environment()->RunUntilIdle();
  ASSERT_FALSE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));

  std::string newer_version = "1.0.0.1";
  // Repeat the same thing with a newer version of the extension.
  path = data_dir().AppendASCII("good2.crx");
  info = CreateExternalExtension(good_crx, newer_version, path,
                                 ManifestLocation::kExternalPref,
                                 Extension::NO_FLAGS);
  provider->UpdateOrAddExtension(std::move(info));
  task_environment()->RunUntilIdle();
  ASSERT_FALSE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));

  // Try adding the same extension from an external update URL.
  ASSERT_FALSE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      good_crx, std::string(), GURL("http:://fake.update/url"),
      ManifestLocation::kExternalPrefDownload, Extension::NO_FLAGS, false));

  // Installation of the same extension through the policy should be successful.
  ASSERT_TRUE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      good_crx, std::string(), GURL("http:://fake.update/url"),
      ManifestLocation::kExternalPolicyDownload, Extension::NO_FLAGS, false));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(good_crx));
  EXPECT_TRUE(service()->pending_extension_manager()->Remove(good_crx));

  ASSERT_FALSE(service()->pending_extension_manager()->IsIdPending(good_crx));
}

// Tests that uninstalling an external extension, and then reinstalling the
// extension as a user install (e.g. from the webstore) succeeds.
TEST_F(ExtensionServiceTest, UninstallExternalExtensionAndReinstallAsUser) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  std::string version_str = "1.0.0.0";
  // Install an external extension.
  std::unique_ptr<ExternalInstallInfoFile> info = CreateExternalExtension(
      good_crx, version_str, path, ManifestLocation::kExternalPref,
      Extension::NO_FLAGS);
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(std::move(info));
  WaitForExternalExtensionInstalled(good_crx);

  ASSERT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  // Uninstall the extension.
  UninstallExtension(good_crx);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));

  // Reinstall the extension as a user-space extension. This should succeed.
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  base::RunLoop run_loop;
  installer->AddInstallerCallback(base::BindOnce(
      [](base::OnceClosure quit_closure,
         const std::optional<CrxInstallError>& result) {
        ASSERT_FALSE(result) << result->message();
        std::move(quit_closure).Run();
      },
      run_loop.QuitWhenIdleClosure()));
  installer->InstallCrx(path);
  run_loop.Run();

  ASSERT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
}

// Tests uninstalling an external extension from a higher version, and then
// installing a lower version as a user. This should succeed.
// Regression test for https://crbug.com/795026.
TEST_F(ExtensionServiceTest,
       UninstallExternalExtensionAndReinstallAsUserWithLowerVersion) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good2.crx");

  constexpr char kExternalVersion[] = "1.0.0.1";
  // Install an external extension.
  std::unique_ptr<ExternalInstallInfoFile> info = CreateExternalExtension(
      good_crx, kExternalVersion, path, ManifestLocation::kExternalPref,
      Extension::NO_FLAGS);
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(std::move(info));
  WaitForExternalExtensionInstalled(good_crx);

  ASSERT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  // Uninstall the extension.
  UninstallExtension(good_crx);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));

  // Reinstall the extension as a user-space extension with a lower version.
  // This should succeed.
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  base::RunLoop run_loop;
  installer->AddInstallerCallback(base::BindOnce(
      [](base::OnceClosure quit_closure,
         const std::optional<CrxInstallError>& result) {
        ASSERT_FALSE(result) << result->message();
        std::move(quit_closure).Run();
      },
      run_loop.QuitWhenIdleClosure()));
  installer->InstallCrx(data_dir().AppendASCII("good.crx"));
  run_loop.Run();

  const Extension* extension =
      registry()->enabled_extensions().GetByID(good_crx);
  ASSERT_TRUE(extension);
  constexpr char kLowerVersion[] = "1.0.0.0";
  EXPECT_EQ(kLowerVersion, extension->version().GetString());
}

// Test that uninstalling an external extension does not crash when
// the extension could not be loaded.
// This extension shown in preferences file requires an experimental permission.
// It could not be loaded without such permission.
TEST_F(ExtensionServiceTest, UninstallingNotLoadedExtension) {
  base::FilePath test_data_dir = data_dir().AppendASCII("good");
  ExtensionServiceInitParams params;
  // The preference contains an external extension
  // that requires 'experimental' permission.
  ASSERT_TRUE(params.SetPrefsContentFromFile(
      test_data_dir.AppendASCII("PreferencesExperimental")));
  params.extensions_dir = test_data_dir.AppendASCII("Extensions");
  // Aforementioned extension will not be loaded if
  // there is no '--enable-experimental-extension-apis' command line flag.
  InitializeExtensionService(std::move(params));

  service()->Init();

  // Check and try to uninstall it.
  // If we don't check whether the extension is loaded before we uninstall it
  // in CheckExternalUninstall, a crash will happen here because we will get or
  // dereference a NULL pointer (extension) inside UninstallExtension.
  MockExternalProvider provider(nullptr, ManifestLocation::kExternalRegistry);
  service()->OnExternalProviderReady(&provider);
}

// Test that external extensions with incorrect IDs are not installed.
// TODO(b/300670172): This test is extremely flaky.
TEST_F(ExtensionServiceTest, DISABLED_FailOnWrongId) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");

  std::string version_str = "1.0.0.0";

  const std::string wrong_id = all_zero;
  const std::string correct_id = good_crx;
  ASSERT_NE(correct_id, wrong_id);

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  // Install an external extension with an ID from the external
  // source that is not equal to the ID in the extension manifest.
  std::unique_ptr<ExternalInstallInfoFile> info = CreateExternalExtension(
      wrong_id, version_str, path, ManifestLocation::kExternalPref,
      Extension::NO_FLAGS);
  provider->UpdateOrAddExtension(std::move(info));
  WaitForInstallationAttemptToComplete(wrong_id);
  ASSERT_FALSE(registry()->enabled_extensions().GetByID(good_crx));

  // Try again with the right ID. Expect success.
  info = CreateExternalExtension(correct_id, version_str, path,
                                 ManifestLocation::kExternalPref,
                                 Extension::NO_FLAGS);
  provider->UpdateOrAddExtension(std::move(info));
  WaitForExternalExtensionInstalled(good_crx);
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
}

// Test that external extensions with incorrect versions are not installed.
TEST_F(ExtensionServiceTest, FailOnWrongVersion) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  // Install an external extension with a version from the external
  // source that is not equal to the version in the extension manifest.
  std::string wrong_version_str = "1.2.3.4";
  std::unique_ptr<ExternalInstallInfoFile> wrong_info = CreateExternalExtension(
      good_crx, wrong_version_str, path, ManifestLocation::kExternalPref,
      Extension::NO_FLAGS);
  provider->UpdateOrAddExtension(std::move(wrong_info));
  WaitForInstallationAttemptToComplete(good_crx);
  ASSERT_FALSE(registry()->enabled_extensions().GetByID(good_crx));

  // Try again with the right version. Expect success.
  service()->pending_extension_manager()->Remove(good_crx);
  std::unique_ptr<ExternalInstallInfoFile> correct_info =
      CreateExternalExtension(good_crx, "1.0.0.0", path,
                              ManifestLocation::kExternalPref,
                              Extension::NO_FLAGS);
  provider->UpdateOrAddExtension(std::move(correct_info));
  WaitForExternalExtensionInstalled(good_crx);
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
}

// Install a user script (they get converted automatically to an extension)
TEST_F(ExtensionServiceTest, InstallUserScript) {
  // The details of script conversion are tested elsewhere, this just tests
  // integration with ExtensionService.
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("user_script_basic.user.js");

  ASSERT_TRUE(base::PathExists(path));
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  installer->InstallUserScript(
      path,
      GURL("http://www.aaronboodman.com/scripts/user_script_basic.user.js"));

  task_environment()->RunUntilIdle();
  std::vector<std::u16string> errors = GetErrors();
  EXPECT_TRUE(installed_extension()) << "Nothing was installed.";
  EXPECT_FALSE(was_update()) << path.value();
  ASSERT_EQ(1u, loaded_extensions().size()) << "Nothing was loaded.";
  EXPECT_EQ(0u, errors.size())
      << "There were errors: " << base::JoinString(errors, u",");
  EXPECT_TRUE(
      registry()->enabled_extensions().GetByID(loaded_extensions()[0]->id()))
      << path.value();
}

// Extensions don't install during shutdown.
TEST_F(ExtensionServiceTest, InstallExtensionDuringShutdown) {
  InitializeEmptyExtensionService();

  // Simulate shutdown.
  service()->set_browser_terminating_for_test(true);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service()));
  installer->set_allow_silent_install(true);
  installer->InstallCrx(path);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(installed_extension()) << "Extension installed during shutdown.";
  ASSERT_EQ(0u, loaded_extensions().size())
      << "Extension loaded during shutdown.";
}

// This tests that the granted permissions preferences are correctly set when
// installing an extension.
TEST_F(ExtensionServiceTest, GrantedPermissions) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("permissions");

  base::FilePath pem_path = path.AppendASCII("unknown.pem");
  path = path.AppendASCII("unknown");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  APIPermissionSet expected_api_perms;
  URLPatternSet expected_host_perms;

  // Make sure there aren't any granted permissions before the
  // extension is installed.
  EXPECT_FALSE(prefs->GetGrantedPermissions(permissions_crx).get());

  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(permissions_crx, extension->id());

  // Verify that the valid API permissions have been recognized.
  expected_api_perms.insert(APIPermissionID::kTab);

  AddPattern(&expected_host_perms, "http://*.google.com/*");
  AddPattern(&expected_host_perms, "https://*.google.com/*");
  AddPattern(&expected_host_perms, "http://*.google.com.hk/*");
  AddPattern(&expected_host_perms, "http://www.example.com/*");

  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension->id());
  ASSERT_TRUE(known_perms.get());
  EXPECT_FALSE(known_perms->IsEmpty());
  EXPECT_EQ(expected_api_perms, known_perms->apis());
  EXPECT_EQ(expected_host_perms, known_perms->effective_hosts());
}

// This tests that the granted permissions stored in prefs ignore internal
// permissions specified in the extension manifest.
TEST_F(ExtensionServiceTest,
       GrantedPermissionsIgnoreInternalPermissionsFromManifest) {
  InitializeEmptyExtensionService();

  // Load an extension that tries to include an internal permission in its
  // manifest. The internal permission should be ignored on the resulting
  // extension object and should not be included in persisted permissions in
  // preferences.
  constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "1.2.3",
           "permissions": ["searchProvider", "storage"]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  const Extension* extension = InstallCRX(test_dir.Pack(), INSTALL_NEW);
  ASSERT_TRUE(extension);

  EXPECT_FALSE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kSearchProvider));
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kStorage));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  std::unique_ptr<const PermissionSet> granted_perms =
      prefs->GetGrantedPermissions(extension->id());
  ASSERT_TRUE(granted_perms);
  EXPECT_EQ(1u, granted_perms->apis().size());
  EXPECT_TRUE(
      granted_perms->HasAPIPermission(mojom::APIPermissionID::kStorage));
  EXPECT_FALSE(
      granted_perms->HasAPIPermission(mojom::APIPermissionID::kSearchProvider));
}

// This tests that the granted permissions preferences are correctly set when
// updating an extension, and the extension is disabled in case of a permission
// escalation.
TEST_F(ExtensionServiceTest, GrantedPermissionsOnUpdate) {
  InitializeEmptyExtensionService();
  const base::FilePath base_path = data_dir().AppendASCII("permissions");

  const base::FilePath pem_path = base_path.AppendASCII("update.pem");
  const base::FilePath path1 = base_path.AppendASCII("update_1");
  const base::FilePath path2 = base_path.AppendASCII("update_2");
  const base::FilePath path3 = base_path.AppendASCII("update_3");
  const base::FilePath path4 = base_path.AppendASCII("update_4");
  const base::FilePath path5 = base_path.AppendASCII("update_5");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path1));
  ASSERT_TRUE(base::PathExists(path2));
  ASSERT_TRUE(base::PathExists(path3));
  ASSERT_TRUE(base::PathExists(path4));
  ASSERT_TRUE(base::PathExists(path5));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Install version 1, which has the kHistory permission.
  const Extension* extension = PackAndInstallCRX(path1, pem_path, INSTALL_NEW);
  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  // Verify that the history permission has been recognized.
  APIPermissionSet expected_api_perms;
  expected_api_perms.insert(APIPermissionID::kHistory);
  {
    std::unique_ptr<const PermissionSet> known_perms =
        prefs->GetGrantedPermissions(id);
    ASSERT_TRUE(known_perms.get());
    EXPECT_EQ(expected_api_perms, known_perms->apis());
  }

  // Update to version 2 that adds the kTopSites permission, which has a
  // separate message, but is implied by kHistory. The extension should remain
  // enabled.
  PackCRXAndUpdateExtension(id, path2, pem_path, ENABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));

  // The extra permission should have been granted automatically.
  expected_api_perms.insert(APIPermissionID::kTopSites);
  {
    std::unique_ptr<const PermissionSet> known_perms =
        prefs->GetGrantedPermissions(id);
    ASSERT_TRUE(known_perms.get());
    EXPECT_EQ(expected_api_perms, known_perms->apis());
  }

  // Update to version 3 that adds the kStorage permission, which does not have
  // a message. The extension should remain enabled.
  PackCRXAndUpdateExtension(id, path3, pem_path, ENABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));

  // The extra permission should have been granted automatically.
  expected_api_perms.insert(APIPermissionID::kStorage);
  {
    std::unique_ptr<const PermissionSet> known_perms =
        prefs->GetGrantedPermissions(id);
    ASSERT_TRUE(known_perms.get());
    EXPECT_EQ(expected_api_perms, known_perms->apis());
  }

  // Update to version 4 that adds the kNotifications permission, which has a
  // message and hence is considered a permission increase. Now the extension
  // should get disabled.
  PackCRXAndUpdateExtension(id, path4, pem_path, DISABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));

  // No new permissions should have been granted.
  {
    std::unique_ptr<const PermissionSet> known_perms =
        prefs->GetGrantedPermissions(id);
    ASSERT_TRUE(known_perms.get());
    EXPECT_EQ(expected_api_perms, known_perms->apis());
  }
}

TEST_F(ExtensionServiceTest, ReenableWithAllPermissionsGranted) {
  InitializeEmptyExtensionService();
  const base::FilePath base_path = data_dir().AppendASCII("permissions");

  const base::FilePath pem_path = base_path.AppendASCII("update.pem");
  const base::FilePath path1 = base_path.AppendASCII("update_1");
  const base::FilePath path4 = base_path.AppendASCII("update_4");
  const base::FilePath path5 = base_path.AppendASCII("update_5");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path1));
  ASSERT_TRUE(base::PathExists(path4));
  ASSERT_TRUE(base::PathExists(path5));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Install version 1, which has the kHistory permission.
  const Extension* extension = PackAndInstallCRX(path1, pem_path, INSTALL_NEW);
  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  // Update to version 4 that adds the kNotifications permission, which has a
  // message and hence is considered a permission increase. The extension
  // should get disabled due to a permissions increase.
  PackCRXAndUpdateExtension(id, path4, pem_path, DISABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, disable_reason::DISABLE_PERMISSIONS_INCREASE));

  // Update to version 5 that removes the kNotifications permission again.
  // The extension should get re-enabled.
  PackCRXAndUpdateExtension(id, path5, pem_path, ENABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
}

TEST_F(ExtensionServiceTest, ReenableWithAllPermissionsGrantedOnStartup) {
  InitializeEmptyExtensionService();
  const base::FilePath base_path = data_dir().AppendASCII("permissions");

  const base::FilePath pem_path = base_path.AppendASCII("update.pem");
  const base::FilePath path1 = base_path.AppendASCII("update_1");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path1));

  // Install an extension which has the kHistory permission.
  const Extension* extension = PackAndInstallCRX(path1, pem_path, INSTALL_NEW);
  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Disable the extension due to a supposed permission increase, but retain its
  // granted permissions.
  service()->DisableExtension(id, disable_reason::DISABLE_PERMISSIONS_INCREASE);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, disable_reason::DISABLE_PERMISSIONS_INCREASE));

  // Simulate a Chrome restart. Since the extension has all required
  // permissions, it should get re-enabled.
  service()->ReloadExtensionsForTest();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(prefs->HasDisableReason(
      id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
}

TEST_F(ExtensionServiceTest,
       DontReenableWithAllPermissionsGrantedButOtherReason) {
  InitializeEmptyExtensionService();
  const base::FilePath base_path = data_dir().AppendASCII("permissions");

  const base::FilePath pem_path = base_path.AppendASCII("update.pem");
  const base::FilePath path1 = base_path.AppendASCII("update_1");
  const base::FilePath path4 = base_path.AppendASCII("update_4");
  const base::FilePath path5 = base_path.AppendASCII("update_5");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path1));
  ASSERT_TRUE(base::PathExists(path4));
  ASSERT_TRUE(base::PathExists(path5));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Install version 1, which has the kHistory permission.
  const Extension* extension = PackAndInstallCRX(path1, pem_path, INSTALL_NEW);
  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  // Disable the extension.
  service()->DisableExtension(id, disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(id, disable_reason::DISABLE_USER_ACTION));

  // Update to version 4 that adds the kNotifications permission, which has a
  // message and hence is considered a permission increase. The extension
  // should get disabled due to a permissions increase.
  PackCRXAndUpdateExtension(id, path4, pem_path, DISABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
  // The USER_ACTION reason should also still be there.
  EXPECT_TRUE(prefs->HasDisableReason(id, disable_reason::DISABLE_USER_ACTION));

  // Update to version 5 that removes the kNotifications permission again.
  // The PERMISSIONS_INCREASE should be removed, but the extension should stay
  // disabled since USER_ACTION is still there.
  PackCRXAndUpdateExtension(id, path5, pem_path, DISABLED);
  extension = registry()->GetInstalledExtension(id);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION, prefs->GetDisableReasons(id));
}

TEST_F(ExtensionServiceTest,
       DontReenableWithAllPermissionsGrantedOnStartupButOtherReason) {
  InitializeEmptyExtensionService();
  const base::FilePath base_path = data_dir().AppendASCII("permissions");

  const base::FilePath pem_path = base_path.AppendASCII("update.pem");
  const base::FilePath path1 = base_path.AppendASCII("update_1");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path1));

  // Install an extension which has the kHistory permission.
  const Extension* extension = PackAndInstallCRX(path1, pem_path, INSTALL_NEW);
  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Disable the extension due to a supposed permission increase, but retain its
  // granted permissions.
  service()->DisableExtension(id, disable_reason::DISABLE_PERMISSIONS_INCREASE |
                                      disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, disable_reason::DISABLE_PERMISSIONS_INCREASE));

  // Simulate a Chrome restart. Since the extension has all required
  // permissions, the DISABLE_PERMISSIONS_INCREASE should get removed, but it
  // should stay disabled due to the remaining DISABLE_USER_ACTION reason.
  service()->ReloadExtensionsForTest();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION, prefs->GetDisableReasons(id));
}

// Tests that installing an extension with a permission adds it to the granted
// permissions, so that if it is later removed and then re-added the extension
// is not disabled.
TEST_F(ExtensionServiceTest,
       ReaddingOldPermissionInUpdateDoesntDisableExtension) {
  InitializeEmptyExtensionService();

  // Borrow a PEM for consistent IDs.
  const base::FilePath pem_path =
      data_dir().AppendASCII("permissions/update.pem");
  ASSERT_TRUE(base::PathExists(pem_path));

  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test",
           "description": "Test permissions update flow",
           "manifest_version": 2,
           "version": "%s",
           "permissions": [%s]
         })";

  // Install version 1, which includes the tabs permission.
  TestExtensionDir version1;
  version1.WriteManifest(
      base::StringPrintf(kManifestTemplate, "1", R"("tabs")"));

  const Extension* extension =
      PackAndInstallCRX(version1.UnpackedPath(), pem_path, INSTALL_NEW);
  ASSERT_TRUE(extension);

  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  auto get_granted_permissions = [prefs, id]() {
    return prefs->GetGrantedPermissions(id);
  };

  auto get_active_permissions = [prefs, id]() {
    return prefs->GetDesiredActivePermissions(id);
  };

  APIPermissionSet tabs_permission_set;
  tabs_permission_set.insert(APIPermissionID::kTab);

  EXPECT_EQ(tabs_permission_set, get_granted_permissions()->apis());
  EXPECT_EQ(tabs_permission_set, get_active_permissions()->apis());

  // Version 2 removes the tabs permission. The tabs permission should be
  // gone from the active permissions, but retained in the granted permissions.
  TestExtensionDir version2;
  version2.WriteManifest(base::StringPrintf(kManifestTemplate, "2", ""));

  PackCRXAndUpdateExtension(id, version2.UnpackedPath(), pem_path, ENABLED);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));

  EXPECT_EQ(tabs_permission_set, get_granted_permissions()->apis());
  EXPECT_TRUE(get_active_permissions()->IsEmpty());

  // Version 3 re-adds the tabs permission. Even though this is an increase in
  // privilege from version 2, it's not from the granted permissions (which
  // include the permission from version 1). Therefore, the extension should
  // remain enabled.
  TestExtensionDir version3;
  version3.WriteManifest(
      base::StringPrintf(kManifestTemplate, "3", R"("tabs")"));

  PackCRXAndUpdateExtension(id, version3.UnpackedPath(), pem_path, ENABLED);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));

  EXPECT_EQ(tabs_permission_set, get_granted_permissions()->apis());
  EXPECT_EQ(tabs_permission_set, get_active_permissions()->apis());
}

// Tests that updating incognito to not_allowed revokes extension's permission
// to run in incognito.
TEST_F(ExtensionServiceTest, UpdateIncognitoMode) {
  InitializeEmptyExtensionService();

  // Borrow a PEM for consistent IDs.
  const base::FilePath path = data_dir().AppendASCII("permissions/update.pem");
  ASSERT_TRUE(base::PathExists(path));

  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test",
           "description": "Test incognito mode update flow",
           "manifest_version": 2,
           "version": "%s",
           "incognito": "%s"
         })";

  // Install version 1, which has incognito set to split.
  TestExtensionDir version1;
  version1.WriteManifest(base::StringPrintf(kManifestTemplate, "1", "split"));

  const Extension* extension =
      PackAndInstallCRX(version1.UnpackedPath(), path, INSTALL_NEW);
  ASSERT_TRUE(extension);

  const std::string id = extension->id();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));
  util::SetIsIncognitoEnabled(id, profile(), true);

  EXPECT_TRUE(util::IsIncognitoEnabled(id, profile()));

  // Version 2 updates the incognito mode to not_allowed. This should revoke its
  // permissions, i.e., the extension should not be allowed to run in incognito.
  TestExtensionDir version2;
  version2.WriteManifest(
      base::StringPrintf(kManifestTemplate, "2", "not_allowed"));

  PackCRXAndUpdateExtension(id, version2.UnpackedPath(), path, ENABLED);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(util::IsIncognitoEnabled(id, profile()));

  // Version 3 updates the incognito mode to split. The extension should not
  // have the permissions.
  TestExtensionDir version3;
  version3.WriteManifest(base::StringPrintf(kManifestTemplate, "3", "split"));

  service()->EnableExtension(id);
  PackCRXAndUpdateExtension(id, version3.UnpackedPath(), path, ENABLED);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));

  EXPECT_FALSE(util::IsIncognitoEnabled(id, profile()));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// This tests that the granted permissions preferences are correctly set for
// pre-installed apps.
TEST_F(ExtensionServiceTest, PreinstalledAppsGrantedPermissions) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("permissions");

  base::FilePath pem_path = path.AppendASCII("unknown.pem");
  path = path.AppendASCII("unknown");

  ASSERT_TRUE(base::PathExists(pem_path));
  ASSERT_TRUE(base::PathExists(path));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  APIPermissionSet expected_api_perms;
  URLPatternSet expected_host_perms;

  // Make sure there aren't any granted permissions before the
  // extension is installed.
  EXPECT_FALSE(prefs->GetGrantedPermissions(permissions_crx).get());

  const Extension* extension = PackAndInstallCRX(
      path, pem_path, INSTALL_NEW, Extension::WAS_INSTALLED_BY_DEFAULT,
      mojom::ManifestLocation::kInternal);

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(permissions_crx, extension->id());

  // Verify that the valid API permissions have been recognized.
  expected_api_perms.insert(APIPermissionID::kTab);

  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension->id());
  EXPECT_TRUE(known_perms.get());
  EXPECT_FALSE(known_perms->IsEmpty());
  EXPECT_EQ(expected_api_perms, known_perms->apis());
}
#endif

// Tests that the extension is disabled when permissions are missing from
// the extension's granted permissions preferences. (This simulates updating
// the browser to a version which recognizes more permissions).
TEST_F(ExtensionServiceTest, GrantedAPIAndHostPermissions) {
  InitializeEmptyExtensionService();

  base::FilePath path =
      data_dir().AppendASCII("permissions").AppendASCII("unknown");

  ASSERT_TRUE(base::PathExists(path));

  const Extension* extension = PackAndInstallCRX(path, INSTALL_NEW);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  std::string extension_id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  APIPermissionSet expected_api_permissions;
  URLPatternSet expected_host_permissions;

  expected_api_permissions.insert(APIPermissionID::kTab);
  AddPattern(&expected_host_permissions, "http://*.google.com/*");
  AddPattern(&expected_host_permissions, "https://*.google.com/*");
  AddPattern(&expected_host_permissions, "http://*.google.com.hk/*");
  AddPattern(&expected_host_permissions, "http://www.example.com/*");

  std::set<std::string> host_permissions;

  // Test that the extension is disabled when an API permission is missing from
  // the extension's granted api permissions preference. (This simulates
  // updating the browser to a version which recognizes a new API permission).
  base::Value::List empty_list;
  SetPrefList(extension_id, "granted_permissions.api", empty_list,
              "granted_permissions.api");
  service()->ReloadExtensionsForTest();

  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  extension = registry()->disabled_extensions().begin()->get();

  ASSERT_TRUE(prefs->IsExtensionDisabled(extension_id));
  ASSERT_FALSE(service()->IsExtensionEnabled(extension_id));
  ASSERT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Now grant and re-enable the extension, making sure the prefs are updated.
  service()->GrantPermissionsAndEnableExtension(extension);

  ASSERT_FALSE(prefs->IsExtensionDisabled(extension_id));
  ASSERT_TRUE(service()->IsExtensionEnabled(extension_id));
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  std::unique_ptr<const PermissionSet> current_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(current_perms.get());
  ASSERT_FALSE(current_perms->IsEmpty());
  ASSERT_EQ(expected_api_permissions, current_perms->apis());
  ASSERT_EQ(expected_host_permissions, current_perms->effective_hosts());

  // Tests that the extension is disabled when a host permission is missing from
  // the extension's granted host permissions preference. (This simulates
  // updating the browser to a version which recognizes additional host
  // permissions).
  host_permissions.clear();
  current_perms = nullptr;

  host_permissions.insert("http://*.google.com/*");
  host_permissions.insert("https://*.google.com/*");
  host_permissions.insert("http://*.google.com.hk/*");

  auto api_permissions = base::Value::List().Append("tabs");
  SetPrefList(extension_id, "granted_permissions.api", api_permissions,
              "granted_permissions.api");
  SetPrefStringSet(
      extension_id, "granted_permissions.scriptable_host", host_permissions);

  service()->ReloadExtensionsForTest();

  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  extension = registry()->disabled_extensions().begin()->get();

  ASSERT_TRUE(prefs->IsExtensionDisabled(extension_id));
  ASSERT_FALSE(service()->IsExtensionEnabled(extension_id));
  ASSERT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Now grant and re-enable the extension, making sure the prefs are updated.
  service()->GrantPermissionsAndEnableExtension(extension);

  ASSERT_TRUE(service()->IsExtensionEnabled(extension_id));
  ASSERT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));

  current_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(current_perms.get());
  ASSERT_FALSE(current_perms->IsEmpty());
  ASSERT_EQ(expected_api_permissions, current_perms->apis());
  ASSERT_EQ(expected_host_permissions, current_perms->effective_hosts());
}

// Test Packaging and installing an extension.
TEST_F(ExtensionServiceTest, PackExtension) {
  InitializeEmptyExtensionService();
  base::FilePath input_directory =
      data_dir()
          .AppendASCII("good")
          .AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath output_directory = temp_dir.GetPath();

  base::FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  base::FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags));
  ASSERT_TRUE(base::PathExists(crx_path));
  ASSERT_TRUE(base::PathExists(privkey_path));

  // Repeat the run with the pem file gone, and no special flags
  // Should refuse to overwrite the existing crx.
  base::DeleteFile(privkey_path);
  ASSERT_FALSE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags));

  // OK, now try it with a flag to overwrite existing crx.  Should work.
  ASSERT_TRUE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kOverwriteCRX));

  // Repeat the run allowing existing crx, but the existing pem is still
  // an error.  Should fail.
  ASSERT_FALSE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kOverwriteCRX));

  ASSERT_TRUE(base::PathExists(privkey_path));
  InstallCRX(crx_path, INSTALL_NEW);

  // Try packing with invalid paths.
  creator = std::make_unique<ExtensionCreator>();
  ASSERT_FALSE(
      creator->Run(base::FilePath(), base::FilePath(), base::FilePath(),
                   base::FilePath(), ExtensionCreator::kOverwriteCRX));

  // Try packing an empty directory. Should fail because an empty directory is
  // not a valid extension.
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  creator = std::make_unique<ExtensionCreator>();
  ASSERT_FALSE(creator->Run(temp_dir2.GetPath(), crx_path, privkey_path,
                            base::FilePath(), ExtensionCreator::kOverwriteCRX));

  // Try packing with an invalid manifest.
  std::string invalid_manifest_content = "I am not a manifest.";
  ASSERT_TRUE(base::WriteFile(temp_dir2.GetPath().Append(kManifestFilename),
                              invalid_manifest_content));
  creator = std::make_unique<ExtensionCreator>();
  ASSERT_FALSE(creator->Run(temp_dir2.GetPath(), crx_path, privkey_path,
                            base::FilePath(), ExtensionCreator::kOverwriteCRX));

  // Try packing with a private key that is a valid key, but invalid for the
  // extension.
  base::FilePath bad_private_key_dir =
      data_dir().AppendASCII("bad_private_key");
  crx_path = output_directory.AppendASCII("bad_private_key.crx");
  privkey_path = data_dir().AppendASCII("bad_private_key.pem");
  ASSERT_FALSE(creator->Run(bad_private_key_dir, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kOverwriteCRX));
}

// Test Packaging and installing an extension whose name contains punctuation.
TEST_F(ExtensionServiceTest, PackPunctuatedExtension) {
  InitializeEmptyExtensionService();
  base::FilePath input_directory = data_dir()
                                       .AppendASCII("good")
                                       .AppendASCII("Extensions")
                                       .AppendASCII(good0)
                                       .AppendASCII("1.0.0.0");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Extension names containing punctuation, and the expected names for the
  // packed extensions.
  const base::FilePath punctuated_names[] = {
    base::FilePath(FILE_PATH_LITERAL("this.extensions.name.has.periods")),
    base::FilePath(FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod")),
    base::FilePath(FILE_PATH_LITERAL("thisextensionhasaslashinitsname/")).
        NormalizePathSeparators(),
  };
  const base::FilePath expected_crx_names[] = {
    base::FilePath(FILE_PATH_LITERAL("this.extensions.name.has.periods.crx")),
    base::FilePath(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod.crx")),
    base::FilePath(FILE_PATH_LITERAL("thisextensionhasaslashinitsname.crx")),
  };
  const base::FilePath expected_private_key_names[] = {
    base::FilePath(FILE_PATH_LITERAL("this.extensions.name.has.periods.pem")),
    base::FilePath(
        FILE_PATH_LITERAL(".thisextensionsnamestartswithaperiod.pem")),
    base::FilePath(FILE_PATH_LITERAL("thisextensionhasaslashinitsname.pem")),
  };

  for (size_t i = 0; i < std::size(punctuated_names); ++i) {
    SCOPED_TRACE(punctuated_names[i].value().c_str());
    base::FilePath output_dir = temp_dir.GetPath().Append(punctuated_names[i]);
    base::RunLoop loop;
    // Copy the extension into the output directory, as PackExtensionJob doesn't
    // let us choose where to output the packed extension.
    ASSERT_TRUE(base::CopyDirectory(input_directory, output_dir, true));

    base::FilePath expected_crx_path =
        temp_dir.GetPath().Append(expected_crx_names[i]);
    base::FilePath expected_private_key_path =
        temp_dir.GetPath().Append(expected_private_key_names[i]);
    PackExtensionTestClient pack_client(expected_crx_path,
                                        expected_private_key_path,
                                        loop.QuitWhenIdleClosure());
    {
      PackExtensionJob packer(&pack_client, output_dir, base::FilePath(),
                              ExtensionCreator::kOverwriteCRX);
      packer.Start();

      // The packer will post a notification task to the current thread's
      // message loop when it is finished.  We manually run the loop here so
      // that we block and catch the notification; otherwise, the process would
      // exit.
      // This call to |Run()| is matched by a call to |Quit()| in the
      // |PackExtensionTestClient|'s notification handling code.
      loop.Run();
    }

    if (HasFatalFailure())
      return;

    InstallCRX(expected_crx_path, INSTALL_NEW);
  }
}

TEST_F(ExtensionServiceTest, PackExtensionContainingKeyFails) {
  InitializeEmptyExtensionService();

  base::ScopedTempDir extension_temp_dir;
  ASSERT_TRUE(extension_temp_dir.CreateUniqueTempDir());
  base::FilePath input_directory =
      extension_temp_dir.GetPath().AppendASCII("ext");
  ASSERT_TRUE(
      base::CopyDirectory(data_dir()
                              .AppendASCII("good")
                              .AppendASCII("Extensions")
                              .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                              .AppendASCII("1.0.0.0"),
                          input_directory,
                          /*recursive=*/true));

  base::ScopedTempDir output_temp_dir;
  ASSERT_TRUE(output_temp_dir.CreateUniqueTempDir());
  base::FilePath output_directory = output_temp_dir.GetPath();

  base::FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  base::FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  // Pack the extension once to get a private key.
  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags))
      << creator->error_message();
  ASSERT_TRUE(base::PathExists(crx_path));
  ASSERT_TRUE(base::PathExists(privkey_path));

  base::DeleteFile(crx_path);
  // Move the pem file into the extension.
  base::Move(privkey_path,
                  input_directory.AppendASCII("privkey.pem"));

  // This pack should fail because of the contained private key.
  EXPECT_FALSE(creator->Run(input_directory, crx_path, base::FilePath(),
      privkey_path, ExtensionCreator::kNoRunFlags));
  EXPECT_THAT(creator->error_message(),
              testing::ContainsRegex(
                  "extension includes the key file.*privkey.pem"));
}

// Test Packaging and installing an extension using an openssl generated key.
// The openssl is generated with the following:
// > openssl genrsa -out privkey.pem 1024
// > openssl pkcs8 -topk8 -nocrypt -in privkey.pem -out privkey_asn1.pem
// The privkey.pem is a PrivateKey, and the pcks8 -topk8 creates a
// PrivateKeyInfo ASN.1 structure, we our RSAPrivateKey expects.
TEST_F(ExtensionServiceTest, PackExtensionOpenSSLKey) {
  InitializeEmptyExtensionService();
  base::FilePath input_directory =
      data_dir()
          .AppendASCII("good")
          .AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0");
  base::FilePath privkey_path(
      data_dir().AppendASCII("openssl_privkey_asn1.pem"));
  ASSERT_TRUE(base::PathExists(privkey_path));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath output_directory = temp_dir.GetPath();

  base::FilePath crx_path(output_directory.AppendASCII("ex1.crx"));

  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, privkey_path,
      base::FilePath(), ExtensionCreator::kOverwriteCRX));

  InstallCRX(crx_path, INSTALL_NEW);
}

TEST_F(ExtensionServiceTest, TestInstallThemeWithExtensionsDisabled) {
  // Themes can be installed, even when extensions are disabled.
  InitializeExtensionServiceWithExtensionsDisabled();
  base::FilePath path = data_dir().AppendASCII("theme.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(theme_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Flaky on these platforms. http://crbug.com/1148894
#define MAYBE_InstallTheme DISABLED_InstallTheme
#else
#define MAYBE_InstallTheme InstallTheme
#endif
TEST_F(ExtensionServiceTest, MAYBE_InstallTheme) {
  InitializeEmptyExtensionService();
  service()->Init();

  // A theme.
  base::FilePath path = data_dir().AppendASCII("theme.crx");
  InstallCRX(path, INSTALL_NEW);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  path = data_dir().AppendASCII("theme2.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ValidateIntegerPref(theme2_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(theme2_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // A theme with extension elements. Themes cannot have extension elements,
  // so any such elements (like content scripts) should be ignored.
  {
    path = data_dir().AppendASCII("theme_with_extension.crx");
    const Extension* extension = InstallCRX(path, INSTALL_NEW);
    ValidatePrefKeyCount(++pref_count);
    ASSERT_TRUE(extension);
    EXPECT_TRUE(extension->is_theme());
    EXPECT_TRUE(ContentScriptsInfo::GetContentScripts(extension).empty());
  }

  // A theme with image resources missing (misspelt path).
  path = data_dir().AppendASCII("theme_missing_image.crx");
  InstallCRX(path, INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);
}

TEST_F(ExtensionServiceTest, LoadLocalizedTheme) {
  // Load.
  InitializeEmptyExtensionService();
  service()->Init();

  base::FilePath extension_path = data_dir().AppendASCII("theme_i18n");

  // Don't create "Cached Theme.pak" in the extension directory, so as not to
  // modify the source tree.
  ThemeService::DisableThemePackForTesting();

  UnpackedInstaller::Create(service())->Load(extension_path);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  const Extension* theme = registry()->enabled_extensions().begin()->get();
  EXPECT_EQ("name", theme->name());
  EXPECT_EQ("description", theme->description());
}

#if BUILDFLAG(IS_POSIX)
TEST_F(ExtensionServiceTest, UnpackedExtensionMayContainSymlinkedFiles) {
  base::FilePath source_data_dir =
      data_dir().AppendASCII("unpacked").AppendASCII("symlinks_allowed");

  // Paths to test data files.
  base::FilePath source_manifest = source_data_dir.AppendASCII("manifest.json");
  ASSERT_TRUE(base::PathExists(source_manifest));
  base::FilePath source_icon = source_data_dir.AppendASCII("icon.png");
  ASSERT_TRUE(base::PathExists(source_icon));

  // Set up the temporary extension directory.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath extension_path = temp.GetPath();
  base::FilePath manifest = extension_path.Append(kManifestFilename);
  base::FilePath icon_symlink = extension_path.AppendASCII("icon.png");
  base::CopyFile(source_manifest, manifest);
  base::CreateSymbolicLink(source_icon, icon_symlink);

  // Load extension.
  InitializeEmptyExtensionService();
  UnpackedInstaller::Create(service())->Load(extension_path);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(GetErrors().empty());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
}
#endif

// Tests than an unpacked extension with an empty kMetadataFolder loads
// successfully.
TEST_F(ExtensionServiceTest, UnpackedExtensionWithEmptyMetadataFolder) {
  InitializeEmptyExtensionService();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath extension_dir = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath metadata_dir = extension_dir.Append(kMetadataFolder);
  PersistExtensionWithPaths(extension_dir, {metadata_dir}, {});
  EXPECT_TRUE(base::DirectoryExists(metadata_dir));

  UnpackedInstaller::Create(service())->Load(extension_dir);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  // The kMetadataFolder should have been deleted since it did not contain
  // any non-reserved filenames.
  EXPECT_FALSE(base::DirectoryExists(metadata_dir));
}

// Tests that an unpacked extension with only reserved filenames in the
// kMetadataFolder loads successfully.
TEST_F(ExtensionServiceTest, UnpackedExtensionWithReservedMetadataFiles) {
  InitializeEmptyExtensionService();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath extension_dir = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath metadata_dir = extension_dir.Append(kMetadataFolder);
  PersistExtensionWithPaths(
      extension_dir, {metadata_dir},
      file_util::GetReservedMetadataFilePaths(extension_dir));
  EXPECT_TRUE(base::DirectoryExists(metadata_dir));

  UnpackedInstaller::Create(service())->Load(extension_dir);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  // The kMetadataFolder should have been deleted since it did not contain
  // any non-reserved filenames.
  EXPECT_FALSE(base::DirectoryExists(metadata_dir));
}

// Tests that an unpacked extension with non-reserved files in the
// kMetadataFolder fails to load.
TEST_F(ExtensionServiceTest, UnpackedExtensionWithUserMetadataFiles) {
  InitializeEmptyExtensionService();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath extension_dir = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath metadata_dir = extension_dir.Append(kMetadataFolder);
  base::FilePath non_reserved_file =
      metadata_dir.Append(FILE_PATH_LITERAL("a.txt"));
  PersistExtensionWithPaths(
      extension_dir, {metadata_dir},
      {file_util::GetVerifiedContentsPath(extension_dir), non_reserved_file});
  EXPECT_TRUE(base::PathExists(non_reserved_file));

  UnpackedInstaller::Create(service())->Load(extension_dir);
  task_environment()->RunUntilIdle();
  ASSERT_EQ(1u, GetErrors().size());

  // Format expected error string.
  std::string expected("Failed to load extension from: ");
  expected.append(extension_dir.MaybeAsASCII())
      .append(
          ". Cannot load extension with file or directory name _metadata. "
          "Filenames starting with \"_\" are reserved for use by the system.");

  EXPECT_EQ(base::UTF8ToUTF16(expected), GetErrors()[0]);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  // Non-reserved filepaths inside the kMetadataFolder should not have been
  // deleted.
  EXPECT_TRUE(base::PathExists(non_reserved_file));
}

// Tests than an unpacked extension with an empty kMetadataFolder and a folder
// beginning with "_" fails to load.
TEST_F(ExtensionServiceTest,
       UnpackedExtensionWithEmptyMetadataAndUnderscoreFolders) {
  InitializeEmptyExtensionService();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath extension_dir = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath metadata_dir = extension_dir.Append(kMetadataFolder);
  PersistExtensionWithPaths(
      extension_dir,
      {metadata_dir, extension_dir.Append(FILE_PATH_LITERAL("_badfolder"))},
      {});

  UnpackedInstaller::Create(service())->Load(extension_dir);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());

  // Format expected error string.
  std::string expected("Failed to load extension from: ");
  expected.append(extension_dir.MaybeAsASCII())
      .append(
          ". Cannot load extension with file or directory name _badfolder. "
          "Filenames starting with \"_\" are reserved for use by the system.");

  EXPECT_EQ(base::UTF8ToUTF16(expected), GetErrors()[0]);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  // The kMetadataFolder should have been deleted since it did not contain any
  // non-reserved filenames.
  EXPECT_FALSE(base::DirectoryExists(metadata_dir));
}

// Tests that an unpacked extension with an arbitrary folder beginning with an
// underscore can't load.
TEST_F(ExtensionServiceTest, UnpackedExtensionMayNotHaveUnderscore) {
  InitializeEmptyExtensionService();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath extension_dir = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath underscore_folder =
      extension_dir.Append(FILE_PATH_LITERAL("_badfolder"));
  PersistExtensionWithPaths(
      extension_dir, {underscore_folder},
      {underscore_folder.Append(FILE_PATH_LITERAL("a.js"))});
  EXPECT_TRUE(base::DirectoryExists(underscore_folder));

  UnpackedInstaller::Create(service())->Load(extension_dir);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());

  // Format expected error string.
  std::string expected("Failed to load extension from: ");
  expected.append(extension_dir.MaybeAsASCII())
      .append(
          ". Cannot load extension with file or directory name _badfolder. "
          "Filenames starting with \"_\" are reserved for use by the system.");

  EXPECT_EQ(base::UTF8ToUTF16(expected), GetErrors()[0]);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests that an unpacked extension with a malformed manifest can't reload.
// Reload succeeds after fixing the manifest.
TEST_F(ExtensionServiceTest,
       ReloadExtensionWithMalformedManifestAndCorrectManifest) {
  InitializeEmptyExtensionService();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath extension_dir = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath manifest_dir = extension_dir.Append(kManifestFilename);
  ASSERT_FALSE(base::PathExists(manifest_dir));

  // First create a correct manifest and Load the extension successfully.
  auto manifest = base::Value::Dict().Set("version", "1.0");
  manifest.Set("name", "malformed manifest reload test");
  manifest.Set("manifest_version", 2);

  JSONFileValueSerializer serializer(manifest_dir);
  ASSERT_TRUE(serializer.Serialize(manifest));

  // Load the extension successfully.
  UnpackedInstaller::Create(service())->Load(extension_dir);
  task_environment()->RunUntilIdle();
  // Verify that Load was successful
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(ManifestLocation::kUnpacked, loaded_extensions()[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("1.0", loaded_extensions()[0]->VersionString());

  // Change the version to a malformed version.
  manifest.Set("version", "2.0b");
  ASSERT_TRUE(serializer.Serialize(manifest));

  std::string extension_id = loaded_extensions()[0]->id();

  // Reload the extension.
  service()->ReloadExtension(extension_id);
  task_environment()->RunUntilIdle();

  // An error is generated.
  ASSERT_EQ(1u, GetErrors().size());
  EXPECT_THAT(
      base::UTF16ToUTF8(GetErrors()[0]),
      ::testing::HasSubstr("Required value 'version' is missing or invalid."));

  // Verify that ReloadExtension() was not successful.
  ASSERT_EQ(0u, loaded_extensions().size());
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  // Fix the version.
  manifest.Set("version", "2.0");
  ASSERT_TRUE(serializer.Serialize(manifest));

  // Reload the extension.
  service()->ReloadExtension(extension_id);
  task_environment()->RunUntilIdle();

  // No new error is generated. Since the error generated above is still there,
  // the error size is 1.
  EXPECT_EQ(1u, GetErrors().size());
  // Verify that ReloadExtension() was successful.
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(ManifestLocation::kUnpacked, loaded_extensions()[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("2.0", loaded_extensions()[0]->VersionString());
}

TEST_F(ExtensionServiceTest, InstallLocalizedTheme) {
  InitializeEmptyExtensionService();
  service()->Init();

  base::FilePath theme_path = data_dir().AppendASCII("theme_i18n");

  const Extension* theme = PackAndInstallCRX(theme_path, INSTALL_NEW);

  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("name", theme->name());
  EXPECT_EQ("description", theme->description());
}

TEST_F(ExtensionServiceTest, InstallApps) {
  InitializeEmptyExtensionService();

  // An empty app.
  const Extension* app =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  ValidateIntegerPref(app->id(), "state", Extension::ENABLED);
  ValidateIntegerPref(app->id(), "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // Another app with non-overlapping extent. Should succeed.
  PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);

  // A third app whose extent overlaps the first. Should fail.
  PackAndInstallCRX(data_dir().AppendASCII("app3"), INSTALL_FAILED);
  ValidatePrefKeyCount(pref_count);
}

// Tests that file access is OFF by default for normal packed extensions.
TEST_F(ExtensionServiceTest, DefaultPackedFileAccess) {
  InitializeEmptyExtensionService();
  GURL file_url("file:///etc/passwd");
  const Extension* extension = PackAndInstallCRX(
      data_dir().AppendASCII("permissions").AppendASCII("files"), INSTALL_NEW);
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->HasAllowFileAccessSetting(extension->id()));
  EXPECT_FALSE(prefs->AllowFileAccess(extension->id()));
  EXPECT_FALSE(prefs->GetCreationFlags(extension->id()) &
               Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
}

// Tests that file access is ON by default for unpacked extensions and the
// associated pref is added.
TEST_F(ExtensionServiceTest, DefaultUnpackedFileAccess) {
  InitializeEmptyExtensionService();
  GURL file_url("file:///etc/passwd");

  ChromeTestExtensionLoader loader(testing_profile());
  loader.set_pack_extension(false);
  scoped_refptr<const Extension> extension = loader.LoadExtension(
      data_dir().AppendASCII("permissions").AppendASCII("files"));
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasAllowFileAccessSetting(extension->id()));
  EXPECT_TRUE(prefs->AllowFileAccess(extension->id()));
  EXPECT_TRUE(prefs->GetCreationFlags(extension->id()) &
              Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
}

// Tests that adding a packed extension grants file access if the appropriate
// creation flag is set. Note: This doesn't normally happen in practice but it
// is tested here to document the behavior.
// TODO(crbug.com/40263865): The werid behavior here should be cleared up and we
// should simplify how we're storing and checking if file access has been
// granted to an extension.
TEST_F(ExtensionServiceTest, DefaultPackedFileAccessWithCreationFlag) {
  InitializeEmptyExtensionService();
  GURL file_url("file:///etc/passwd");
  const Extension* extension = PackAndInstallCRX(
      /*dir_path=*/data_dir().AppendASCII("permissions").AppendASCII("files"),
      /*pem_path=*/base::FilePath(),
      /*install_state=*/INSTALL_NEW,
      /*creation_flags=*/Extension::ALLOW_FILE_ACCESS,
      /*install_location=*/ManifestLocation::kInternal);
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  std::string id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->HasAllowFileAccessSetting(id));
  EXPECT_FALSE(prefs->AllowFileAccess(id));
  // Even though there is no file access pref, the stored creation flags and the
  // computed creation flags on the extension will mean that it does have file
  // access. This is weird.
  EXPECT_TRUE(prefs->GetCreationFlags(extension->id()) &
              Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));

  // If the extension gets reloaded in this state, the (lack of) pref will take
  // presedence and the computed creation flags on the extension object will
  // mean that it will not longer have file access. Again this is weird.
  service()->ReloadExtensionsForTest();
  extension = registry()->GetInstalledExtension(id);
  EXPECT_FALSE(prefs->HasAllowFileAccessSetting(id));
  EXPECT_FALSE(prefs->AllowFileAccess(id));
  EXPECT_TRUE(prefs->GetCreationFlags(extension->id()) &
              Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
}

// Tests that if an extension is created with creation flags granting file
// access, but the assocaited pref for file access becomes mismatched to say
// that the extension shouldn't have file access, then on the next reload of the
// extension (e.g. on Chrome startup) the pref will take precedence.
// Regression test for crbug.com/1414398.
TEST_F(ExtensionServiceTest, FileAccessFlagAndPrefMismatch) {
  InitializeEmptyExtensionService();
  GURL file_url("file:///etc/passwd");
  // Note: We use an unpacked extension here in order to start with creation
  // flags that say the extension was installed with file access as well as
  // having the file access pref explicitly set to true (which we do for
  // unpacked extensions on install)
  ChromeTestExtensionLoader loader(testing_profile());
  loader.set_pack_extension(false);
  scoped_refptr<const Extension> extension = loader.LoadExtension(
      data_dir().AppendASCII("permissions").AppendASCII("files"));
  std::string id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasAllowFileAccessSetting(id));
  EXPECT_TRUE(prefs->AllowFileAccess(id));
  EXPECT_TRUE(prefs->GetCreationFlags(extension->id()) &
              Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));

  // If we cause a mismatch with the pref saying the extension doesn't have file
  // access, on installed extension reload (i.e. browser restart) it will have
  // lost file access.
  prefs->SetAllowFileAccess(id, false);
  service()->ReloadExtensionsForTest();
  extension = registry()->GetInstalledExtension(id);
  EXPECT_FALSE(prefs->AllowFileAccess(id));
  EXPECT_TRUE(prefs->GetCreationFlags(extension->id()) &
              Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));

  // Similarly, if the pref is mismatched to say the extension does have file
  // access, on installed extension reload (i.e. browser restart) file access
  // will be granted.
  prefs->SetAllowFileAccess(id, true);
  service()->ReloadExtensionsForTest();
  extension = registry()->GetInstalledExtension(id);
  EXPECT_TRUE(prefs->AllowFileAccess(id));
  EXPECT_TRUE(prefs->GetCreationFlags(extension->id()) &
              Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
}

TEST_F(ExtensionServiceTest, UpdateApps) {
  InitializeEmptyExtensionService();
  base::FilePath extensions_path = data_dir().AppendASCII("app_update");

  // First install v1 of a hosted app.
  const Extension* extension =
      InstallCRX(extensions_path.AppendASCII("v1.crx"), INSTALL_NEW);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  std::string id = extension->id();
  ASSERT_EQ(std::string("1"), extension->version().GetString());

  // Now try updating to v2.
  UpdateExtension(id,
                  extensions_path.AppendASCII("v2.crx"),
                  ENABLED);
  ASSERT_EQ(
      std::string("2"),
      registry()->enabled_extensions().GetByID(id)->version().GetString());
}

// Verifies that the NTP page and launch ordinals are kept when updating apps.
TEST_F(ExtensionServiceTest, UpdateAppsRetainOrdinals) {
  InitializeEmptyExtensionService();
  AppSorting* sorting = ExtensionSystem::Get(profile())->app_sorting();
  base::FilePath extensions_path = data_dir().AppendASCII("app_update");

  // First install v1 of a hosted app.
  const Extension* extension =
      InstallCRX(extensions_path.AppendASCII("v1.crx"), INSTALL_NEW);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  std::string id = extension->id();
  ASSERT_EQ(std::string("1"), extension->version().GetString());

  // Modify the ordinals so we can distinguish them from the defaults.
  syncer::StringOrdinal new_page_ordinal =
      sorting->GetPageOrdinal(id).CreateAfter();
  syncer::StringOrdinal new_launch_ordinal =
      sorting->GetAppLaunchOrdinal(id).CreateBefore();

  sorting->SetPageOrdinal(id, new_page_ordinal);
  sorting->SetAppLaunchOrdinal(id, new_launch_ordinal);

  // Now try updating to v2.
  UpdateExtension(id, extensions_path.AppendASCII("v2.crx"), ENABLED);
  ASSERT_EQ(
      std::string("2"),
      registry()->enabled_extensions().GetByID(id)->version().GetString());

  // Verify that the ordinals match.
  ASSERT_TRUE(new_page_ordinal.Equals(sorting->GetPageOrdinal(id)));
  ASSERT_TRUE(new_launch_ordinal.Equals(sorting->GetAppLaunchOrdinal(id)));
}

// Ensures that the CWS has properly initialized ordinals.
TEST_F(ExtensionServiceTest, EnsureCWSOrdinalsInitialized) {
  InitializeEmptyExtensionService();
  service()->component_loader()->Add(
      IDR_WEBSTORE_MANIFEST, base::FilePath(FILE_PATH_LITERAL("web_store")));
  service()->Init();

  AppSorting* sorting = ExtensionSystem::Get(profile())->app_sorting();
  EXPECT_TRUE(sorting->GetPageOrdinal(kWebStoreAppId).IsValid());
  EXPECT_TRUE(sorting->GetAppLaunchOrdinal(kWebStoreAppId).IsValid());
}

TEST_F(ExtensionServiceTest, InstallAppsWithUnlimitedStorage) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(registry()->enabled_extensions().empty());

  int pref_count = 0;

  // Install app1 with unlimited storage.
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  const std::string id1 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kUnlimitedStorage));
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      AppLaunchInfo::GetFullLaunchURL(extension)));
  const GURL origin1(
      AppLaunchInfo::GetFullLaunchURL(extension).DeprecatedGetOriginAsURL());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));

  // Install app2 from the same origin with unlimited storage.
  extension = PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  const std::string id2 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kUnlimitedStorage));
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      AppLaunchInfo::GetFullLaunchURL(extension)));
  const GURL origin2(
      AppLaunchInfo::GetFullLaunchURL(extension).DeprecatedGetOriginAsURL());
  EXPECT_EQ(origin1, origin2);
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin2));

  // Uninstall one of them, unlimited storage should still be granted
  // to the origin.
  UninstallExtension(id1);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));

  // Uninstall the other, unlimited storage should be revoked.
  UninstallExtension(id2);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          origin2));
}

TEST_F(ExtensionServiceTest, InstallAppsAndCheckStorageProtection) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(registry()->enabled_extensions().empty());

  int pref_count = 0;

  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->is_app());
  const std::string id1 = extension->id();
  const GURL origin1(
      AppLaunchInfo::GetFullLaunchURL(extension).DeprecatedGetOriginAsURL());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
      origin1));

  // App 4 has a different origin (maps.google.com).
  extension = PackAndInstallCRX(data_dir().AppendASCII("app4"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  const std::string id2 = extension->id();
  const GURL origin2(
      AppLaunchInfo::GetFullLaunchURL(extension).DeprecatedGetOriginAsURL());
  ASSERT_NE(origin1, origin2);
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
      origin2));

  UninstallExtension(id1);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  UninstallExtension(id2);

  EXPECT_TRUE(registry()->enabled_extensions().empty());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
          origin1));
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageProtected(
          origin2));
}

// Test that when an extension version is reinstalled, nothing happens.
TEST_F(ExtensionServiceTest, Reinstall) {
  InitializeEmptyExtensionService();

  // A simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // Reinstall the same version, it should overwrite the previous one.
  InstallCRX(path, INSTALL_UPDATED);

  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));
}

// Test that we can determine if extensions came from the
// Chrome web store.
TEST_F(ExtensionServiceTest, FromWebStore) {
  InitializeEmptyExtensionService();

  // A simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  // Not from web store.
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  ValidatePrefKeyCount(1);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "from_webstore", false));
  ASSERT_FALSE(extension->from_webstore());

  // Test install from web store.
  InstallCRXFromWebStore(path, INSTALL_UPDATED);  // From web store.

  ValidatePrefKeyCount(1);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "from_webstore", true));

  // Reload so extension gets reinitialized with new value.
  service()->ReloadExtensionsForTest();
  extension = registry()->enabled_extensions().GetByID(id);
  ASSERT_TRUE(extension->from_webstore());

  // Upgrade to version 2.0
  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  ValidatePrefKeyCount(1);
  ASSERT_TRUE(ValidateBooleanPref(good_crx, "from_webstore", true));
}

// Test upgrading a signed extension.
TEST_F(ExtensionServiceTest, UpgradeSignedGood) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  ASSERT_EQ("1.0.0.0", extension->version().GetString());
  ASSERT_EQ(0u, GetErrors().size());

  // Upgrade to version 1.0.0.1.
  // Also test that the extension's old and new title are correctly retrieved.
  path = data_dir().AppendASCII("good2.crx");
  InstallCRX(path, INSTALL_UPDATED, Extension::NO_FLAGS, "My extension 1");
  extension = registry()->enabled_extensions().GetByID(id);

  ASSERT_EQ("1.0.0.1", extension->version().GetString());
  ASSERT_EQ("My updated extension 1", extension->name());
  ASSERT_EQ(0u, GetErrors().size());
}

// Test upgrading a signed extension with a bad signature.
TEST_F(ExtensionServiceTest, UpgradeSignedBad) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  // Try upgrading with a bad signature. This should fail during the unpack,
  // because the key will not match the signature.
  path = data_dir().AppendASCII("bad_signature.crx");
  InstallCRX(path, INSTALL_FAILED);
}

// Test a normal update via the UpdateExtension API
TEST_F(ExtensionServiceTest, UpdateExtension) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ("1.0.0.0", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  ASSERT_EQ("1.0.0.1", registry()
                           ->enabled_extensions()
                           .GetByID(good_crx)
                           ->version()
                           .GetString());
}

// Extensions should not be updated during browser shutdown.
TEST_F(ExtensionServiceTest, UpdateExtensionDuringShutdown) {
  InitializeEmptyExtensionService();

  // Install an extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, good->id());

  // Simulate shutdown.
  service()->set_browser_terminating_for_test(true);

  // Update should fail and extension should not be updated.
  path = data_dir().AppendASCII("good2.crx");
  CRXFileInfo crx_info(path, GetTestVerifierFormat());
  crx_info.extension_id = good_crx;
  scoped_refptr<extensions::CrxInstaller> installer =
      service()->CreateUpdateInstaller(crx_info, true);
  ASSERT_FALSE(installer);
  ASSERT_EQ("1.0.0.0", registry()
                           ->enabled_extensions()
                           .GetByID(good_crx)
                           ->version()
                           .GetString());
}

// Test updating a not-already-installed extension - this should fail
TEST_F(ExtensionServiceTest, UpdateNotInstalledExtension) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(good_crx, path, UPDATED);
  task_environment()->RunUntilIdle();

  ASSERT_EQ(0u, registry()->enabled_extensions().size());
  ASSERT_FALSE(installed_extension());
  ASSERT_EQ(0u, loaded_extensions().size());
}

// Makes sure you can't downgrade an extension via UpdateExtension
TEST_F(ExtensionServiceTest, UpdateWillNotDowngrade) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good2.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ("1.0.0.1", good->VersionString());
  ASSERT_EQ(good_crx, good->id());

  // Change path from good2.crx -> good.crx
  path = data_dir().AppendASCII("good.crx");
  UpdateExtension(good_crx, path, FAILED);
  ASSERT_EQ("1.0.0.1", registry()
                           ->enabled_extensions()
                           .GetByID(good_crx)
                           ->version()
                           .GetString());
}

// Make sure calling update with an identical version does nothing
TEST_F(ExtensionServiceTest, UpdateToSameVersionIsNoop) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, FAILED_SILENTLY);
}

// Tests that updating an extension does not clobber old state.
TEST_F(ExtensionServiceTest, UpdateExtensionPreservesState) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* goodext = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ("1.0.0.0", goodext->VersionString());
  ASSERT_EQ(good_crx, goodext->id());

  // Disable it and allow it to run in incognito. These settings should carry
  // over to the updated version.
  service()->DisableExtension(goodext->id(),
                              disable_reason::DISABLE_USER_ACTION);
  util::SetIsIncognitoEnabled(goodext->id(), profile(), true);

  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, INSTALLED);
  ASSERT_EQ(1u, registry()->disabled_extensions().size());
  const Extension* goodext2 =
      registry()->disabled_extensions().GetByID(good_crx);
  ASSERT_EQ("1.0.0.1", goodext2->version().GetString());
  EXPECT_TRUE(util::IsIncognitoEnabled(goodext2->id(), profile()));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION,
            ExtensionPrefs::Get(profile())->GetDisableReasons(goodext2->id()));
}

// Tests that updating preserves extension location.
TEST_F(ExtensionServiceTest, UpdateExtensionPreservesLocation) {
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* goodext =
      InstallCRX(path, mojom::ManifestLocation::kExternalPref, INSTALL_NEW,
                 Extension::NO_FLAGS);

  ASSERT_EQ("1.0.0.0", goodext->VersionString());
  ASSERT_EQ(good_crx, goodext->id());

  path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(good_crx, path, ENABLED);
  const Extension* goodext2 =
      registry()->enabled_extensions().GetByID(good_crx);
  ASSERT_EQ("1.0.0.1", goodext2->version().GetString());
  EXPECT_EQ(goodext2->location(), ManifestLocation::kExternalPref);
}

// Makes sure that LOAD extension types can downgrade.
TEST_F(ExtensionServiceTest, LoadExtensionsCanDowngrade) {
  InitializeEmptyExtensionService();

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // We'll write the extension manifest dynamically to a temporary path
  // to make it easier to change the version number.
  base::FilePath extension_path = temp.GetPath();
  base::FilePath manifest_path = extension_path.Append(kManifestFilename);
  ASSERT_FALSE(base::PathExists(manifest_path));

  // Start with version 2.0.
  base::Value::Dict manifest;
  manifest.Set("version", "2.0");
  manifest.Set("name", "LOAD Downgrade Test");
  manifest.Set("manifest_version", 2);

  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(manifest));

  UnpackedInstaller::Create(service())->Load(extension_path);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(ManifestLocation::kUnpacked, loaded_extensions()[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("2.0", loaded_extensions()[0]->VersionString());

  // Now set the version number to 1.0, reload the extensions and verify that
  // the downgrade was accepted.
  manifest.Set("version", "1.0");
  ASSERT_TRUE(serializer.Serialize(manifest));

  UnpackedInstaller::Create(service())->Load(extension_path);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(ManifestLocation::kUnpacked, loaded_extensions()[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ("1.0", loaded_extensions()[0]->VersionString());
}

namespace {

bool IsExtension(const Extension* extension, content::BrowserContext* context) {
  return extension->GetType() == Manifest::TYPE_EXTENSION;
}

#if defined(ENABLE_BLOCKLIST_TESTS)
std::set<std::string> StringSet(const std::string& s) {
  std::set<std::string> set;
  set.insert(s);
  return set;
}
std::set<std::string> StringSet(const std::string& s1, const std::string& s2) {
  std::set<std::string> set = StringSet(s1);
  set.insert(s2);
  return set;
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

}  // namespace

// Test adding a pending extension.
TEST_F(ExtensionServiceTest, AddPendingExtensionFromSync) {
  InitializeEmptyExtensionService();

  const std::string kFakeId(all_zero);
  const GURL kFakeUpdateURL("http:://fake.update/url");
  const bool kFakeRemoteInstall(false);

  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kFakeId,
          kFakeUpdateURL,
          base::Version(),
          &IsExtension,
          kFakeRemoteInstall));

  const PendingExtensionInfo* pending_extension_info;
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kFakeId)));
  EXPECT_EQ(kFakeUpdateURL, pending_extension_info->update_url());
  EXPECT_EQ(&IsExtension, pending_extension_info->should_allow_install_);
  // Use
  // EXPECT_TRUE(kFakeRemoteInstall == pending_extension_info->remote_install())
  // instead of
  // EXPECT_EQ(kFakeRemoteInstall, pending_extension_info->remote_install())
  // as gcc 4.7 issues the following warning on EXPECT_EQ(false, x), which is
  // turned into an error with -Werror=conversion-null:
  //   converting 'false' to pointer type for argument 1 of
  //   'char testing::internal::IsNullLiteralHelper(testing::internal::Secret*)'
  // https://code.google.com/p/googletest/issues/detail?id=458
  EXPECT_TRUE(kFakeRemoteInstall == pending_extension_info->remote_install());
}

namespace {
const char kGoodId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodUpdateURL[] = "http://good.update/url";
const char kGoodVersion[] = "1";
const bool kGoodIsFromSync = true;
const bool kGoodRemoteInstall = false;
}  // namespace

// Test installing a pending extension (this goes through
// ExtensionService::UpdateExtension).
TEST_F(ExtensionServiceTest, UpdatePendingExtension) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(kGoodVersion),
          &IsExtension,
          kGoodRemoteInstall));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  const Extension* extension =
      registry()->enabled_extensions().GetByID(kGoodId);
  EXPECT_TRUE(extension);
}

TEST_F(ExtensionServiceTest, UpdatePendingExtensionWrongVersion) {
  InitializeEmptyExtensionService();
  base::Version other_version("0.1");
  ASSERT_TRUE(other_version.IsValid());
  ASSERT_NE(other_version, base::Version(kGoodVersion));
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          other_version,
          &IsExtension,
          kGoodRemoteInstall));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // After installation, the extension should be disabled, because it's missing
  // permissions.
  UpdateExtension(kGoodId, path, DISABLED);

  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->DidExtensionEscalatePermissions(kGoodId));

  // It should still have been installed though.
  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  const Extension* extension =
      registry()->disabled_extensions().GetByID(kGoodId);
  EXPECT_TRUE(extension);
}

namespace {

bool IsTheme(const Extension* extension, content::BrowserContext* context) {
  return extension->is_theme();
}

}  // namespace

// Test updating a pending theme.
TEST_F(ExtensionServiceTest, UpdatePendingTheme) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromSync(
      theme_crx, GURL(), base::Version(), &IsTheme, false));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  const Extension* extension =
      registry()->enabled_extensions().GetByID(theme_crx);
  ASSERT_TRUE(extension);

  EXPECT_FALSE(
      ExtensionPrefs::Get(profile())->IsExtensionDisabled(extension->id()));
  EXPECT_TRUE(service()->IsExtensionEnabled(theme_crx));
}

// Test updating a pending CRX as if the source is an external extension
// with an update URL.  In this case we don't know if the CRX is a theme
// or not.
TEST_F(ExtensionServiceTest, UpdatePendingExternalCrx) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      theme_crx, std::string(), GURL(), ManifestLocation::kExternalPrefDownload,
      Extension::NO_FLAGS, false));

  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  const Extension* extension =
      registry()->enabled_extensions().GetByID(theme_crx);
  ASSERT_TRUE(extension);

  EXPECT_FALSE(
      ExtensionPrefs::Get(profile())->IsExtensionDisabled(extension->id()));
  EXPECT_TRUE(service()->IsExtensionEnabled(extension->id()));
  EXPECT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));
}

// Test updating a pending CRX as if the source is an external extension
// with an update URL.  The external update should overwrite a sync update,
// but a sync update should not overwrite a non-sync update.
TEST_F(ExtensionServiceTest, UpdatePendingExternalCrxWinsOverSync) {
  InitializeEmptyExtensionService();

  // Add a crx to be installed from the update mechanism.
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(),
          &IsExtension,
          kGoodRemoteInstall));

  // Check that there is a pending crx, with is_from_sync set to true.
  const PendingExtensionInfo* pending_extension_info;
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kGoodId)));
  EXPECT_TRUE(pending_extension_info->is_from_sync());

  // Add a crx to be updated, with the same ID, from a non-sync source.
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromExternalUpdateUrl(
      kGoodId, std::string(), GURL(kGoodUpdateURL),
      ManifestLocation::kExternalPrefDownload, Extension::NO_FLAGS, false));

  // Check that there is a pending crx, with is_from_sync set to false.
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kGoodId)));
  EXPECT_FALSE(pending_extension_info->is_from_sync());
  EXPECT_EQ(ManifestLocation::kExternalPrefDownload,
            pending_extension_info->install_source());

  // Add a crx to be installed from the update mechanism.
  EXPECT_FALSE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(),
          &IsExtension,
          kGoodRemoteInstall));

  // Check that the external, non-sync update was not overridden.
  ASSERT_TRUE((pending_extension_info =
                   service()->pending_extension_manager()->GetById(kGoodId)));
  EXPECT_FALSE(pending_extension_info->is_from_sync());
  EXPECT_EQ(ManifestLocation::kExternalPrefDownload,
            pending_extension_info->install_source());
}

// Updating a theme should fail if the updater is explicitly told that
// the CRX is not a theme.
TEST_F(ExtensionServiceTest, UpdatePendingCrxThemeMismatch) {
  InitializeEmptyExtensionService();
  EXPECT_TRUE(service()->pending_extension_manager()->AddFromSync(
      theme_crx, GURL(), base::Version(), &IsExtension, false));

  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  UpdateExtension(theme_crx, path, FAILED_SILENTLY);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(theme_crx));

  const Extension* extension =
      registry()->GetExtensionById(theme_crx, ExtensionRegistry::EVERYTHING);
  ASSERT_FALSE(extension);
}

// TODO(akalin): Test updating a pending extension non-silently once
// we can mock out ExtensionInstallUI and inject our version into
// UpdateExtension().

// Test updating a pending extension which fails the should-install test.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionFailedShouldInstallTest) {
  InitializeEmptyExtensionService();
  // Add pending extension with a flipped is_theme.
  EXPECT_TRUE(
      service()->pending_extension_manager()->AddFromSync(
          kGoodId,
          GURL(kGoodUpdateURL),
          base::Version(),
          &IsTheme,
          kGoodRemoteInstall));
  EXPECT_TRUE(service()->pending_extension_manager()->IsIdPending(kGoodId));

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, UPDATED);

  // TODO(akalin): Figure out how to check that the extensions
  // directory is cleaned up properly in OnExtensionInstalled().

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));
}

// TODO(akalin): Figure out how to test that installs of pending
// unsyncable extensions are blocked.

// Test updating a pending extension for one that is not pending.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionNotPending) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  UpdateExtension(kGoodId, path, UPDATED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));
}

// Test updating a pending extension for one that is already
// installed.
TEST_F(ExtensionServiceTest, UpdatePendingExtensionAlreadyInstalled) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* good = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());

  EXPECT_FALSE(good->is_theme());

  // Use AddExtensionImpl() as AddFrom*() would balk.
  service()->pending_extension_manager()->AddExtensionImpl(
      good->id(), std::string(), ManifestURL::GetUpdateURL(good),
      base::Version(), &IsExtension, kGoodIsFromSync,
      ManifestLocation::kInternal, Extension::NO_FLAGS, false,
      kGoodRemoteInstall);
  UpdateExtension(good->id(), path, ENABLED);

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(kGoodId));
}

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests blocklisting then unblocklisting extensions after the service has been
// initialized.
TEST_F(ExtensionServiceTest, SetUnsetBlocklistInPrefs) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  const ExtensionSet& enabled_extensions = registry()->enabled_extensions();
  const ExtensionSet& blocklisted_extensions =
      registry()->blocklisted_extensions();

  EXPECT_TRUE(enabled_extensions.Contains(good0) &&
              !blocklisted_extensions.Contains(good0));
  EXPECT_TRUE(enabled_extensions.Contains(good1) &&
              !blocklisted_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2) &&
              !blocklisted_extensions.Contains(good2));

  EXPECT_FALSE(DoesIntegerPrefExist(good0, kPrefBlocklistState));
  EXPECT_FALSE(DoesIntegerPrefExist(good1, kPrefBlocklistState));
  EXPECT_FALSE(DoesIntegerPrefExist(good2, kPrefBlocklistState));
  EXPECT_FALSE(DoesIntegerPrefExist("invalid_id", kPrefBlocklistState));

  // Blocklist good0 and good1 (and an invalid extension ID).
  test_blocklist.SetBlocklistState(good0, BLOCKLISTED_MALWARE, true);
  test_blocklist.SetBlocklistState(good1, BLOCKLISTED_MALWARE, true);
  test_blocklist.SetBlocklistState("invalid_id", BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(!enabled_extensions.Contains(good0) &&
              blocklisted_extensions.Contains(good0));
  EXPECT_TRUE(!enabled_extensions.Contains(good1) &&
              blocklisted_extensions.Contains(good1));
  EXPECT_TRUE(enabled_extensions.Contains(good2) &&
              !blocklisted_extensions.Contains(good2));

  ValidateIntegerPref(good0, kPrefBlocklistState, kBlocklistedMalwareInteger);
  ValidateIntegerPref(good1, kPrefBlocklistState, kBlocklistedMalwareInteger);
  EXPECT_FALSE(DoesIntegerPrefExist(good2, kPrefBlocklistState));
  EXPECT_FALSE(DoesIntegerPrefExist("invalid_id", kPrefBlocklistState));

  // Un-blocklist good1 and blocklist good2.
  test_blocklist.Clear(false);
  test_blocklist.SetBlocklistState(good0, BLOCKLISTED_MALWARE, true);
  test_blocklist.SetBlocklistState(good2, BLOCKLISTED_MALWARE, true);
  test_blocklist.SetBlocklistState("invalid_id", BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(!enabled_extensions.Contains(good0) &&
              blocklisted_extensions.Contains(good0));
  EXPECT_TRUE(enabled_extensions.Contains(good1) &&
              !blocklisted_extensions.Contains(good1));
  EXPECT_TRUE(!enabled_extensions.Contains(good2) &&
              blocklisted_extensions.Contains(good2));

  ValidateIntegerPref(good0, kPrefBlocklistState, kBlocklistedMalwareInteger);
  EXPECT_FALSE(DoesIntegerPrefExist(good1, kPrefBlocklistState));
  ValidateIntegerPref(good2, kPrefBlocklistState, kBlocklistedMalwareInteger);
  EXPECT_FALSE(DoesIntegerPrefExist("invalid_id", kPrefBlocklistState));
}

// Tests that an extension that was disabled through Omaha won't be
// re-enabled if it's not present in the Safe Browsing blocklist.
// Regression test for https://crbug.com/1107040.
TEST_F(ExtensionServiceTest, NoUnsetBlocklistInPrefs) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  // We really only care about good0 for this test since the other
  // functionality is already tested in the above test.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good0));
  EXPECT_FALSE(registry()->blocklisted_extensions().Contains(good0));

  auto attributes = base::Value::Dict().Set("_malware", true);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  service()->PerformActionBasedOnOmahaAttributes(good0, attributes);
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      good0, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));
  EXPECT_FALSE(registry()->enabled_extensions().Contains(good0));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good0));

  // Un-blocklist all extensions from the Safe Browsing blocklist.
  test_blocklist.Clear(false);
  task_environment()->RunUntilIdle();

  // If the extension has a BLOCKLISTED_MALWARE state in the Omaha blocklist
  // pref, the extension should still not be enabled even if it's not on the SB
  // blocklist. This state needs to be removed prior to
  // unblocklisting/re-enabling.
  EXPECT_FALSE(registry()->enabled_extensions().Contains(good0));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good0));
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      good0, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));
  EXPECT_FALSE(DoesIntegerPrefExist(good1, kPrefBlocklistState));
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests trying to install a blocklisted extension.
TEST_F(ExtensionServiceTest, BlocklistedExtensionWillNotInstall) {
  scoped_refptr<FakeSafeBrowsingDatabaseManager> blocklist_db(
      new FakeSafeBrowsingDatabaseManager(true));
  ScopedDatabaseManagerForTest scoped_blocklist_db(blocklist_db);

  InitializeEmptyExtensionService();
  service()->Init();

  // After blocklisting good_crx, we cannot install it.
  blocklist_db->SetUnsafe(good_crx).NotifyUpdate();
  task_environment()->RunUntilIdle();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // HACK: specify WAS_INSTALLED_BY_DEFAULT so that test machinery doesn't
  // decide to install this silently. Somebody should fix these tests, all
  // 6,000 lines of them. Hah!
  InstallCRX(path, INSTALL_FAILED, Extension::WAS_INSTALLED_BY_DEFAULT);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests that previously blocklisted extension will be enabled if it is removed
// from the blocklist. Also checks that all blocklisted preferences will be
// cleared in that case.
TEST_F(ExtensionServiceTest, RemoveExtensionFromBlocklist) {
  TestBlocklist test_blocklist;
  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  service()->Init();

  ASSERT_TRUE(registry()->enabled_extensions().Contains(good0));
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         good0);

  // Add the extension to the blocklist.
  test_blocklist.SetBlocklistState(good0, BLOCKLISTED_MALWARE, true);
  observer.WaitForExtensionUnloaded();

  // The extension should be disabled, "blocklist_state" prefs should be set.
  auto* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(registry()->enabled_extensions().Contains(good0));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good0, prefs));
  EXPECT_EQ(
      BitMapBlocklistState::BLOCKLISTED_MALWARE,
      blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(good0, prefs));

  // Remove the extension from the blocklist.
  test_blocklist.SetBlocklistState(good0, NOT_BLOCKLISTED, true);
  observer.WaitForExtensionLoaded()->id();

  // The extension should be enabled, "blocklist_state" should be cleared.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good0));
  EXPECT_FALSE(blocklist_prefs::IsExtensionBlocklisted(good0, prefs));
  EXPECT_EQ(
      BitMapBlocklistState::NOT_BLOCKLISTED,
      blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(good0, prefs));
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

#if defined(ENABLE_BLOCKLIST_TESTS)
// Unload blocklisted extension on policy change.
TEST_F(ExtensionServiceTest, UnloadBlocklistedExtensionPolicy) {
  TestBlocklist test_blocklist;

  // A profile with no extensions installed.
  InitializeEmptyExtensionServiceWithTestingPrefs();
  test_blocklist.Attach(service()->blocklist_);

  base::FilePath path = data_dir().AppendASCII("good.crx");

  const Extension* good = InstallCRX(path, INSTALL_NEW);
  EXPECT_EQ(good_crx, good->id());
  UpdateExtension(good_crx, path, FAILED_SILENTLY);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good_crx, true);
  }

  test_blocklist.SetBlocklistState(good_crx, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  // The good_crx is blocklisted and the allowlist doesn't negate it.
  ValidateIntegerPref(good_crx, kPrefBlocklistState,
                      kBlocklistedMalwareInteger);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests that a blocklisted extension is eventually unloaded on startup, if it
// wasn't already.
TEST_F(ExtensionServiceTest, WillNotLoadBlocklistedExtensionsFromDirectory) {
  TestBlocklist test_blocklist;

  // A profile with 3 extensions installed: good0, good1, and good2.
  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);

  // Blocklist good1 before the service initializes.
  test_blocklist.SetBlocklistState(good1, BLOCKLISTED_MALWARE, false);

  // Load extensions and verify they haven't been blocklisted yet.
  service()->Init();
  ASSERT_EQ(3u, loaded_extensions().size());

  // Notify service about new extension is blocklisted.
  test_blocklist.NotifyUpdate();
  task_environment()->RunUntilIdle();

  ASSERT_EQ(1u, registry()->blocklisted_extensions().size());
  ASSERT_EQ(2u, registry()->enabled_extensions().size());

  ASSERT_TRUE(registry()->enabled_extensions().Contains(good0));
  ASSERT_TRUE(registry()->blocklisted_extensions().Contains(good1));
  ASSERT_TRUE(registry()->enabled_extensions().Contains(good2));
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests extensions blocklisted in prefs on startup; one still blocklisted by
// safe browsing, the other not. The not-blocklisted one should recover.
TEST_F(ExtensionServiceTest, BlocklistedInPrefsFromStartup) {
  TestBlocklist test_blocklist;

  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      good0, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      ExtensionPrefs::Get(profile()));
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      good1, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      ExtensionPrefs::Get(profile()));

  // Extension service hasn't loaded yet, but IsExtensionEnabled reads out of
  // prefs. Ensure it takes into account the blocklist state (crbug.com/373842).
  EXPECT_FALSE(service()->IsExtensionEnabled(good0));
  EXPECT_FALSE(service()->IsExtensionEnabled(good1));
  EXPECT_TRUE(service()->IsExtensionEnabled(good2));

  service()->Init();

  // Give time for state to update
  // Ensure that extension is loaded.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(2u, registry()->blocklisted_extensions().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good0));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good1));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good2));

  test_blocklist.SetBlocklistState(good1, BLOCKLISTED_MALWARE, true);

  // Give time for the blocklist to update.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1u, registry()->blocklisted_extensions().size());
  EXPECT_EQ(2u, registry()->enabled_extensions().size());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good0));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good1));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good2));
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests that blocklisted extensions cannot be reloaded, both those loaded
// before and after extension service startup.
TEST_F(ExtensionServiceTest, ReloadBlocklistedExtension) {
  TestBlocklist test_blocklist;

  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);

  test_blocklist.SetBlocklistState(good1, BLOCKLISTED_MALWARE, false);
  service()->Init();
  test_blocklist.SetBlocklistState(good2, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(StringSet(good0), registry()->enabled_extensions().GetIDs());
  EXPECT_EQ(StringSet(good1, good2),
            registry()->blocklisted_extensions().GetIDs());

  service()->ReloadExtension(good1);
  service()->ReloadExtension(good2);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(StringSet(good0), registry()->enabled_extensions().GetIDs());
  EXPECT_EQ(StringSet(good1, good2),
            registry()->blocklisted_extensions().GetIDs());
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

// Tests blocking then unblocking enabled extensions after the service has been
// initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockEnabledExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  AssertExtensionBlocksAndUnblocks(true, good0);
}

// Tests blocking then unblocking disabled extensions after the service has been
// initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockDisabledExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  service()->DisableExtension(good0, disable_reason::DISABLE_RELOAD);

  AssertExtensionBlocksAndUnblocks(true, good0);
}

// Tests blocking then unblocking terminated extensions after the service has
// been initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockTerminatedExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();

  TerminateExtension(good0);

  AssertExtensionBlocksAndUnblocks(true, good0);
}

// Tests blocking then unblocking policy-forced extensions after the service has
// been initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockPolicyExtension) {
  // Mark as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  InitializeEmptyExtensionServiceWithTestingPrefs();

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Blocklist everything.
    pref.SetBlocklistedByDefault(true);
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  // Have policy force-install an extension.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                 data_dir().AppendASCII("good.crx"));

  // Reloading extensions should find our externally registered extension
  // and install it.
  WaitForExternalExtensionInstalled(good_crx);

  AssertExtensionBlocksAndUnblocks(false, good_crx);
}

#if defined(ENABLE_BLOCKLIST_TESTS)
// Tests blocking then unblocking extensions that are blocklisted both before
// and after Init().
TEST_F(ExtensionServiceTest, BlockAndUnblockBlocklistedExtension) {
  TestBlocklist test_blocklist;

  InitializeGoodInstalledExtensionService();
  test_blocklist.Attach(service()->blocklist_);

  test_blocklist.SetBlocklistState(good0, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  service()->Init();

  test_blocklist.SetBlocklistState(good1, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  // Blocklisted extensions stay blocklisted.
  AssertExtensionBlocksAndUnblocks(false, good0);
  AssertExtensionBlocksAndUnblocks(false, good1);

  service()->BlockAllExtensions();

  // Remove an extension from the blocklist while the service is blocked.
  test_blocklist.SetBlocklistState(good0, NOT_BLOCKLISTED, true);
  // Add an extension to the blocklist while the service is blocked.
  test_blocklist.SetBlocklistState(good2, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();

  // Go directly to blocked, do not pass go, do not collect $200.
  ASSERT_TRUE(IsBlocked(good0));
  // Get on the blocklist - even if you were blocked!
  ASSERT_FALSE(IsBlocked(good2));
}
#endif  // defined(ENABLE_BLOCKLIST_TESTS)

// Tests blocking then unblocking enabled component extensions after the service
// has been initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockEnabledComponentExtension) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install a component extension.
  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII(good0)
                            .AppendASCII("1.0.0.0");
  std::string manifest;
  ASSERT_TRUE(
      base::ReadFileToString(path.Append(kManifestFilename), &manifest));
  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Component extension should never block.
  AssertExtensionBlocksAndUnblocks(false, good0);
}

// Tests blocking then unblocking a theme after the service has been
// initialized.
TEST_F(ExtensionServiceTest, BlockAndUnblockTheme) {
  InitializeEmptyExtensionService();
  service()->Init();

  base::FilePath path = data_dir().AppendASCII("theme.crx");
  InstallCRX(path, INSTALL_NEW);

  AssertExtensionBlocksAndUnblocks(true, theme_crx);
}

// Tests that blocking extensions before Init() results in loading blocked
// extensions.
TEST_F(ExtensionServiceTest, WillNotLoadExtensionsWhenBlocked) {
  InitializeGoodInstalledExtensionService();

  service()->BlockAllExtensions();

  service()->Init();

  ASSERT_TRUE(IsBlocked(good0));
  ASSERT_TRUE(IsBlocked(good0));
  ASSERT_TRUE(IsBlocked(good0));
}

// Tests that IsEnabledExtension won't crash on an uninstalled extension.
TEST_F(ExtensionServiceTest, IsEnabledExtensionBlockedAndNotInstalled) {
  InitializeEmptyExtensionService();

  service()->BlockAllExtensions();

  service()->IsExtensionEnabled(theme_crx);
}

// Will not install extension blocklisted by policy.
TEST_F(ExtensionServiceTest, BlocklistedByPolicyWillNotInstall) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Blocklist everything.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetBlocklistedByDefault(true);
  }

  // Blocklist prevents us from installing good_crx.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_FAILED);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  // Now allowlist this particular extension.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good_crx, true);
  }

  // Ensure we can now install good_crx.
  InstallCRX(path, INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
}

// Extension blocklisted by policy get unloaded after installing.
TEST_F(ExtensionServiceTest, BlocklistedByPolicyRemovedIfRunning) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install good_crx.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Blocklist this extension.
    pref.SetIndividualExtensionInstallationAllowed(good_crx, false);
  }

  // Extension should not be running now.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests that component extensions are not blocklisted by policy.
TEST_F(ExtensionServiceTest, ComponentExtensionAllowlisted) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Blocklist everything.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetBlocklistedByDefault(true);
  }

  // Install a component extension.
  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII(good0)
                            .AppendASCII("1.0.0.0");
  std::string manifest;
  ASSERT_TRUE(
      base::ReadFileToString(path.Append(kManifestFilename), &manifest));
  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Extension should be installed despite blocklist.
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good0));

  // Poke external providers and make sure the extension is still present.
  service()->CheckForExternalUpdates();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good0));

  // Extension should not be uninstalled on blocklist changes.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good0, false);
  }
  task_environment()->RunUntilIdle();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good0));
}

// Tests that active permissions are not revoked from component extensions
// by policy when the policy is updated. https://crbug.com/746017.
TEST_F(ExtensionServiceTest, ComponentExtensionAllowlistedPermission) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install a component extension.
  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII(good0)
                            .AppendASCII("1.0.0.0");
  std::string manifest;
  ASSERT_TRUE(
      base::ReadFileToString(path.Append(kManifestFilename), &manifest));
  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Extension should have the "tabs" permission.
  EXPECT_TRUE(registry()
                  ->enabled_extensions()
                  .GetByID(good0)
                  ->permissions_data()
                  ->active_permissions()
                  .HasAPIPermission(APIPermissionID::kTab));

  // Component should not lose permissions on policy change.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission(good0, "tabs");
  }

  service()->OnExtensionManagementSettingsChanged();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(registry()
                  ->enabled_extensions()
                  .GetByID(good0)
                  ->permissions_data()
                  ->active_permissions()
                  .HasAPIPermission(APIPermissionID::kTab));
}

// Tests that policy-installed extensions are not blocklisted by policy.
TEST_F(ExtensionServiceTest, PolicyInstalledExtensionsAllowlisted) {
  // Mark as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  InitializeEmptyExtensionServiceWithTestingPrefs();

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Blocklist everything.
    pref.SetBlocklistedByDefault(true);
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  // Have policy force-install an extension.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
  provider->UpdateOrAddExtension(
      good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"));

  // Reloading extensions should find our externally registered extension
  // and install it.
  WaitForExternalExtensionInstalled(good_crx);

  // Extension should be installed despite blocklist.
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  // Blocklist update should not uninstall the extension.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(good0, false);
  }
  task_environment()->RunUntilIdle();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
}

// These tests are restricted to Windows and Mac platforms because the disable
// logic for off-store force-installed extensions in low-trust environments
// (non-domain-joined) is only implemented on these platforms as per the changes
// in `standard_management_policy_provider.cc`.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Tests that non-CWS extensions are disabled when force-installed in a non
// domain-join environment.
TEST_F(ExtensionServiceTest, NonCWSForceInstalledDisabledOnNonDomainJoin) {
  // Mark the environment as non-enterprise managed.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Add a policy-installed extension.
  scoped_refptr<const Extension> extension =
      CreateExtension("policy_installed", data_dir().AppendASCII("good.crx"),
                      ManifestLocation::kExternalPolicyDownload);
  service()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        extension->id(), "http://example.com/update_url", true);
  }

  // The extension should be disabled because it is force-installed from a
  // non-CWS source in a non-domain-joined (low-trust) environment.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(extension->id()));
  EXPECT_EQ(disable_reason::DISABLE_NOT_VERIFIED,
            ExtensionPrefs::Get(profile())->GetDisableReasons(extension->id()));
}

// Tests that non-CWS extensions are enabled when force-installed in a domain-
// join environment.
TEST_F(ExtensionServiceTest, NonCWSForceInstalledEnabledOnDomainJoin) {
  // Mark the environment as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Add a policy-installed extension.
  scoped_refptr<const Extension> extension =
      CreateExtension("policy_installed", data_dir().AppendASCII("good.crx"),
                      ManifestLocation::kExternalPolicyDownload);
  service()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        extension->id(), "http://example.com/update_url", true);
  }

  // The extension should be enabled because it is force-installed from a
  // non-CWS source in a domain-joined (trusted) environment.
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(extension->id()));
}
#endif

// Tests that extensions cannot be installed if the policy provider prohibits
// it. This functionality is implemented in CrxInstaller::ConfirmInstall().
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsInstall) {
  InitializeEmptyExtensionService();

  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider_(
      TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider_);

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_FAILED);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests that extensions cannot be loaded from prefs if the policy provider
// prohibits it. This functionality is implemented in InstalledLoader::Load().
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsLoadFromPrefs) {
  InitializeEmptyExtensionService();

  // Create a fake extension to be loaded as though it were read from prefs.
  base::FilePath path =
      data_dir().AppendASCII("management").AppendASCII("simple_extension");
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "simple_extension");
  manifest.Set(keys::kVersion, "1");
  manifest.Set(keys::kManifestVersion, 2);
  // UNPACKED is for extensions loaded from a directory. We use it here, even
  // though we're testing loading from prefs, so that we don't need to provide
  // an extension key.
  ExtensionInfo extension_info(&manifest, std::string(), path,
                               ManifestLocation::kUnpacked);

  // Ensure we can load it with no management policy in place.
  GetManagementPolicy()->UnregisterAllProviders();
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  InstalledLoader(service()).Load(extension_info, false);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  const Extension* extension =
      (registry()->enabled_extensions().begin())->get();
  EXPECT_TRUE(service()->UninstallExtension(
      extension->id(), UNINSTALL_REASON_FOR_TESTING, nullptr));
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  // Ensure we cannot load it if management policy prohibits installation.
  TestManagementPolicyProvider provider_(
      TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider_);

  InstalledLoader(service()).Load(extension_info, false);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

// Tests disabling an extension when prohibited by the ManagementPolicy.
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsDisable) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Attempt to disable it.
  service()->DisableExtension(good_crx, disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(disable_reason::DISABLE_NONE,
            ExtensionPrefs::Get(profile())->GetDisableReasons(good_crx));

  // Internal disable reasons are allowed.
  service()->DisableExtension(
      good_crx,
      disable_reason::DISABLE_CORRUPTED | disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(good_crx));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_CORRUPTED,
            ExtensionPrefs::Get(profile())->GetDisableReasons(good_crx));
}

// Tests uninstalling an extension when prohibited by the ManagementPolicy.
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsUninstall) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Attempt to uninstall it.
  EXPECT_FALSE(service()->UninstallExtension(
      good_crx, UNINSTALL_REASON_FOR_TESTING, nullptr));

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
}

// Tests that previously installed extensions that are now prohibited from
// being installed are disabled.
TEST_F(ExtensionServiceTest, ManagementPolicyUnloadsAllProhibited) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("page_action.crx"), INSTALL_NEW);
  EXPECT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Run the policy check.
  service()->CheckManagementPolicy();
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(2u, registry()->disabled_extensions().size());
  EXPECT_EQ(disable_reason::DISABLE_BLOCKED_BY_POLICY,
            prefs->GetDisableReasons(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_BLOCKED_BY_POLICY,
            prefs->GetDisableReasons(page_action));

  // Removing the extensions from policy blocklist should re-enable them.
  GetManagementPolicy()->UnregisterAllProviders();
  service()->CheckManagementPolicy();
  EXPECT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests that previously disabled extensions that are now required to be
// enabled are re-enabled on reinstall.
TEST_F(ExtensionServiceTest, ManagementPolicyRequiresEnable) {
  InitializeEmptyExtensionService();

  // Install, then disable, an extension.
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  service()->DisableExtension(good_crx, disable_reason::DISABLE_USER_ACTION);
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  // Register an ExtensionManagementPolicy that requires the extension to remain
  // enabled.
  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Reinstall the extension.
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_UPDATED);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests that extensions disabled by management policy can be installed but
// will get disabled after installing.
TEST_F(ExtensionServiceTest, ManagementPolicyProhibitsEnableOnInstalled) {
  InitializeEmptyExtensionService();

  // Register an ExtensionManagementPolicy that disables all extensions, with
  // a specified disable_reason::DisableReason.
  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_DISABLED);
  provider.SetDisableReason(disable_reason::DISABLE_NOT_VERIFIED);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Attempts to install an extensions, it should be installed but disabled.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_WITHOUT_LOAD);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  // Verifies that the disable reason is set properly.
  EXPECT_EQ(disable_reason::DISABLE_NOT_VERIFIED,
            service()->extension_prefs_->GetDisableReasons(kGoodId));
}

// Tests that extensions with conflicting required permissions by enterprise
// policy cannot be installed.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionNewExtensionInstall) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");

  {
    // Update policy to block one of the required permissions of target.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "tabs");
  }

  // The extension should be failed to install.
  PackAndInstallCRX(path, INSTALL_FAILED);

  {
    // Update policy to block one of the optional permissions instead.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission("*", "history");
  }

  // The extension should succeed to install this time.
  std::string id = PackAndInstallCRX(path, INSTALL_NEW)->id();

  // Uninstall the extension and update policy to block some arbitrary
  // unknown permission.
  UninstallExtension(id);
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.ClearBlockedPermissions("*");
    pref.AddBlockedPermission("*", "unknown.permission.for.testing");
  }

  // The extension should succeed to install as well.
  PackAndInstallCRX(path, INSTALL_NEW);
}

// Tests that extension supposed to be force installed but with conflicting
// required permissions cannot be installed.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionConflictsWithForceInstall) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Pack the crx file.
  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");
  base::FilePath pem_path = data_dir().AppendASCII("permissions_blocklist.pem");
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath crx_path = temp_dir.GetPath().AppendASCII("temp.crx");

  PackCRX(path, pem_path, crx_path);

  {
    // Block one of the required permissions.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "tabs");
  }

  // Use MockExternalProvider to simulate force installing extension.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
  provider->UpdateOrAddExtension(permissions_blocklist, "1.0", crx_path);

  // Attempts to force install this extension.
  WaitForInstallationAttemptToComplete(permissions_blocklist);

  // The extension should not be installed.
  ASSERT_FALSE(registry()->GetInstalledExtension(permissions_blocklist));

  // Remove this extension from pending extension manager as we would like to
  // give another attempt later.
  service()->pending_extension_manager()->Remove(permissions_blocklist);

  {
    // Clears the permission block list.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.ClearBlockedPermissions("*");
  }

  // Attempts to force install this extension again.
  WaitForInstallationAttemptToComplete(permissions_blocklist);

  const Extension* installed =
      registry()->GetInstalledExtension(permissions_blocklist);
  ASSERT_TRUE(installed);
  EXPECT_EQ(installed->location(), ManifestLocation::kExternalPolicyDownload);
}

// Tests that newer versions of an extension with conflicting required
// permissions by enterprise policy cannot be updated to.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionExtensionUpdate) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");
  base::FilePath path2 = data_dir().AppendASCII("permissions_blocklist2");
  base::FilePath pem_path = data_dir().AppendASCII("permissions_blocklist.pem");

  // Install 'permissions_blocklist'.
  const Extension* installed = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  EXPECT_EQ(installed->id(), permissions_blocklist);

  {
    // Block one of the required permissions of 'permissions_blocklist2'.
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "downloads");
  }

  // Install 'permissions_blocklist' again, should be updated.
  const Extension* updated = PackAndInstallCRX(path, pem_path, INSTALL_UPDATED);
  EXPECT_EQ(updated->id(), permissions_blocklist);

  std::string old_version = updated->VersionString();

  // Attempts to update to 'permissions_blocklist2' should fail.
  PackAndInstallCRX(path2, pem_path, INSTALL_FAILED);

  // Verify that the old version is still enabled.
  updated = registry()->enabled_extensions().GetByID(permissions_blocklist);
  ASSERT_TRUE(updated);
  EXPECT_EQ(old_version, updated->VersionString());
}

// Tests that policy update with additional permissions blocked revoke
// conflicting granted optional permissions and unload extensions with
// conflicting required permissions, including the force installed ones.
TEST_F(ExtensionServiceTest, PolicyBlockedPermissionPolicyUpdate) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  base::FilePath path = data_dir().AppendASCII("permissions_blocklist");
  base::FilePath path2 = data_dir().AppendASCII("permissions_blocklist2");
  base::FilePath pem_path = data_dir().AppendASCII("permissions_blocklist.pem");

  // Pack the crx file.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath crx_path = temp_dir.GetPath().AppendASCII("temp.crx");

  PackCRX(path2, pem_path, crx_path);

  // Install two arbitary extensions with specified manifest.
  std::string ext1 = PackAndInstallCRX(path, INSTALL_NEW)->id();
  std::string ext2 = PackAndInstallCRX(path2, INSTALL_NEW)->id();
  ASSERT_NE(ext1, permissions_blocklist);
  ASSERT_NE(ext2, permissions_blocklist);
  ASSERT_NE(ext1, ext2);

  // Force install another extension with known id and same manifest as 'ext2'.
  std::string ext2_forced = permissions_blocklist;
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
  provider->UpdateOrAddExtension(ext2_forced, "2.0", crx_path);

  WaitForExternalExtensionInstalled(ext2_forced);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  // Verify all three extensions are installed and enabled.
  ASSERT_TRUE(registry->enabled_extensions().GetByID(ext1));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(ext2));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(ext2_forced));

  // Grant all optional permissions to each extension.
  GrantAllOptionalPermissions(ext1);
  GrantAllOptionalPermissions(ext2);
  GrantAllOptionalPermissions(ext2_forced);

  std::unique_ptr<const PermissionSet> active_permissions =
      ExtensionPrefs::Get(profile())->GetDesiredActivePermissions(ext1);
  EXPECT_TRUE(
      active_permissions->HasAPIPermission(APIPermissionID::kDownloads));

  // Set policy to block 'downloads' permission.
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    pref.AddBlockedPermission("*", "downloads");
  }

  task_environment()->RunUntilIdle();

  // 'ext1' should still be enabled, but with 'downloads' permission revoked.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(ext1));
  active_permissions =
      ExtensionPrefs::Get(profile())->GetDesiredActivePermissions(ext1);
  EXPECT_FALSE(
      active_permissions->HasAPIPermission(APIPermissionID::kDownloads));

  // 'ext2' should be disabled because one of its required permissions is
  // blocked.
  EXPECT_FALSE(registry->enabled_extensions().GetByID(ext2));

  // 'ext2_forced' should be handled the same as 'ext2'
  EXPECT_FALSE(registry->enabled_extensions().GetByID(ext2_forced));
}

// Flaky on windows; http://crbug.com/309833
#if BUILDFLAG(IS_WIN)
#define MAYBE_ExternalExtensionAutoAcknowledgement DISABLED_ExternalExtensionAutoAcknowledgement
#else
#define MAYBE_ExternalExtensionAutoAcknowledgement ExternalExtensionAutoAcknowledgement
#endif
TEST_F(ExtensionServiceTest, MAYBE_ExternalExtensionAutoAcknowledgement) {
  InitializeEmptyExtensionService();

  {
    // Register and install an external extension.
    MockExternalProvider* provider =
        AddMockExternalProvider(ManifestLocation::kExternalPref);
    provider->UpdateOrAddExtension(
        good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"));
  }
  {
    // Have policy force-install an extension.
    MockExternalProvider* provider =
        AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
    provider->UpdateOrAddExtension(
        page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));
  }

  // Providers are set up. Let them run.
  {
    ExtensionLoadedObserver good_crx_loaded(registry(), good_crx);
    ExtensionLoadedObserver page_action_loaded(registry(), page_action);
    service()->CheckForExternalUpdates();
    good_crx_loaded.Wait();
    page_action_loaded.Wait();
  }

  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(page_action));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(!prefs->IsExternalExtensionAcknowledged(good_crx));
  ASSERT_TRUE(prefs->IsExternalExtensionAcknowledged(page_action));
}

// Tests that an extension added through an external source is initially
// disabled with the "prompt for external extensions" feature.
TEST_F(ExtensionServiceTest, ExternalExtensionDisabledOnInstallation) {
  FeatureSwitch::ScopedOverride external_prompt_override(
      FeatureSwitch::prompt_for_external_extensions(), true);
  InitializeEmptyExtensionService();

  // Register and install an external extension.
  MockExternalProvider* provider = AddMockExternalProvider(
      ManifestLocation::kExternalPref);  // Takes ownership.
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                 data_dir().AppendASCII("good.crx"));

  WaitForInstallationAttemptToComplete(good_crx);

  EXPECT_TRUE(registry()->disabled_extensions().Contains(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(good_crx));

  // Updating the extension shouldn't cause it to be enabled.
  provider->UpdateOrAddExtension(good_crx, "1.0.0.1",
                                 data_dir().AppendASCII("good2.crx"));
  WaitForInstallationAttemptToComplete(good_crx);

  EXPECT_TRUE(registry()->disabled_extensions().Contains(good_crx));
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(good_crx));
  const Extension* extension =
      registry()->disabled_extensions().GetByID(good_crx);
  ASSERT_TRUE(extension);
  // Double check that we did, in fact, update the extension.
  EXPECT_EQ("1.0.0.1", extension->version().GetString());
}

// Test that if an extension is installed before the "prompt for external
// extensions" feature is enabled, but is updated when the feature is
// enabled, the extension is not disabled.
TEST_F(ExtensionServiceTest, ExternalExtensionIsNotDisabledOnUpdate) {
  auto external_prompt_override =
      std::make_unique<FeatureSwitch::ScopedOverride>(
          FeatureSwitch::prompt_for_external_extensions(), false);
  InitializeEmptyExtensionService();

  // Register and install an external extension.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                 data_dir().AppendASCII("good.crx"));

  WaitForExternalExtensionInstalled(good_crx);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));

  provider->UpdateOrAddExtension(good_crx, "1.0.0.1",
                                 data_dir().AppendASCII("good2.crx"));

  // We explicitly reset the override first. ScopedOverrides reset the value
  // to the original value on destruction, but if we reset by passing a new
  // object, the new object is constructed (overriding the current value)
  // before the old is destructed (which will immediately reset to the
  // original).
  external_prompt_override.reset();
  external_prompt_override = std::make_unique<FeatureSwitch::ScopedOverride>(
      FeatureSwitch::prompt_for_external_extensions(), true);
  WaitForExternalExtensionInstalled(good_crx);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good_crx));
  {
    const Extension* extension =
        registry()->enabled_extensions().GetByID(good_crx);
    ASSERT_TRUE(extension);
    EXPECT_EQ("1.0.0.1", extension->version().GetString());
  }
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));
}

// Test that if an external extension warning is ignored three times, the
// extension no longer prompts
TEST_F(ExtensionServiceTest, ExternalExtensionRemainsDisabledIfIgnored) {
  FeatureSwitch::ScopedOverride prompt_override(
      FeatureSwitch::prompt_for_external_extensions(), true);
  InitializeEmptyExtensionService();

  // Register and install an external extension.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                 data_dir().AppendASCII("good.crx"));

  WaitForInstallationAttemptToComplete(good_crx);

  EXPECT_TRUE(registry()->disabled_extensions().Contains(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(good_crx));

  ExternalInstallManager* external_install_manager =
      service()->external_install_manager();

  for (int i = 0; i < 3; ++i) {
    std::vector<ExternalInstallError*> errors =
        external_install_manager->GetErrorsForTesting();
    ASSERT_EQ(1u, errors.size());
    errors[0]->OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
        ExtensionInstallPrompt::Result::ABORTED));
    base::RunLoop().RunUntilIdle();
    // Note: Calling OnInstallPromptDone() can result in the removal of the
    // error by the manager (which owns the object), so the contents |errors|
    // are invalidated now!
    EXPECT_TRUE(external_install_manager->GetErrorsForTesting().empty());
    external_install_manager->ClearShownIdsForTesting();
    external_install_manager->UpdateExternalExtensionAlert();
  }

  // We should have stopped prompting, since the user was shown the warning
  // three times.
  EXPECT_TRUE(external_install_manager->GetErrorsForTesting().empty());
  EXPECT_TRUE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(good_crx));

  // The extension should remain disabled.
  service()->ReloadExtensionsForTest();
  EXPECT_TRUE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(good_crx));

  // Then re-enabling the extension (or otherwise causing the alert to be
  // updated again) should work. Regression test for https://crbug.com/736292.
  {
    TestExtensionRegistryObserver registry_observer(registry());
    service()->EnableExtension(good_crx);
    registry_observer.WaitForExtensionLoaded();
    base::RunLoop().RunUntilIdle();
  }
}

// Test that if an external extension becomes force-installed, it's enabled
// (even if the user hasn't acknowledged the prompt).
TEST_F(ExtensionServiceTest, ExternalExtensionBecomesEnabledIfForceInstalled) {
  FeatureSwitch::ScopedOverride prompt_override(
      FeatureSwitch::prompt_for_external_extensions(), true);
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Initially, the extension is installed externally and is disabled.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                 data_dir().AppendASCII("good.crx"));
  WaitForInstallationAttemptToComplete(good_crx);

  EXPECT_TRUE(registry()->disabled_extensions().Contains(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(good_crx));

  // Make the extension force-installed now. It should flip from disabled to
  // enabled.
  TestManagementPolicyProvider policy_provider(
      TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  GetManagementPolicy()->RegisterProvider(&policy_provider);
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good_crx));
  EXPECT_TRUE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// This tests if pre-installed apps are installed correctly.
TEST_F(ExtensionServiceTest, PreinstalledAppsInstall) {
  InitializeEmptyExtensionService();

  {
    // Initializing the ExtensionService will have set the pre-installed app
    // state; reset it for the sake of testing.
    profile()->GetPrefs()->SetInteger(prefs::kPreinstalledAppsInstallState,
                                      preinstalled_apps::kUnknown);
    std::string json_data =
        "{"
        "  \"ldnnhddmnhbkjipkidpdiheffobcpfmf\" : {"
        "    \"external_crx\": \"good.crx\","
        "    \"external_version\": \"1.0.0.0\","
        "    \"is_bookmark_app\": false"
        "  }"
        "}";
    preinstalled_apps::Provider* provider = new preinstalled_apps::Provider(
        profile(), service(), new ExternalTestingLoader(json_data, data_dir()),
        ManifestLocation::kInternal, ManifestLocation::kInvalidLocation,
        Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT);

    service()->AddProviderForTesting(base::WrapUnique(provider));
  }

  ASSERT_EQ(0u, registry()->enabled_extensions().size());
  WaitForExternalExtensionInstalled(good_crx);

  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
  const Extension* extension =
      registry()->enabled_extensions().GetByID(good_crx);
  EXPECT_TRUE(extension->from_webstore());
  EXPECT_TRUE(extension->was_installed_by_default());
}
#endif

// Tests disabling extensions
TEST_F(ExtensionServiceTest, DisableExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());

  // Disable it.
  service()->DisableExtension(good_crx, disable_reason::DISABLE_USER_ACTION);

  EXPECT_TRUE(registry()->disabled_extensions().GetByID(good_crx));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());
}

// Tests the malware Omaha attributes to remotely disable an extension for
// malware.
TEST_F(ExtensionServiceTest, DisableRemotelyForMalware) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  auto attributes = base::Value::Dict().Set("_malware", true);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  service()->PerformActionBasedOnOmahaAttributes(good_crx, attributes);
  EXPECT_TRUE(blocklist_prefs::HasOmahaBlocklistState(
      good_crx, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good_crx, prefs));

  attributes.Set("_malware", false);
  service()->PerformActionBasedOnOmahaAttributes(good_crx, attributes);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0, prefs->GetDisableReasons(good_crx));
  EXPECT_FALSE(blocklist_prefs::IsExtensionBlocklisted(good_crx, prefs));
}

// Tests not re-enabling previously remotely disabled extension if it's not the
// only reason but the disable reasons should be gone.
TEST_F(ExtensionServiceTest, NoEnableRemotelyDisabledExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));

  auto attributes = base::Value::Dict().Set("_malware", true);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  service()->DisableExtension(good_crx, disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(good_crx));
  service()->PerformActionBasedOnOmahaAttributes(good_crx, attributes);
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good_crx, prefs));

  attributes.Set("_malware", false);
  service()->PerformActionBasedOnOmahaAttributes(good_crx, attributes);
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(good_crx));
  EXPECT_FALSE(blocklist_prefs::HasOmahaBlocklistState(
      good_crx, BitMapBlocklistState::BLOCKLISTED_MALWARE, prefs));
  EXPECT_FALSE(blocklist_prefs::IsExtensionBlocklisted(good_crx, prefs));
}

TEST_F(ExtensionServiceTest, CanAddDisableReasonToBlocklistedExtension) {
  InitializeGoodInstalledExtensionService();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  TestBlocklist blocklist;

  blocklist.Attach(service()->blocklist_);
  service()->Init();

  blocklist.SetBlocklistState(good0, BLOCKLISTED_MALWARE, true);
  blocklist.SetBlocklistState(good1, BLOCKLISTED_MALWARE, true);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good0, prefs));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good1, prefs));

  // Test that a blocklisted extension can be disabled.
  service()->DisableExtension(good1, disable_reason::DISABLE_BLOCKED_BY_POLICY);
  EXPECT_TRUE(prefs->HasDisableReason(
      good1, disable_reason::DISABLE_BLOCKED_BY_POLICY));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good1, prefs));
  // Even though the extension was disabled with a new disable reason, it should
  // remain in the blocklisted set (which can't be re-enabled by the user).
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good1));
  // Since the extension is blocklisted, it should not be in the disabled set.
  EXPECT_FALSE(registry()->disabled_extensions().Contains(good1));

  // Extensions should remain in the appropriate sets after being reloaded (as
  // in a profile restart).
  service()->ReloadExtensionsForTest();
  EXPECT_TRUE(prefs->HasDisableReason(
      good1, disable_reason::DISABLE_BLOCKED_BY_POLICY));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good1, prefs));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(good1));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(good1));

  // Test that the extension is disabled when unblocklisted.
  blocklist.SetBlocklistState(good1, NOT_BLOCKLISTED, true);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(blocklist_prefs::IsExtensionBlocklisted(good1, prefs));
  EXPECT_TRUE(prefs->IsExtensionDisabled(good1));
  EXPECT_FALSE(registry()->blocklisted_extensions().Contains(good1));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(good1));
  EXPECT_TRUE(prefs->HasDisableReason(
      good1, disable_reason::DISABLE_BLOCKED_BY_POLICY));
}

// Tests the Extension Telemetry service verdict to remotely disable an
// extension for malware.
TEST_F(ExtensionServiceTest,
       DisableRemotelyForMalwareFromExtensionTelemetryServiceVerdict) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  Blocklist::BlocklistStateMap state_map;
  state_map[good_crx] = BlocklistState::BLOCKLISTED_MALWARE;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
                good_crx, prefs),
            BitMapBlocklistState::BLOCKLISTED_MALWARE);
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(good_crx, prefs));

  state_map[good_crx] = BlocklistState::NOT_BLOCKLISTED;
  service()->PerformActionBasedOnExtensionTelemetryServiceVerdicts(state_map);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0, prefs->GetDisableReasons(good_crx));
  EXPECT_FALSE(blocklist_prefs::IsExtensionBlocklisted(good_crx, prefs));
}

TEST_F(ExtensionServiceTest,
       DisableAndReenableUnpackedExtensionBasedOnDeveloperMode) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kExtensionDisableUnsupportedDeveloper);
  InitializeEmptyExtensionService();
  // Turn developer mode ON.
  util::SetDeveloperModeForProfile(profile(), true);

  // Load an unpacked extension and verify enablement.
  scoped_refptr<const Extension> unpacked_crx =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          data_dir().AppendASCII("simple_with_file"));
  EXPECT_EQ(ManifestLocation::kUnpacked, unpacked_crx->location());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(unpacked_crx->id()));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(unpacked_crx->id()));

  // Turn developer mode OFF. Verify that the unpacked extension is disabled.
  util::SetDeveloperModeForProfile(profile(), false);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(unpacked_crx->id()));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(unpacked_crx->id()));
  EXPECT_EQ(
      disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION,
      ExtensionPrefs::Get(profile())->GetDisableReasons(unpacked_crx->id()));

  // Turn developer mode ON. The unpacked extension should now be enabled.
  util::SetDeveloperModeForProfile(profile(), true);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(unpacked_crx->id()));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(unpacked_crx->id()));
}

TEST_F(ExtensionServiceTest,
       UnpackedExtensionStatusUnaffectedWhenFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kExtensionDisableUnsupportedDeveloper);
  InitializeEmptyExtensionService();
  // Turn developer mode ON.
  util::SetDeveloperModeForProfile(profile(), true);

  // Load an unpacked extension and verify enablement.
  scoped_refptr<const Extension> unpacked_crx =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          data_dir().AppendASCII("simple_with_file"));
  EXPECT_EQ(ManifestLocation::kUnpacked, unpacked_crx->location());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(unpacked_crx->id()));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(unpacked_crx->id()));

  // Turn developer mode OFF. Verify that the unpacked extension is still
  // enabled.
  util::SetDeveloperModeForProfile(profile(), false);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(unpacked_crx->id()));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(unpacked_crx->id()));
}

TEST_F(ExtensionServiceTest, TerminateExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());

  TerminateExtension(good_crx);

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(1u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());
}

TEST_F(ExtensionServiceTest, DisableTerminatedExtension) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  EXPECT_TRUE(registry()->terminated_extensions().GetByID(good_crx));

  // Disable it.
  service()->DisableExtension(good_crx, disable_reason::DISABLE_USER_ACTION);

  EXPECT_FALSE(registry()->terminated_extensions().GetByID(good_crx));
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(good_crx));

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());
}

// Tests that with the kDisableExtensions flag, extensions are not loaded by
// the ExtensionService...
TEST_F(ExtensionServiceTest, PRE_DisableAllExtensions) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kDisableExtensions);
  InitializeGoodInstalledExtensionService();
  service()->Init();
  EXPECT_TRUE(registry()->GenerateInstalledExtensionsSet().empty());
}

// ... But, if we remove the switch, they are.
TEST_F(ExtensionServiceTest, DisableAllExtensions) {
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kDisableExtensions));
  InitializeGoodInstalledExtensionService();
  service()->Init();
  EXPECT_FALSE(registry()->GenerateInstalledExtensionsSet().empty());
  EXPECT_FALSE(registry()->enabled_extensions().empty());
}

// Tests reloading extensions.
TEST_F(ExtensionServiceTest, ReloadExtensions) {
  InitializeEmptyExtensionService();

  // Simple extension that should install without error.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW,
             Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT);
  const char* const extension_id = good_crx;
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  service()->ReloadExtensionsForTest();

  // The creation flags should not change when reloading the extension.
  const Extension* extension =
      registry()->disabled_extensions().GetByID(good_crx);
  EXPECT_TRUE(extension->from_webstore());
  EXPECT_TRUE(extension->was_installed_by_default());

  // Extension counts shouldn't change.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());

  service()->EnableExtension(extension_id);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  // Need to clear |loaded_| manually before reloading as the
  // EnableExtension() call above inserted into it and
  // UnloadAllExtensions() doesn't send out notifications.
  ClearLoadedExtensions();
  service()->ReloadExtensionsForTest();

  // Extension counts shouldn't change.
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// Tests reloading an extension.
TEST_F(ExtensionServiceTest, ReloadExtension) {
  InitializeEmptyExtensionService();

  // Simple extension that should install without error.
  const char extension_id[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
  base::FilePath ext = data_dir()
                           .AppendASCII("good")
                           .AppendASCII("Extensions")
                           .AppendASCII(extension_id)
                           .AppendASCII("1.0.0.0");
  UnpackedInstaller::Create(service())->Load(ext);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());

  service()->ReloadExtension(extension_id);

  // Extension should be disabled now, waiting to be reloaded.
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(disable_reason::DISABLE_RELOAD,
            ExtensionPrefs::Get(profile())->GetDisableReasons(extension_id));

  // Reloading again should not crash.
  service()->ReloadExtension(extension_id);

  // Finish reloading
  task_environment()->RunUntilIdle();

  // Extension should be enabled again.
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
}

// TODO(jlulejian): Reuse this in other places in this file.
// Test class that sets up an empty extension service before the test starts.
class ExtensionServiceWithEmptyServiceTest : public ExtensionServiceTest {
 public:
  void SetUp() override {
    ExtensionServiceTest::SetUp();
    InitializeEmptyExtensionService();
  }
};

TEST_F(ExtensionServiceWithEmptyServiceTest, UninstallExtensionFromWebstore) {
  const Extension* extension =
      InstallCRXFromWebStore(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  UninstallExtension(good_crx);
  EXPECT_TRUE(registry()->enabled_extensions().empty());
  EXPECT_EQ(UnloadedExtensionReason::UNINSTALL, unloaded_reason());
}

TEST_F(ExtensionServiceWithEmptyServiceTest, UninstallExtensionFromCrx) {
  const Extension* extension =
      InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  UninstallExtension(good_crx);
  EXPECT_TRUE(registry()->enabled_extensions().empty());
  EXPECT_EQ(UnloadedExtensionReason::UNINSTALL, unloaded_reason());
}

TEST_F(ExtensionServiceWithEmptyServiceTest,
       UninstallExtensionFromUnpackedFolder_DoNotDeleteExtensionFolder) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Good Extension",
           "version": "0.1",
           "manifest_version": 3
         })");

  ChromeTestExtensionLoader loader(testing_profile());
  loader.set_pack_extension(false);
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  UninstallExtension(extension->id(), /*delete_type=*/kDoNotDelete);
  EXPECT_TRUE(registry()->enabled_extensions().empty());
  EXPECT_EQ(UnloadedExtensionReason::UNINSTALL, unloaded_reason());
}

// Test uninstall to profile dir for .zip file installs.
class ExtensionServiceZipUninstallProfileFeatureTest
    : public ExtensionServiceWithEmptyServiceTest {
 public:
  void SetUp() override {
    ExtensionServiceWithEmptyServiceTest::SetUp();
      expected_extension_install_directory_ =
          service()->unpacked_install_directory();
  }

 protected:
  base::FilePath expected_extension_install_directory_;
};

TEST_F(ExtensionServiceZipUninstallProfileFeatureTest,
       UninstallExtensionFromZip) {
  MockExtensionRegistryObserver observer;

  // Install the extension from .zip.
  base::FilePath original_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &original_path));
  original_path = original_path.AppendASCII("extensions")
                      .AppendASCII("zipfile_installer")
                      .AppendASCII("good.zip");
  ASSERT_TRUE(base::PathExists(original_path)) << original_path.value();
  scoped_refptr<ZipFileInstaller> zipfile_installer = ZipFileInstaller::Create(
      GetExtensionFileTaskRunner(),
      MakeRegisterInExtensionServiceCallback(service()));

  registry()->AddObserver(&observer);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ZipFileInstaller::InstallZipFileToUnpackedExtensionsDir,
                       zipfile_installer, original_path,
                       service()->unpacked_install_directory()));
  task_environment()->RunUntilIdle();

  std::string extension_id = std::string(observer.last_extension_installed);
  EXPECT_EQ(observer.last_extension_installed, extension_id);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  UninstallExtension(extension_id, /*delete_type=*/kDeletePath);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(
      observer.last_extension_installed));
  EXPECT_TRUE(registry()->enabled_extensions().empty());
  EXPECT_EQ(observer.last_extension_uninstalled, extension_id);
  EXPECT_EQ(UnloadedExtensionReason::UNINSTALL, unloaded_reason());
  registry()->RemoveObserver(&observer);
}

TEST_F(ExtensionServiceWithEmptyServiceTest, UninstallTerminatedExtension) {
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  UninstallExtension(good_crx);
  EXPECT_EQ(UnloadedExtensionReason::TERMINATE, unloaded_reason());
}

TEST_F(ExtensionServiceWithEmptyServiceTest, UninstallBlockedExtension) {
  MockExtensionRegistryObserver observer;
  registry()->AddObserver(&observer);

  ASSERT_TRUE(observer.last_extension_installed.empty());
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  ASSERT_EQ(good_crx, observer.last_extension_installed);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  BlockAllExtensions();
  EXPECT_EQ(UnloadedExtensionReason::LOCK_ALL, unloaded_reason());
  EXPECT_EQ(1u, registry()->blocked_extensions().size());

  ASSERT_TRUE(observer.last_extension_uninstalled.empty());
  UninstallExtension(good_crx);
  ASSERT_EQ(good_crx, observer.last_extension_uninstalled);
  EXPECT_EQ(0u, registry()->blocked_extensions().size());

  registry()->RemoveObserver(&observer);
}

// An extension disabled because of unsupported requirements should re-enabled
// if updated to a version with supported requirements as long as there are no
// other disable reasons.
TEST_F(ExtensionServiceTest, UpgradingRequirementsEnabled) {
  InitializeEmptyExtensionService();
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();

  base::FilePath path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path =
      data_dir().AppendASCII("requirements").AppendASCII("v1_good.pem");
  const Extension* extension_v1 = PackAndInstallCRX(path.AppendASCII("v1_good"),
                                                    pem_path,
                                                    INSTALL_NEW);
  std::string id = extension_v1->id();
  EXPECT_TRUE(service()->IsExtensionEnabled(id));

  base::FilePath v2_bad_requirements_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v2_bad_requirements"),
          pem_path,
          v2_bad_requirements_crx);
  UpdateExtension(id, v2_bad_requirements_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v3_good_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v3_good"), pem_path, v3_good_crx);
  UpdateExtension(id, v3_good_crx, ENABLED);
  EXPECT_TRUE(service()->IsExtensionEnabled(id));
}

// Extensions disabled through user action should stay disabled.
TEST_F(ExtensionServiceTest, UpgradingRequirementsDisabled) {
  InitializeEmptyExtensionService();
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();

  base::FilePath path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path =
      data_dir().AppendASCII("requirements").AppendASCII("v1_good.pem");
  const Extension* extension_v1 = PackAndInstallCRX(path.AppendASCII("v1_good"),
                                                    pem_path,
                                                    INSTALL_NEW);
  std::string id = extension_v1->id();
  service()->DisableExtension(id, disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v2_bad_requirements_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v2_bad_requirements"),
          pem_path,
          v2_bad_requirements_crx);
  UpdateExtension(id, v2_bad_requirements_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v3_good_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v3_good"), pem_path, v3_good_crx);
  UpdateExtension(id, v3_good_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));
}

// The extension should not re-enabled because it was disabled from a
// permission increase.
TEST_F(ExtensionServiceTest, UpgradingRequirementsPermissions) {
  InitializeEmptyExtensionService();
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();

  base::FilePath path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path =
      data_dir().AppendASCII("requirements").AppendASCII("v1_good.pem");
  const Extension* extension_v1 = PackAndInstallCRX(path.AppendASCII("v1_good"),
                                                    pem_path,
                                                    INSTALL_NEW);
  std::string id = extension_v1->id();
  EXPECT_TRUE(service()->IsExtensionEnabled(id));

  base::FilePath v2_bad_requirements_and_permissions_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v2_bad_requirements_and_permissions"),
          pem_path,
          v2_bad_requirements_and_permissions_crx);
  UpdateExtension(id, v2_bad_requirements_and_permissions_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));

  base::FilePath v3_bad_permissions_crx = GetTemporaryFile();

  PackCRX(path.AppendASCII("v3_bad_permissions"),
          pem_path,
          v3_bad_permissions_crx);
  UpdateExtension(id, v3_bad_permissions_crx, INSTALLED);
  EXPECT_FALSE(service()->IsExtensionEnabled(id));
}

// Unpacked extensions are not allowed to be installed if they have unsupported
// requirements.
TEST_F(ExtensionServiceTest, UnpackedRequirements) {
  InitializeEmptyExtensionService();
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();

  base::FilePath path =
      data_dir().AppendASCII("requirements").AppendASCII("v2_bad_requirements");
  UnpackedInstaller::Create(service())->Load(path);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
}

class ExtensionCookieCallback {
 public:
  ExtensionCookieCallback() = default;

  void SetCookieCallback(net::CookieAccessResult result) {
    result_ = result.status.IsInclude();
  }

  void GetAllCookiesCallback(const net::CookieAccessResultList& list,
                             const net::CookieAccessResultList& excluded_list) {
    list_ = net::cookie_util::StripAccessResults(list);
  }
  net::CookieList list_;
  bool result_ = false;
};

// Verifies extension state is removed upon uninstall.
TEST_F(ExtensionServiceTest, ClearExtensionData) {
  InitializeEmptyExtensionService();
  ExtensionCookieCallback callback;

  // Load a test extension.
  base::FilePath path = data_dir();
  path = path.AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  GURL ext_url(extension->url());
  std::string origin_id = storage::GetIdentifierFromOrigin(ext_url);

  // Set a cookie for the extension.
  net::CookieStore* cookie_store =
      extensions::ChromeExtensionCookies::Get(profile())
          ->GetCookieStoreForTesting();
  ASSERT_TRUE(cookie_store);
  auto cookie = net::CanonicalCookie::CreateForTesting(ext_url, "dummy=value",
                                                       base::Time::Now());
  cookie_store->SetCanonicalCookieAsync(
      std::move(cookie), ext_url, net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&ExtensionCookieCallback::SetCookieCallback,
                     base::Unretained(&callback)));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(callback.result_);

  cookie_store->GetCookieListWithOptionsAsync(
      ext_url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&ExtensionCookieCallback::GetAllCookiesCallback,
                     base::Unretained(&callback)));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1U, callback.list_.size());

  // Create local storage.
  auto* local_storage_control =
      profile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  mojo::Remote<blink::mojom::StorageArea> area;
  local_storage_control->BindStorageArea(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(ext_url)),
      area.BindNewPipeAndPassReceiver());
  {
    base::test::TestFuture<bool> future;
    area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, std::nullopt,
              "source", future.GetCallback());
    ASSERT_TRUE(future.Get());
  }

  // Create indexed db. It is enough to only simulate this by
  // creating the directory on the disk, and resetting the caches of
  // "known" origins.
  auto& idb_control =
      profile()->GetDefaultStoragePartition()->GetIndexedDBControl();
  mojo::Remote<storage::mojom::IndexedDBControlTest> idb_control_test;
  idb_control.BindTestInterface(idb_control_test.BindNewPipeAndPassReceiver());

  base::FilePath idb_path;
  {
    ASSERT_OK_AND_ASSIGN(auto bucket_locator,
                         GetStorageBucket(blink::StorageKey::CreateFirstParty(
                             url::Origin::Create(ext_url))));
    base::RunLoop run_loop;
    idb_control_test->GetFilePathForTesting(
        bucket_locator,
        base::BindLambdaForTesting([&](const base::FilePath& path) {
          idb_path = path;
          EXPECT_TRUE(base::CreateDirectory(idb_path));
          EXPECT_TRUE(base::DirectoryExists(idb_path));
          idb_control_test->ResetCachesForTesting(run_loop.QuitClosure());
        }));
    run_loop.Run();
  }

  // Uninstall the extension.
  ASSERT_TRUE(service()->UninstallExtension(
      good_crx, UNINSTALL_REASON_FOR_TESTING, nullptr));
  // The data deletion happens on the IO thread; since we use a
  // BrowserTaskEnvironment (without REAL_IO_THREAD), the IO and UI threads are
  // the same, and RunAllTasksUntilIdle() should run IO thread tasks.
  task_environment()->RunUntilIdle();

  // Check that the cookie is gone.
  cookie_store->GetCookieListWithOptionsAsync(
      ext_url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&ExtensionCookieCallback::GetAllCookiesCallback,
                     base::Unretained(&callback)));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0U, callback.list_.size());


  // Check that the localStorage data been removed.
  {
    base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
        future;
    local_storage_control->GetUsage(future.GetCallback());
    EXPECT_TRUE(future.Get().empty());
  }

  // Check if the indexed db has disappeared too.
  EXPECT_FALSE(base::DirectoryExists(idb_path));
}

std::vector<net::CanonicalCookie> IncludedCookies(
    const net::CookieAccessResultList& result,
    const net::CookieAccessResultList& excluded) {
  return net::cookie_util::StripAccessResults(result);
}

// Verifies app state is removed upon uninstall.
TEST_F(ExtensionServiceTest, ClearAppData) {
  InitializeEmptyExtensionService();
  ExtensionCookieCallback callback;

  int pref_count = 0;

  // Install app1 with unlimited storage.
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  const std::string id1 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kUnlimitedStorage));
  const GURL origin1(
      AppLaunchInfo::GetFullLaunchURL(extension).DeprecatedGetOriginAsURL());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));
  std::string origin_id = storage::GetIdentifierFromOrigin(origin1);

  // Install app2 from the same origin with unlimited storage.
  extension = PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  ValidatePrefKeyCount(++pref_count);
  ASSERT_EQ(2u, registry()->enabled_extensions().size());
  const std::string id2 = extension->id();
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kUnlimitedStorage));
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      AppLaunchInfo::GetFullLaunchURL(extension)));
  const GURL origin2(
      AppLaunchInfo::GetFullLaunchURL(extension).DeprecatedGetOriginAsURL());
  EXPECT_EQ(origin1, origin2);
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin2));

  network::mojom::NetworkContext* network_context =
      profile()->GetDefaultStoragePartition()->GetNetworkContext();
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote;
  network_context->GetCookieManager(
      cookie_manager_remote.BindNewPipeAndPassReceiver());

  std::unique_ptr<net::CanonicalCookie> cc(
      net::CanonicalCookie::CreateForTesting(origin1, "dummy=value",
                                             base::Time::Now()));
  ASSERT_TRUE(cc.get());

  {
    base::test::TestFuture<bool> future;
    cookie_manager_remote->SetCanonicalCookie(
        *cc.get(), origin1, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce([](net::CookieAccessResult result) {
          return result.status.IsInclude();
        }).Then(future.GetCallback()));
    EXPECT_TRUE(future.Get());
  }

  {
    base::test::TestFuture<std::vector<net::CanonicalCookie>> future;
    cookie_manager_remote->GetCookieList(
        origin1, net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection(),
        base::BindOnce(IncludedCookies).Then(future.GetCallback()));
    EXPECT_EQ(1U, future.Get().size());
  }

  // Create local storage.
  auto* local_storage_control =
      profile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  mojo::Remote<blink::mojom::StorageArea> area;
  local_storage_control->BindStorageArea(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin1)),
      area.BindNewPipeAndPassReceiver());
  {
    base::test::TestFuture<bool> future;
    area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, std::nullopt,
              "source", future.GetCallback());
    ASSERT_TRUE(future.Get());
  }

  // Create indexed db. It is enough to only simulate this by
  // creating the directory on the disk, and resetting the caches of
  // "known" origins.
  auto& idb_control =
      profile()->GetDefaultStoragePartition()->GetIndexedDBControl();
  mojo::Remote<storage::mojom::IndexedDBControlTest> idb_control_test;
  idb_control.BindTestInterface(idb_control_test.BindNewPipeAndPassReceiver());

  base::FilePath idb_path;
  {
    ASSERT_OK_AND_ASSIGN(auto bucket_locator,
                         GetStorageBucket(blink::StorageKey::CreateFirstParty(
                             url::Origin::Create(origin1))));
    base::RunLoop run_loop;
    idb_control_test->GetFilePathForTesting(
        bucket_locator,
        base::BindLambdaForTesting([&](const base::FilePath& path) {
          idb_path = path;
          EXPECT_TRUE(base::CreateDirectory(idb_path));
          EXPECT_TRUE(base::DirectoryExists(idb_path));
          idb_control_test->ResetCachesForTesting(run_loop.QuitClosure());
        }));
    run_loop.Run();
  }

  // Uninstall one of them, unlimited storage should still be granted
  // to the origin.
  UninstallExtension(id1);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      origin1));

  {
    // Check that the cookie is still there.
    base::test::TestFuture<std::vector<net::CanonicalCookie>> future;
    cookie_manager_remote->GetCookieList(
        origin1, net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection(),
        base::BindOnce(IncludedCookies).Then(future.GetCallback()));
    EXPECT_EQ(1U, future.Get().size());
  }

  // Now uninstall the other. Storage should be cleared for the apps.
  UninstallExtension(id2);
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          origin1));

  {
    // Check that the cookie is gone.
    base::test::TestFuture<std::vector<net::CanonicalCookie>> future;
    cookie_manager_remote->GetCookieList(
        origin1, net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection(),
        base::BindOnce(IncludedCookies).Then(future.GetCallback()));
    EXPECT_EQ(0U, future.Get().size());
  }

  // Check that the localStorage data been removed.
  {
    base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
        future;
    local_storage_control->GetUsage(future.GetCallback());
    EXPECT_TRUE(future.Get().empty());
  }

  // Check if the indexed db has disappeared too.
  EXPECT_FALSE(base::DirectoryExists(idb_path));
}

// Tests loading single extensions (like --load-extension)
TEST_F(ExtensionServiceTest, LoadExtension) {
  InitializeEmptyExtensionService();
  TestExtensionDir good_extension_dir;
  good_extension_dir.WriteManifest(
      R"({
           "name": "Good Extension",
           "version": "0.1",
           "manifest_version": 3
         })");

  {
    ChromeTestExtensionLoader loader(profile());
    loader.set_pack_extension(false);
    loader.LoadExtension(good_extension_dir.UnpackedPath());
  }
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  ValidatePrefKeyCount(1);

  auto get_extension_by_name = [](const ExtensionSet& extensions,
                                  const std::string& name) {
    // NOTE: lambda type deduction doesn't recognize returning
    // const Extension* in one place and nullptr in another as the same type, so
    // we have to make sure to return an explicit type here.
    const Extension* result = nullptr;
    for (const auto& extension : extensions) {
      if (extension->name() == name) {
        result = extension.get();
        break;
      }
    }
    return result;
  };
  constexpr const char kGoodExtension[] = "Good Extension";
  {
    const Extension* extension =
        get_extension_by_name(registry()->enabled_extensions(), kGoodExtension);
    ASSERT_TRUE(extension);
    EXPECT_EQ(ManifestLocation::kUnpacked, extension->location());
  }

  // Try loading an extension with no manifest. It should fail.
  TestExtensionDir bad_extension_dir;
  bad_extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// some JS");
  {
    ChromeTestExtensionLoader loader(profile());
    loader.set_pack_extension(false);
    loader.set_should_fail(true);
    loader.LoadExtension(bad_extension_dir.UnpackedPath());
  }

  EXPECT_EQ(1u, GetErrors().size());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->GenerateInstalledExtensionsSet().size());
  EXPECT_TRUE(
      get_extension_by_name(registry()->enabled_extensions(), kGoodExtension));

  // Test uninstalling the good extension.
  const ExtensionId good_id =
      get_extension_by_name(registry()->enabled_extensions(), kGoodExtension)
          ->id();
  service()->UninstallExtension(good_id, UNINSTALL_REASON_FOR_TESTING, nullptr);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(registry()->GenerateInstalledExtensionsSet().empty());
}

// Tests that --load-extension is ignored for users opted in to Enhanced Safe
// Browsing (ESB).
TEST_F(ExtensionServiceTest, WillNotLoadFromCommandLineForESBUsers) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  // Enable ESB.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  // Try to load an extension from command line.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();
  ASSERT_EQ(0u, loaded_extensions().size());
  ValidatePrefKeyCount(0);
}

// Tests --load-extension works for non-ESB users.
TEST_F(ExtensionServiceTest, LoadsFromCommandLineForNonESBUsers) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  // Disable ESB.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  // Try to load an extension from command line.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  ValidatePrefKeyCount(1);
}

// Tests that --load-extension is ignored for users with policy
// ExtensionInstallTypeBlocklist containing command_line.
TEST_F(ExtensionServiceTest,
       WillNotLoadFromCommandLineForUsersWithPolicyFalse) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  profile()->GetPrefs()->SetList(pref_names::kExtensionInstallTypeBlocklist,
                                 base::Value::List().Append("command_line"));

  // Try to load an extension from command line.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();
  ASSERT_EQ(0u, loaded_extensions().size());
  ValidatePrefKeyCount(0);
}

// Tests --load-extension works for users with policy
// ExtensionInstallTypeBlocklist not containing "command_line" (default value)
TEST_F(ExtensionServiceTest, LoadsFromCommandLineForUsersWithoutPolicy) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  // Not setting pref as false is default value.
  // Try to load an extension from command line.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  ValidatePrefKeyCount(1);
}

// Tests that we generate IDs when they are not specified in the manifest for
// --load-extension.
TEST_F(ExtensionServiceTest, GenerateID) {
  InitializeEmptyExtensionService();

  base::FilePath no_id_ext = data_dir().AppendASCII("no_id");
  UnpackedInstaller::Create(service())->Load(no_id_ext);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  ASSERT_TRUE(crx_file::id_util::IdIsValid(loaded_extensions()[0]->id()));
  EXPECT_EQ(loaded_extensions()[0]->location(), ManifestLocation::kUnpacked);

  ValidatePrefKeyCount(1);

  std::string previous_id = loaded_extensions()[0]->id();

  // If we reload the same path, we should get the same extension ID.
  UnpackedInstaller::Create(service())->Load(no_id_ext);
  task_environment()->RunUntilIdle();
  ASSERT_EQ(1u, loaded_extensions().size());
  ASSERT_EQ(previous_id, loaded_extensions()[0]->id());
}

TEST_F(ExtensionServiceTest, UnpackedValidatesLocales) {
  InitializeEmptyExtensionService();

  base::FilePath bad_locale =
      data_dir().AppendASCII("unpacked").AppendASCII("bad_messages_file");
  UnpackedInstaller::Create(service())->Load(bad_locale);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1u, GetErrors().size());
  base::FilePath ms_messages_file = bad_locale.AppendASCII("_locales")
                                              .AppendASCII("ms")
                                              .AppendASCII("messages.json");
  EXPECT_THAT(base::UTF16ToUTF8(GetErrors()[0]),
              testing::HasSubstr(
                  base::UTF16ToUTF8(ms_messages_file.LossyDisplayName())));
  ASSERT_EQ(0u, loaded_extensions().size());
}

void ExtensionServiceTest::TestExternalProvider(MockExternalProvider* provider,
                                                ManifestLocation location) {
  // Verify that starting with no providers loads no extensions.
  service()->Init();
  ASSERT_EQ(0u, loaded_extensions().size());

  provider->set_visit_count(0);

  // Register a test extension externally using the mock registry provider.
  base::FilePath source_path = data_dir().AppendASCII("good.crx");

  // Add the extension.
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Reloading extensions should find our externally registered extension
  // and install it.
  WaitForExternalExtensionInstalled(good_crx);

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  ASSERT_EQ(location, loaded_extensions()[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_extensions()[0]->version().GetString());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(good_crx));
  // TODO(devlin): Testing the underlying values of the prefs for extensions
  // should be done in an ExtensionPrefs test, not here. This should only be
  // using the public ExtensionPrefs interfaces.
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", static_cast<int>(location));

  // Reload extensions without changing anything. The extension should be
  // loaded again. Note that Reload doesn't trigger 'OnExtensionUnloaded`, so we
  // have to manually clear the loaded extensions.
  ClearLoadedExtensions();
  service()->ReloadExtensionsForTest();
  task_environment()->RunUntilIdle();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(good_crx));
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", static_cast<int>(location));

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  WaitForExternalExtensionInstalled(good_crx);
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  ASSERT_EQ("1.0.0.1", loaded_extensions()[0]->version().GetString());
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(good_crx));
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", static_cast<int>(location));

  // Uninstall the extension and reload. Nothing should happen because the
  // preference should prevent us from reinstalling.
  std::string id = loaded_extensions()[0]->id();
  EXPECT_EQ(id, good_crx);
  bool no_uninstall = GetManagementPolicy()->MustRemainEnabled(
      loaded_extensions()[0].get(), nullptr);
  service()->UninstallExtension(id, UNINSTALL_REASON_FOR_TESTING, nullptr);
  task_environment()->RunUntilIdle();

  base::FilePath install_path = extensions_install_dir().AppendASCII(id);
  if (no_uninstall) {
    // Policy controlled extensions should not have been touched by uninstall.
    ASSERT_TRUE(base::PathExists(install_path));
    EXPECT_TRUE(prefs->GetInstalledExtensionInfo(good_crx));
    EXPECT_FALSE(prefs->IsExternalExtensionUninstalled(good_crx));
  } else {
    // The extension should also be gone from the install directory.
    ASSERT_FALSE(base::PathExists(install_path));
    service()->CheckForExternalUpdates();
    task_environment()->RunUntilIdle();
    ASSERT_EQ(0u, loaded_extensions().size());
    EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));
    EXPECT_FALSE(prefs->GetInstalledExtensionInfo(good_crx));

    // Now clear the preference and reinstall.
    prefs->ClearExternalExtensionUninstalled(good_crx);

    WaitForExternalExtensionInstalled(good_crx);
    ASSERT_EQ(1u, loaded_extensions().size());
  }
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(good_crx));
  EXPECT_FALSE(prefs->IsExternalExtensionUninstalled(good_crx));
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location", static_cast<int>(location));

  if (GetManagementPolicy()->MustRemainEnabled(loaded_extensions()[0].get(),
                                               nullptr)) {
    EXPECT_EQ(2, provider->visit_count());
  } else {
    // Now test an externally triggered uninstall (deleting the registry key or
    // the pref entry).
    provider->RemoveExtension(good_crx);
    service()->OnExternalProviderReady(provider);
    task_environment()->RunUntilIdle();
    ASSERT_EQ(0u, loaded_extensions().size());
    EXPECT_FALSE(prefs->IsExternalExtensionUninstalled(good_crx));
    EXPECT_FALSE(prefs->GetInstalledExtensionInfo(good_crx));

    // The extension should also be gone from the install directory.
    ASSERT_FALSE(base::PathExists(install_path));

    // Now test the case where user uninstalls and then the extension is removed
    // from the external provider.
    provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);
    WaitForExternalExtensionInstalled(good_crx);

    ASSERT_EQ(1u, loaded_extensions().size());
    ASSERT_EQ(0u, GetErrors().size());

    // User uninstalls.
    service()->UninstallExtension(id, UNINSTALL_REASON_FOR_TESTING, nullptr);
    task_environment()->RunUntilIdle();
    ASSERT_EQ(0u, loaded_extensions().size());

    // Then remove the extension from the extension provider.
    provider->RemoveExtension(good_crx);

    // Should still be at 0.
    ClearLoadedExtensions();
    service()->ReloadExtensionsForTest();
    task_environment()->RunUntilIdle();
    ASSERT_EQ(0u, loaded_extensions().size());

    EXPECT_FALSE(prefs->GetInstalledExtensionInfo(good_crx));
    EXPECT_TRUE(prefs->IsExternalExtensionUninstalled(good_crx));

    EXPECT_EQ(5, provider->visit_count());
  }
}

// Tests the external installation feature
#if BUILDFLAG(IS_WIN)
TEST_F(ExtensionServiceTest, ExternalInstallRegistry) {
  // This should all work, even when normal extension installation is disabled.
  InitializeExtensionServiceWithExtensionsDisabled();

  // Now add providers. Extension system takes ownership of the objects.
  MockExternalProvider* reg_provider =
      AddMockExternalProvider(ManifestLocation::kExternalRegistry);
  TestExternalProvider(reg_provider, ManifestLocation::kExternalRegistry);
}
#endif

TEST_F(ExtensionServiceTest, ExternalInstallPref) {
  InitializeEmptyExtensionService();

  // Now add providers. Extension system takes ownership of the objects.
  MockExternalProvider* pref_provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  TestExternalProvider(pref_provider, ManifestLocation::kExternalPref);
}

TEST_F(ExtensionServiceTest, ExternalInstallPrefUpdateUrl) {
  // This should all work, even when normal extension installation is disabled.
  InitializeExtensionServiceWithExtensionsDisabled();

  // TODO(skerner): The mock provider is not a good model of a provider
  // that works with update URLs, because it adds file and version info.
  // Extend the mock to work with update URLs.  This test checks the
  // behavior that is common to all external extension visitors.  The
  // browser test ExtensionManagementTest.ExternalUrlUpdate tests that
  // what the visitor does results in an extension being downloaded and
  // installed.
  MockExternalProvider* pref_provider =
      AddMockExternalProvider(ManifestLocation::kExternalPrefDownload);
  TestExternalProvider(pref_provider, ManifestLocation::kExternalPrefDownload);
}

TEST_F(ExtensionServiceTest, ExternalInstallPolicyUpdateUrl) {
  // This should all work, even when normal extension installation is disabled.
  InitializeExtensionServiceWithExtensionsDisabled();

  // TODO(skerner): The mock provider is not a good model of a provider
  // that works with update URLs, because it adds file and version info.
  // Extend the mock to work with update URLs. This test checks the
  // behavior that is common to all external extension visitors. The
  // browser test ExtensionManagementTest.ExternalUrlUpdate tests that
  // what the visitor does results in an extension being downloaded and
  // installed.
  MockExternalProvider* pref_provider =
      AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
  TestExternalProvider(pref_provider,
                       ManifestLocation::kExternalPolicyDownload);
}

// Tests that external extensions get uninstalled when the external extension
// providers can't account for them.
TEST_F(ExtensionServiceTest, ExternalUninstall) {
  // Start the extensions service with one external extension already installed.
  base::FilePath test_data_dir = data_dir().AppendASCII("good");
  ExtensionServiceInitParams params;
  ASSERT_TRUE(params.SetPrefsContentFromFile(
      test_data_dir.AppendASCII("PreferencesExternal")));
  params.extensions_dir = test_data_dir.AppendASCII("Extensions");
  InitializeExtensionService(std::move(params));
  service()->Init();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(0u, loaded_extensions().size());
}

// Test that running multiple update checks simultaneously does not
// keep the update from succeeding.
TEST_F(ExtensionServiceTest, MultipleExternalUpdateCheck) {
  InitializeEmptyExtensionService();

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  // Verify that starting with no providers loads no extensions.
  service()->Init();
  ASSERT_EQ(0u, loaded_extensions().size());

  // Start two checks for updates.
  provider->set_visit_count(0);
  service()->CheckForExternalUpdates();
  service()->CheckForExternalUpdates();
  task_environment()->RunUntilIdle();

  // Two calls should cause two checks for external extensions.
  EXPECT_EQ(2, provider->visit_count());
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(0u, loaded_extensions().size());

  // Register a test extension externally using the mock registry provider.
  base::FilePath source_path = data_dir().AppendASCII("good.crx");
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Two checks for external updates should find the extension, and install it
  // once.
  provider->set_visit_count(0);
  {
    ExtensionLoadedObserver good_crx_loaded(registry(), good_crx);
    service()->CheckForExternalUpdates();
    service()->CheckForExternalUpdates();
    good_crx_loaded.Wait();
  }
  EXPECT_EQ(2, provider->visit_count());
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  ASSERT_EQ(ManifestLocation::kExternalPref,
            loaded_extensions()[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_extensions()[0]->version().GetString());
  ValidatePrefKeyCount(1);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location",
                      static_cast<int>(ManifestLocation::kExternalPref));

  provider->RemoveExtension(good_crx);
  provider->set_visit_count(0);
  service()->CheckForExternalUpdates();
  service()->CheckForExternalUpdates();
  task_environment()->RunUntilIdle();

  // Two calls should cause two checks for external extensions.
  // Because the external source no longer includes good_crx,
  // good_crx will be uninstalled.  So, expect that no extensions
  // are loaded.
  EXPECT_EQ(2, provider->visit_count());
  EXPECT_EQ(0u, GetErrors().size());
  EXPECT_EQ(0u, loaded_extensions().size());
}

TEST_F(ExtensionServiceTest, ExternalPrefProvider) {
  InitializeEmptyExtensionService();

  // Test some valid extension records.
  // Set a base path to avoid erroring out on relative paths.
  // Paths starting with // are absolute on every platform we support.
  base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockProviderVisitor visitor(base_path);
  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  }"
      "}";
  EXPECT_EQ(3, visitor.Visit(json_data));

  // Simulate an external_extensions.json file that contains seven invalid
  // records:
  // - One that is missing the 'external_crx' key.
  // - One that is missing the 'external_version' key.
  // - One that is specifying .. in the path.
  // - One that specifies both a file and update URL.
  // - One that specifies no file or update URL.
  // - One that has an update URL that is not well formed.
  // - One that contains a malformed version.
  // - One that has an invalid id.
  // - One that has a non-dictionary value.
  // - One that has an integer 'external_version' instead of a string.
  // The final extension is valid, and we check that it is read to make sure
  // failures don't stop valid records from being read.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension.crx\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"..\\\\foo\\\\RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"dddddddddddddddddddddddddddddddd\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\","
      "    \"external_update_url\": \"http:\\\\foo.com/update\""
      "  },"
      "  \"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\": {"
      "  },"
      "  \"ffffffffffffffffffffffffffffffff\": {"
      "    \"external_update_url\": \"This string is not a valid URL\""
      "  },"
      "  \"gggggggggggggggggggggggggggggggg\": {"
      "    \"external_crx\": \"RandomExtension3.crx\","
      "    \"external_version\": \"This is not a valid version!\""
      "  },"
      "  \"This is not a valid id!\": {},"
      "  \"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh\": true,"
      "  \"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii\": {"
      "    \"external_crx\": \"RandomExtension4.crx\","
      "    \"external_version\": 1.0"
      "  },"
      "  \"pppppppppppppppppppppppppppppppp\": {"
      "    \"external_crx\": \"RandomValidExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  }"
      "}";
  EXPECT_EQ(1, visitor.Visit(json_data));

  // Check that if a base path is not provided, use of a relative
  // path fails.
  base::FilePath empty;
  MockProviderVisitor visitor_no_relative_paths(empty);

  // Use absolute paths.  Expect success.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"//RandomExtension1.crx\","
      "    \"external_version\": \"3.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"//path/to/RandomExtension2.crx\","
      "    \"external_version\": \"3.0\""
      "  }"
      "}";
  EXPECT_EQ(2, visitor_no_relative_paths.Visit(json_data));

  // Use a relative path.  Expect that it will error out.
  json_data =
      "{"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"3.0\""
      "  }"
      "}";
  EXPECT_EQ(0, visitor_no_relative_paths.Visit(json_data));

  // Test supported_locales.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"supported_locales\": [ \"en\" ]"
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\","
      "    \"supported_locales\": [ \"en-GB\" ]"
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"3.0\","
      "    \"supported_locales\": [ \"en_US\", \"fr\" ]"
      "  }"
      "}";
  {
    ScopedBrowserLocale guard("en-US");
    EXPECT_EQ(2, visitor.Visit(json_data));
  }

  // Test web_app_migration_flag.
  {
    json_data = R"(
      {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa": {
          "external_crx": "RandomExtension.crx",
          "external_version": "1.0",
          "web_app_migration_flag": "TestFeature"
        }
      })";

    {
      base::AutoReset<bool> testing_scope =
          web_app::SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();
      EXPECT_EQ(0, visitor.Visit(json_data));
      visitor.provider()->HasExtension("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    }

    {
      EXPECT_EQ(1, visitor.Visit(json_data));
      visitor.provider()->HasExtension("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    }
  }

  // Test keep_if_present.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"keep_if_present\": true"
      "  }"
      "}";
  {
    EXPECT_EQ(0, visitor.Visit(json_data));
  }

  // Test is_bookmark_app.
  // Bookmark apps are deprecated and should no longer be loaded.
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"is_bookmark_app\": true"
      "  }"
      "}";
  EXPECT_EQ(0, visitor.Visit(json_data));

  // Test is_from_webstore.
  MockProviderVisitor from_webstore_visitor(
      base_path, Extension::FROM_WEBSTORE);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"is_from_webstore\": true"
      "  }"
      "}";
  EXPECT_EQ(1, from_webstore_visitor.Visit(json_data));

  // Test was_installed_by_eom.
  MockProviderVisitor was_installed_by_eom_visitor(
      base_path, Extension::WAS_INSTALLED_BY_OEM);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"was_installed_by_oem\": true"
      "  }"
      "}";
  EXPECT_EQ(1, was_installed_by_eom_visitor.Visit(json_data));

  // Test min_profile_created_by_version.
  MockProviderVisitor min_profile_created_by_version_visitor(base_path);
  json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"min_profile_created_by_version\": \"42.0.0.1\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"1.0\","
      "    \"min_profile_created_by_version\": \"43.0.0.1\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_crx\": \"RandomExtension3.crx\","
      "    \"external_version\": \"3.0\","
      "    \"min_profile_created_by_version\": \"44.0.0.1\""
      "  }"
      "}";
  min_profile_created_by_version_visitor.profile()->GetPrefs()->SetString(
      prefs::kProfileCreatedByVersion, "40.0.0.1");
  EXPECT_EQ(0, min_profile_created_by_version_visitor.Visit(json_data));
  min_profile_created_by_version_visitor.profile()->GetPrefs()->SetString(
      prefs::kProfileCreatedByVersion, "43.0.0.1");
  EXPECT_EQ(2, min_profile_created_by_version_visitor.Visit(json_data));
  min_profile_created_by_version_visitor.profile()->GetPrefs()->SetString(
      prefs::kProfileCreatedByVersion, "45.0.0.1");
  EXPECT_EQ(3, min_profile_created_by_version_visitor.Visit(json_data));
}

TEST_F(ExtensionServiceTest, ReinstallProviderExtensions) {
  InitializeEmptyExtensionService();

  // Check that ReinstallProviderExtensions() will not return any extensions if
  // no prefs are already stored in the provider.
  base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockProviderVisitor visitor(base_path);
  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  }"
      "}";
  EXPECT_EQ(0, visitor.ReinstallProviderExtensions(json_data));

  // Add the extension records to the provider's prefs.
  visitor.provider()->VisitRegisteredExtension();

  // Check that ReinstallProviderExtensions() returns the extensions from the
  // saved prefs.
  EXPECT_EQ(3, visitor.ReinstallProviderExtensions(json_data));
}

TEST_F(ExtensionServiceTest, DoNotInstallForEnterprise) {
  InitializeEmptyExtensionService();

  const base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockProviderVisitor visitor(base_path);
  policy::ProfilePolicyConnector* const connector =
      visitor.profile()->GetProfilePolicyConnector();
  connector->OverrideIsManagedForTesting(true);
  EXPECT_TRUE(connector->IsManaged());

  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\","
      "    \"do_not_install_for_enterprise\": true"
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"1.0\""
      "  }"
      "}";
  EXPECT_EQ(1, visitor.Visit(json_data));
}

TEST_F(ExtensionServiceTest, IncrementalUpdateThroughRegistry) {
  InitializeEmptyExtensionService();

  // Test some valid extension records.
  // Set a base path to avoid erroring out on relative paths.
  // Paths starting with // are absolute on every platform we support.
  base::FilePath base_path(FILE_PATH_LITERAL("//base/path"));
  ASSERT_TRUE(base_path.IsAbsolute());
  MockUpdateProviderVisitor visitor(base_path);
  std::string json_data =
      "{"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  },"
      "  \"cccccccccccccccccccccccccccccccc\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  }"
      "}";
  EXPECT_EQ(3, visitor.Visit(json_data, ManifestLocation::kExternalRegistry,
                             ManifestLocation::kExternalPrefDownload));

  // c* removed and d*, e*, f* added, a*, b* existing.
  json_data =
      "{"
      "  \"dddddddddddddddddddddddddddddddd\": {"
      "    \"external_crx\": \"RandomExtension3.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\": {"
      "    \"external_update_url\": \"http:\\\\foo.com/update\","
      "    \"install_parameter\": \"id\""
      "  },"
      "  \"ffffffffffffffffffffffffffffffff\": {"
      "    \"external_update_url\": \"http:\\\\bar.com/update\","
      "    \"install_parameter\": \"id\""
      "  },"
      "  \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\": {"
      "    \"external_crx\": \"RandomExtension.crx\","
      "    \"external_version\": \"1.0\""
      "  },"
      "  \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\": {"
      "    \"external_crx\": \"RandomExtension2.crx\","
      "    \"external_version\": \"2.0\""
      "  }"
      "}";

  // This will simulate registry loader observing new changes in registry and
  // hence will discover new extensions.
  visitor.VisitDueToUpdate(json_data);

  // UpdateUrl.
  EXPECT_EQ(2u, visitor.GetUpdateURLExtensionCount());
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithUpdateUrl("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"));
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithUpdateUrl("ffffffffffffffffffffffffffffffff"));

  // File.
  EXPECT_EQ(3u, visitor.GetFileExtensionCount());
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithFile("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithFile("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  EXPECT_TRUE(
      visitor.HasSeenUpdateWithFile("dddddddddddddddddddddddddddddddd"));

  // Removed extensions.
  EXPECT_EQ(1u, visitor.GetRemovedExtensionCount());
  EXPECT_TRUE(visitor.HasSeenRemoval("cccccccccccccccccccccccccccccccc"));

  // Simulate all 5 extensions being removed.
  json_data = "{}";
  visitor.VisitDueToUpdate(json_data);
  EXPECT_EQ(0u, visitor.GetUpdateURLExtensionCount());
  EXPECT_EQ(0u, visitor.GetFileExtensionCount());
  EXPECT_EQ(5u, visitor.GetRemovedExtensionCount());
}

// Test loading good extensions from the profile directory.
TEST_F(ExtensionServiceTest, LoadAndRelocalizeExtensions) {
  // Ensure we're testing in "en" and leave global state untouched.
  extension_l10n_util::ScopedLocaleForTest testLocale("en");

  // Initialize the test dir with a good Preferences/extensions.
  base::FilePath test_data_dir = data_dir().AppendASCII("l10n");
  ExtensionServiceInitParams params;
  ASSERT_TRUE(params.SetPrefsContentFromFile(
      test_data_dir.Append(chrome::kPreferencesFilename)));
  params.extensions_dir = test_data_dir;
  InitializeExtensionService(std::move(params));

  service()->Init();

  ASSERT_EQ(3u, loaded_extensions().size());

  // This was equal to "sr" on load.
  ValidateStringPref(loaded_extensions()[0]->id(), keys::kCurrentLocale, "en");

  // These are untouched by re-localization.
  ValidateStringPref(loaded_extensions()[1]->id(), keys::kCurrentLocale, "en");
  EXPECT_FALSE(IsPrefExist(loaded_extensions()[1]->id(), keys::kCurrentLocale));

  // This one starts with Serbian name, and gets re-localized into English.
  EXPECT_EQ("My name is simple.", loaded_extensions()[0]->name());

  // These are untouched by re-localization.
  EXPECT_EQ("My name is simple.", loaded_extensions()[1]->name());
  EXPECT_EQ("no l10n", loaded_extensions()[2]->name());
}

// Test that we get enabled/disabled correctly for all the pref/command-line
// combinations. We don't want to derive from the ExtensionServiceTest class
// for this test, so we use ExtensionServiceTestSimple.
//
// Also tests that we always fire EXTENSIONS_READY, no matter whether we are
// enabled or not.
class ExtensionServiceTestSimple : public testing::Test {
 public:
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExtensionServiceTestSimple, Enabledness) {
#if BUILDFLAG(ENABLE_PLUGINS)
  content::PluginService::GetInstance()->Init();
#endif

  LoadErrorReporter::Init(false);  // no noisy errors
  std::unique_ptr<base::CommandLine> command_line;

  // The profile lifetimes must not overlap: services may use global variables.
  {
    auto profile = std::make_unique<TestingProfile>();
    bool ready = false;
    auto on_ready = [](bool* ready) { *ready = true; };
    ExtensionSystem::Get(profile.get())
        ->ready()
        .Post(FROM_HERE, base::BindOnce(on_ready, &ready));

    base::FilePath install_dir =
        profile->GetPath().AppendASCII(kInstallDirectoryName);

    // By default, we are enabled.
    command_line =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    ExtensionService* service =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile.get()))
            ->CreateExtensionService(command_line.get(), install_dir, false);
    EXPECT_TRUE(service->extensions_enabled());
    service->Init();
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(ready);
  }

  {
    auto profile = std::make_unique<TestingProfile>();
    bool ready = false;
    auto on_ready = [](bool* ready) { *ready = true; };
    ExtensionSystem::Get(profile.get())
        ->ready()
        .Post(FROM_HERE, base::BindOnce(on_ready, &ready));

    base::FilePath install_dir =
        profile->GetPath().AppendASCII(kInstallDirectoryName);
    command_line->AppendSwitch(::switches::kDisableExtensions);
    ExtensionService* service =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile.get()))
            ->CreateExtensionService(command_line.get(), install_dir, false);
    EXPECT_FALSE(service->extensions_enabled());
    service->Init();
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(ready);
  }

  {
    auto profile = std::make_unique<TestingProfile>();
    bool ready = false;
    auto on_ready = [](bool* ready) { *ready = true; };
    ExtensionSystem::Get(profile.get())
        ->ready()
        .Post(FROM_HERE, base::BindOnce(on_ready, &ready));

    base::FilePath install_dir =
        profile->GetPath().AppendASCII(kInstallDirectoryName);
    profile->GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
    ExtensionService* service =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile.get()))
            ->CreateExtensionService(command_line.get(), install_dir, false);
    EXPECT_FALSE(service->extensions_enabled());
    service->Init();
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(ready);
  }

  {
    auto profile = std::make_unique<TestingProfile>();
    bool ready = false;
    auto on_ready = [](bool* ready) { *ready = true; };
    ExtensionSystem::Get(profile.get())
        ->ready()
        .Post(FROM_HERE, base::BindOnce(on_ready, &ready));

    base::FilePath install_dir =
        profile->GetPath().AppendASCII(kInstallDirectoryName);
    profile->GetPrefs()->SetBoolean(prefs::kDisableExtensions, true);
    command_line =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    ExtensionService* service =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile.get()))
            ->CreateExtensionService(command_line.get(), install_dir, false);
    EXPECT_FALSE(service->extensions_enabled());
    service->Init();
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(ready);
  }

  // Execute any pending deletion tasks.
  task_environment()->RunUntilIdle();
}

// Test loading extensions that require limited and unlimited storage quotas.
TEST_F(ExtensionServiceTest, StorageQuota) {
  InitializeEmptyExtensionService();

  base::FilePath extensions_path = data_dir().AppendASCII("storage_quota");

  base::FilePath limited_quota_ext =
      extensions_path.AppendASCII("limited_quota")
      .AppendASCII("1.0");

  // The old permission name for unlimited quota was "unlimited_storage", but
  // we changed it to "unlimitedStorage". This tests both versions.
  base::FilePath unlimited_quota_ext =
      extensions_path.AppendASCII("unlimited_quota")
      .AppendASCII("1.0");
  base::FilePath unlimited_quota_ext2 =
      extensions_path.AppendASCII("unlimited_quota")
      .AppendASCII("2.0");
  UnpackedInstaller::Create(service())->Load(limited_quota_ext);
  UnpackedInstaller::Create(service())->Load(unlimited_quota_ext);
  UnpackedInstaller::Create(service())->Load(unlimited_quota_ext2);
  task_environment()->RunUntilIdle();

  ASSERT_EQ(3u, loaded_extensions().size());
  EXPECT_TRUE(profile());
  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_FALSE(
      profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          loaded_extensions()[0]->url()));
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      loaded_extensions()[1]->url()));
  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      loaded_extensions()[2]->url()));
}

// Tests ComponentLoader::Add().
TEST_F(ExtensionServiceTest, ComponentExtensions) {
  // Component extensions should work even when extensions are disabled.
  InitializeExtensionServiceWithExtensionsDisabled();

  base::FilePath path = data_dir()
                            .AppendASCII("good")
                            .AppendASCII("Extensions")
                            .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                            .AppendASCII("1.0.0.0");

  std::string manifest;
  ASSERT_TRUE(
      base::ReadFileToString(path.Append(kManifestFilename), &manifest));

  service()->component_loader()->Add(manifest, path);
  service()->Init();

  // Note that we do not pump messages -- the extension should be loaded
  // immediately.

  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_extensions().size());
  EXPECT_EQ(ManifestLocation::kComponent, loaded_extensions()[0]->location());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());

  // Component extensions get a prefs entry on first install.
  ValidatePrefKeyCount(1);

  // Reload all extensions, and make sure it comes back.
  std::string extension_id = (*registry()->enabled_extensions().begin())->id();
  ClearLoadedExtensions();
  service()->ReloadExtensionsForTest();
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(extension_id, (*registry()->enabled_extensions().begin())->id());
}

TEST_F(ExtensionServiceTest, InstallPriorityExternalUpdateUrl) {
  InitializeEmptyExtensionService();

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(1u);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  PendingExtensionManager* pending = service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Skip install when the location is the same.
  GURL good_update_url(kGoodUpdateURL);
  ExternalInstallInfoUpdateUrl info(
      kGoodId, std::string(), std::move(good_update_url),
      ManifestLocation::kInternal, Extension::NO_FLAGS, false);
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(info, true));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Update the download location when install is requested from higher priority
  // location.
  info.download_location = ManifestLocation::kExternalPolicyDownload;
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(info, true));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Try the low priority again.  Should be rejected.
  info.download_location = ManifestLocation::kExternalPrefDownload;
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(info, true));
  // The existing record should still be present in the pending extension
  // manager.
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Skip install when the location has the same priority as the installed
  // location.
  info.download_location = ManifestLocation::kExternalPolicyDownload;
  EXPECT_FALSE(service()->OnExternalExtensionUpdateUrlFound(info, true));

  EXPECT_FALSE(pending->IsIdPending(kGoodId));
}

TEST_F(ExtensionServiceTest, FailedLocalFileInstallIsNotPending) {
  base::Version version("1.0.0.0");

  // We don't want the extension to be installed.  A path that doesn't
  // point to a valid CRX ensures this.
  const base::FilePath kInvalidPathToCrx(FILE_PATH_LITERAL("invalid_path"));

  const int kCreationFlags = 0;
  const bool kDontMarkAcknowledged = false;
  const bool kDontInstallImmediately = false;

  InitializeEmptyExtensionService();

  PendingExtensionManager* pending = service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  ExternalInstallInfoFile info(kGoodId, version, kInvalidPathToCrx,
                               ManifestLocation::kInternal, kCreationFlags,
                               kDontMarkAcknowledged, kDontInstallImmediately);
  {
    PendingRemovalObserver observer(pending, kGoodId);

    // Simulate an external source adding the extension.
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));
    observer.WaitForRemoval();
    EXPECT_FALSE(pending->IsIdPending(kGoodId));
  }
}

TEST_F(ExtensionServiceTest, InstallPriorityExternalLocalFile) {
  base::Version older_version("0.1.0.0");
  base::Version newer_version("2.0.0.0");

  // We don't want the extension to be installed.  A path that doesn't
  // point to a valid CRX ensures this.
  const base::FilePath kInvalidPathToCrx(FILE_PATH_LITERAL("invalid_path"));

  const int kCreationFlags = 0;
  const bool kDontMarkAcknowledged = false;
  const bool kDontInstallImmediately = false;

  InitializeEmptyExtensionService();

  // The test below uses install source constants to test that
  // priority is enforced.  It assumes a specific ranking of install
  // sources: Registry (kExternalRegistry) overrides external pref
  // (kExternalPref), and external pref overrides user install (kInternal).
  // The following assertions verify these assumptions:
  ASSERT_EQ(
      ManifestLocation::kExternalRegistry,
      Manifest::GetHigherPriorityLocation(ManifestLocation::kExternalRegistry,
                                          ManifestLocation::kExternalPref));
  ASSERT_EQ(
      ManifestLocation::kExternalRegistry,
      Manifest::GetHigherPriorityLocation(ManifestLocation::kExternalRegistry,
                                          ManifestLocation::kInternal));
  ASSERT_EQ(ManifestLocation::kExternalPref,
            Manifest::GetHigherPriorityLocation(ManifestLocation::kExternalPref,
                                                ManifestLocation::kInternal));

  PendingExtensionManager* pending = service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  ExternalInstallInfoFile info(kGoodId, older_version, kInvalidPathToCrx,
                               ManifestLocation::kInternal, kCreationFlags,
                               kDontMarkAcknowledged, kDontInstallImmediately);
  {
    // Simulate an external source adding the extension as kInternal.
    PendingRemovalObserver observer(pending, kGoodId);
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));
    observer.WaitForRemoval();
    VerifyCrxInstall(kInvalidPathToCrx, INSTALL_FAILED);
  }

  {
    // Simulate an external source adding the extension as kExternalPref.
    PendingRemovalObserver observer(pending, kGoodId);
    info.crx_location = ManifestLocation::kExternalPref;
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));

    // Simulate an external source adding as EXTERNAL_PREF again.
    // This is rejected because the version and the location are the same as
    // the previous installation, which is still pending.
    EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));

    // Try INTERNAL again.  Should fail.
    info.crx_location = ManifestLocation::kInternal;
    EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));

    observer.WaitForRemoval();
    VerifyCrxInstall(kInvalidPathToCrx, INSTALL_FAILED);
  }

  {
    // Now the registry adds the extension.
    PendingRemovalObserver observer(pending, kGoodId);
    info.crx_location = ManifestLocation::kExternalRegistry;
    EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));

    // Registry outranks both external pref and internal, so both fail.
    info.crx_location = ManifestLocation::kExternalPref;
    EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));

    info.crx_location = ManifestLocation::kInternal;
    EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
    EXPECT_TRUE(pending->IsIdPending(kGoodId));

    observer.WaitForRemoval();
    VerifyCrxInstall(kInvalidPathToCrx, INSTALL_FAILED);
  }

  // Install the extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* ext = InstallCRX(path, INSTALL_NEW);
  ValidatePrefKeyCount(1u);
  ValidateIntegerPref(good_crx, "state", Extension::ENABLED);
  ValidateIntegerPref(good_crx, "location",
                      static_cast<int>(ManifestLocation::kInternal));

  // Now test the logic of OnExternalExtensionFileFound() when the extension
  // being added is already installed.

  // Tests assume |older_version| is less than the installed version, and
  // |newer_version| is greater.  Verify this:
  ASSERT_LT(older_version, ext->version());
  ASSERT_GT(newer_version, ext->version());

  // An external install for the same location should fail if the version is
  // older, or the same, and succeed if the version is newer.

  // Older than the installed version...
  info.version = older_version;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Same version as the installed version...
  info.version = ext->version();
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // Newer than the installed version...
  info.version = newer_version;
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // An external install for a higher priority install source should succeed
  // if the version is greater.  |older_version| is not...
  info.version = older_version;
  info.crx_location = ManifestLocation::kExternalPref;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // |newer_version| is newer.
  info.version = newer_version;
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // An external install for an even higher priority install source should
  // succeed if the version is greater.
  info.crx_location = ManifestLocation::kExternalRegistry;
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));

  // Because kExternalPref is a lower priority source than kExternalRegistry,
  // adding from external pref will now fail.
  info.crx_location = ManifestLocation::kExternalPref;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE(pending->IsIdPending(kGoodId));
}

TEST_F(ExtensionServiceTest, ConcurrentExternalLocalFile) {
  base::Version kVersion123("1.2.3");
  base::Version kVersion124("1.2.4");
  base::Version kVersion125("1.2.5");
  const base::FilePath kInvalidPathToCrx(FILE_PATH_LITERAL("invalid_path"));
  const int kCreationFlags = 0;
  const bool kDontMarkAcknowledged = false;
  const bool kDontInstallImmediately = false;

  InitializeEmptyExtensionService();

  PendingExtensionManager* pending = service()->pending_extension_manager();
  EXPECT_FALSE(pending->IsIdPending(kGoodId));

  // An external provider starts installing from a local crx.
  ExternalInstallInfoFile info(kGoodId, kVersion123, kInvalidPathToCrx,
                               ManifestLocation::kExternalPref, kCreationFlags,
                               kDontMarkAcknowledged, kDontInstallImmediately);
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));

  const PendingExtensionInfo* pending_info;
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion123);

  // Adding a newer version overrides the currently pending version.
  info.version = base::Version(kVersion124);
  EXPECT_TRUE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion124);

  // Adding an older version fails.
  info.version = kVersion123;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion124);

  // Adding an older version fails even when coming from a higher-priority
  // location.
  info.crx_location = ManifestLocation::kExternalRegistry;
  EXPECT_FALSE(service()->OnExternalExtensionFileFound(info));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_TRUE(pending_info->version().IsValid());
  EXPECT_EQ(pending_info->version(), kVersion124);

  // Adding the latest version from the webstore overrides a specific version.
  GURL kUpdateUrl("http://example.com/update");
  ExternalInstallInfoUpdateUrl update_info(
      kGoodId, std::string(), kUpdateUrl,
      ManifestLocation::kExternalPolicyDownload, Extension::NO_FLAGS, false);
  EXPECT_TRUE(service()->OnExternalExtensionUpdateUrlFound(update_info, true));
  EXPECT_TRUE((pending_info = pending->GetById(kGoodId)));
  EXPECT_FALSE(pending_info->version().IsValid());
}

// This makes sure we can package and install CRX files that use allowlisted
// permissions.
TEST_F(ExtensionServiceTest, InstallAllowlistedExtension) {
  std::string test_id = "hdkklepkcpckhnpgjnmbdfhehckloojk";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAllowlistedExtensionID, test_id);

  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("permissions");
  base::FilePath pem_path = path.AppendASCII("allowlist.pem");
  path = path.AppendASCII("allowlist");

  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(test_id, extension->id());
}

// Test that when multiple sources try to install an extension,
// we consistently choose the right one. To make tests easy to read,
// methods that fake requests to install crx files in several ways
// are provided.
class ExtensionSourcePriorityTest : public ExtensionServiceTest {
 public:
  void SetUp() override {
    ExtensionServiceTest::SetUp();

    // All tests use a single extension.  Put the id and path in member vars
    // that all methods can read.
    crx_id_ = kGoodId;
    crx_path_ = data_dir().AppendASCII("good.crx");
  }

  // Fake an external source adding a URL to fetch an extension from.
  bool AddPendingExternalPrefUrl() {
    return service()->pending_extension_manager()->AddFromExternalUpdateUrl(
        crx_id_, std::string(), GURL(), ManifestLocation::kExternalPrefDownload,
        Extension::NO_FLAGS, false);
  }

  // Fake an external file from external_extensions.json.
  bool AddPendingExternalPrefFileInstall() {
    ExternalInstallInfoFile info(crx_id_, base::Version("1.0.0.0"), crx_path_,
                                 ManifestLocation::kExternalPref,
                                 Extension::NO_FLAGS, false, false);
    return service()->OnExternalExtensionFileFound(info);
  }

  // Fake a request from sync to install an extension.
  bool AddPendingSyncInstall() {
    return service()->pending_extension_manager()->AddFromSync(
        crx_id_,
        GURL(kGoodUpdateURL),
        base::Version(),
        &IsExtension,
        kGoodRemoteInstall);
  }

  // Fake a policy install.
  bool AddPendingPolicyInstall() {
    // Get path to the CRX with id |kGoodId|.
    ExternalInstallInfoUpdateUrl info(crx_id_, std::string(), GURL(),
                                      ManifestLocation::kExternalPolicyDownload,
                                      Extension::NO_FLAGS, false);
    return service()->OnExternalExtensionUpdateUrlFound(info, true);
  }

  // Get the install source of a pending extension.
  ManifestLocation GetPendingLocation() {
    const PendingExtensionInfo* info;
    EXPECT_TRUE(
        (info = service()->pending_extension_manager()->GetById(crx_id_)));
    return info->install_source();
  }

  // Is an extension pending from a sync request?
  bool GetPendingIsFromSync() {
    const PendingExtensionInfo* info;
    EXPECT_TRUE(
        (info = service()->pending_extension_manager()->GetById(crx_id_)));
    return info->is_from_sync();
  }

  // Is the CRX id these tests use pending?
  bool IsCrxPending() {
    return service()->pending_extension_manager()->IsIdPending(crx_id_);
  }

  // Is an extension installed?
  bool IsCrxInstalled() {
    return (registry()->GetExtensionById(
                crx_id_, ExtensionRegistry::EVERYTHING) != nullptr);
  }

 protected:
  // All tests use a single extension.  Making the id and path member
  // vars avoids pasing the same argument to every method.
  std::string crx_id_;
  base::FilePath crx_path_;
};

// Test that a pending request for installation of an external CRX from
// an update URL overrides a pending request to install the same extension
// from sync.
TEST_F(ExtensionSourcePriorityTest, PendingExternalFileOverSync) {
  InitializeEmptyExtensionService();

  ASSERT_FALSE(IsCrxInstalled());

  // Install pending extension from sync.
  EXPECT_TRUE(AddPendingSyncInstall());
  ASSERT_EQ(ManifestLocation::kInternal, GetPendingLocation());
  EXPECT_TRUE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());

  // Install pending as external prefs json would.
  AddPendingExternalPrefFileInstall();
  ASSERT_EQ(ManifestLocation::kExternalPref, GetPendingLocation());
  ASSERT_FALSE(IsCrxInstalled());

  // Another request from sync should be ignored.
  EXPECT_FALSE(AddPendingSyncInstall());
  ASSERT_EQ(ManifestLocation::kExternalPref, GetPendingLocation());
  ASSERT_FALSE(IsCrxInstalled());

  WaitForExternalExtensionInstalled(crx_id_);
  VerifyCrxInstall(crx_path_, INSTALL_NEW);
  ASSERT_TRUE(IsCrxInstalled());
}

// Test that an install of an external CRX from an update overrides
// an install of the same extension from sync.
TEST_F(ExtensionSourcePriorityTest, PendingExternalUrlOverSync) {
  InitializeEmptyExtensionService();
  ASSERT_FALSE(IsCrxInstalled());

  EXPECT_TRUE(AddPendingSyncInstall());
  ASSERT_EQ(ManifestLocation::kInternal, GetPendingLocation());
  EXPECT_TRUE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());

  ASSERT_TRUE(AddPendingExternalPrefUrl());
  ASSERT_EQ(ManifestLocation::kExternalPrefDownload, GetPendingLocation());
  EXPECT_FALSE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());

  EXPECT_FALSE(AddPendingSyncInstall());
  ASSERT_EQ(ManifestLocation::kExternalPrefDownload, GetPendingLocation());
  EXPECT_FALSE(GetPendingIsFromSync());
  ASSERT_FALSE(IsCrxInstalled());
}

// Test that an external install request stops sync from installing
// the same extension.
TEST_F(ExtensionSourcePriorityTest, InstallExternalBlocksSyncRequest) {
  InitializeEmptyExtensionService();
  ASSERT_FALSE(IsCrxInstalled());

  // External prefs starts an install.
  AddPendingExternalPrefFileInstall();

  // Crx installer was made, but has not yet run.
  ASSERT_FALSE(IsCrxInstalled());

  // Before the CRX installer runs, Sync requests that the same extension
  // be installed. Should fail, because an external source is pending.
  ASSERT_FALSE(AddPendingSyncInstall());

  // Wait for the external source to install.
  WaitForExternalExtensionInstalled(crx_id_);
  VerifyCrxInstall(crx_path_, INSTALL_NEW);
  ASSERT_TRUE(IsCrxInstalled());

  // Now that the extension is installed, sync request should fail
  // because the extension is already installed.
  ASSERT_FALSE(AddPendingSyncInstall());
}

// Test that the blocked pending external extension should be ignored until
// it's unblocked. (crbug.com/797369)
TEST_F(ExtensionServiceTest, BlockedExternalExtension) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  service()->external_install_manager()->UpdateExternalExtensionAlert();
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  service()->BlockAllExtensions();

  provider->UpdateOrAddExtension(page_action, "1.0.0.0",
                                 data_dir().AppendASCII("page_action.crx"));

  WaitForInstallationAttemptToComplete(page_action);
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  service()->UnblockAllExtensions();
  EXPECT_TRUE(HasExternalInstallErrors(service()));
}

// Test that installing an external extension displays a GlobalError.
TEST_F(ExtensionServiceTest, ExternalInstallGlobalError) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  service()->external_install_manager()->UpdateExternalExtensionAlert();
  // Should return false, meaning there aren't any extensions that the user
  // needs to know about.
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // This is a normal extension, installed normally.
  // This should NOT trigger an alert.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  service()->CheckForExternalUpdates();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // A hosted app, installed externally.
  // This should NOT trigger an alert.
  provider->UpdateOrAddExtension(
      hosted_app, "1.0.0.0", data_dir().AppendASCII("hosted_app.crx"));

  WaitForExternalExtensionInstalled(hosted_app);
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // Another normal extension, but installed externally.
  // This SHOULD trigger an alert.
  provider->UpdateOrAddExtension(
      page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));

  WaitForInstallationAttemptToComplete(page_action);
  EXPECT_TRUE(HasExternalInstallErrors(service()));
}

// Test that external extensions are initially disabled, and that enabling
// them clears the prompt.
TEST_F(ExtensionServiceTest, ExternalInstallInitiallyDisabled) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  provider->UpdateOrAddExtension(
      page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));
  WaitForInstallationAttemptToComplete(page_action);

  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(service()->IsExtensionEnabled(page_action));

  const Extension* extension =
      registry()->disabled_extensions().GetByID(page_action);
  EXPECT_TRUE(extension);
  EXPECT_EQ(page_action, extension->id());

  service()->EnableExtension(page_action);
  EXPECT_FALSE(HasExternalInstallErrors(service()));
  EXPECT_TRUE(service()->IsExtensionEnabled(page_action));
}

// As for components, only external component extensions can be disabled.
TEST_F(ExtensionServiceTest, DisablingComponentExtensions) {
  InitializeEmptyExtensionService();
  service_->Init();

  scoped_refptr<const Extension> external_component_extension = CreateExtension(
      "external_component_extension",
      base::FilePath(FILE_PATH_LITERAL("//external_component_extension")),
      ManifestLocation::kExternalComponent);
  service_->AddExtension(external_component_extension.get());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(
      external_component_extension->id()));
  service_->DisableExtension(external_component_extension->id(),
                             disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(
      external_component_extension->id()));

  scoped_refptr<const Extension> component_extension = CreateExtension(
      "component_extension",
      base::FilePath(FILE_PATH_LITERAL("//component_extension")),
      ManifestLocation::kComponent);
  service_->AddExtension(component_extension.get());
  EXPECT_TRUE(
      registry()->enabled_extensions().Contains(component_extension->id()));
  service_->DisableExtension(component_extension->id(),
                             disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(
      registry()->disabled_extensions().Contains(component_extension->id()));
}

// Test that installing multiple external extensions works.
// Flaky on windows; http://crbug.com/295757 .
// Causes race conditions with an in-process utility thread, so disable under
// TSan: https://crbug.com/518957
#if BUILDFLAG(IS_WIN) || defined(THREAD_SANITIZER)
#define MAYBE_ExternalInstallMultiple DISABLED_ExternalInstallMultiple
#else
#define MAYBE_ExternalInstallMultiple ExternalInstallMultiple
#endif
TEST_F(ExtensionServiceTest, MAYBE_ExternalInstallMultiple) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  provider->UpdateOrAddExtension(
      page_action, "1.0.0.0", data_dir().AppendASCII("page_action.crx"));
  provider->UpdateOrAddExtension(
      good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"));
  provider->UpdateOrAddExtension(
      theme_crx, "2.0", data_dir().AppendASCII("theme.crx"));

  {
    PendingExtensionManager* pending = service()->pending_extension_manager();
    PendingRemovalObserver page_action_observer(pending, page_action);
    PendingRemovalObserver good_crx_observer(pending, good_crx);
    PendingRemovalObserver theme_crx_observer(pending, theme_crx);

    service()->CheckForExternalUpdates();

    page_action_observer.WaitForRemoval();
    good_crx_observer.WaitForRemoval();
    theme_crx_observer.WaitForRemoval();

    EXPECT_TRUE(HasExternalInstallErrors(service()));
    EXPECT_FALSE(service()->IsExtensionEnabled(page_action));
    EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
    EXPECT_FALSE(service()->IsExtensionEnabled(theme_crx));
  }

  service()->EnableExtension(page_action);
  EXPECT_FALSE(GetError(page_action));
  EXPECT_TRUE(GetError(good_crx));
  EXPECT_TRUE(GetError(theme_crx));
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(HasExternalInstallBubble(service()));

  service()->EnableExtension(theme_crx);
  EXPECT_FALSE(GetError(page_action));
  EXPECT_FALSE(GetError(theme_crx));
  EXPECT_TRUE(GetError(good_crx));
  EXPECT_TRUE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(HasExternalInstallBubble(service()));

  service()->EnableExtension(good_crx);
  EXPECT_FALSE(GetError(page_action));
  EXPECT_FALSE(GetError(good_crx));
  EXPECT_FALSE(GetError(theme_crx));
  EXPECT_FALSE(HasExternalInstallErrors(service()));
  EXPECT_FALSE(HasExternalInstallBubble(service()));
}

TEST_F(ExtensionServiceTest, MultipleExternalInstallErrors) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);
  InitializeEmptyExtensionService();

  MockExternalProvider* reg_provider =
      AddMockExternalProvider(ManifestLocation::kExternalRegistry);

  std::string extension_info[][3] = {
      // {id, path, version}
      {good_crx, "1.0.0.0", "good.crx"},
      {page_action, "1.0.0.0", "page_action.crx"},
      {minimal_platform_app_crx, "0.1", "minimal_platform_app.crx"}};

  for (size_t i = 0; i < std::size(extension_info); ++i) {
    reg_provider->UpdateOrAddExtension(
        extension_info[i][0], extension_info[i][1],
        data_dir().AppendASCII(extension_info[i][2]));
    WaitForInstallationAttemptToComplete(extension_info[i][0]);
    const size_t expected_error_count = i + 1u;
    EXPECT_EQ(
        expected_error_count,
        service()->external_install_manager()->GetErrorsForTesting().size());
    EXPECT_FALSE(service()->IsExtensionEnabled(extension_info[i][0]));
  }

  std::string extension_ids[] = {
    extension_info[0][0], extension_info[1][0], extension_info[2][0]
  };

  // Each extension should end up in error.
  ASSERT_TRUE(GetError(extension_ids[0]));
  EXPECT_TRUE(GetError(extension_ids[1]));
  EXPECT_TRUE(GetError(extension_ids[2]));

  // Accept the first extension, this will remove the error associated with
  // this extension. Also verify the other errors still exist.
  GetError(extension_ids[0])
      ->OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ACCEPTED));
  EXPECT_FALSE(GetError(extension_ids[0]));
  ASSERT_TRUE(GetError(extension_ids[1]));
  EXPECT_TRUE(GetError(extension_ids[2]));

  // Abort the second extension.
  GetError(extension_ids[1])
      ->OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
  EXPECT_FALSE(GetError(extension_ids[0]));
  EXPECT_FALSE(GetError(extension_ids[1]));
  ASSERT_TRUE(GetError(extension_ids[2]));

  // Finally, re-enable the third extension, all errors should be removed.
  service()->EnableExtension(extension_ids[2]);
  EXPECT_FALSE(GetError(extension_ids[0]));
  EXPECT_FALSE(GetError(extension_ids[1]));
  EXPECT_FALSE(GetError(extension_ids[2]));

  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

// Regression test for crbug.com/739142. Verifies that no UAF occurs when
// ExternalInstallError needs to be deleted asynchronously.
TEST_F(ExtensionServiceTest, InstallPromptAborted) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);
  InitializeEmptyExtensionService();

  MockExternalProvider* reg_provider =
      AddMockExternalProvider(ManifestLocation::kExternalRegistry);

  reg_provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                     data_dir().AppendASCII("good.crx"));
  WaitForInstallationAttemptToComplete(good_crx);
  EXPECT_EQ(
      1u, service()->external_install_manager()->GetErrorsForTesting().size());
  EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
  EXPECT_TRUE(GetError(good_crx));

  // Abort the extension install prompt. This should cause the
  // ExternalInstallError to be deleted asynchronously.
  GetError(good_crx)->OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ABORTED));
  EXPECT_TRUE(GetError(good_crx));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetError(good_crx));

  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

TEST_F(ExtensionServiceTest, MultipleExternalInstallBubbleErrors) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);
  // This sets up the ExtensionPrefs used by our ExtensionService to be
  // post-first run.
  ExtensionServiceInitParams params;
  params.is_first_run = false;
  InitializeExtensionService(std::move(params));

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  std::vector<BubbleErrorsTestData> data;
  data.emplace_back(updates_from_webstore, "1",
                    temp_dir().GetPath().AppendASCII("webstore.crx"), 1u);
  data.emplace_back(updates_from_webstore2, "1",
                    temp_dir().GetPath().AppendASCII("webstore2.crx"), 2u);
  data.emplace_back(good_crx, "1.0.0.0", data_dir().AppendASCII("good.crx"),
                    2u);

  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"), data[0].crx_path);
  PackCRX(data_dir().AppendASCII("update_from_webstore2"),
          data_dir().AppendASCII("update_from_webstore2.pem"),
          data[1].crx_path);

  // Install extensions from |data| one by one and expect each of them to result
  // in an error. The first two extensions are from webstore, so they will
  // trigger BUBBLE_ALERT type errors. After each step, we verify that we got
  // the expected number of errors in external_install_manager(). We also verify
  // that only the first BUBBLE_ALERT error is shown.
  for (size_t i = 0; i < data.size(); ++i) {
    test::GlobalErrorWaiter error_waiter(profile());
    provider->UpdateOrAddExtension(data[i].id, data[i].version,
                                   data[i].crx_path);
    WaitForInstallationAttemptToComplete(data[i].id);
    // Make sure ExternalInstallError::OnDialogReady() fires.
    error_waiter.Wait();

    const size_t expected_error_count = i + 1u;
    std::vector<ExternalInstallError*> errors =
        service_->external_install_manager()->GetErrorsForTesting();
    EXPECT_EQ(expected_error_count, errors.size());
    EXPECT_EQ(data[i].expected_bubble_error_count,
              GetExternalInstallBubbleCount(service()));
    EXPECT_TRUE(service()
                    ->external_install_manager()
                    ->has_currently_visible_install_alert());
    // Make sure that the first error is only being shown.
    EXPECT_EQ(errors[0], service()
                             ->external_install_manager()
                             ->currently_visible_install_alert_for_testing());
    EXPECT_FALSE(service()->IsExtensionEnabled(data[i].id));
  }

  // Cancel all the install prompts.
  for (const auto& item : data) {
    const std::string& extension_id = item.id;
    EXPECT_TRUE(GetError(extension_id));
    GetError(extension_id)
        ->OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
            ExtensionInstallPrompt::Result::USER_CANCELED));
    EXPECT_FALSE(GetError(extension_id));
  }
  EXPECT_FALSE(service()
                   ->external_install_manager()
                   ->has_currently_visible_install_alert());
  EXPECT_EQ(0u, GetExternalInstallBubbleCount(service()));
  EXPECT_FALSE(HasExternalInstallErrors(service()));

  // Add a new webstore install. Verify that this shows an error bubble since
  // there are no error bubbles pending at this point. Also verify that the
  // error bubble is for this newly added extension.
  {
    base::FilePath webstore_crx_three =
        temp_dir().GetPath().AppendASCII("webstore3.crx");
    PackCRX(data_dir().AppendASCII("update_from_webstore3"),
            data_dir().AppendASCII("update_from_webstore3.pem"),
            webstore_crx_three);

    test::GlobalErrorWaiter error_waiter(profile());
    provider->UpdateOrAddExtension(
        updates_from_webstore3, "1",
        temp_dir().GetPath().AppendASCII("webstore3.crx"));
    WaitForInstallationAttemptToComplete(updates_from_webstore3);
    // Make sure ExternalInstallError::OnDialogReady() fires.
    error_waiter.Wait();

    std::vector<ExternalInstallError*> errors =
        service_->external_install_manager()->GetErrorsForTesting();
    EXPECT_EQ(1u, errors.size());
    EXPECT_EQ(1u, GetExternalInstallBubbleCount(service()));
    EXPECT_TRUE(service()
                    ->external_install_manager()
                    ->has_currently_visible_install_alert());
    // Verify that the visible alert is for the current error.
    EXPECT_EQ(errors[0], service()
                             ->external_install_manager()
                             ->currently_visible_install_alert_for_testing());
    EXPECT_FALSE(service()->IsExtensionEnabled(updates_from_webstore3));
  }
}

// Verifies that an error alert of type BUBBLE_ALERT does not replace an
// existing visible alert that was previously opened by clicking menu item.
TEST_F(ExtensionServiceTest, BubbleAlertDoesNotHideAnotherAlertFromMenu) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);
  // This sets up the ExtensionPrefs used by our ExtensionService to be
  // post-first run.
  ExtensionServiceInitParams params;
  params.is_first_run = false;
  InitializeExtensionService(std::move(params));

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);

  std::vector<BubbleErrorsTestData> data;
  data.emplace_back(updates_from_webstore, "1",
                    temp_dir().GetPath().AppendASCII("webstore.crx"), 1u);
  data.emplace_back(updates_from_webstore2, "1",
                    temp_dir().GetPath().AppendASCII("webstore2.crx"), 2u);

  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"), data[0].crx_path);
  PackCRX(data_dir().AppendASCII("update_from_webstore2"),
          data_dir().AppendASCII("update_from_webstore2.pem"),
          data[1].crx_path);
  {
    test::GlobalErrorWaiter error_waiter(profile());
    provider->UpdateOrAddExtension(data[0].id, data[0].version,
                                   data[0].crx_path);
    WaitForInstallationAttemptToComplete(data[0].id);
    // Make sure ExternalInstallError::OnDialogReady() fires.
    error_waiter.Wait();

    std::vector<ExternalInstallError*> errors =
        service_->external_install_manager()->GetErrorsForTesting();
    EXPECT_EQ(1u, errors.size());
    EXPECT_EQ(1u, GetExternalInstallBubbleCount(service()));
    EXPECT_TRUE(service()
                    ->external_install_manager()
                    ->has_currently_visible_install_alert());
    // Verify that the visible alert is for the current error.
    EXPECT_EQ(errors[0], service()
                             ->external_install_manager()
                             ->currently_visible_install_alert_for_testing());
  }

  ExternalInstallError* first_extension_error = GetError(data[0].id);

  // Close the bubble alert.
  GlobalError* global_error =
      GlobalErrorServiceFactory::GetForProfile(profile())
          ->GetHighestSeverityGlobalErrorWithAppMenuItem();
  first_extension_error->DidCloseBubbleView();

  // Bring the bubble alert error again by clicking its menu item.
  global_error->ExecuteMenuItem(nullptr);

  // Install another webstore extension that will trigger an error of type
  // BUBBLE_ALERT.
  // Make sure that this bubble alert does not replace the current bubble alert.
  {
    test::GlobalErrorWaiter error_waiter(profile());
    provider->UpdateOrAddExtension(data[1].id, data[1].version,
                                   data[1].crx_path);
    WaitForInstallationAttemptToComplete(data[1].id);
    // Make sure ExternalInstallError::OnDialogReady() fires.
    error_waiter.Wait();

    std::vector<ExternalInstallError*> errors =
        service_->external_install_manager()->GetErrorsForTesting();
    EXPECT_EQ(2u, errors.size());
    EXPECT_EQ(2u, GetExternalInstallBubbleCount(service()));
    EXPECT_TRUE(service()
                    ->external_install_manager()
                    ->has_currently_visible_install_alert());
    // Verify that the old bubble alert was *not* replaced by the new alert.
    EXPECT_EQ(first_extension_error,
              service()
                  ->external_install_manager()
                  ->currently_visible_install_alert_for_testing());
  }
}

// Test that there is a bubble for external extensions that update
// from the webstore if the profile is not new.
TEST_F(ExtensionServiceTest, ExternalInstallUpdatesFromWebstoreOldProfile) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  // This sets up the ExtensionPrefs used by our ExtensionService to be
  // post-first run.
  ExtensionServiceInitParams params;
  params.is_first_run = false;
  InitializeExtensionService(std::move(params));

  base::FilePath crx_path = temp_dir().GetPath().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);
  WaitForInstallationAttemptToComplete(updates_from_webstore);

  EXPECT_TRUE(HasExternalInstallErrors(service()));
  ASSERT_TRUE(GetError(updates_from_webstore));
  EXPECT_EQ(ExternalInstallError::BUBBLE_ALERT,
            GetError(updates_from_webstore)->alert_type());
  EXPECT_FALSE(service()->IsExtensionEnabled(updates_from_webstore));
}

// Test that there is no bubble for external extensions if the profile is new.
TEST_F(ExtensionServiceTest, ExternalInstallUpdatesFromWebstoreNewProfile) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  InitializeEmptyExtensionService();

  base::FilePath crx_path = temp_dir().GetPath().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);
  WaitForInstallationAttemptToComplete(updates_from_webstore);

  EXPECT_TRUE(HasExternalInstallErrors(service()));
  ASSERT_TRUE(GetError(updates_from_webstore));
  EXPECT_NE(ExternalInstallError::BUBBLE_ALERT,
            GetError(updates_from_webstore)->alert_type());
  EXPECT_FALSE(service()->IsExtensionEnabled(updates_from_webstore));
}

// Test that clicking to remove the extension on an external install warning
// uninstalls the extension.
TEST_F(ExtensionServiceTest, ExternalInstallClickToRemove) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  ExtensionServiceInitParams params;
  params.is_first_run = false;
  InitializeExtensionService(std::move(params));

  base::FilePath crx_path = temp_dir().GetPath().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);
  WaitForInstallationAttemptToComplete(updates_from_webstore);

  EXPECT_TRUE(HasExternalInstallErrors(service_));

  // We check both enabled and disabled, since these are "eventually exclusive"
  // sets.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(updates_from_webstore));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(updates_from_webstore));

  // Click the negative response.
  service_->external_install_manager()
      ->GetErrorsForTesting()[0]
      ->OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
  // The Extension should be uninstalled.
  EXPECT_FALSE(registry()->GetExtensionById(updates_from_webstore,
                                            ExtensionRegistry::EVERYTHING));
  // The error should be removed.
  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

// Test that clicking to keep the extension on an external install warning
// re-enables the extension.
TEST_F(ExtensionServiceTest, ExternalInstallClickToKeep) {
  FeatureSwitch::ScopedOverride prompt(
      FeatureSwitch::prompt_for_external_extensions(), true);

  ExtensionServiceInitParams params;
  params.is_first_run = false;
  InitializeExtensionService(std::move(params));

  base::FilePath crx_path = temp_dir().GetPath().AppendASCII("webstore.crx");
  PackCRX(data_dir().AppendASCII("update_from_webstore"),
          data_dir().AppendASCII("update_from_webstore.pem"),
          crx_path);

  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(updates_from_webstore, "1", crx_path);
  WaitForInstallationAttemptToComplete(updates_from_webstore);

  EXPECT_TRUE(HasExternalInstallErrors(service_));

  // We check both enabled and disabled, since these are "eventually exclusive"
  // sets.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(updates_from_webstore));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(updates_from_webstore));

  // Accept the extension.
  service_->external_install_manager()
      ->GetErrorsForTesting()[0]
      ->OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ACCEPTED));

  // It should be enabled again.
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(updates_from_webstore));
  EXPECT_FALSE(
      registry()->disabled_extensions().GetByID(updates_from_webstore));

  // The error should be removed.
  EXPECT_FALSE(HasExternalInstallErrors(service_));
}

// Test that the external install bubble only takes disabled extensions into
// account - enabled extensions, even those that weren't acknowledged, should
// not be warned about. This lets us grandfather extensions in.
TEST_F(ExtensionServiceTest,
       ExternalInstallBubbleDoesntShowForEnabledExtensions) {
  auto external_prompt_override =
      std::make_unique<FeatureSwitch::ScopedOverride>(
          FeatureSwitch::prompt_for_external_extensions(), false);
  InitializeEmptyExtensionService();

  // Register and install an external extension.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(good_crx, "1.0.0.0",
                                 data_dir().AppendASCII("good.crx"));

  WaitForExternalExtensionInstalled(good_crx);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(good_crx));
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));

  // We explicitly reset the override first. ScopedOverrides reset the value
  // to the original value on destruction, but if we reset by passing a new
  // object, the new object is constructed (overriding the current value)
  // before the old is destructed (which will immediately reset to the
  // original).
  external_prompt_override.reset();
  external_prompt_override = std::make_unique<FeatureSwitch::ScopedOverride>(
      FeatureSwitch::prompt_for_external_extensions(), true);

  ExternalInstallManager* external_manager =
      service()->external_install_manager();
  external_manager->UpdateExternalExtensionAlert();
  EXPECT_FALSE(external_manager->has_currently_visible_install_alert());
  EXPECT_TRUE(external_manager->GetErrorsForTesting().empty());

  provider->UpdateOrAddExtension(good_crx, "1.0.0.1",
                                 data_dir().AppendASCII("good2.crx"));

  WaitForExternalExtensionInstalled(good_crx);

  external_manager->UpdateExternalExtensionAlert();
  EXPECT_FALSE(external_manager->has_currently_visible_install_alert());
  EXPECT_TRUE(external_manager->GetErrorsForTesting().empty());
}

TEST_F(ExtensionServiceTest, InstallBlocklistedExtension) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  ASSERT_TRUE(extension.get());
  const std::string& id = extension->id();

  std::set<std::string> id_set;
  id_set.insert(id);

  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()));
  // Installation should be allowed but the extension should never have been
  // loaded and it should be blocklisted in prefs.
  service()->OnExtensionInstalled(
      extension.get(), syncer::StringOrdinal(),
      (kInstallFlagIsBlocklistedForMalware | kInstallFlagInstallImmediately));
  task_environment()->RunUntilIdle();

  // Extension was installed but not loaded.
  observer.WaitForExtensionWillBeInstalled();
  EXPECT_TRUE(registry()->GetInstalledExtension(id));

  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(id));

  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(
      id, ExtensionPrefs::Get(profile())));
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->IsBlocklistedExtensionAcknowledged(id));
}

// Test that we won't allow enabling a blocklisted extension.
TEST_F(ExtensionServiceTest, CannotEnableBlocklistedExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();
  ASSERT_FALSE(registry()->enabled_extensions().empty());

  // Blocklist the first extension; then try enabling it.
  std::string id = (*(registry()->enabled_extensions().begin()))->id();
  service()->BlocklistExtensionForTest(id);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  service()->EnableExtension(id);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(id));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(
      id, ExtensionPrefs::Get(profile())));

  service()->DisableExtension(id, disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(registry()->blocklisted_extensions().Contains(id));
  EXPECT_TRUE(blocklist_prefs::IsExtensionBlocklisted(
      id, ExtensionPrefs::Get(profile())));
}

// Test that calls to disable Shared Modules do not work.
TEST_F(ExtensionServiceTest, CannotDisableSharedModules) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Shared Module")
          .SetManifestPath("export.resources",
                           base::Value::List().Append("foo.js"))
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();

  service()->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(),
                                  kInstallFlagInstallImmediately);

  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  // Try to disable the extension.
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);
  // Shared Module should still be enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
}

// Make sure we can uninstall a blocklisted extension
TEST_F(ExtensionServiceTest, UninstallBlocklistedExtension) {
  InitializeGoodInstalledExtensionService();
  service()->Init();
  ASSERT_FALSE(registry()->enabled_extensions().empty());

  // Blocklist the first extension; then try uninstalling it.
  std::string id = (*(registry()->enabled_extensions().begin()))->id();
  service()->BlocklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));
  std::u16string error;
  EXPECT_TRUE(service()->UninstallExtension(id, UNINSTALL_REASON_USER_INITIATED,
                                            nullptr));
  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(id));
}

// Tests a profile being destroyed correctly disables extensions.
TEST_F(ExtensionServiceTest, DestroyingProfileClearsExtensions) {
  InitializeEmptyExtensionService();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_NE(UnloadedExtensionReason::PROFILE_SHUTDOWN, unloaded_reason());
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());

  service()->OnProfileMarkedForPermanentDeletion(profile());
  EXPECT_EQ(UnloadedExtensionReason::PROFILE_SHUTDOWN, unloaded_reason());
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(0u, registry()->disabled_extensions().size());
  EXPECT_EQ(0u, registry()->terminated_extensions().size());
  EXPECT_EQ(0u, registry()->blocklisted_extensions().size());
}

// Test that updating a corrupt extension removes the DISABLE_CORRUPTED disable
// reason.
TEST_F(ExtensionServiceTest, CorruptExtensionUpdate) {
  InitializeEmptyExtensionService();

  base::FilePath v1_path = data_dir().AppendASCII("good.crx");
  const Extension* v1 = InstallCRX(v1_path, INSTALL_NEW);
  std::string id = v1->id();

  service()->DisableExtension(id, disable_reason::DISABLE_CORRUPTED);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(id, disable_reason::DISABLE_CORRUPTED));

  base::FilePath v2_path = data_dir().AppendASCII("good2.crx");
  UpdateExtension(id, v2_path, ENABLED);

  EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(prefs->HasDisableReason(id, disable_reason::DISABLE_CORRUPTED));
}

// Try re-enabling a reloading extension. Regression test for crbug.com/676815.
TEST_F(ExtensionServiceTest, ReloadAndReEnableExtension) {
  InitializeEmptyExtensionService();

  // Add an extension in an unpacked location.
  scoped_refptr<const Extension> extension =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          data_dir().AppendASCII("simple_with_file"));
  const std::string kExtensionId = extension->id();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(Manifest::IsUnpackedLocation(extension->location()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtensionId));

  // Begin the reload process.
  service()->ReloadExtension(extension->id());
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtensionId));

  // While the extension is reloading, try to re-enable it. This is the flow
  // that could happen if, e.g., the user hit the enable toggle in the
  // chrome://extensions page while it was reloading.
  service()->GrantPermissionsAndEnableExtension(extension.get());
  EXPECT_FALSE(registry()->enabled_extensions().Contains(kExtensionId));

  // Wait for the reload to complete. This previously crashed (see
  // crbug.com/676815).
  task_environment()->RunUntilIdle();
  // The extension should be enabled again...
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtensionId));
  // ...and should have reloaded (for ease, we just compare the extension
  // objects).
  EXPECT_NE(extension, registry()->enabled_extensions().GetByID(kExtensionId));
}

// Test reloading a shared module. Regression test for crbug.com/676815.
TEST_F(ExtensionServiceTest, ReloadSharedModule) {
  InitializeEmptyExtensionService();

  // Add a shared module and an extension that depends on it (the latter is
  // important to ensure we don't remove the unused shared module).
  scoped_refptr<const Extension> shared_module =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          data_dir().AppendASCII("api_test/shared_module/shared"));
  scoped_refptr<const Extension> dependent =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          data_dir().AppendASCII("api_test/shared_module/import_pass"));
  ASSERT_TRUE(shared_module);
  ASSERT_TRUE(dependent);
  const std::string kExtensionId = shared_module->id();
  ASSERT_TRUE(Manifest::IsUnpackedLocation(shared_module->location()));
  ASSERT_EQ(Manifest::TYPE_SHARED_MODULE, shared_module->manifest()->type());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtensionId));

  // Reload the extension and wait for it to complete. This previously crashed
  // (see crbug.com/676815).
  service()->ReloadExtension(kExtensionId);
  task_environment()->RunUntilIdle();
  // The shared module should be enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtensionId));
}

// Tests that component extensions that have been migrated can be uninstalled.
TEST_F(ExtensionServiceTest, UninstallMigratedComponentExtensions) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs->ShouldInstallObsoleteComponentExtension(video_player_app));

  scoped_refptr<const Extension> video_player_extension =
      ExtensionBuilder("video player")
          .SetID(video_player_app)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  service()->AddComponentExtension(video_player_extension.get());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(video_player_app));

  service()->UninstallMigratedExtensionsForTest();
  EXPECT_FALSE(registry()->GetInstalledExtension(video_player_app));
  EXPECT_FALSE(
      prefs->ShouldInstallObsoleteComponentExtension(video_player_app));
}

// Tests that component extensions that are not marked as obsolete will not be
// uninstalled.
TEST_F(ExtensionServiceTest, UninstallMigratedExtensionsKeepsGoodComponents) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  scoped_refptr<const Extension> good_extension =
      ExtensionBuilder("good")
          .SetID(good0)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  service()->AddComponentExtension(good_extension.get());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(good0));

  service()->UninstallMigratedExtensionsForTest();
  // Because good0 is not a migrated component extension it should still be
  // currently installed, and should continue to be installed in the future.
  EXPECT_TRUE(registry()->GetInstalledExtension(good0));
  EXPECT_TRUE(prefs->ShouldInstallObsoleteComponentExtension(good0));
}

// Tests that repeat calls to UninstallMigratedExtensions doesn't crash/fail.
TEST_F(ExtensionServiceTest, UninstallMigratedExtensionsMultipleCalls) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  scoped_refptr<const Extension> video_player_extension =
      ExtensionBuilder("video player")
          .SetID(video_player_app)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  service()->AddComponentExtension(video_player_extension.get());

  service()->UninstallMigratedExtensionsForTest();
  service()->UninstallMigratedExtensionsForTest();
  service()->UninstallMigratedExtensionsForTest();
  EXPECT_FALSE(registry()->GetInstalledExtension(video_player_app));
}

// Tests the case of a user installing a non-policy extension (e.g. through the
// webstore), and that extension later becoming required by policy.
// Regression test for https://crbug.com/894184.
TEST_F(ExtensionServiceTest, UserInstalledExtensionThenRequiredByPolicy) {
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install an extension as if the user did it.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  EXPECT_EQ(good_crx, extension->id());
  EXPECT_EQ(ManifestLocation::kInternal, extension->location());

  std::string kVersionStr = "1.0.0.0";
  EXPECT_EQ(kVersionStr, extension->VersionString());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  // Require good.crx by policy.
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPolicyDownload);
  // TODO(devlin): Do we also need to check installing extensions with different
  // versions?
  provider->UpdateOrAddExtension(good_crx, kVersionStr,
                                 data_dir().AppendASCII("good.crx"));
  service()->CheckForExternalUpdates();

  ExtensionManagement* management =
      ExtensionManagementFactory::GetForBrowserContext(profile());
  ExtensionManagement::InstallationMode installation_mode =
      management->GetInstallationMode(extension);
  EXPECT_EQ(ExtensionManagement::INSTALLATION_FORCED, installation_mode);

  // Reload all extensions.
  service()->ReloadExtensionsForTest();

  extension = registry()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);
  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  // The extension should still be installed, and should be required to
  // remain installed.
  EXPECT_TRUE(policy->MustRemainInstalled(extension, nullptr));
  // TODO(devlin): This currently doesn't work, because the extension is still
  // installed with mojom::ManifestLocation kInternal.
  // EXPECT_FALSE(policy->UserMayModifySettings(extension, nullptr));

  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));
  EXPECT_FALSE(prefs->IsExtensionDisabled(good_crx));
}

// If the extension is first manually installed by the user, and then added to
// the force installed list, on restarting, the extension should behave as a
// force installed extension.
TEST_F(ExtensionServiceTest,
       UserInstalledExtensionThenRequiredByPolicyOnRestart) {
  // Mark as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  InitializeEmptyExtensionServiceWithTestingPrefs();

  // Install an extension as if the user did it.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  EXPECT_EQ(good_crx, extension->id());
  EXPECT_EQ(ManifestLocation::kInternal, extension->location());

  std::string kVersionStr = "1.0.0.0";
  EXPECT_EQ(kVersionStr, extension->VersionString());

  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Mark good.crx for force-installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", true);
  }

  ExtensionManagement* management =
      ExtensionManagementFactory::GetForBrowserContext(profile());
  ExtensionManagement::InstallationMode installation_mode =
      management->GetInstallationMode(extension);
  EXPECT_EQ(ExtensionManagement::INSTALLATION_FORCED, installation_mode);

  GURL good_update_url(kGoodUpdateURL);
  ExternalInstallInfoUpdateUrl info(
      good_crx, std::string(), std::move(good_update_url),
      ManifestLocation::kExternalPolicyDownload, Extension::NO_FLAGS, false);
  service()->OnExternalExtensionUpdateUrlFound(info, true);
  base::RunLoop().RunUntilIdle();

  extension = registry()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);
  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();

  // The extension should still be installed, and should be required to
  // remain installed.
  EXPECT_TRUE(policy->MustRemainInstalled(extension, nullptr));
  EXPECT_FALSE(policy->UserMayModifySettings(extension, nullptr));
  EXPECT_EQ(extension->location(), ManifestLocation::kExternalPolicyDownload);

  EXPECT_TRUE(registry()->enabled_extensions().GetByID(good_crx));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));
  EXPECT_FALSE(prefs->IsExtensionDisabled(good_crx));

  // Simulate a chrome process restart.
  service()->ReloadExtensionsForTest();
  policy = ExtensionSystem::Get(browser_context())->management_policy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good_crx));
  extension = registry()->GetInstalledExtension(good_crx);
  // The location should remain same on restart.
  EXPECT_EQ(extension->location(), ManifestLocation::kExternalPolicyDownload);
  // Extension should behave similar to force installed on restart.
  EXPECT_TRUE(policy->MustRemainInstalled(extension, nullptr));
  EXPECT_FALSE(policy->UserMayModifySettings(extension, nullptr));
}

TEST_F(ExtensionServiceTest, InstallingUnacknowledgedExternalExtension) {
  InitializeEmptyExtensionServiceWithTestingPrefs();
  {
    ManagementPrefUpdater pref(profile_->GetTestingPrefService());
    // Mark good.crx for recommended installation.
    pref.SetIndividualExtensionAutoInstalled(
        good_crx, "http://example.com/update_url", false);
  }

  base::FilePath path = data_dir().AppendASCII("good.crx");
  std::string version_str = "1.0.0.0";
  // Install an external extension.
  std::unique_ptr<ExternalInstallInfoFile> info = CreateExternalExtension(
      good_crx, version_str, path, ManifestLocation::kExternalPrefDownload,
      Extension::NO_FLAGS);
  MockExternalProvider* provider =
      AddMockExternalProvider(ManifestLocation::kExternalPrefDownload);
  provider->UpdateOrAddExtension(std::move(info));
  WaitForExternalExtensionInstalled(good_crx);

  const Extension* extension =
      registry()->enabled_extensions().GetByID(good_crx);
  ASSERT_TRUE(extension);
  EXPECT_EQ(good_crx, extension->id());
  EXPECT_EQ(ManifestLocation::kExternalPrefDownload, extension->location());
  EXPECT_EQ(version_str, extension->VersionString());

  ExtensionManagement::InstallationMode installation_mode =
      ExtensionManagementFactory::GetForBrowserContext(profile())
          ->GetInstallationMode(extension);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  EXPECT_EQ(ExtensionManagement::INSTALLATION_RECOMMENDED, installation_mode);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(good_crx));
  EXPECT_TRUE(prefs->IsExternalExtensionAcknowledged(extension->id()));
  EXPECT_EQ(disable_reason::DISABLE_NONE, prefs->GetDisableReasons(good_crx));
  EXPECT_FALSE(prefs->IsExtensionDisabled(good_crx));
}

// Regression test for crbug.com/979010.
TEST_F(ExtensionServiceTest, ReloadingExtensionFromNotification) {
  // Initialize a new extension.
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, extension->id());

  // Show the "Extension crashed" notification.
  base::RunLoop run_loop;
  NotificationDisplayServiceTester display_service(profile());
  display_service.SetNotificationAddedClosure(run_loop.QuitClosure());
  std::string notification_id = BackgroundContentsService::
      GetNotificationDelegateIdForExtensionForTesting(extension->id());
  BackgroundContentsService background_service(profile());
  background_service.ShowBalloonForTesting(extension);
  run_loop.Run();

  // Click on the "Extension crashed" notification and expect the extension to
  // be reloaded without a crash.
  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), extension->id());
  display_service.SimulateClick(NotificationHandler::Type::TRANSIENT,
                                notification_id, std::nullopt, std::nullopt);
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Regression test for crbug.com/460699. Ensure PluginManager doesn't crash even
// if OnExtensionUnloaded is invoked twice in succession.
TEST_F(ExtensionServiceTest, PluginManagerCrash) {
  InitializeEmptyExtensionService();
  PluginManager manager(profile());

  // Load an extension using a NaCl module.
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("native_client"), INSTALL_NEW);
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);

  // crbug.com/708230: This will cause OnExtensionUnloaded to be called
  // redundantly for a disabled extension.
  service()->BlockAllExtensions();
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// Test that blocking extension doesn't trigger unload notification for disabled
// extensions. (crbug.com/708230)
TEST_F(ExtensionServiceTest, BlockDisabledExtensionNotification) {
  // Initialize a new extension.
  InitializeEmptyExtensionService();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_EQ(good_crx, extension->id());

  // Disable the extension.
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);

  // Create observer
  MockExtensionRegistryObserver observer;
  registry()->AddObserver(&observer);

  // Block the extension
  service()->BlockAllExtensions();

  // Check that we didn't get unloading notification
  EXPECT_EQ(std::string(), observer.last_extension_unloaded);
  registry()->RemoveObserver(&observer);
}

class ExternalExtensionPriorityTest
    : public ExtensionServiceTest,
      public testing::WithParamInterface<ManifestLocation> {};

// Policy-forced extensions should be fetched with FOREGROUND priority,
// otherwise they may be throttled (web store sends “noupdate” response to
// reduce load), which is OK for updates, but not for a new install. This is
// a regression test for problems described in https://crbug.com/904600 and
// https://crbug.com/917700.
TEST_P(ExternalExtensionPriorityTest, PolicyForegroundFetch) {
  ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_checks;
  ExtensionServiceInitParams params;
  params.autoupdate_enabled = true;
  InitializeExtensionService(std::move(params));

  ExtensionDownloaderTestHelper helper;
  NullExtensionCache extension_cache;
  service()->updater()->SetExtensionDownloaderForTesting(
      helper.CreateDownloader());
  service()->updater()->SetExtensionCacheForTesting(&extension_cache);
  service()->updater()->Start();

  GURL update_url(extension_urls::kChromeWebstoreUpdateURL);
  service()->OnExternalExtensionUpdateUrlFound(
      ExternalInstallInfoUpdateUrl(all_zero /* extension_id */,
                                   "" /* install_parameter */, update_url,
                                   GetParam() /* download_location */,
                                   Extension::NO_FLAGS /* creation_flag */,
                                   true /* mark_acknowledged */),
      true /* force_update */);

  MockExternalProvider provider(nullptr,
                                ManifestLocation::kExternalPolicyDownload);
  service()->OnExternalProviderReady(&provider);

  task_environment()->RunUntilIdle();

  EXPECT_EQ(helper.test_url_loader_factory().NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      helper.test_url_loader_factory().GetPendingRequest(0);
  bool is_high_priority =
      GetParam() == ManifestLocation::kExternalPolicyDownload ||
      GetParam() == ManifestLocation::kExternalComponent;
  std::string expected_header = is_high_priority ? "fg" : "bg";
  EXPECT_EQ(expected_header, pending_request->request.headers.GetHeader(
                                 "X-Goog-Update-Interactivity"));

  // Destroy updater's downloader as it uses |helper|.
  service()->updater()->SetExtensionDownloaderForTesting(nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternalExtensionPriorityTest,
    testing::Values(ManifestLocation::kExternalPolicyDownload,
                    ManifestLocation::kExternalComponent,
                    ManifestLocation::kExternalPrefDownload));

}  // namespace extensions
