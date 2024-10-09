// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
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
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/fake_crx_installer.h"
#include "chrome/browser/extensions/mock_crx_installer.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/update_client/update_query_params.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/browser/updater/request_queue_impl.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/verifier_formats.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

using base::Time;
using content::BrowserThread;
using extensions::mojom::ManifestLocation;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using update_client::UpdateQueryParams;

namespace extensions {

using Error = ExtensionDownloaderDelegate::Error;
using PingResult = ExtensionDownloaderDelegate::PingResult;

namespace {

const net::BackoffEntry::Policy kNoBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  1000,

  // Initial delay for exponential back-off in ms.
  0,

  // Factor by which the waiting time will be multiplied.
  0,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  0,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

const char kAuthUserQueryKey[] = "authuser";

int kExpectedLoadFlags = net::LOAD_DISABLE_CACHE;

int kExpectedLoadFlagsForDownloadWithCookies = net::LOAD_DISABLE_CACHE;

// Fake authentication constants
const char kFakeOAuth2Token[] = "ce n'est pas un jeton";

// Extracts the integer value of the |authuser| query parameter. Returns 0 if
// the parameter is not set.
int GetAuthUserQueryValue(const GURL& url) {
  std::string_view query_piece = url.query_piece();
  url::Component query(0, query_piece.length());
  url::Component key, value;
  while (url::ExtractQueryKeyValue(query_piece, &query, &key, &value)) {
    std::string_view key_string = query_piece.substr(key.begin, key.len);
    if (key_string == kAuthUserQueryKey) {
      int user_index = 0;
      base::StringToInt(query_piece.substr(value.begin, value.len),
                        &user_index);
      return user_index;
    }
  }
  return 0;
}

class MockUpdateService : public UpdateService {
 public:
  MockUpdateService() : UpdateService(nullptr, nullptr) {}
  MOCK_CONST_METHOD0(IsBusy, bool());
  MOCK_METHOD3(SendUninstallPing,
               void(const std::string& id,
                    const base::Version& version,
                    int reason));
  MOCK_METHOD(void,
              StartUpdateCheck,
              (const ExtensionUpdateCheckParams& params,
               UpdateFoundCallback update_found_callback,
               base::OnceClosure callback),
              (override));
};

}  // namespace

// Base class for further specialized test classes.
class MockService : public TestExtensionService {
 public:
  explicit MockService(
      TestExtensionPrefs* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : prefs_(prefs),
        pending_extension_manager_(prefs->profile()),
        corrupted_extension_reinstaller_(prefs->profile()),
        downloader_delegate_override_(nullptr),
        test_shared_url_loader_factory_(url_loader_factory) {}

  MockService(const MockService&) = delete;
  MockService& operator=(const MockService&) = delete;

  ~MockService() override = default;

  PendingExtensionManager* pending_extension_manager() override {
    ADD_FAILURE() << "Subclass should override this if it will "
                  << "be accessed by a test.";
    return &pending_extension_manager_;
  }

  Profile* profile() { return prefs_->profile(); }

  ExtensionPrefs* extension_prefs() { return prefs_->prefs(); }

  PrefService* pref_service() { return prefs_->pref_service(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_.get();
  }

  CorruptedExtensionReinstaller* corrupted_extension_reinstaller() override {
    return &corrupted_extension_reinstaller_;
  }

  const CoreAccountId& account_id() { return account_info_.account_id; }

  // Creates test extensions and inserts them into list. The name and
  // version are all based on their index. If |update_url| is non-null, it
  // will be used as the update_url for each extension.
  // The |id| is used to distinguish extension names and make sure that
  // no two extensions share the same name.
  void CreateTestExtensions(int id,
                            int count,
                            ExtensionList* list,
                            const std::string* update_url,
                            ManifestLocation location) {
    for (int i = 1; i <= count; i++) {
      base::Value::Dict manifest;
      manifest.Set(manifest_keys::kVersion, base::StringPrintf("%d.0.0.0", i));
      manifest.Set(manifest_keys::kName,
                   base::StringPrintf("Extension %d.%d", id, i));
      manifest.Set(manifest_keys::kManifestVersion, 2);
      if (update_url)
        manifest.Set(manifest_keys::kUpdateURL, *update_url);
      scoped_refptr<Extension> e =
          prefs_->AddExtensionWithManifest(manifest, location);
      ASSERT_TRUE(e.get() != nullptr);
      list->push_back(e);
    }
  }

  ExtensionDownloader::Factory GetDownloaderFactory() {
    return base::BindRepeating(&MockService::CreateExtensionDownloader,
                               base::Unretained(this));
  }

  ExtensionDownloader::Factory GetAuthenticatedDownloaderFactory() {
    return base::BindRepeating(
        &MockService::CreateExtensionDownloaderWithIdentity,
        base::Unretained(this));
  }

  void OverrideDownloaderDelegate(ExtensionDownloaderDelegate* delegate) {
    downloader_delegate_override_ = delegate;
  }

 protected:
  const raw_ptr<TestExtensionPrefs, DanglingUntriaged> prefs_;
  PendingExtensionManager pending_extension_manager_;
  CorruptedExtensionReinstaller corrupted_extension_reinstaller_;

 private:
  std::unique_ptr<ExtensionDownloader> CreateExtensionDownloader(
      ExtensionDownloaderDelegate* delegate) {
    std::unique_ptr<ExtensionDownloader> downloader =
        ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
            test_shared_url_loader_factory_,
            downloader_delegate_override_ ? downloader_delegate_override_.get()
                                          : delegate,
            GetTestVerifierFormat());
    return downloader;
  }

  std::unique_ptr<ExtensionDownloader> CreateExtensionDownloaderWithIdentity(
      ExtensionDownloaderDelegate* delegate) {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    account_info_ = identity_test_env_->MakePrimaryAccountAvailable(
        "bobloblaw@lawblog.example.com", signin::ConsentLevel::kSync);

    std::unique_ptr<ExtensionDownloader> downloader(
        CreateExtensionDownloader(delegate));
    downloader->SetIdentityManager(identity_test_env_->identity_manager());
    return downloader;
  }

  AccountInfo account_info_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  raw_ptr<ExtensionDownloaderDelegate> downloader_delegate_override_;

  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

bool ShouldInstallExtensionsOnly(const Extension* extension,
                                 content::BrowserContext* context) {
  return extension->GetType() == Manifest::TYPE_EXTENSION;
}

bool ShouldInstallThemesOnly(const Extension* extension,
                             content::BrowserContext* context) {
  return extension->is_theme();
}

bool ShouldAlwaysInstall(const Extension* extension,
                         content::BrowserContext* context) {
  return true;
}

// Loads some pending extension records into a pending extension manager.
void SetupPendingExtensionManagerForTest(
    int count,
    const GURL& update_url,
    PendingExtensionManager* pending_extension_manager) {
  for (int i = 1; i <= count; ++i) {
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install =
        (i % 2 == 0) ? &ShouldInstallThemesOnly : &ShouldInstallExtensionsOnly;
    const bool kIsFromSync = true;
    const bool kMarkAcknowledged = false;
    const bool kRemoteInstall = false;
    std::string id =
        crx_file::id_util::GenerateId(base::StringPrintf("extension%i", i));

    pending_extension_manager->AddForTesting(PendingExtensionInfo(
        id, std::string(), update_url, base::Version(), should_allow_install,
        kIsFromSync, ManifestLocation::kInternal, Extension::NO_FLAGS,
        kMarkAcknowledged, kRemoteInstall));
  }
}

class ServiceForManifestTests : public MockService {
 public:
  explicit ServiceForManifestTests(
      TestExtensionPrefs* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : MockService(prefs, url_loader_factory),
        registry_(ExtensionRegistry::Get(profile())) {}

  ~ServiceForManifestTests() override = default;

  PendingExtensionManager* pending_extension_manager() override {
    return &pending_extension_manager_;
  }

  const Extension* GetPendingExtensionUpdate(
      const std::string& id) const override {
    return nullptr;
  }

  bool IsExtensionEnabled(const std::string& id) const override {
    return !registry_->disabled_extensions().Contains(id);
  }

  void set_extensions(ExtensionList extensions,
                      ExtensionList disabled_extensions,
                      ExtensionList blocklisted_extensions = ExtensionList()) {
    registry_->ClearAll();
    for (ExtensionList::const_iterator it = extensions.begin();
         it != extensions.end(); ++it) {
      registry_->AddEnabled(*it);
    }
    for (ExtensionList::const_iterator it = disabled_extensions.begin();
         it != disabled_extensions.end(); ++it) {
      registry_->AddDisabled(*it);
    }
    for (ExtensionList::const_iterator it = blocklisted_extensions.begin();
         it != blocklisted_extensions.end(); ++it) {
      registry_->AddBlocklisted(*it);
    }
  }

 private:
  raw_ptr<ExtensionRegistry> registry_;
};

class ServiceForDownloadTests : public MockService {
 public:
  explicit ServiceForDownloadTests(
      TestExtensionPrefs* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : MockService(prefs, url_loader_factory) {}

  // Add a fake crx installer to be returned by a call to
  // CreateUpdateInstaller() with a specific ID.
  void AddFakeCrxInstaller(const std::string& id,
                           scoped_refptr<CrxInstaller> crx_installer) {
    fake_crx_installers_[id] = crx_installer;
  }

  scoped_refptr<CrxInstaller> CreateUpdateInstaller(
      const CRXFileInfo& file,
      bool file_ownership_passed) override {
    extension_id_ = file.extension_id;
    install_path_ = file.path;

    if (base::Contains(fake_crx_installers_, extension_id_)) {
      return fake_crx_installers_[extension_id_];
    }

    return nullptr;
  }

  PendingExtensionManager* pending_extension_manager() override {
    return &pending_extension_manager_;
  }

  CorruptedExtensionReinstaller* corrupted_extension_reinstaller() override {
    return &corrupted_extension_reinstaller_;
  }

  const ExtensionId& extension_id() const { return extension_id_; }
  const base::FilePath& install_path() const { return install_path_; }

 private:
  // Hold the set of ids that CreateUpdateInstaller() returns.
  std::map<std::string, scoped_refptr<CrxInstaller>> fake_crx_installers_;

  ExtensionId extension_id_;
  base::FilePath install_path_;
  GURL download_url_;
};

static const int kUpdateFrequencySecs = 15;

// Takes a string with KEY=VALUE parameters separated by '&' in |params| and
// puts the key/value pairs into |result|. For keys with no value, the empty
// string is used. So for "a=1&b=foo&c", result would map "a" to "1", "b" to
// "foo", and "c" to "".
static void ExtractParameters(const std::string& params,
                              std::map<std::string, std::string>* result) {
  for (const std::string& pair : base::SplitString(
           params, "&", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<std::string> key_val = base::SplitString(
        pair, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (!key_val.empty()) {
      std::string key = key_val[0];
      EXPECT_TRUE(result->find(key) == result->end());
      (*result)[key] = (key_val.size() == 2) ? key_val[1] : std::string();
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

// Helper function to extract the ping data param values for each extension in
// a manifest fetch url, returned in a map keyed by extension id.
// E.g. for "x=id%3Dabcdef%26ping%3Ddr%253D1%2526dr%253D1024" we'd return
// {"abcdef": {"dr": set("1", "1024")}}
typedef std::map<std::string, std::set<std::string>> ParamsMap;
static std::map<std::string, ParamsMap> GetPingDataFromURL(
    const GURL& manifest_url) {
  std::map<std::string, ParamsMap> result;

  base::StringPairs toplevel_params;
  base::SplitStringIntoKeyValuePairs(
      manifest_url.query(), '=', '&', &toplevel_params);
  for (const auto& param : toplevel_params) {
    if (param.first != "x")
      continue;

    // We've found "x=<something>", now unescape <something> and look for
    // the "id=<id>&ping=<ping_value>" parameters within.
    std::string unescaped = base::UnescapeBinaryURLComponent(param.second);
    base::StringPairs extension_params;
    base::SplitStringIntoKeyValuePairs(unescaped, '=', '&', &extension_params);
    std::multimap<std::string, std::string> param_map;
    param_map.insert(extension_params.begin(), extension_params.end());
    if (base::Contains(param_map, "id") && base::Contains(param_map, "ping")) {
      std::string id = param_map.find("id")->second;
      result[id] = ParamsMap();

      // Pull the key=value pairs out of the ping parameter for this id and
      // put into the result.
      std::string ping =
          base::UnescapeBinaryURLComponent(param_map.find("ping")->second);
      base::StringPairs ping_params;
      base::SplitStringIntoKeyValuePairs(ping, '=', '&', &ping_params);
      for (const auto& ping_param : ping_params) {
        if (!base::Contains(result[id], ping_param.first))
          result[id][ping_param.first] = std::set<std::string>();
        result[id][ping_param.first].insert(ping_param.second);
      }
    }
  }
  return result;
}

static void VerifyQueryAndExtractParameters(
    const std::string& query,
    std::map<std::string, std::string>* result) {
  std::map<std::string, std::string> params;
  ExtractParameters(query, &params);

  std::string omaha_params = UpdateQueryParams::Get(UpdateQueryParams::CRX);
  std::map<std::string, std::string> expected;
  ExtractParameters(omaha_params, &expected);

  for (auto it = expected.begin(); it != expected.end(); ++it) {
    EXPECT_EQ(it->second, params[it->first]);
  }

  EXPECT_EQ(1U, params.count("x"));
  std::string decoded = base::UnescapeBinaryURLComponent(params["x"]);
  ExtractParameters(decoded, result);
}

// All of our tests that need to use private APIs of ExtensionUpdater live
// inside this class (which is a friend to ExtensionUpdater).
class ExtensionUpdaterTest : public testing::Test {
 public:
  ExtensionUpdaterTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    prefs_ = std::make_unique<TestExtensionPrefs>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    // Some tests create URLRequestContextGetters, whose destruction must run
    // on the IO thread. Make sure the IO loop spins before shutdown so that
    // those objects are released.
    RunUntilIdle();
    prefs_.reset();
  }

  void RunUntilIdle() {
    prefs_->pref_service()->CommitPendingWrite();
    base::RunLoop().RunUntilIdle();
  }

  void SimulateTimerFired(ExtensionUpdater* updater) { updater->NextCheck(); }

  void OverrideUpdateService(ExtensionUpdater* updater,
                             UpdateService* service) {
    updater->update_service_ = service;
  }

  bool CanUseUpdateService(ExtensionUpdater* updater,
                           const std::string& extension_id) {
    return updater->CanUseUpdateService(extension_id);
  }

  // Adds a Result with the given data to results.
  void AddParseResult(const std::string& id,
                      const std::string& version,
                      const std::string& url,
                      UpdateManifestResults* results) {
    UpdateManifestResult result;
    result.extension_id = id;
    result.version = version;
    result.crx_url = GURL(url);
    results->update_list.push_back(result);
  }

  void StartUpdateCheck(ExtensionDownloader* downloader,
                        ManifestFetchData* fetch_data) {
    downloader->StartUpdateCheck(
        std::unique_ptr<ManifestFetchData>(fetch_data));
  }

  size_t ManifestFetchersCount(ExtensionDownloader* downloader) {
    return downloader->manifests_queue_.size() +
           (downloader->HasActiveManifestRequestForTesting() ? 1 : 0);
  }

  std::set<std::string> GetRunningInstallIds(const ExtensionUpdater& updater) {
    std::set<std::string> ret;
    for (const auto& pair : updater.running_crx_installs_)
      ret.insert(pair.second.info.extension_id);
    return ret;
  }

  const DownloadFailure* GetFailureWithId(
      const std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>>&
          failures,
      const ExtensionId& id) {
    auto it = base::ranges::find(
        failures, id, [](const auto& failure) { return failure.first.id; });
    return it == failures.end() ? nullptr : &it->second;
  }

  void TestExtensionUpdateCheckRequests(bool pending) {
    // Create an extension with an update_url.
    ExtensionDownloaderTestHelper helper;
    ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
    std::string update_url("http://foo.com/bar");
    ExtensionList extensions;
    PendingExtensionManager* pending_extension_manager =
        service.pending_extension_manager();
    if (pending) {
      SetupPendingExtensionManagerForTest(1, GURL(update_url),
                                          pending_extension_manager);
    } else {
      service.CreateTestExtensions(1, 1, &extensions, &update_url,
                                   ManifestLocation::kInternal);
      service.set_extensions(extensions, ExtensionList());
    }

    // Set up and start the updater.
    ExtensionUpdater updater(&service, service.extension_prefs(),
                             service.pref_service(), service.profile(),
                             60 * 60 * 24, nullptr,
                             service.GetDownloaderFactory());
    updater.Start();

    // Tell the update that it's time to do update checks.
    SimulateTimerFired(&updater);

    // Get the url our loader was asked to fetch.
    const ManifestFetchData& fetch =
        *updater.downloader_->manifests_queue_.active_request();

    const GURL& url = fetch.full_url();

    EXPECT_FALSE(url.is_empty());
    EXPECT_TRUE(url.is_valid());
    EXPECT_TRUE(url.SchemeIs("http"));
    EXPECT_EQ("foo.com", url.host());
    EXPECT_EQ("/bar", url.path());

    // Validate the extension request parameters in the query. It should
    // look something like "x=id%3D<id>%26v%3D<version>%26uc".
    EXPECT_TRUE(url.has_query());
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(url.query(), &params);
    if (pending) {
      EXPECT_TRUE(pending_extension_manager->IsIdPending(params["id"]));
      EXPECT_EQ("0.0.0.0", params["v"]);
    } else {
      EXPECT_EQ(extensions[0]->id(), params["id"]);
      EXPECT_EQ(extensions[0]->VersionString(), params["v"]);
    }
    EXPECT_EQ("", params["uc"]);
  }

  void TestUpdateUrlDataEmpty() {
    ExtensionDownloaderTestHelper helper;
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an empty update URL data string does not cause a ap=
    // option to appear in the x= parameter.
    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    AddExtensionToFetchDataForTesting(fetch_data.get(), id, version,
                                      kUpdateURL);

    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(0U, params.count("ap"));
  }

  void TestUpdateUrlDataSimple() {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an update URL data string causes an appropriate ap=
    // option to appear in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(id, version,
                             &ExtensionDownloaderTestHelper::kNeverPingedData,
                             "bar", std::string(), ManifestLocation::kInternal,
                             DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ("bar", params["ap"]);
  }

  void TestUpdateUrlDataCompound() {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an update URL data string causes an appropriate ap=
    // option to appear in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(
        id, version, &ExtensionDownloaderTestHelper::kNeverPingedData,
        "a=1&b=2&c", std::string(), ManifestLocation::kInternal,
        DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ("a%3D1%26b%3D2%26c", params["ap"]);
  }

  void TestUpdateUrlDataFromUrl(
      const std::string& update_url,
      DownloadFetchPriority fetch_priority,
      int num_extensions,
      bool should_include_traffic_management_headers) {
    ExtensionDownloaderTestHelper helper;
    MockService service(prefs_.get(), helper.url_loader_factory());
    ExtensionList extensions;

    service.CreateTestExtensions(1, num_extensions, &extensions, &update_url,
                                 ManifestLocation::kInternal);

    for (int i = 0; i < num_extensions; ++i) {
      const std::string& id = extensions[i]->id();
      EXPECT_CALL(helper.delegate(), GetPingDataForExtension(id, _));

      helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
          id, ManifestURL::GetUpdateURL(extensions[i].get()),
          extensions[i]->location(), false, 0, fetch_priority,
          extensions[i]->version(), extensions[i]->GetType(), std::string()));
    }

    // Get the headers our loader was asked to fetch.
    base::RunLoop loop;
    net::HttpRequestHeaders last_request_headers;
    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_headers = request.headers;
          loop.Quit();
        }));

    helper.downloader().StartAllPending(nullptr);
    EXPECT_TRUE(helper.downloader().HasActiveManifestRequestForTesting());

    loop.Run();

    // Make sure that extensions that update from the gallery ignore any
    // update URL data.
    const ManifestFetchData& fetch =
        *helper.downloader().manifests_queue_.active_request();
    const std::string& fetcher_url = fetch.full_url().spec();
    std::string::size_type x = fetcher_url.find("x=");
    EXPECT_NE(std::string::npos, x);
    std::string::size_type ap = fetcher_url.find("ap%3D", x);
    EXPECT_EQ(std::string::npos, ap);

    net::HttpRequestHeaders fetch_headers;
    std::swap(fetch_headers, last_request_headers);
    EXPECT_EQ(should_include_traffic_management_headers,
              fetch_headers.HasHeader(
                  ExtensionDownloader::kUpdateInteractivityHeader));
    EXPECT_EQ(should_include_traffic_management_headers,
              fetch_headers.HasHeader(ExtensionDownloader::kUpdateAppIdHeader));
    EXPECT_EQ(
        should_include_traffic_management_headers,
        fetch_headers.HasHeader(ExtensionDownloader::kUpdateUpdaterHeader));

    if (should_include_traffic_management_headers) {
      std::string interactivity_value =
          fetch_headers
              .GetHeader(ExtensionDownloader::kUpdateInteractivityHeader)
              .value_or(std::string());

      std::string expected_interactivity_value =
          fetch_priority == DownloadFetchPriority::kForeground ? "fg" : "bg";
      EXPECT_EQ(expected_interactivity_value, interactivity_value);

      std::string appid_value =
          fetch_headers.GetHeader(ExtensionDownloader::kUpdateAppIdHeader)
              .value_or(std::string());
      if (num_extensions > 1) {
        for (int i = 0; i < num_extensions; ++i) {
          EXPECT_TRUE(
              testing::IsSubstring("", "", extensions[i]->id(), appid_value));
        }
      } else {
        EXPECT_EQ(extensions[0]->id(), appid_value);
      }

      std::string updater_value =
          fetch_headers.GetHeader(ExtensionDownloader::kUpdateUpdaterHeader)
              .value_or(std::string());
      const std::string expected_updater_value = base::StringPrintf(
          "%s-%s", UpdateQueryParams::GetProdIdString(UpdateQueryParams::CRX),
          UpdateQueryParams::GetProdVersion().c_str());
      EXPECT_EQ(expected_updater_value, updater_value);
    }
  }

  void TestInstallSource() {
    const std::string id(32, 'a');
    const std::string version = "1.0";
    const std::string install_source = "instally";

    // Make sure that an installsource= appears in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(
        id, version, &ExtensionDownloaderTestHelper::kNeverPingedData,
        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData, install_source,
        ManifestLocation::kInternal, DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(install_source, params["installsource"]);
  }

  void TestInstallLocation() {
    const std::string id(32, 'a');
    const std::string version = "1.0";
    const std::string install_location = "external";

    // Make sure that installedby= appears in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(
        id, version, &ExtensionDownloaderTestHelper::kNeverPingedData,
        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData, std::string(),
        ManifestLocation::kExternalPrefDownload,
        DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(install_location, params["installedby"]);
  }

  void TestDetermineUpdates() {
    ExtensionDownloaderTestHelper helper;

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));

    // Check passing an empty list of parse results to DetermineUpdates
    UpdateManifestResults updates;
    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    EXPECT_TRUE(updateable.empty());
    EXPECT_TRUE(failures.empty());

    // Create two updates - expect that DetermineUpdates will return the first
    // one (v1.0 installed, v1.1 available) but not the second one (both
    // installed and available at v2.0).
    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    AddExtensionToFetchDataForTesting(fetch_data.get(), id1, "1.0.0.0",
                                      kUpdateURL);
    AddParseResult(id1, "1.1", "http://localhost/e1_1.1.crx", &updates);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id2, "2.0.0.0",
                                      kUpdateURL);
    AddParseResult(id2, "2.0.0.0", "http://localhost/e2_2.0.crx", &updates);

    EXPECT_CALL(helper.delegate(), IsExtensionPending(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(helper.delegate(), GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.0.0.0"), Return(true)));
    EXPECT_CALL(helper.delegate(), GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("2.0.0.0"), Return(true)));

    updateable.clear();
    failures.clear();
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    ASSERT_EQ(1u, failures.size());
    EXPECT_EQ(id2, failures[0].first.id);
    EXPECT_EQ(ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
              failures[0].second.error);
    ASSERT_EQ(1u, updateable.size());
    EXPECT_EQ("1.1", updateable[0].second->version);
  }

  void TestDetermineUpdatesError() {
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    UpdateManifestResults updates;

    // id1 => updatable (current version (1.1) is older than update version).
    // id2 => non_updateable (current version (2.0.0.0) is the same as update
    //        version).
    // id3 => non_updateable (manifest update version is empty).
    // id4 => errors (|updates| doesn't contain id4).
    // id5 => errors (the extension is not currently installed).
    // id6 => errors (manifest update version is invalid).
    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    const std::string id3 = crx_file::id_util::GenerateId("3");
    const std::string id4 = crx_file::id_util::GenerateId("4");
    const std::string id5 = crx_file::id_util::GenerateId("5");
    const std::string id6 = crx_file::id_util::GenerateId("6");

    AddExtensionToFetchDataForTesting(fetch_data.get(), id1, "1.0.0.0",
                                      kUpdateURL);
    AddParseResult(id1, "1.1", "http://localhost/e1_1.1.crx", &updates);

    AddExtensionToFetchDataForTesting(fetch_data.get(), id2, "2.0.0.0",
                                      kUpdateURL);
    AddParseResult(id2, "2.0.0.0", "http://localhost/e2_2.0.crx", &updates);

    // Empty update version in manifest.
    AddExtensionToFetchDataForTesting(fetch_data.get(), id3, "0.0.0.0",
                                      kUpdateURL);
    AddParseResult(id3, "", "http://localhost/e3_3.0.crx", &updates);

    AddExtensionToFetchDataForTesting(fetch_data.get(), id4, "0.0.0.0",
                                      kUpdateURL);

    AddExtensionToFetchDataForTesting(fetch_data.get(), id5, "0.0.0.0",
                                      kUpdateURL);
    AddParseResult(id5, "5.0.0.0", "http://localhost/e5_5.0.crx", &updates);

    // Invalid update version in manifest.
    AddExtensionToFetchDataForTesting(fetch_data.get(), id6, "0.0.0.0",
                                      kUpdateURL);
    AddParseResult(id6, "invalid_version", "http://localhost/e6_6.0.crx",
                   &updates);

    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.0.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("2.0.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id3, _))
        .WillOnce(DoAll(SetArgPointee<1>("0.0.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id5, _))
        .WillOnce(DoAll(SetArgPointee<1>("0.0.0.0"), Return(false)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id6, _))
        .WillOnce(DoAll(SetArgPointee<1>("0.0.0.0"), Return(true)));

    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    std::vector<ExtensionId> ids_not_updateable({id2, id3});
    for (const auto& id : ids_not_updateable) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
                failure->error);
    }
    std::vector<ExtensionId> ids_with_error({id4, id5, id6});
    for (const auto& id : ids_with_error) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
                failure->error);
    }
    EXPECT_EQ(5u, failures.size());
    ASSERT_EQ(1u, updateable.size());
    EXPECT_EQ("1.1", updateable[0].second->version);
  }

  void TestDetermineUpdatesPending() {
    // Create a set of test extensions
    ExtensionDownloaderTestHelper helper;
    ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
    PendingExtensionManager* pending_extension_manager =
        service.pending_extension_manager();
    SetupPendingExtensionManagerForTest(3, GURL(), pending_extension_manager);

    MockExtensionDownloaderDelegate& delegate = helper.delegate();

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    UpdateManifestResults updates;

    std::list<std::string> ids_for_update_check =
        pending_extension_manager->GetPendingIdsForUpdateCheck();

    for (const std::string& id : ids_for_update_check) {
      AddExtensionToFetchDataForTesting(fetch_data.get(), id, "1.0.0.0",
                                        kUpdateURL);
      AddParseResult(id, "1.1", "http://localhost/e1_1.1.crx", &updates);
    }

    // The delegate will tell the downloader that all the extensions are
    // pending.
    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(true));

    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    // All the apps should be updateable.
    EXPECT_EQ(3u, updateable.size());
    EXPECT_TRUE(failures.empty());
  }

  void TestDetermineUpdatesDuplicates() {
    base::HistogramTester histogram_tester;
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();

    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    const std::string id3 = crx_file::id_util::GenerateId("3");
    const std::string id4 = crx_file::id_util::GenerateId("4");
    const std::string id5 = crx_file::id_util::GenerateId("5");
    const std::string id6 = crx_file::id_util::GenerateId("6");
    const std::string id7 = crx_file::id_util::GenerateId("7");

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    AddExtensionToFetchDataForTesting(fetch_data.get(), id1, "1.1.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id2, "1.2.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id3, "1.3.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id4, "1.4.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id5, "1.5.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id6, "1.6.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id7, "1.7.0.0",
                                      kUpdateURL);

    UpdateManifestResults updates;
    AddParseResult(id1, "1.1.0.0", "http://localhost/e1_1.1.crx", &updates);
    AddParseResult(id2, "1.2.0.a", "http://localhost/e2_1.1.crx", &updates);
    AddParseResult(id3, "1.3.1.0", "http://localhost/e3_1.1.crx", &updates);
    AddParseResult(id4, "1.4.0.0", "http://localhost/e4_1.1.crx", &updates);
    AddParseResult(id4, "1.4.0.a", "http://localhost/e4_1.1.crx", &updates);
    AddParseResult(id5, "1.5.0.a", "http://localhost/e5_1.1.crx", &updates);
    AddParseResult(id5, "1.5.0.b", "http://localhost/e5_1.1.crx", &updates);
    AddParseResult(id6, "1.6.0.a", "http://localhost/e6_1.1.crx", &updates);
    AddParseResult(id6, "1.6.0.0", "http://localhost/e6_1.1.crx", &updates);
    AddParseResult(id6, "1.6.1.0", "http://localhost/e6_1.1.crx", &updates);
    AddParseResult(id6, "1.6.2.0", "http://localhost/e6_1.1.crx", &updates);

    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.1.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.2.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id3, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.3.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id4, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.4.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id5, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.5.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id6, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.6.0.0"), Return(true)));

    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    std::vector<ExtensionId> ids_not_updateable({id1, id4});
    for (const auto& id : ids_not_updateable) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
                failure->error);
    }
    std::vector<ExtensionId> ids_with_error({id2, id5, id7});
    for (const auto& id : ids_with_error) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
                failure->error);
    }
    EXPECT_EQ(5u, failures.size());
    ASSERT_EQ(2u, updateable.size());
    EXPECT_EQ("1.3.1.0", updateable[0].second->version);
    EXPECT_EQ("1.6.1.0", updateable[1].second->version);
  }

  void TestMultipleManifestDownloading() {
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch1(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch2(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch3(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch4(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zeroDays(0, 0, true, 0);
    AddExtensionToFetchDataForTesting(fetch1.get(), "1111", "1.0", kUpdateUrl,
                                      zeroDays);
    AddExtensionToFetchDataForTesting(fetch2.get(), "2222", "2.0", kUpdateUrl,
                                      zeroDays);
    AddExtensionToFetchDataForTesting(fetch3.get(), "3333", "3.0", kUpdateUrl,
                                      zeroDays);
    AddExtensionToFetchDataForTesting(fetch4.get(), "4444", "4.0", kUpdateUrl,
                                      zeroDays);

    // This will start the first fetcher and queue the others. The next in queue
    // is started as each fetcher receives its response. Note that the fetchers
    // don't necessarily run in the order that they are started from here.
    GURL fetch1_url = fetch1->full_url();
    GURL fetch2_url = fetch2->full_url();
    GURL fetch3_url = fetch3->full_url();
    GURL fetch4_url = fetch4->full_url();

    // fetch1_url
    {
      helper.StartUpdateCheck(std::move(fetch1));
      RunUntilIdle();
      helper.test_url_loader_factory().AddResponse(fetch1_url.spec(), "",
                                                   net::HTTP_BAD_REQUEST);
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "1111", ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
              _, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch1_url = GURL();

      RunUntilIdle();
    }

    // fetch2_url
    {
      helper.StartUpdateCheck(std::move(fetch2));
      RunUntilIdle();
      const std::string kInvalidXml = "invalid xml";
      helper.test_url_loader_factory().AddResponse(fetch2_url.spec(),
                                                   kInvalidXml, net::HTTP_OK);
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "2222", ExtensionDownloaderDelegate::Error::MANIFEST_INVALID, _,
              _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch2_url = GURL();

      RunUntilIdle();
    }

    // fetch3_url
    {
      helper.StartUpdateCheck(std::move(fetch3));
      RunUntilIdle();
      const std::string kNoUpdate = CreateUpdateManifest(
          {UpdateManifestItem("3333")
               .version("3.0.0.0")
               .prodversionmin("3.0.0.0")
               .codebase("http://example.com/extension_3.0.0.0.crx")});
      helper.test_url_loader_factory().AddResponse(fetch3_url.spec(), kNoUpdate,
                                                   net::HTTP_OK);
      // The third fetcher doesn't have an update available.
      EXPECT_CALL(delegate, IsExtensionPending("3333")).WillOnce(Return(false));
      EXPECT_CALL(delegate, GetExtensionExistingVersion("3333", _))
          .WillOnce(DoAll(SetArgPointee<1>("3.0.0.0"), Return(true)));
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "3333", ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
              _, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch3_url = GURL();

      RunUntilIdle();
    }

    // fetch4_url
    {
      helper.StartUpdateCheck(std::move(fetch4));
      RunUntilIdle();
      // The last fetcher has an update.
      const std::string kUpdateAvailable = CreateUpdateManifest(
          {UpdateManifestItem("4444")
               .version("4.0.42.0")
               .prodversionmin("4.0.42.0")
               .codebase("http://example.com/extension_1.2.3.4.crx")});
      helper.test_url_loader_factory().AddResponse(
          fetch4_url.spec(), kUpdateAvailable, net::HTTP_OK);
      EXPECT_CALL(delegate, IsExtensionPending("4444")).WillOnce(Return(false));
      EXPECT_CALL(delegate, GetExtensionExistingVersion("4444", _))
          .WillOnce(DoAll(SetArgPointee<1>("4.0.0.0"), Return(true)));

      // Verify that the downloader decided to update this extension.
      EXPECT_CALL(delegate,
                  OnExtensionUpdateFound("4444", _, base::Version("4.0.42.0")))
          .WillOnce([&delegate]() { delegate.Quit(); });
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);

      fetch4_url = GURL();
    }
    if (helper.downloader().HasActiveManifestRequestForTesting())
      ADD_FAILURE() << "Unexpected load";
  }

  void TestManifestRetryDownloading() {
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zeroDays(0, 0, true, 0);
    fetch->AddExtension("1111", "1.0", &zeroDays,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    // This will start the first fetcher.
    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    // ExtensionDownloader should retry kMaxRetries times and then fail.
    EXPECT_CALL(
        delegate,
        OnExtensionDownloadFailed(
            "1111", ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
            _, _, _));
    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.load_flags == kExpectedLoadFlags);
          EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
                    request.credentials_mode);
        }));
    for (int i = 0; i <= ExtensionDownloader::kMaxRetries; ++i) {
      // All fetches will fail.
      auto* request = helper.GetPendingRequest(0);
      // Code 5xx causes ExtensionDownloader to retry.
      helper.test_url_loader_factory().SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
      RunUntilIdle();
    }
    Mock::VerifyAndClearExpectations(&delegate);

    // For response codes that are not in the 5xx range ExtensionDownloader
    // should not retry.
    fetch.reset(CreateManifestFetchData(kUpdateUrl));
    fetch->AddExtension("1111", "1.0", &zeroDays,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    // This will start the first fetcher.
    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    EXPECT_CALL(
        delegate,
        OnExtensionDownloadFailed(
            "1111", ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
            _, _, _));

    // The first fetch will fail, and require retrying.
    {
      auto* request = helper.GetPendingRequest(0);
      helper.test_url_loader_factory().SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    }
    RunUntilIdle();

    // The second fetch will fail with response 400 and should not cause
    // ExtensionDownloader to retry.
    {
      auto* request = helper.GetPendingRequest(0);
      helper.test_url_loader_factory().SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(net::HTTP_BAD_REQUEST), "");
    }
    RunUntilIdle();

    Mock::VerifyAndClearExpectations(&delegate);
  }

  void TestManifestCredentialsNonWebstore() {
    ExtensionDownloaderTestHelper helper;
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zeroDays(0, 0, true, 0);
    fetch->AddExtension("1111", "1.0", &zeroDays,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
                    request.credentials_mode);
        }));
    auto* request = helper.GetPendingRequest(0);
    helper.test_url_loader_factory().SimulateResponseForPendingRequest(
        request->request.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    RunUntilIdle();
  }

  void TestManifestCredentialsWebstore() {
    ExtensionDownloaderTestHelper helper;
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl(extension_urls::kChromeWebstoreUpdateURL);

    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zeroDays(0, 0, true, 0);
    fetch->AddExtension("1111", "1.0", &zeroDays,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
                    request.credentials_mode);
        }));
    auto* request = helper.GetPendingRequest(0);
    helper.test_url_loader_factory().SimulateResponseForPendingRequest(
        request->request.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    RunUntilIdle();
  }

  // Checks that the manifest is fetched with a priority of |net::MEDIUM| (which
  // maps to |TaskPriority::USER_BLOCKING| for the task runner) when the active
  // request's |fetch_priority| is in the FOREGROUND.
  void TestManifestFetchPriority(DownloadFetchPriority fetch_priority) {
    ExtensionDownloaderTestHelper helper;
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);
    GURL test_url("http://localhost/manifest1");
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(test_url));
    DownloadPingData zero_days(0, 0, true, 0);

    fetch->AddExtension("1111", "1.0", &zero_days,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        fetch_priority);

    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    auto* request = helper.GetPendingRequest(0);
    helper.test_url_loader_factory().SimulateResponseForPendingRequest(
        request->request.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    RunUntilIdle();

    if (fetch_priority == DownloadFetchPriority::kForeground) {
      EXPECT_EQ(net::MEDIUM, request->request.priority);
    } else {
      EXPECT_EQ(net::IDLE, request->request.priority);
    }
  }

  // Checks that the crx of a given extension is downloaded with a priority of
  // |net::MEDIUM| (maps to |TaskPriority::USER_BLOCKING| for the task runner)
  // when the active request's |fetch_priority| is in the FOREGROUND.
  void TestSingleExtensionDownloadingPriority(
      DownloadFetchPriority fetch_priority) {
    ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_checks;
    ExtensionDownloaderTestHelper helper;
    std::unique_ptr<ServiceForDownloadTests> service =
        std::make_unique<ServiceForDownloadTests>(prefs_.get(),
                                                  helper.url_loader_factory());
    ExtensionUpdater updater(service.get(), service->extension_prefs(),
                             service->pref_service(), service->profile(),
                             kUpdateFrequencySecs, nullptr,
                             service->GetDownloaderFactory());
    MockExtensionDownloaderDelegate delegate;
    delegate.DelegateTo(&updater);
    service->OverrideDownloaderDelegate(&delegate);
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL test_url("http://localhost/extension.crx");
    const std::string id(32, 'a');
    std::string hash;
    CRXFileInfo crx_file_info;
    base::Version version("0.0.1");
    ExtensionDownloaderTask task = CreateDownloaderTask(id);
    task.fetch_priority = fetch_priority;
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            std::move(task), test_url, hash, version.GetString(),
            fetch_priority);

    updater.downloader_->FetchUpdatedExtension(std::move(fetch), std::nullopt);

    auto* request = helper.GetPendingRequest(0);
    if (fetch_priority == DownloadFetchPriority::kForeground) {
      EXPECT_EQ(net::MEDIUM, request->request.priority);
    } else {
      EXPECT_EQ(net::IDLE, request->request.priority);
    }
  }

  void TestSingleExtensionDownloading(bool pending, bool retry, bool fail) {
    ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_checks;
    ExtensionDownloaderTestHelper helper;
    std::unique_ptr<ServiceForDownloadTests> service =
        std::make_unique<ServiceForDownloadTests>(prefs_.get(),
                                                  helper.url_loader_factory());
    ExtensionUpdater updater(service.get(), service->extension_prefs(),
                             service->pref_service(), service->profile(),
                             kUpdateFrequencySecs, nullptr,
                             service->GetDownloaderFactory());
    MockExtensionDownloaderDelegate delegate;
    delegate.DelegateTo(&updater);
    service->OverrideDownloaderDelegate(&delegate);
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL test_url("http://localhost/extension.crx");

    const std::string id(32, 'a');
    std::string hash;
    CRXFileInfo crx_file_info;
    base::Version version("0.0.1");
    std::set<int> requests;
    requests.insert(0);
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id), test_url, hash, version.GetString(),
            DownloadFetchPriority::kBackground);
    updater.downloader_->FetchUpdatedExtension(std::move(fetch), std::nullopt);

    if (pending) {
      const bool kIsFromSync = true;
      const bool kMarkAcknowledged = false;
      const bool kRemoteInstall = false;
      PendingExtensionManager* pending_extension_manager =
          service->pending_extension_manager();
      pending_extension_manager->AddForTesting(PendingExtensionInfo(
          id, std::string(), test_url, version, &ShouldAlwaysInstall,
          kIsFromSync, ManifestLocation::kInternal, Extension::NO_FLAGS,
          kMarkAcknowledged, kRemoteInstall));
    }

    if (retry) {
      EXPECT_CALL(delegate, OnExtensionDownloadRetryForTests())
          .WillOnce(DoAll(
              InvokeWithoutArgs(&delegate,
                                &MockExtensionDownloaderDelegate::Quit),
              InvokeWithoutArgs(&helper, &ExtensionDownloaderTestHelper::
                                             ClearURLLoaderFactoryResponses)));
      helper.test_url_loader_factory().AddResponse(
          test_url.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);
      delegate.Wait();
      EXPECT_TRUE(updater.downloader_->extension_loader_);
    }

    if (fail) {
      EXPECT_CALL(delegate, OnExtensionDownloadFailed(id, _, _, requests, _))
          .WillOnce(DoAll(
              InvokeWithoutArgs(&delegate,
                                &MockExtensionDownloaderDelegate::Quit),
              InvokeWithoutArgs(&helper, &ExtensionDownloaderTestHelper::
                                             ClearURLLoaderFactoryResponses)));
      helper.test_url_loader_factory().AddResponse(
          test_url.spec(), "Any content. It is irrelevant.",
          net::HTTP_NOT_FOUND);
      delegate.Wait();
    } else {
      EXPECT_TRUE(updater.downloader_->extension_loader_);
      EXPECT_CALL(delegate,
                  OnExtensionDownloadFinished_(_, _, _, _, requests, _))
          .WillOnce(
              DoAll(testing::SaveArg<0>(&crx_file_info),
                    InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit)));
      helper.test_url_loader_factory().AddResponse(
          test_url.spec(), "Any content. It is irrelevant.");
      delegate.Wait();
      EXPECT_EQ(version, crx_file_info.expected_version);
    }

    if (fail) {
      // Don't expect any extension to have been installed.
      EXPECT_TRUE(service->extension_id().empty());
    } else {
      // Expect that ExtensionUpdater asked the mock extensions service to
      // install a file with the test data for the right id.
      EXPECT_EQ(id, crx_file_info.extension_id);
      base::FilePath tmpfile_path = crx_file_info.path;
      EXPECT_FALSE(tmpfile_path.empty());
    }
  }

  // Helper to create a new test file of zeros.
  void CreateFile(const base::FilePath& file,
                  size_t size,
                  const base::Time& timestamp) {
    const std::string data(size, 0);
    EXPECT_TRUE(base::WriteFile(file, data));
    EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This tests the condition when the entry for the crx file is already
  // present in the cache but the crx file is itself corrupted. In this case,
  // after detecting the corruption of the crx file, it's entry should be
  // removed from the cache and re-downloaded from the link in the update
  // manifest.
  void TestCacheCorruption() {
    const char kTestExtensionId[] = "test_app";
    const std::string version = "1.1";
    const std::string hash = "abcd";
    ExtensionDownloaderTestHelper helper;

    // Set update manifest fetch data and result.
    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateURL));
    AddExtensionToFetchDataForTesting(fetch.get(), kTestExtensionId, "1.0",
                                      kUpdateURL);
    const std::string manifest = CreateUpdateManifest(
        {UpdateManifestItem(kTestExtensionId)
             .version(version)
             .hash(hash)
             .codebase("http://example.com/extension_1.2.3.4.crx")
             .prodversionmin("1.1")});
    helper.test_url_loader_factory().AddResponse(fetch->full_url().spec(),
                                                 manifest, net::HTTP_OK);

    // We need crx installer to install the crx file and also for mock extension
    // service. CrxInstallers require a real ExtensionService.  Create one on
    // the testing profile.  Any action the CrxInstallers take is on the testing
    // profile's extension service, not on our mock |service|.
    TestingProfile profile;
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile))
        ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                 base::FilePath(), false);
    ExtensionService* extension_service =
        ExtensionSystem::Get(&profile)->extension_service();
    scoped_refptr<MockCrxInstaller> mock_installer =
        base::MakeRefCounted<MockCrxInstaller>(extension_service);

    // Do nothing when called the first time, by ExtensionUpdater
    // But do the real action when called later on by this test code.
    EXPECT_CALL(*mock_installer, InstallCrxFile(_))
        .WillOnce(Return())
        .WillOnce([&mock_installer](const CRXFileInfo& info) {
          return mock_installer->CrxInstaller::InstallCrxFile(info);
        });
    // Just let the real CrxInstaller implementation have the callback.
    EXPECT_CALL(*mock_installer, AddInstallerCallback(_))
        .WillOnce(Invoke([&](CrxInstaller::InstallerResultCallback callback) {
          mock_installer->CrxInstaller::AddInstallerCallback(
              std::move(callback));
        }));

    mock_installer->set_expected_id(kTestExtensionId);
    mock_installer->set_expected_hash(hash);

    // Create mock extension service for test. We need this mock service so that
    // the extension updater process can be intercepted before the installer
    // which is then called explicitly.
    std::unique_ptr<ServiceForDownloadTests> service =
        std::make_unique<ServiceForDownloadTests>(prefs_.get(),
                                                  helper.url_loader_factory());
    service->AddFakeCrxInstaller(kTestExtensionId, mock_installer);

    ExtensionUpdater updater(service.get(), service->extension_prefs(),
                             service->pref_service(), service->profile(),
                             kUpdateFrequencySecs, nullptr,
                             service->GetDownloaderFactory());
    MockExtensionDownloaderDelegate& delegate = helper.delegate();
    delegate.DelegateTo(&updater);
    service->OverrideDownloaderDelegate(&delegate);
    updater.Start();

    // Create and initialize local cache.
    const base::Time now = base::Time::Now();
    base::ScopedTempDir cache_dir;
    ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
    const base::FilePath cache_path = cache_dir.GetPath();
    CreateFile(cache_path.Append(LocalExtensionCache::kCacheReadyFlagFileName),
               0, now);

    ExtensionCacheImpl test_extension_cache(
        std::make_unique<ChromeOSExtensionCacheDelegate>(cache_path));
    base::RunLoop cache_init_run_loop;
    test_extension_cache.Start(cache_init_run_loop.QuitClosure());
    cache_init_run_loop.Run();

    // Create crx file in a temp directory.
    base::ScopedTempDir tmp_dir;
    ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
    const base::FilePath tmp_path = tmp_dir.GetPath();
    const base::FilePath filename =
        tmp_path.Append(LocalExtensionCache::ExtensionFileName(
            kTestExtensionId, version, "" /* hash */));
    // Create a small file of zeroes, e.g. 100 bytes size.
    CreateFile(filename, 100, now - base::Seconds(3));

    // Add crx file entry in the cache.
    base::RunLoop put_extension_run_loop;
    test_extension_cache.AllowCaching("test_app");
    test_extension_cache.PutExtension(
        kTestExtensionId, "" /* expected hash*/, filename, version,
        base::BindLambdaForTesting(
            [&put_extension_run_loop](const base::FilePath& file_path,
                                      bool file_ownership_passed) {
              put_extension_run_loop.Quit();
            }));
    put_extension_run_loop.Run();

    // Set cache in extension downloader.
    helper.downloader().StartAllPending(&test_extension_cache);

    EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
        .WillOnce(Return(true));
    // Download the update manifest for the extension, find the same extension
    // version in the cache, start installing the cached crx file which fails
    // due to unpacker error and is hence, removed from the cache and
    // re-downlaoded for installation.
    testing::Sequence sequence;
    EXPECT_CALL(delegate,
                OnExtensionDownloadStageChanged(
                    kTestExtensionId,
                    ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST))
        .Times(testing::AnyNumber());
    EXPECT_CALL(delegate,
                OnExtensionDownloadStageChanged(
                    kTestExtensionId,
                    ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST))
        .InSequence(sequence);
    EXPECT_CALL(delegate,
                OnExtensionDownloadStageChanged(
                    kTestExtensionId,
                    ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST))
        .InSequence(sequence);
    EXPECT_CALL(delegate,
                OnExtensionDownloadStageChanged(
                    kTestExtensionId,
                    ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED))
        .InSequence(sequence);
    EXPECT_CALL(delegate,
                OnExtensionDownloadCacheStatusRetrieved(
                    kTestExtensionId,
                    ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT))
        .InSequence(sequence);
    EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                              kTestExtensionId,
                              ExtensionDownloaderDelegate::Stage::FINISHED))
        .InSequence(sequence);
    EXPECT_CALL(delegate,
                OnExtensionDownloadStageChanged(
                    kTestExtensionId,
                    ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX))
        .InSequence(sequence);

    helper.StartUpdateCheck(std::move(fetch));

    content::RunAllTasksUntilIdle();

    LoadErrorReporter::Init(false);

    updater.SetExtensionCacheForTesting(&test_extension_cache);
    CRXFileInfo crx_info(filename, GetTestVerifierFormat());
    crx_info.extension_id = kTestExtensionId;
    crx_info.expected_hash = hash;

    // This time call the real InstallCrxFile implementation (see expectation
    // set above for mock_installer).
    mock_installer->InstallCrxFile(crx_info);

    content::RunAllTasksUntilIdle();

    testing::Mock::VerifyAndClearExpectations(&delegate);
  }
#endif

  // Update a single extension in an environment where the download request
  // initially responds with a 403 status. If |identity_provider| is not NULL,
  // this will first expect a request which includes an Authorization header
  // with an OAuth2 bearer token; otherwise, or if OAuth2 failure is simulated,
  // this expects the downloader to fall back onto cookie-based credentials.
  void TestProtectedDownload(
      const std::string& url_prefix,
      bool enable_oauth2,
      bool succeed_with_oauth2,
      int valid_authuser,
      int max_authuser) {
    ExtensionDownloaderTestHelper helper;
    std::unique_ptr<ServiceForDownloadTests> service =
        std::make_unique<ServiceForDownloadTests>(prefs_.get(),
                                                  helper.url_loader_factory());
    const ExtensionDownloader::Factory& downloader_factory =
        enable_oauth2 ? service->GetAuthenticatedDownloaderFactory()
            : service->GetDownloaderFactory();
    ExtensionUpdater updater(service.get(), service->extension_prefs(),
                             service->pref_service(), service->profile(),
                             kUpdateFrequencySecs, nullptr, downloader_factory);

    MockExtensionDownloaderDelegate delegate;
    delegate.DelegateTo(&updater);
    service->OverrideDownloaderDelegate(&delegate);

    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL test_url(base::StringPrintf("%s/extension.crx", url_prefix.c_str()));

    const std::string id(32, 'a');
    std::string hash;
    base::Version version("0.0.1");
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> extension_fetch =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id), test_url, hash, version.GetString(),
            DownloadFetchPriority::kBackground);
    updater.downloader_->FetchUpdatedExtension(std::move(extension_fetch),
                                               std::nullopt);

    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    // Fake a 403 response.
    EXPECT_CALL(delegate, OnExtensionDownloadRetryForTests())
        .WillOnce(DoAll(
            InvokeWithoutArgs(&delegate,
                              &MockExtensionDownloaderDelegate::Quit),
            InvokeWithoutArgs(&helper, &ExtensionDownloaderTestHelper::
                                           ClearURLLoaderFactoryResponses)));
    helper.test_url_loader_factory().AddResponse(test_url.spec(), "",
                                                 net::HTTP_FORBIDDEN);
    delegate.Wait();

    // Only call out to WaitForAccessTokenRequest(...) method below if
    // HTTPS is in use in a google domain and oauth is explicitly enabled.
    // Otherwise, test will await an access token request that have not
    // (and will not) happen.
    //
    // Note that in case the condition below isn't satisfied, the download
    // proceeds normally, but the request does not carry an 'Authorization'
    // HTTP header.
    if (enable_oauth2 && test_url.DomainIs("google.com") &&
        test_url.SchemeIsCryptographic()) {
      service->identity_test_env()
          ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
              service->account_id(), kFakeOAuth2Token, base::Time::Now());
    }

    bool using_oauth2 = false;
    int expected_load_flags = kExpectedLoadFlags;

    // Verify that the fetch has had its credentials properly incremented.
    EXPECT_TRUE(updater.downloader_->extension_loader_);
    net::HttpRequestHeaders fetch_headers =
        updater.downloader_
            ->last_extension_loader_resource_request_headers_for_testing_;
    // If the download URL is not https, no credentials should be provided.
    if (!test_url.SchemeIsCryptographic()) {
      // No cookies.
      EXPECT_EQ(
          kExpectedLoadFlags,
          updater.downloader_->last_extension_loader_load_flags_for_testing_);
      // No Authorization header.
      EXPECT_FALSE(
          fetch_headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
      expected_load_flags = kExpectedLoadFlags;
    } else {
      // HTTPS is in use, so credentials are allowed.
      if (enable_oauth2 && test_url.DomainIs("google.com")) {
        // If an IdentityProvider is present and the URL is a google.com
        // URL, the fetcher should be in OAuth2 mode after the intitial
        // challenge.
        EXPECT_TRUE(fetch_headers.HasHeader(
            net::HttpRequestHeaders::kAuthorization));
        std::string expected_header_value = base::StringPrintf("Bearer %s",
            kFakeOAuth2Token);
        std::string actual_header_value =
            fetch_headers.GetHeader(net::HttpRequestHeaders::kAuthorization)
                .value_or(std::string());
        EXPECT_EQ(expected_header_value, actual_header_value);
        using_oauth2 = true;
      } else {
        // No IdentityProvider (or no google.com), so expect cookies instead of
        // an Authorization header.
        EXPECT_FALSE(fetch_headers.HasHeader(
            net::HttpRequestHeaders::kAuthorization));
        EXPECT_EQ(
            kExpectedLoadFlagsForDownloadWithCookies,
            updater.downloader_->last_extension_loader_load_flags_for_testing_);
        expected_load_flags = kExpectedLoadFlagsForDownloadWithCookies;
      }
    }

    bool success = false;
    if (using_oauth2) {
      if (succeed_with_oauth2) {
        success = true;
      } else {
        // Simulate OAuth2 failure and ensure that we fall back on cookies.
        EXPECT_CALL(delegate, OnExtensionDownloadRetryForTests())
            .WillOnce(
                DoAll(InvokeWithoutArgs(&delegate,
                                        &MockExtensionDownloaderDelegate::Quit),
                      InvokeWithoutArgs(&helper,
                                        &ExtensionDownloaderTestHelper::
                                            ClearURLLoaderFactoryResponses)));
        helper.test_url_loader_factory().AddResponse(test_url.spec(), "",
                                                     net::HTTP_FORBIDDEN);
        delegate.Wait();

        const ExtensionDownloader::ExtensionFetch& fetch =
            *updater.downloader_->extensions_queue_.active_request();
        EXPECT_EQ(0, GetAuthUserQueryValue(fetch.url));
        EXPECT_EQ(ExtensionDownloader::ExtensionFetch::CREDENTIALS_COOKIES,
                  fetch.credentials);

        EXPECT_TRUE(updater.downloader_->extension_loader_);
        fetch_headers =
            updater.downloader_
                ->last_extension_loader_resource_request_headers_for_testing_;
        EXPECT_FALSE(
            fetch_headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
        EXPECT_EQ(
            kExpectedLoadFlagsForDownloadWithCookies,
            updater.downloader_->last_extension_loader_load_flags_for_testing_);
        expected_load_flags = kExpectedLoadFlagsForDownloadWithCookies;
      }
    }

    if (!success) {
      // Not yet ready to simulate a successful fetch. At this point we begin
      // simulating cookie-based authentication with increasing values of
      // authuser (starting from 0.)
      int user_index = 0;
      for (; user_index <= max_authuser; ++user_index) {
        const ExtensionDownloader::ExtensionFetch& fetch =
            *updater.downloader_->extensions_queue_.active_request();
        EXPECT_EQ(user_index, GetAuthUserQueryValue(fetch.url));
        if (user_index == valid_authuser) {
          success = true;
          break;
        }
        // Simulate an authorization failure which should elicit an increment
        // of the authuser value.
        EXPECT_TRUE(updater.downloader_->extension_loader_);
        EXPECT_EQ(
            expected_load_flags,
            updater.downloader_->last_extension_loader_load_flags_for_testing_);
        EXPECT_CALL(delegate, OnExtensionDownloadRetryForTests())
            .WillOnce(
                DoAll(InvokeWithoutArgs(&delegate,
                                        &MockExtensionDownloaderDelegate::Quit),
                      InvokeWithoutArgs(&helper,
                                        &ExtensionDownloaderTestHelper::
                                            ClearURLLoaderFactoryResponses)));
        helper.test_url_loader_factory().AddResponse(
            fetch.url.spec(), "whatever", net::HTTP_FORBIDDEN);
        delegate.Wait();
      }

      // Simulate exhaustion of all available authusers.
      if (!success && user_index > max_authuser) {
        const ExtensionDownloader::ExtensionFetch& fetch =
            *updater.downloader_->extensions_queue_.active_request();
        EXPECT_TRUE(updater.downloader_->extension_loader_);
        helper.test_url_loader_factory().AddResponse(
            fetch.url.spec(), std::string(), net::HTTP_UNAUTHORIZED);
        EXPECT_CALL(delegate, OnExtensionDownloadFailed(_, _, _, _, _))
            .WillOnce(InvokeWithoutArgs(
                &delegate, &MockExtensionDownloaderDelegate::Quit));
        delegate.Wait();
      }
    }

    // Simulate successful authorization with a 200 response.
    if (success) {
      EXPECT_TRUE(updater.downloader_->extension_loader_);
      const ExtensionDownloader::ExtensionFetch& fetch =
          *updater.downloader_->extensions_queue_.active_request();

      CRXFileInfo crx_file_info;
      EXPECT_CALL(delegate, OnExtensionDownloadFinished_(_, _, _, _, _, _))
          .WillOnce(
              DoAll(testing::SaveArg<0>(&crx_file_info),
                    InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit)));
      helper.test_url_loader_factory().AddResponse(fetch.url.spec(),
                                                   "whatever");
      delegate.Wait();

      // Verify installation would proceed as normal.
      EXPECT_EQ(id, crx_file_info.extension_id);
      base::FilePath tmpfile_path = crx_file_info.path;
      EXPECT_FALSE(tmpfile_path.empty());
    }
  }

  // Two extensions are updated.  If |updates_start_running| is true, the
  // mock extensions service has CreateUpdateInstaller(...) return the
  // fake CrxInstallers created by the test.  Otherwise, CreateUpdateInstaller()
  // returns nullptr, signaling install failures.
  void TestMultipleExtensionDownloading(bool updates_start_running) {
    ExtensionDownloaderTestHelper helper;
    ServiceForDownloadTests service(prefs_.get(), helper.url_loader_factory());
    ExtensionUpdater updater(&service, service.extension_prefs(),
                             service.pref_service(), service.profile(),
                             kUpdateFrequencySecs, nullptr,
                             service.GetDownloaderFactory());
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    EXPECT_THAT(GetRunningInstallIds(updater), testing::IsEmpty());

    GURL url1("http://localhost/extension1.crx");
    GURL url2("http://localhost/extension2.crx");

    const std::string id1(32, 'a');
    const std::string id2(32, 'b');

    std::string hash1;
    std::string hash2;

    std::string version1 = "0.1";
    std::string version2 = "0.1";

    // Start two fetches
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch1 =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id1), url1, hash1, version1,
            DownloadFetchPriority::kBackground);
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch2 =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id2), url2, hash2, version2,
            DownloadFetchPriority::kBackground);
    updater.downloader_->FetchUpdatedExtension(std::move(fetch1),
                                               std::optional<std::string>());
    updater.downloader_->FetchUpdatedExtension(std::move(fetch2),
                                               std::optional<std::string>());

    // Make the first fetch complete.
    EXPECT_TRUE(updater.downloader_->extension_loader_);
    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    // We need some CrxInstallers, and CrxInstallers require a real
    // ExtensionService.  Create one on the testing profile.  Any action
    // the CrxInstallers take is on the testing profile's extension
    // service, not on our mock |service|.  This allows us to fake
    // the CrxInstaller actions we want.
    TestingProfile profile;
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile))
        ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                 base::FilePath(), false);
    ExtensionService* extension_service =
        ExtensionSystem::Get(&profile)->extension_service();

    scoped_refptr<FakeCrxInstaller> fake_crx1 =
        base::MakeRefCounted<FakeCrxInstaller>(extension_service);
    scoped_refptr<FakeCrxInstaller> fake_crx2 =
        base::MakeRefCounted<FakeCrxInstaller>(extension_service);

    if (updates_start_running) {
      // Add mock CrxInstaller to be returned by
      // service.CreateUpdateInstaller().
      service.AddFakeCrxInstaller(id1, fake_crx1);
      service.AddFakeCrxInstaller(id2, fake_crx2);
    } else {
      // If we don't add mock CRX installers, the mock service will just return
      // nullptr, meaning a failure.
    }

    helper.test_url_loader_factory().AddResponse(
        url1.spec(), "Any content. This is irrelevant.", net::HTTP_OK);
    content::RunAllTasksUntilIdle();

    // Expect that the service was asked to do an install with the right data.
    base::FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    EXPECT_EQ(id1, service.extension_id());
    RunUntilIdle();

    // Make sure the second fetch finished and asked the service to do an
    // update.
    EXPECT_TRUE(updater.downloader_->extension_loader_);
    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    helper.test_url_loader_factory().AddResponse(
        url2.spec(), "Any other content. This is irrelevant.", net::HTTP_OK);
    content::RunAllTasksUntilIdle();

    if (updates_start_running) {
      // Both installations should have launched in parallel.
      EXPECT_THAT(GetRunningInstallIds(updater),
                  testing::UnorderedElementsAre(id1, id2));

      // Fake install notice.  This should end the first installation.
      fake_crx1->RunInstallerCallbacks(CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE));
      content::RunAllTasksUntilIdle();
      EXPECT_THAT(GetRunningInstallIds(updater),
                  testing::UnorderedElementsAre(id2));

      fake_crx2->RunInstallerCallbacks(CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE));
      content::RunAllTasksUntilIdle();
    }
    EXPECT_THAT(GetRunningInstallIds(updater), testing::IsEmpty());
  }

  void TestGalleryRequestsWithBrand(bool use_organic_brand_code) {
    google_brand::BrandForTesting brand_for_testing(
        use_organic_brand_code ? "GGLS" : "TEST");

    // We want to test a variety of combinations of expected ping conditions for
    // rollcall and active pings.
    int ping_cases[] = { ManifestFetchData::kNeverPinged, 0, 1, 5 };

    for (size_t i = 0; i < std::size(ping_cases); i++) {
      for (size_t j = 0; j < std::size(ping_cases); j++) {
        for (size_t k = 0; k < 2; k++) {
          int rollcall_ping_days = ping_cases[i];
          int active_ping_days = ping_cases[j];
          // Skip cases where rollcall_ping_days == -1, but
          // active_ping_days > 0, because rollcall_ping_days == -1 means the
          // app was just installed and this is the first update check after
          // installation.
          if (rollcall_ping_days == ManifestFetchData::kNeverPinged &&
              active_ping_days > 0)
            continue;

          bool active_bit = k > 0;
          TestGalleryRequests(rollcall_ping_days, active_ping_days, active_bit,
                              !use_organic_brand_code);
          ASSERT_FALSE(HasFailure()) <<
            " rollcall_ping_days=" << ping_cases[i] <<
            " active_ping_days=" << ping_cases[j] <<
            " active_bit=" << active_bit;
        }
      }
    }
  }

  // Test requests to both a Google server and a non-google server. This allows
  // us to test various combinations of installed (ie roll call) and active
  // (ie app launch) ping scenarios. The invariant is that each type of ping
  // value should be present at most once per day, and can be calculated based
  // on the delta between now and the last ping time (or in the case of active
  // pings, that delta plus whether the app has been active).
  void TestGalleryRequests(int rollcall_ping_days,
                           int active_ping_days,
                           bool active_bit,
                           bool expect_brand_code) {
    // Set up 2 mock extensions, one with a google.com update url and one
    // without.
    prefs_ = std::make_unique<TestExtensionPrefs>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    ExtensionDownloaderTestHelper helper;
    ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
    ExtensionList tmp;
    GURL url1("http://clients2.google.com/service/update2/crx");
    GURL url2("http://www.somewebsite.com");
    service.CreateTestExtensions(1, 1, &tmp, &url1.possibly_invalid_spec(),
                                 ManifestLocation::kInternal);
    service.CreateTestExtensions(2, 1, &tmp, &url2.possibly_invalid_spec(),
                                 ManifestLocation::kInternal);
    EXPECT_EQ(2u, tmp.size());
    service.set_extensions(tmp, ExtensionList());

    ExtensionPrefs* prefs = service.extension_prefs();
    const std::string& id = tmp[0]->id();
    Time now = Time::Now();
    if (rollcall_ping_days == 0) {
      prefs->SetLastPingDay(id, now - base::Seconds(15));
    } else if (rollcall_ping_days > 0) {
      Time last_ping_day =
          now - base::Days(rollcall_ping_days) - base::Seconds(15);
      prefs->SetLastPingDay(id, last_ping_day);
    }

    // Store a value for the last day we sent an active ping.
    if (active_ping_days == 0) {
      prefs->SetLastActivePingDay(id, now - base::Seconds(15));
    } else if (active_ping_days > 0) {
      Time last_active_ping_day =
          now - base::Days(active_ping_days) - base::Seconds(15);
      prefs->SetLastActivePingDay(id, last_active_ping_day);
    }
    if (active_bit)
      prefs->SetActiveBit(id, true);

    ExtensionUpdater updater(&service, service.extension_prefs(),
                             service.pref_service(), service.profile(),
                             kUpdateFrequencySecs, nullptr,
                             service.GetDownloaderFactory());
    updater.Start();
    updater.CheckNow(ExtensionUpdater::CheckParams());

    // Make the updater do manifest fetching, and note the urls it tries to
    // fetch.
    std::vector<GURL> fetched_urls;
    ASSERT_TRUE(updater.downloader_->HasActiveManifestRequestForTesting());
    const ManifestFetchData& fetch =
        *updater.downloader_->manifests_queue_.active_request();
    fetched_urls.push_back(fetch.full_url());
    helper.test_url_loader_factory().AddResponse(
        fetched_urls[0].spec(), std::string(), net::HTTP_INTERNAL_SERVER_ERROR);
    RunUntilIdle();

    const ManifestFetchData& fetch2 =
        *updater.downloader_->manifests_queue_.active_request();
    fetched_urls.push_back(fetch2.full_url());

    // The urls could have been fetched in either order, so use the host to
    // tell them apart and note the query each used.
    GURL url1_fetch_url;
    GURL url2_fetch_url;
    std::string url1_query;
    std::string url2_query;
    if (fetched_urls[0].host() == url1.host()) {
      url1_fetch_url = fetched_urls[0];
      url2_fetch_url = fetched_urls[1];

      url1_query = fetched_urls[0].query();
      url2_query = fetched_urls[1].query();
    } else if (fetched_urls[0].host() == url2.host()) {
      url1_fetch_url = fetched_urls[1];
      url2_fetch_url = fetched_urls[0];
      url1_query = fetched_urls[1].query();
      url2_query = fetched_urls[0].query();
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    std::map<std::string, ParamsMap> url1_ping_data =
        GetPingDataFromURL(url1_fetch_url);
    ParamsMap url1_params = ParamsMap();
    if (!url1_ping_data.empty() && base::Contains(url1_ping_data, id))
      url1_params = url1_ping_data[id];

    // First make sure the non-google query had no ping parameter.
    EXPECT_TRUE(GetPingDataFromURL(url2_fetch_url).empty());

    // Now make sure the google query had the correct ping parameter.
    bool did_rollcall = false;
    if (rollcall_ping_days != 0) {
      ASSERT_TRUE(base::Contains(url1_params, "r"));
      ASSERT_EQ(1u, url1_params["r"].size());
      EXPECT_EQ(base::NumberToString(rollcall_ping_days),
                *url1_params["r"].begin());
      did_rollcall = true;
    }
    if (active_bit && active_ping_days != 0 && did_rollcall) {
      ASSERT_TRUE(base::Contains(url1_params, "a"));
      ASSERT_EQ(1u, url1_params["a"].size());
      EXPECT_EQ(base::NumberToString(active_ping_days),
                *url1_params["a"].begin());
    }

    // Make sure the non-google query has no brand parameter.
    const std::string brand_string = "brand%3D";
    EXPECT_TRUE(url2_query.find(brand_string) == std::string::npos);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Make sure the google query has a brand parameter, but only if the
    // brand is non-organic.
    if (expect_brand_code) {
      EXPECT_TRUE(url1_query.find(brand_string) != std::string::npos);
    } else {
      EXPECT_TRUE(url1_query.find(brand_string) == std::string::npos);
    }
#else
    // Chromium builds never add the brand to the parameter, even for google
    // queries.
    EXPECT_TRUE(url1_query.find(brand_string) == std::string::npos);
#endif

    RunUntilIdle();
  }

  // This makes sure that the extension updater properly stores the results
  // of a <daystart> tag from a manifest fetch in one of two cases: 1) This is
  // the first time we fetched the extension, or 2) We sent a ping value of
  // >= 1 day for the extension.
  void TestHandleManifestResults() {
    ExtensionDownloaderTestHelper helper;
    ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
    GURL update_url("http://www.google.com/manifest");
    ExtensionList tmp;
    service.CreateTestExtensions(1, 1, &tmp, &update_url.spec(),
                                 ManifestLocation::kInternal);
    service.set_extensions(tmp, ExtensionList());

    ExtensionUpdater updater(&service, service.extension_prefs(),
                             service.pref_service(), service.profile(),
                             kUpdateFrequencySecs, nullptr,
                             service.GetDownloaderFactory());
    updater.Start();
    updater.EnsureDownloaderCreated();

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(update_url));
    const Extension* extension = tmp[0].get();
    AddExtensionToFetchDataForTesting(fetch_data.get(), extension->id(),
                                      extension->VersionString(), update_url);
    auto results = std::make_unique<UpdateManifestResults>();
    constexpr int kDaystartElapsedSeconds = 750;
    results->daystart_elapsed_seconds = kDaystartElapsedSeconds;

    updater.downloader_->HandleManifestResults(std::move(fetch_data),
                                               std::move(results),
                                               /*error=*/std::nullopt);
    Time last_ping_day =
        service.extension_prefs()->LastPingDay(extension->id());
    EXPECT_FALSE(last_ping_day.is_null());
    int64_t seconds_diff = (Time::Now() - last_ping_day).InSeconds();
    EXPECT_LT(seconds_diff - kDaystartElapsedSeconds, 5);
  }

  void TestManifestAddExtension(DownloadFetchPriority data_priority,
                                DownloadFetchPriority extension_priority,
                                DownloadFetchPriority expected_priority) {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo"), data_priority));
    ASSERT_TRUE(fetch_data->AddExtension(
        id, version, &ExtensionDownloaderTestHelper::kNeverPingedData,
        std::string(), std::string(), ManifestLocation::kInternal,
        extension_priority));
    ASSERT_EQ(expected_priority, fetch_data->fetch_priority());
  }

  void TestManifestMerge(DownloadFetchPriority data_priority,
                         DownloadFetchPriority other_priority,
                         DownloadFetchPriority expected_priority) {
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo"), data_priority));

    std::unique_ptr<ManifestFetchData> fetch_other(
        CreateManifestFetchData(GURL("http://localhost/foo"), other_priority));

    fetch_data->Merge(std::move(fetch_other));
    ASSERT_EQ(expected_priority, fetch_data->fetch_priority());
  }

 protected:
  std::unique_ptr<TestExtensionPrefs> prefs_;
  content::BrowserTaskEnvironment task_environment_;

  ManifestFetchData* CreateManifestFetchData(
      const GURL& update_url,
      DownloadFetchPriority fetch_priority) {
    return new ManifestFetchData(update_url, 0, "",
                                 UpdateQueryParams::Get(UpdateQueryParams::CRX),
                                 ManifestFetchData::PING, fetch_priority);
  }

  ManifestFetchData* CreateManifestFetchData(const GURL& update_url) {
    return CreateManifestFetchData(update_url,
                                   DownloadFetchPriority::kBackground);
  }

 private:
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  ScopedTestingLocalState testing_local_state_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          testing_local_state_.Get(),
          ash::CrosSettings::Get())};
#endif
};

// Because we test some private methods of ExtensionUpdater, it's easier for the
// actual test code to live in ExtensionUpdaterTest methods instead of TEST_F
// subclasses where friendship with ExtensionUpdater is not inherited.

TEST_F(ExtensionUpdaterTest, TestExtensionUpdateCheckRequests) {
  TestExtensionUpdateCheckRequests(false);
}

TEST_F(ExtensionUpdaterTest, TestExtensionUpdateCheckRequestsPending) {
  TestExtensionUpdateCheckRequests(true);
}

TEST_F(ExtensionUpdaterTest, TestUpdateUrlData) {
  TestUpdateUrlDataEmpty();
  TestUpdateUrlDataSimple();
  TestUpdateUrlDataCompound();
  std::string gallery_url_spec = extension_urls::GetWebstoreUpdateUrl().spec();
  TestUpdateUrlDataFromUrl(gallery_url_spec, DownloadFetchPriority::kBackground,
                           1, true);
  TestUpdateUrlDataFromUrl(gallery_url_spec, DownloadFetchPriority::kForeground,
                           1, true);
  TestUpdateUrlDataFromUrl(gallery_url_spec, DownloadFetchPriority::kBackground,
                           2, true);
  TestUpdateUrlDataFromUrl(gallery_url_spec, DownloadFetchPriority::kForeground,
                           4, true);
  TestUpdateUrlDataFromUrl("http://example.com/update",
                           DownloadFetchPriority::kForeground, 4, false);
}

TEST_F(ExtensionUpdaterTest, TestInstallSource) {
  TestInstallSource();
}

TEST_F(ExtensionUpdaterTest, TestInstallLocation) {
  TestInstallLocation();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdates) {
  TestDetermineUpdates();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdatesPending) {
  TestDetermineUpdatesPending();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdatesDuplicates) {
  TestDetermineUpdatesDuplicates();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdatesError) {
  TestDetermineUpdatesError();
}

TEST_F(ExtensionUpdaterTest, TestMultipleManifestDownloading) {
  TestMultipleManifestDownloading();
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloading) {
  TestSingleExtensionDownloading(false, false, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingPending) {
  TestSingleExtensionDownloading(true, false, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingWithRetry) {
  TestSingleExtensionDownloading(false, true, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingPendingWithRetry) {
  TestSingleExtensionDownloading(true, true, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingFailure) {
  TestSingleExtensionDownloading(false, false, true);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingFailureWithRetry) {
  TestSingleExtensionDownloading(false, true, true);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingFailurePending) {
  TestSingleExtensionDownloading(true, false, true);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ExtensionUpdaterTest, TestCacheCorruptionCrxDownload) {
  TestCacheCorruption();
}
#endif

TEST_F(ExtensionUpdaterTest, ProtectedDownloadCookieAuth) {
  TestProtectedDownload(
      "https://chrome.google.com/webstore/download",
      false, false,  // No OAuth2 support
      0, 0);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadCookieFailure) {
  TestProtectedDownload(
      "https://chrome.google.com/webstore/download",
      false, false,  // No OAuth2 support
      0, -1);  // max_authuser=-1 simulates no valid authuser value.
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadWithNonDefaultAuthUser1) {
  TestProtectedDownload("https://google.com", false, false, 1, 1);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadWithNonDefaultAuthUser2) {
  TestProtectedDownload("https://google.com", false, false, 2, 2);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadAuthUserExhaustionFailure) {
  TestProtectedDownload("https://google.com", false, false, 2, 5);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadWithOAuth2Token) {
  TestProtectedDownload(
      "https://google.com",
      true, true,
      0, -1);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadWithOAuth2Failure) {
  TestProtectedDownload(
      "https://google.com",
      true, false,
      0, -1);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadNoOAuth2WithNonGoogleDomain) {
  TestProtectedDownload(
      "https://not-google.com",
      true, true,
      0, -1);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadFailWithoutHTTPS) {
  TestProtectedDownload(
      "http://google.com",
      true, true,
      0, 0);
}

TEST_F(ExtensionUpdaterTest, TestMultipleExtensionDownloadingUpdatesFail) {
  TestMultipleExtensionDownloading(false);
}
TEST_F(ExtensionUpdaterTest, TestMultipleExtensionDownloadingUpdatesSucceed) {
  TestMultipleExtensionDownloading(true);
}

TEST_F(ExtensionUpdaterTest, TestManifestRetryDownloading) {
  TestManifestRetryDownloading();
}

TEST_F(ExtensionUpdaterTest, DISABLED_TestGalleryRequestsWithOrganicBrand) {
  TestGalleryRequestsWithBrand(true);
}

TEST_F(ExtensionUpdaterTest, DISABLED_TestGalleryRequestsWithNonOrganicBrand) {
  TestGalleryRequestsWithBrand(false);
}

TEST_F(ExtensionUpdaterTest, TestHandleManifestResults) {
  TestHandleManifestResults();
}

TEST_F(ExtensionUpdaterTest, TestNonAutoUpdateableLocations) {
  ExtensionDownloaderTestHelper helper;
  ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kUpdateFrequencySecs, nullptr,
                           service.GetDownloaderFactory());
  MockExtensionDownloaderDelegate delegate;
  service.OverrideDownloaderDelegate(&delegate);

  // Non-internal non-external extensions should be rejected.
  ExtensionList extensions;
  service.CreateTestExtensions(1, 1, &extensions, nullptr,
                               ManifestLocation::kInvalidLocation);
  ASSERT_EQ(1u, extensions.size());
  // The test will fail with unexpected calls if the delegate's methods are
  // invoked for the extension.
  service.set_extensions(extensions, ExtensionList());
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

TEST_F(ExtensionUpdaterTest, TestUpdatingDisabledExtensions) {
  ExtensionDownloaderTestHelper helper;
  ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kUpdateFrequencySecs, nullptr,
                           service.GetDownloaderFactory());
  NiceMock<MockUpdateService> update_service;
  OverrideUpdateService(&updater, &update_service);

  // Non-internal non-external extensions should be rejected.
  ExtensionList enabled_extensions;
  ExtensionList disabled_extensions;
  service.CreateTestExtensions(1, 1, &enabled_extensions, nullptr,
                               ManifestLocation::kInternal);
  service.CreateTestExtensions(2, 1, &disabled_extensions, nullptr,
                               ManifestLocation::kInternal);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(1u, disabled_extensions.size());

  // We expect that both enabled and disabled extensions are auto-updated.
  EXPECT_CALL(update_service,
              StartUpdateCheck(
                  ::testing::Field(&ExtensionUpdateCheckParams::update_info,
                                   ::testing::SizeIs(2)),
                  _, _));

  service.set_extensions(enabled_extensions, disabled_extensions);
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

// crbug.com/1098540: Tests that removely disabled extensions that are part of
// the blocklisted extensions are still receive updates.
TEST_F(ExtensionUpdaterTest, TestUpdatingRemotelyDisabledExtensions) {
  ExtensionDownloaderTestHelper helper;
  ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kUpdateFrequencySecs, nullptr,
                           service.GetDownloaderFactory());
  NiceMock<MockUpdateService> update_service;
  OverrideUpdateService(&updater, &update_service);

  ExtensionList enabled_extensions;
  ExtensionList blocklisted_extensions;
  service.CreateTestExtensions(1, 1, &enabled_extensions, nullptr,
                               ManifestLocation::kInternal);
  service.CreateTestExtensions(2, 1, &blocklisted_extensions, nullptr,
                               ManifestLocation::kInternal);
  service.CreateTestExtensions(3, 1, &blocklisted_extensions, nullptr,
                               ManifestLocation::kInternal);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(2u, blocklisted_extensions.size());
  const std::string& remotely_blocklisted_id = blocklisted_extensions[0]->id();
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      remotely_blocklisted_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      service.extension_prefs());
  blocklist_prefs::AddOmahaBlocklistState(
      remotely_blocklisted_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      service.extension_prefs());

  // We expect that both enabled and remotely blocklisted extensions are
  // auto-updated.
  EXPECT_CALL(update_service,
              StartUpdateCheck(
                  ::testing::Field(&ExtensionUpdateCheckParams::update_info,
                                   ::testing::SizeIs(2)),
                  _, _));

  service.set_extensions(enabled_extensions, ExtensionList(),
                         blocklisted_extensions);
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

TEST_F(ExtensionUpdaterTest, TestPendingInstall) {
  class ServiceForPendingVersionTests : public MockService {
   public:
    explicit ServiceForPendingVersionTests(
        TestExtensionPrefs* prefs,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
        : MockService(prefs, url_loader_factory),
          registry_(ExtensionRegistry::Get(profile())) {
      base::Value::Dict manifest;
      manifest.Set(manifest_keys::kName, "Fake extension");
      manifest.Set(manifest_keys::kVersion, "1.0.0.1");
      manifest.Set(manifest_keys::kManifestVersion, 2);
      manifest.Set(manifest_keys::kDifferentialFingerprint, "fingerprint");
      pending_update_ = prefs_->AddExtensionWithManifest(
          manifest, ManifestLocation::kInternal);
    }

    void SetExtensions(const ExtensionList& extensions) {
      registry_->ClearAll();
      for (auto extension : extensions) {
        registry_->AddEnabled(extension);
      }
    }

    PendingExtensionManager* pending_extension_manager() override {
      return &pending_extension_manager_;
    }

    const Extension* GetPendingExtensionUpdate(
        const std::string& id) const override {
      return pending_update_.get();
    }

   private:
    raw_ptr<ExtensionRegistry> registry_;
    scoped_refptr<Extension> pending_update_;
  };

  ExtensionDownloaderTestHelper helper;
  ServiceForPendingVersionTests service(prefs_.get(),
                                        helper.url_loader_factory());
  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kUpdateFrequencySecs, nullptr,
                           service.GetDownloaderFactory());
  NiceMock<MockUpdateService> update_service;
  OverrideUpdateService(&updater, &update_service);

  ExtensionList enabled_extensions;
  service.CreateTestExtensions(1, 1, &enabled_extensions, nullptr,
                               ManifestLocation::kInternal);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(enabled_extensions[0]->VersionString(), "1.0.0.0");

  // When StartUpdateCheck is called, we expect the pending version is used.
  EXPECT_CALL(
      update_service,
      StartUpdateCheck(
          ::testing::Field(
              &ExtensionUpdateCheckParams::update_info,
              ::testing::ElementsAre(::testing::Pair(
                  enabled_extensions[0]->id(),
                  ::testing::FieldsAre(
                      "", false, ::testing::Optional(std::string("1.0.0.1")),
                      ::testing::Optional(std::string("fingerprint")))))),
          _, _));

  service.SetExtensions(enabled_extensions);
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchesBuilderAddExtension) {
  auto helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  // First, verify that adding valid extensions does invoke the callbacks on
  // the delegate.
  std::string id = crx_file::id_util::GenerateId("foo");
  EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
      .WillOnce(Return(false));
  EXPECT_TRUE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(id, GURL("http://example.com/update"))));
  helper->downloader().StartAllPending(nullptr);
  Mock::VerifyAndClearExpectations(&helper->delegate());
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  // Extensions with invalid update URLs should be rejected.
  id = crx_file::id_util::GenerateId("foo2");
  EXPECT_FALSE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(id, GURL("http:google.com:foo"))));
  helper->downloader().StartAllPending(nullptr);
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  // Extensions with empty IDs should be rejected.
  EXPECT_FALSE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(std::string(), GURL())));
  helper->downloader().StartAllPending(nullptr);
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  // TODO(akalin): Test that extensions with empty update URLs
  // converted from user scripts are rejected.

  // Reset the ExtensionDownloader so that it drops the current fetcher.
  helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  // Extensions with empty update URLs should have a default one
  // filled in.
  id = crx_file::id_util::GenerateId("foo3");
  EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
      .WillOnce(Return(false));
  EXPECT_TRUE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(id, GURL())));
  helper->downloader().StartAllPending(nullptr);
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  RunUntilIdle();
  auto* request = &(*helper->test_url_loader_factory().pending_requests())[0];
  ASSERT_TRUE(request);
  EXPECT_FALSE(request->request.url.is_empty());
}

TEST_F(ExtensionUpdaterTest, TestAddPendingExtensionWithVersion) {
  constexpr char kVersion[] = "1.2.3.4";
  auto helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  {
    // First, verify that adding valid extensions does invoke the callbacks on
    // the delegate.
    ExtensionId id = crx_file::id_util::GenerateId("foo");
    EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
        .WillOnce(Return(false));
    ExtensionDownloaderTask task =
        CreateDownloaderTask(id, GURL("http://example.com/update"));
    task.version = base::Version(kVersion);
    EXPECT_TRUE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    Mock::VerifyAndClearExpectations(&helper->delegate());
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  {
    // Extensions with invalid update URLs should be rejected.
    ExtensionId id = crx_file::id_util::GenerateId("foo2");
    ExtensionDownloaderTask task =
        CreateDownloaderTask(id, GURL("http:google.com:foo"));
    task.version = base::Version(kVersion);
    EXPECT_FALSE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  {
    // Extensions with empty IDs should be rejected.
    ExtensionDownloaderTask task = CreateDownloaderTask(std::string(), GURL());
    task.version = base::Version(kVersion);
    EXPECT_FALSE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  // Reset the ExtensionDownloader so that it drops the current fetcher.
  helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  {
    // Extensions with empty update URLs should have a default one
    // filled in.
    ExtensionId id = crx_file::id_util::GenerateId("foo3");
    EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
        .WillOnce(Return(false));
    ExtensionDownloaderTask task = CreateDownloaderTask(id, GURL());
    task.version = base::Version(kVersion);
    EXPECT_TRUE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  RunUntilIdle();
  auto* request = &(*helper->test_url_loader_factory().pending_requests())[0];
  ASSERT_TRUE(request);
  EXPECT_FALSE(request->request.url.is_empty());
  EXPECT_THAT(request->request.url.possibly_invalid_spec(),
              testing::HasSubstr(kVersion));
}

TEST_F(ExtensionUpdaterTest, TestStartUpdateCheckMemory) {
  ExtensionDownloaderTestHelper helper;

  StartUpdateCheck(&helper.downloader(),
                   CreateManifestFetchData(GURL("http://localhost/foo")));
  // This should delete the newly-created ManifestFetchData.
  StartUpdateCheck(&helper.downloader(),
                   CreateManifestFetchData(GURL("http://localhost/foo")));
  // This should add into |manifests_pending_|.
  StartUpdateCheck(&helper.downloader(),
                   CreateManifestFetchData(GURL("http://www.google.com")));
  // The dtor of |downloader| should delete the pending fetchers.
}

TEST_F(ExtensionUpdaterTest, TestCheckSoon) {
  ExtensionDownloaderTestHelper helper;
  ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kUpdateFrequencySecs, nullptr,
                           service.GetDownloaderFactory());
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.Start();
  EXPECT_TRUE(updater.WillCheckSoon());
  RunUntilIdle();
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.CheckSoon();
  EXPECT_TRUE(updater.WillCheckSoon());
  updater.CheckSoon();
  EXPECT_TRUE(updater.WillCheckSoon());
  RunUntilIdle();
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.CheckSoon();
  EXPECT_TRUE(updater.WillCheckSoon());
  updater.Stop();
  EXPECT_FALSE(updater.WillCheckSoon());
}

TEST_F(ExtensionUpdaterTest, TestUninstallWhileUpdateCheck) {
  ExtensionDownloaderTestHelper helper;
  ServiceForManifestTests service(prefs_.get(), helper.url_loader_factory());
  ExtensionList tmp;
  service.CreateTestExtensions(1, 1, &tmp, nullptr,
                               ManifestLocation::kInternal);
  service.set_extensions(tmp, ExtensionList());

  ASSERT_EQ(1u, tmp.size());
  ExtensionId id = tmp.front()->id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(service.profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(id));

  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kUpdateFrequencySecs, nullptr,
                           service.GetDownloaderFactory());
  ExtensionUpdater::CheckParams params;
  params.ids = {id};
  updater.Start();
  updater.CheckNow(std::move(params));

  service.set_extensions(ExtensionList(), ExtensionList());
  ASSERT_FALSE(registry->enabled_extensions().GetByID(id));

  // RunUntilIdle is needed to make sure that the UpdateService instance that
  // runs the extension update process has a chance to exit gracefully; without
  // it, the test would crash.
  RunUntilIdle();
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchDataAddExtension) {
  TestManifestAddExtension(DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kBackground);

  TestManifestAddExtension(DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kForeground);

  TestManifestAddExtension(DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kForeground);

  TestManifestAddExtension(DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kForeground);
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchDataMerge) {
  TestManifestMerge(DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kBackground);

  TestManifestMerge(DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kForeground);

  TestManifestMerge(DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kForeground);

  TestManifestMerge(DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kForeground);
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchCredentials) {
  TestManifestCredentialsWebstore();
  TestManifestCredentialsNonWebstore();
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchPriority) {
  TestManifestFetchPriority(DownloadFetchPriority::kBackground);
  TestManifestFetchPriority(DownloadFetchPriority::kForeground);
}

TEST_F(ExtensionUpdaterTest, TestExtensionPriority) {
  TestSingleExtensionDownloadingPriority(DownloadFetchPriority::kBackground);
  TestSingleExtensionDownloadingPriority(DownloadFetchPriority::kForeground);
}

class CanUseUpdateServiceTest : public ExtensionUpdaterTest {
 public:
  CanUseUpdateServiceTest() = default;
  ~CanUseUpdateServiceTest() override = default;

  void SetUp() override {
    ExtensionUpdaterTest::SetUp();

    service_ = std::make_unique<ServiceForDownloadTests>(
        prefs_.get(), downloader_test_helper_.url_loader_factory());
    updater_ = std::make_unique<ExtensionUpdater>(
        service_.get(), service_->extension_prefs(), service_->pref_service(),
        service_->profile(), kUpdateFrequencySecs, nullptr,
        service_->GetDownloaderFactory());

    store_extension_ =
        ExtensionBuilder("store_extension")
            .SetManifestKey(
                "update_url",
                extension_urls::GetDefaultWebstoreUpdateUrl().spec())
            .Build();
    offstore_extension_ =
        ExtensionBuilder("offstore_extension")
            .SetManifestKey("update_url", "http://localhost/test/updates.xml")
            .Build();
    emptyurl_extension_ = ExtensionBuilder("emptyurl_extension").Build();
    userscript_extension_ =
        ExtensionBuilder("userscript_extension")
            .SetManifestKey("converted_from_user_script", true)
            .Build();

    ASSERT_TRUE(store_extension_.get());
    ASSERT_TRUE(ExtensionRegistry::Get(service_->profile())
                    ->AddEnabled(store_extension_));
    ASSERT_TRUE(offstore_extension_.get());
    ASSERT_TRUE(ExtensionRegistry::Get(service_->profile())
                    ->AddEnabled(offstore_extension_));
    ASSERT_TRUE(emptyurl_extension_.get());
    ASSERT_TRUE(ExtensionRegistry::Get(service_->profile())
                    ->AddEnabled(emptyurl_extension_));
    ASSERT_TRUE(userscript_extension_.get());
    ASSERT_TRUE(ExtensionRegistry::Get(service_->profile())
                    ->AddEnabled(userscript_extension_));
  }

 protected:
  ExtensionUpdater* updater() { return updater_.get(); }

  ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_checks_;
  ExtensionDownloaderTestHelper downloader_test_helper_;
  std::unique_ptr<ServiceForDownloadTests> service_;
  std::unique_ptr<ExtensionUpdater> updater_;

  scoped_refptr<const Extension> store_extension_;
  scoped_refptr<const Extension> offstore_extension_;
  scoped_refptr<const Extension> emptyurl_extension_;
  scoped_refptr<const Extension> userscript_extension_;
};

class UpdateServiceCanUpdateFeatureEnabledNonDefaultUpdateUrl
    : public CanUseUpdateServiceTest {
 public:
  void SetUp() override {
    CanUseUpdateServiceTest::SetUp();

    // Change the webstore update url.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    // Note: |offstore_extension_|'s update url is the same.
    command_line->AppendSwitchASCII("apps-gallery-update-url",
                                    "http://localhost/test2/updates.xml");
    ExtensionsClient::Get()->InitializeWebStoreUrls(
        base::CommandLine::ForCurrentProcess());
  }
};

TEST_F(CanUseUpdateServiceTest, TestDefaults) {
  // Update service can only update webstore extensions when enabled.
  EXPECT_TRUE(CanUseUpdateService(updater(), store_extension_->id()));
  // ... and extensions with empty update URL.
  EXPECT_TRUE(CanUseUpdateService(updater(), emptyurl_extension_->id()));
  // It can't update off-store extensions.
  EXPECT_FALSE(CanUseUpdateService(updater(), offstore_extension_->id()));
  // ... or extensions with empty update URL converted from user script.
  EXPECT_FALSE(CanUseUpdateService(updater(), userscript_extension_->id()));
  // ... or extensions that don't exist.
  EXPECT_FALSE(CanUseUpdateService(updater(), std::string(32, 'a')));
  // ... or extensions with empty ID (is it possible?).
  EXPECT_FALSE(CanUseUpdateService(updater(), ""));
}

TEST_F(UpdateServiceCanUpdateFeatureEnabledNonDefaultUpdateUrl,
       CanUseUpdateServiceFeatureEnabledNonDefaultUpdateUrl) {
  // Update service can update extensions when the default webstore update url
  // is changed.
  EXPECT_FALSE(CanUseUpdateService(updater(), store_extension_->id()));
  EXPECT_TRUE(CanUseUpdateService(updater(), emptyurl_extension_->id()));
  EXPECT_FALSE(CanUseUpdateService(updater(), offstore_extension_->id()));
  EXPECT_FALSE(CanUseUpdateService(updater(), userscript_extension_->id()));
  EXPECT_FALSE(CanUseUpdateService(updater(), std::string(32, 'a')));
  EXPECT_FALSE(CanUseUpdateService(updater(), ""));
}

// TODO(asargent) - (http://crbug.com/12780) add tests for:
// -prodversionmin (shouldn't update if browser version too old)
// -manifests & updates arriving out of order / interleaved
// -malformed update url (empty, file://, has query, has a # fragment, etc.)
// -An extension gets uninstalled while updates are in progress (so it doesn't
//  "come back from the dead")
// -An extension gets manually updated to v3 while we're downloading v2 (ie
//  you don't get downgraded accidentally)
// -An update manifest mentions multiple updates

}  // namespace extensions
