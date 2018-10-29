// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/browser/updater/request_queue_impl.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/data_decoder/public/cpp/test_data_decoder_service.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using update_client::UpdateQueryParams;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::_;

namespace extensions {

typedef ExtensionDownloaderDelegate::Error Error;
typedef ExtensionDownloaderDelegate::PingResult PingResult;

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

const char kEmptyUpdateUrlData[] = "";

const char kAuthUserQueryKey[] = "authuser";

int kExpectedLoadFlags =
    net::LOAD_DO_NOT_SEND_COOKIES |
    net::LOAD_DO_NOT_SAVE_COOKIES |
    net::LOAD_DISABLE_CACHE;

int kExpectedLoadFlagsForDownloadWithCookies = net::LOAD_DISABLE_CACHE;

// Fake authentication constants
const char kFakeOAuth2Token[] = "ce n'est pas un jeton";

const ManifestFetchData::PingData kNeverPingedData(
    ManifestFetchData::kNeverPinged,
    ManifestFetchData::kNeverPinged,
    true,
    0);

class MockExtensionDownloaderDelegate : public ExtensionDownloaderDelegate {
 public:
  MOCK_METHOD4(OnExtensionDownloadFailed, void(const std::string&,
                                               Error,
                                               const PingResult&,
                                               const std::set<int>&));
  MOCK_METHOD7(OnExtensionDownloadFinished,
               void(const extensions::CRXFileInfo&,
                    bool,
                    const GURL&,
                    const std::string&,
                    const PingResult&,
                    const std::set<int>&,
                    const InstallCallback&));
  MOCK_METHOD0(OnExtensionDownloadRetryForTests, void());
  MOCK_METHOD2(GetPingDataForExtension,
               bool(const std::string&, ManifestFetchData::PingData*));
  MOCK_METHOD1(GetUpdateUrlData, std::string(const std::string&));
  MOCK_METHOD1(IsExtensionPending, bool(const std::string&));
  MOCK_METHOD2(GetExtensionExistingVersion,
               bool(const std::string&, std::string*));

  void Wait() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    quit_closure_ = runner->QuitClosure();
    runner->Run();
    quit_closure_.Reset();
  }

  void Quit() {
    quit_closure_.Run();
  }

  void DelegateTo(ExtensionDownloaderDelegate* delegate) {
    ON_CALL(*this, OnExtensionDownloadFailed(_, _, _, _))
        .WillByDefault(Invoke(delegate,
            &ExtensionDownloaderDelegate::OnExtensionDownloadFailed));
    ON_CALL(*this, OnExtensionDownloadFinished(_, _, _, _, _, _, _))
        .WillByDefault(
            Invoke(delegate,
                   &ExtensionDownloaderDelegate::OnExtensionDownloadFinished));
    ON_CALL(*this, OnExtensionDownloadRetryForTests())
        .WillByDefault(Invoke(
            delegate,
            &ExtensionDownloaderDelegate::OnExtensionDownloadRetryForTests));
    ON_CALL(*this, GetPingDataForExtension(_, _))
        .WillByDefault(Invoke(delegate,
            &ExtensionDownloaderDelegate::GetPingDataForExtension));
    ON_CALL(*this, GetUpdateUrlData(_))
        .WillByDefault(Invoke(delegate,
            &ExtensionDownloaderDelegate::GetUpdateUrlData));
    ON_CALL(*this, IsExtensionPending(_))
        .WillByDefault(Invoke(delegate,
            &ExtensionDownloaderDelegate::IsExtensionPending));
    ON_CALL(*this, GetExtensionExistingVersion(_, _))
        .WillByDefault(Invoke(delegate,
            &ExtensionDownloaderDelegate::GetExtensionExistingVersion));
  }

 private:
  base::Closure quit_closure_;
};

const int kNotificationsObserved[] = {
    extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED,
    extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
};

// A class that observes the notifications sent by the ExtensionUpdater and
// the ExtensionDownloader.
class NotificationsObserver : public content::NotificationObserver {
 public:
  NotificationsObserver() {
    for (size_t i = 0; i < arraysize(kNotificationsObserved); ++i) {
      count_[i] = 0;
      registrar_.Add(this,
                     kNotificationsObserved[i],
                     content::NotificationService::AllSources());
    }
  }

  ~NotificationsObserver() override {
    for (size_t i = 0; i < arraysize(kNotificationsObserved); ++i) {
      registrar_.Remove(this,
                        kNotificationsObserved[i],
                        content::NotificationService::AllSources());
    }
  }

  size_t StartedCount() { return count_[0]; }
  size_t UpdatedCount() { return count_[1]; }

  bool Updated(const std::string& id) {
    return updated_.find(id) != updated_.end();
  }

  void Wait() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    quit_closure_ = runner->QuitClosure();
    runner->Run();
    quit_closure_.Reset();
  }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    if (!quit_closure_.is_null())
      quit_closure_.Run();
    for (size_t i = 0; i < arraysize(kNotificationsObserved); ++i) {
      if (kNotificationsObserved[i] == type) {
        count_[i]++;
        if (type == extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND) {
          updated_.insert(
              content::Details<UpdateDetails>(details)->id);
        }
        return;
      }
    }
    NOTREACHED();
  }

  content::NotificationRegistrar registrar_;
  size_t count_[arraysize(kNotificationsObserved)];
  std::set<std::string> updated_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(NotificationsObserver);
};

// Extracts the integer value of the |authuser| query parameter. Returns 0 if
// the parameter is not set.
int GetAuthUserQueryValue(const GURL& url) {
  std::string query_string = url.query();
  url::Component query(0, query_string.length());
  url::Component key, value;
  while (
      url::ExtractQueryKeyValue(query_string.c_str(), &query, &key, &value)) {
    std::string key_string = query_string.substr(key.begin, key.len);
    if (key_string == kAuthUserQueryKey) {
      int user_index = 0;
      base::StringToInt(query_string.substr(value.begin, value.len),
                        &user_index);
      return user_index;
    }
  }
  return 0;
}

}  // namespace

// Base class for further specialized test classes.
class MockService : public TestExtensionService {
 public:
  explicit MockService(
      TestExtensionPrefs* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : prefs_(prefs),
        pending_extension_manager_(prefs->profile()),
        fake_account_id_("bobloblaw@lawblog.example.com"),
        downloader_delegate_override_(NULL),
        test_shared_url_loader_factory_(url_loader_factory) {}

  ~MockService() override {}

  PendingExtensionManager* pending_extension_manager() override {
    ADD_FAILURE() << "Subclass should override this if it will "
                  << "be accessed by a test.";
    return &pending_extension_manager_;
  }

  Profile* profile() { return prefs_->profile(); }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return test_shared_url_loader_factory_;
  }

  ExtensionPrefs* extension_prefs() { return prefs_->prefs(); }

  PrefService* pref_service() { return prefs_->pref_service(); }

  FakeOAuth2TokenService* fake_token_service() {
    return fake_token_service_.get();
  }

  const std::string& fake_account_id() { return fake_account_id_; }

  // Creates test extensions and inserts them into list. The name and
  // version are all based on their index. If |update_url| is non-null, it
  // will be used as the update_url for each extension.
  // The |id| is used to distinguish extension names and make sure that
  // no two extensions share the same name.
  void CreateTestExtensions(int id, int count, ExtensionList *list,
                            const std::string* update_url,
                            Manifest::Location location) {
    for (int i = 1; i <= count; i++) {
      base::DictionaryValue manifest;
      manifest.SetString(manifest_keys::kVersion,
                         base::StringPrintf("%d.0.0.0", i));
      manifest.SetString(manifest_keys::kName,
                         base::StringPrintf("Extension %d.%d", id, i));
      manifest.SetInteger(manifest_keys::kManifestVersion, 2);
      if (update_url)
        manifest.SetString(manifest_keys::kUpdateURL, *update_url);
      scoped_refptr<Extension> e =
          prefs_->AddExtensionWithManifest(manifest, location);
      ASSERT_TRUE(e.get() != NULL);
      list->push_back(e);
    }
  }

  ExtensionDownloader::Factory GetDownloaderFactory() {
    return base::Bind(&MockService::CreateExtensionDownloader,
                      base::Unretained(this));
  }

  ExtensionDownloader::Factory GetAuthenticatedDownloaderFactory() {
    return base::Bind(&MockService::CreateExtensionDownloaderWithIdentity,
                      base::Unretained(this));
  }

  void OverrideDownloaderDelegate(ExtensionDownloaderDelegate* delegate) {
    downloader_delegate_override_ = delegate;
  }

 protected:
  TestExtensionPrefs* const prefs_;
  PendingExtensionManager pending_extension_manager_;

 private:
  std::unique_ptr<ExtensionDownloader> CreateExtensionDownloader(
      ExtensionDownloaderDelegate* delegate) {
    std::unique_ptr<ExtensionDownloader> downloader =
        ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
            url_loader_factory(),
            downloader_delegate_override_ ? downloader_delegate_override_
                                          : delegate,
            /*connector=*/nullptr);
    return downloader;
  }

  std::unique_ptr<ExtensionDownloader> CreateExtensionDownloaderWithIdentity(
      ExtensionDownloaderDelegate* delegate) {
    fake_token_service_.reset(new FakeOAuth2TokenService());
    fake_token_service_->AddAccount(fake_account_id_);

    std::unique_ptr<ExtensionDownloader> downloader(
        CreateExtensionDownloader(delegate));
    downloader->SetWebstoreAuthenticationCapabilities(
        base::BindRepeating(&MockService::fake_account_id,
                            base::Unretained(this)),
        fake_token_service_.get());
    return downloader;
  }

  std::string fake_account_id_;
  std::unique_ptr<FakeOAuth2TokenService> fake_token_service_;

  ExtensionDownloaderDelegate* downloader_delegate_override_;

  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockService);
};


bool ShouldInstallExtensionsOnly(const Extension* extension) {
  return extension->GetType() == Manifest::TYPE_EXTENSION;
}

bool ShouldInstallThemesOnly(const Extension* extension) {
  return extension->is_theme();
}

bool ShouldAlwaysInstall(const Extension* extension) {
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

    pending_extension_manager->AddForTesting(
        PendingExtensionInfo(id,
                             std::string(),
                             update_url,
                             base::Version(),
                             should_allow_install,
                             kIsFromSync,
                             Manifest::INTERNAL,
                             Extension::NO_FLAGS,
                             kMarkAcknowledged,
                             kRemoteInstall));
  }
}

class ServiceForManifestTests : public MockService {
 public:
  explicit ServiceForManifestTests(
      TestExtensionPrefs* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : MockService(prefs, url_loader_factory),
        registry_(ExtensionRegistry::Get(profile())) {}

  ~ServiceForManifestTests() override {}

  const Extension* GetExtensionById(const std::string& id,
                                    bool include_disabled) const override {
    const Extension* result = registry_->enabled_extensions().GetByID(id);
    if (result || !include_disabled)
      return result;
    return registry_->disabled_extensions().GetByID(id);
  }

  PendingExtensionManager* pending_extension_manager() override {
    return &pending_extension_manager_;
  }

  const Extension* GetPendingExtensionUpdate(
      const std::string& id) const override {
    return NULL;
  }

  bool IsExtensionEnabled(const std::string& id) const override {
    return !registry_->disabled_extensions().Contains(id);
  }

  void set_extensions(ExtensionList extensions,
                      ExtensionList disabled_extensions) {
    registry_->ClearAll();
    for (ExtensionList::const_iterator it = extensions.begin();
         it != extensions.end(); ++it) {
      registry_->AddEnabled(*it);
    }
    for (ExtensionList::const_iterator it = disabled_extensions.begin();
         it != disabled_extensions.end(); ++it) {
      registry_->AddDisabled(*it);
    }
  }

 private:
  ExtensionRegistry* registry_;
};

class ServiceForDownloadTests : public MockService {
 public:
  explicit ServiceForDownloadTests(
      TestExtensionPrefs* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : MockService(prefs, url_loader_factory) {}

  // Add a fake crx installer to be returned by a call to UpdateExtension()
  // with a specific ID.  Caller keeps ownership of |crx_installer|.
  void AddFakeCrxInstaller(const std::string& id, CrxInstaller* crx_installer) {
    fake_crx_installers_[id] = crx_installer;
  }

  bool UpdateExtension(const CRXFileInfo& file,
                       bool file_ownership_passed,
                       CrxInstaller** out_crx_installer) override {
    extension_id_ = file.extension_id;
    install_path_ = file.path;

    if (base::ContainsKey(fake_crx_installers_, extension_id_)) {
      *out_crx_installer = fake_crx_installers_[extension_id_];
      return true;
    }

    return false;
  }

  PendingExtensionManager* pending_extension_manager() override {
    return &pending_extension_manager_;
  }

  const Extension* GetExtensionById(const std::string& id,
                                    bool) const override {
    last_inquired_extension_id_ = id;
    return NULL;
  }

  const std::string& extension_id() const { return extension_id_; }
  const base::FilePath& install_path() const { return install_path_; }

 private:
  // Hold the set of ids that UpdateExtension() should fake success on.
  // UpdateExtension(id, ...) will return true iff fake_crx_installers_
  // contains key |id|.  |out_install_notification_source| will be set
  // to Source<CrxInstaller(fake_crx_installers_[i]).
  std::map<std::string, CrxInstaller*> fake_crx_installers_;

  std::string extension_id_;
  base::FilePath install_path_;
  GURL download_url_;

  // The last extension ID that GetExtensionById was called with.
  // Mutable because the method that sets it (GetExtensionById) is const
  // in the actual extension service, but must record the last extension
  // ID in this test class.
  mutable std::string last_inquired_extension_id_;
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
      NOTREACHED();
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
    std::string unescaped = net::UnescapeURLComponent(
        param.second,
        net::UnescapeRule::PATH_SEPARATORS |
            net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
    base::StringPairs extension_params;
    base::SplitStringIntoKeyValuePairs(unescaped, '=', '&', &extension_params);
    std::multimap<std::string, std::string> param_map;
    param_map.insert(extension_params.begin(), extension_params.end());
    if (base::ContainsKey(param_map, "id") &&
        base::ContainsKey(param_map, "ping")) {
      std::string id = param_map.find("id")->second;
      result[id] = ParamsMap();

      // Pull the key=value pairs out of the ping parameter for this id and
      // put into the result.
      std::string ping = net::UnescapeURLComponent(
          param_map.find("ping")->second,
          net::UnescapeRule::PATH_SEPARATORS |
              net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
      base::StringPairs ping_params;
      base::SplitStringIntoKeyValuePairs(ping, '=', '&', &ping_params);
      for (const auto& ping_param : ping_params) {
        if (!base::ContainsKey(result[id], ping_param.first))
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
  std::string decoded = net::UnescapeURLComponent(
      params["x"],
      net::UnescapeRule::PATH_SEPARATORS |
          net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  ExtractParameters(decoded, result);
}

// All of our tests that need to use private APIs of ExtensionUpdater live
// inside this class (which is a friend to ExtensionUpdater).
class ExtensionUpdaterTest : public testing::Test {
 public:
  ExtensionUpdaterTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    prefs_.reset(new TestExtensionPrefs(base::ThreadTaskRunnerHandle::Get()));
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

  void SimulateTimerFired(ExtensionUpdater* updater) {
    EXPECT_TRUE(updater->timer_.IsRunning());
    updater->timer_.Stop();
    updater->TimerFired();
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
    results->list.push_back(result);
  }

  void StartUpdateCheck(ExtensionDownloader* downloader,
                        ManifestFetchData* fetch_data) {
    downloader->StartUpdateCheck(
        std::unique_ptr<ManifestFetchData>(fetch_data));
  }

  size_t ManifestFetchersCount(ExtensionDownloader* downloader) {
    return downloader->manifests_queue_.size() +
           (downloader->manifest_loader_.get() ? 1 : 0);
  }

  void TestExtensionUpdateCheckRequests(bool pending) {
    // Create an extension with an update_url.
    ServiceForManifestTests service(prefs_.get(),
                                    test_shared_url_loader_factory_);
    std::string update_url("http://foo.com/bar");
    ExtensionList extensions;
    NotificationsObserver observer;
    PendingExtensionManager* pending_extension_manager =
        service.pending_extension_manager();
    if (pending) {
      SetupPendingExtensionManagerForTest(1, GURL(update_url),
                                          pending_extension_manager);
    } else {
      service.CreateTestExtensions(1, 1, &extensions, &update_url,
                                   Manifest::INTERNAL);
      service.set_extensions(extensions, ExtensionList());
    }

    // Set up and start the updater.
    ExtensionUpdater updater(&service,
                             service.extension_prefs(),
                             service.pref_service(),
                             service.profile(),
                             60 * 60 * 24,
                             NULL,
                             service.GetDownloaderFactory());
    updater.Start();

    // Tell the update that it's time to do update checks.
    EXPECT_EQ(0u, observer.StartedCount());
    SimulateTimerFired(&updater);
    EXPECT_EQ(1u, observer.StartedCount());

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
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an empty update URL data string does not cause a ap=
    // option to appear in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(id, version, &kNeverPingedData, std::string(),
                             std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);

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
    fetch_data->AddExtension(id, version, &kNeverPingedData, "bar",
                             std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
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
    fetch_data->AddExtension(id, version, &kNeverPingedData, "a=1&b=2&c",
                             std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ("a%3D1%26b%3D2%26c", params["ap"]);
  }

  void TestUpdateUrlDataFromUrl(
      const std::string& update_url,
      ManifestFetchData::FetchPriority fetch_priority,
      int num_extensions,
      bool should_include_traffic_management_headers) {
    MockService service(prefs_.get(), test_shared_url_loader_factory_);
    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, service.url_loader_factory(),
                                   data_decoder_service_connector());
    ExtensionList extensions;

    service.CreateTestExtensions(1, num_extensions, &extensions, &update_url,
                                 Manifest::INTERNAL);

    for (int i = 0; i < num_extensions; ++i) {
      const std::string& id = extensions[i]->id();
      EXPECT_CALL(delegate, GetPingDataForExtension(id, _));

      downloader.AddExtension(*extensions[i], 0, fetch_priority);
    }

    // Get the headers our loader was asked to fetch.
    base::RunLoop loop;
    net::HttpRequestHeaders last_request_headers;
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_headers = request.headers;
          loop.Quit();
        }));

    downloader.StartAllPending(NULL);
    EXPECT_TRUE(downloader.manifest_loader_);

    loop.Run();

    // Make sure that extensions that update from the gallery ignore any
    // update URL data.
    const ManifestFetchData& fetch =
        *downloader.manifests_queue_.active_request();
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
      std::string interactivity_value;
      fetch_headers.GetHeader(ExtensionDownloader::kUpdateInteractivityHeader,
                              &interactivity_value);

      std::string expected_interactivity_value =
          fetch_priority == ManifestFetchData::FetchPriority::FOREGROUND ? "fg"
                                                                         : "bg";
      EXPECT_EQ(expected_interactivity_value, interactivity_value);

      std::string appid_value;
      fetch_headers.GetHeader(ExtensionDownloader::kUpdateAppIdHeader,
                              &appid_value);
      if (num_extensions > 1) {
        for (int i = 0; i < num_extensions; ++i) {
          EXPECT_TRUE(
              testing::IsSubstring("", "", extensions[i]->id(), appid_value));
        }
      } else {
        EXPECT_EQ(extensions[0]->id(), appid_value);
      }

      std::string updater_value;
      fetch_headers.GetHeader(ExtensionDownloader::kUpdateUpdaterHeader,
                              &updater_value);
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
    fetch_data->AddExtension(id, version, &kNeverPingedData,
                             kEmptyUpdateUrlData, install_source, std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
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
        id, version, &kNeverPingedData, kEmptyUpdateUrlData, std::string(),
        install_location, ManifestFetchData::FetchPriority::BACKGROUND);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().query(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(install_location, params["installedby"]);
  }

  void TestDetermineUpdates() {
    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, test_shared_url_loader_factory_,
                                   data_decoder_service_connector());

    // Check passing an empty list of parse results to DetermineUpdates
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    UpdateManifestResults updates;
    std::vector<UpdateManifestResult*> updateable;
    std::set<std::string> not_updateable;
    std::set<std::string> errors;
    downloader.DetermineUpdates(*fetch_data, updates, &updateable,
                                &not_updateable, &errors);
    EXPECT_TRUE(updateable.empty());
    EXPECT_TRUE(not_updateable.empty());
    EXPECT_TRUE(errors.empty());

    // Create two updates - expect that DetermineUpdates will return the first
    // one (v1.0 installed, v1.1 available) but not the second one (both
    // installed and available at v2.0).
    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    fetch_data->AddExtension(id1, "1.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    AddParseResult(id1, "1.1", "http://localhost/e1_1.1.crx", &updates);
    fetch_data->AddExtension(id2, "2.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    AddParseResult(id2, "2.0.0.0", "http://localhost/e2_2.0.crx", &updates);

    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.0.0.0"),
                        Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("2.0.0.0"),
                        Return(true)));

    updateable.clear();
    not_updateable.clear();
    errors.clear();
    downloader.DetermineUpdates(*fetch_data, updates, &updateable,
                                &not_updateable, &errors);
    EXPECT_TRUE(errors.empty());
    EXPECT_THAT(not_updateable, testing::ElementsAre(id2));
    ASSERT_EQ(1u, updateable.size());
    EXPECT_EQ("1.1", updateable[0]->version);
  }

  void TestDetermineUpdatesError() {
    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, test_shared_url_loader_factory_,
                                   data_decoder_service_connector());

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
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

    fetch_data->AddExtension(id1, "1.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    AddParseResult(id1, "1.1", "http://localhost/e1_1.1.crx", &updates);

    fetch_data->AddExtension(id2, "2.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    AddParseResult(id2, "2.0.0.0", "http://localhost/e2_2.0.crx", &updates);

    fetch_data->AddExtension(id3, "0.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    // Empty update version in manifest.
    AddParseResult(id3, "", "http://localhost/e3_3.0.crx", &updates);

    fetch_data->AddExtension(id4, "0.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);

    fetch_data->AddExtension(id5, "0.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    AddParseResult(id5, "5.0.0.0", "http://localhost/e5_5.0.crx", &updates);

    fetch_data->AddExtension(id6, "0.0.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    // Invalid update version in manifest.
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

    std::vector<UpdateManifestResult*> updateable;
    std::set<std::string> not_updateable;
    std::set<std::string> errors;
    downloader.DetermineUpdates(*fetch_data, updates, &updateable,
                                &not_updateable, &errors);
    EXPECT_THAT(not_updateable, testing::UnorderedElementsAre(id2, id3));
    EXPECT_THAT(errors, testing::UnorderedElementsAre(id4, id5, id6));
    ASSERT_EQ(1u, updateable.size());
    EXPECT_EQ("1.1", updateable[0]->version);
  }

  void TestDetermineUpdatesPending() {
    // Create a set of test extensions
    ServiceForManifestTests service(prefs_.get(),
                                    test_shared_url_loader_factory_);
    PendingExtensionManager* pending_extension_manager =
        service.pending_extension_manager();
    SetupPendingExtensionManagerForTest(3, GURL(), pending_extension_manager);

    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, test_shared_url_loader_factory_,
                                   data_decoder_service_connector());

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    UpdateManifestResults updates;

    std::list<std::string> ids_for_update_check;
    pending_extension_manager->GetPendingIdsForUpdateCheck(
        &ids_for_update_check);

    for (const std::string& id : ids_for_update_check) {
      fetch_data->AddExtension(
          id, "1.0.0.0", &kNeverPingedData, kEmptyUpdateUrlData, std::string(),
          std::string(), ManifestFetchData::FetchPriority::BACKGROUND);
      AddParseResult(id, "1.1", "http://localhost/e1_1.1.crx", &updates);
    }

    // The delegate will tell the downloader that all the extensions are
    // pending.
    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(true));

    std::vector<UpdateManifestResult*> updateable;
    std::set<std::string> not_updateable;
    std::set<std::string> errors;
    downloader.DetermineUpdates(*fetch_data, updates, &updateable,
                                &not_updateable, &errors);
    // All the apps should be updateable.
    EXPECT_EQ(3u, updateable.size());
    EXPECT_TRUE(not_updateable.empty());
    EXPECT_TRUE(errors.empty());
  }

  void TestDetermineUpdatesDuplicates() {
    base::HistogramTester histogram_tester;
    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, test_shared_url_loader_factory_,
                                   data_decoder_service_connector());

    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    const std::string id3 = crx_file::id_util::GenerateId("3");
    const std::string id4 = crx_file::id_util::GenerateId("4");
    const std::string id5 = crx_file::id_util::GenerateId("5");
    const std::string id6 = crx_file::id_util::GenerateId("6");
    const std::string id7 = crx_file::id_util::GenerateId("7");

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(id1, "1.1.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    fetch_data->AddExtension(id2, "1.2.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    fetch_data->AddExtension(id3, "1.3.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    fetch_data->AddExtension(id4, "1.4.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    fetch_data->AddExtension(id5, "1.5.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    fetch_data->AddExtension(id6, "1.6.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    fetch_data->AddExtension(id7, "1.7.0.0", &kNeverPingedData,
                             kEmptyUpdateUrlData, std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);

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

    std::vector<UpdateManifestResult*> updateable;
    std::set<std::string> not_updateable;
    std::set<std::string> errors;
    downloader.DetermineUpdates(*fetch_data, updates, &updateable,
                                &not_updateable, &errors);
    EXPECT_THAT(not_updateable, testing::UnorderedElementsAre(id1, id4));
    EXPECT_THAT(errors, testing::UnorderedElementsAre(id2, id5, id7));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Extensions.UpdateManifestDuplicateEntryCount"),
                testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 3),
                                     base::Bucket(2, 2), base::Bucket(4, 1)));
    ASSERT_EQ(2u, updateable.size());
    EXPECT_EQ("1.3.1.0", updateable[0]->version);
    EXPECT_EQ("1.6.1.0", updateable[1]->version);
  }

  void TestMultipleManifestDownloading() {
    MockService service(prefs_.get(), test_shared_url_loader_factory_);
    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, service.url_loader_factory(),
                                   data_decoder_service_connector());
    downloader.manifests_queue_.set_backoff_policy(&kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch1(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch2(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch3(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch4(
        CreateManifestFetchData(kUpdateUrl));
    ManifestFetchData::PingData zeroDays(0, 0, true, 0);
    fetch1->AddExtension("1111", "1.0", &zeroDays, kEmptyUpdateUrlData,
                         std::string(), std::string(),
                         ManifestFetchData::FetchPriority::BACKGROUND);
    fetch2->AddExtension("2222", "2.0", &zeroDays, kEmptyUpdateUrlData,
                         std::string(), std::string(),
                         ManifestFetchData::FetchPriority::BACKGROUND);
    fetch3->AddExtension("3333", "3.0", &zeroDays, kEmptyUpdateUrlData,
                         std::string(), std::string(),
                         ManifestFetchData::FetchPriority::BACKGROUND);
    fetch4->AddExtension("4444", "4.0", &zeroDays, kEmptyUpdateUrlData,
                         std::string(), std::string(),
                         ManifestFetchData::FetchPriority::BACKGROUND);

    // This will start the first fetcher and queue the others. The next in queue
    // is started as each fetcher receives its response. Note that the fetchers
    // don't necessarily run in the order that they are started from here.
    GURL fetch1_url = fetch1->full_url();
    GURL fetch2_url = fetch2->full_url();
    GURL fetch3_url = fetch3->full_url();
    GURL fetch4_url = fetch4->full_url();
    downloader.StartUpdateCheck(std::move(fetch1));
    downloader.StartUpdateCheck(std::move(fetch2));
    downloader.StartUpdateCheck(std::move(fetch3));
    downloader.StartUpdateCheck(std::move(fetch4));
    RunUntilIdle();

    // fetch1_url
    {
      test_url_loader_factory_.AddResponse(fetch1_url.spec(), "",
                                           net::HTTP_BAD_REQUEST);
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "1111", ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch1_url = GURL();

      RunUntilIdle();
    }

    // fetch2_url
    {
      const std::string kInvalidXml = "invalid xml";
      test_url_loader_factory_.AddResponse(fetch2_url.spec(), kInvalidXml,
                                           net::HTTP_OK);
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "2222", ExtensionDownloaderDelegate::MANIFEST_INVALID, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch2_url = GURL();

      RunUntilIdle();
    }

    // fetch3_url
    {
      const std::string kNoUpdate =
          "<?xml version='1.0' encoding='UTF-8'?>"
          "<gupdate xmlns='http://www.google.com/update2/response'"
          "                protocol='2.0'>"
          " <app appid='3333'>"
          "  <updatecheck codebase='http://example.com/extension_3.0.0.0.crx'"
          "               version='3.0.0.0' prodversionmin='3.0.0.0' />"
          " </app>"
          "</gupdate>";
      test_url_loader_factory_.AddResponse(fetch3_url.spec(), kNoUpdate,
                                           net::HTTP_OK);
      // The third fetcher doesn't have an update available.
      EXPECT_CALL(delegate, IsExtensionPending("3333")).WillOnce(Return(false));
      EXPECT_CALL(delegate, GetExtensionExistingVersion("3333", _))
          .WillOnce(DoAll(SetArgPointee<1>("3.0.0.0"), Return(true)));
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "3333", ExtensionDownloaderDelegate::NO_UPDATE_AVAILABLE, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch3_url = GURL();

      RunUntilIdle();
    }

    // fetch4_url
    {
      // The last fetcher has an update.
      NotificationsObserver observer;
      const std::string kUpdateAvailable =
          "<?xml version='1.0' encoding='UTF-8'?>"
          "<gupdate xmlns='http://www.google.com/update2/response'"
          "                protocol='2.0'>"
          " <app appid='4444'>"
          "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
          "               version='4.0.42.0' prodversionmin='4.0.42.0' />"
          " </app>"
          "</gupdate>";
      test_url_loader_factory_.AddResponse(fetch4_url.spec(), kUpdateAvailable,
                                           net::HTTP_OK);
      EXPECT_CALL(delegate, IsExtensionPending("4444")).WillOnce(Return(false));
      EXPECT_CALL(delegate, GetExtensionExistingVersion("4444", _))
          .WillOnce(DoAll(SetArgPointee<1>("4.0.0.0"), Return(true)));
      observer.Wait();
      Mock::VerifyAndClearExpectations(&delegate);

      // Verify that the downloader decided to update this extension.
      EXPECT_EQ(1u, observer.UpdatedCount());
      EXPECT_TRUE(observer.Updated("4444"));
      fetch4_url = GURL();
    }
    if (downloader.manifest_loader_)
      ADD_FAILURE() << "Unexpected load";
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index = 0) {
    if (index >= test_url_loader_factory_.pending_requests()->size())
      return nullptr;
    return &(*test_url_loader_factory_.pending_requests())[index];
  }

  void TestManifestRetryDownloading() {
    NotificationsObserver observer;
    MockService service(prefs_.get(), test_shared_url_loader_factory_);
    MockExtensionDownloaderDelegate delegate;
    ExtensionDownloader downloader(&delegate, service.url_loader_factory(),
                                   data_decoder_service_connector());
    downloader.manifests_queue_.set_backoff_policy(&kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    ManifestFetchData::PingData zeroDays(0, 0, true, 0);
    fetch->AddExtension("1111", "1.0", &zeroDays, kEmptyUpdateUrlData,
                        std::string(), std::string(),
                        ManifestFetchData::FetchPriority::BACKGROUND);

    // This will start the first fetcher.
    downloader.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    // ExtensionDownloader should retry kMaxRetries times and then fail.
    EXPECT_CALL(delegate, OnExtensionDownloadFailed(
        "1111", ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED, _, _));
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.load_flags == kExpectedLoadFlags);
        }));
    for (int i = 0; i <= ExtensionDownloader::kMaxRetries; ++i) {
      // All fetches will fail.
      auto* request = GetPendingRequest(0);
      // Code 5xx causes ExtensionDownloader to retry.
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateResourceResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
          "");
      RunUntilIdle();
    }
    Mock::VerifyAndClearExpectations(&delegate);

    // For response codes that are not in the 5xx range ExtensionDownloader
    // should not retry.
    fetch.reset(CreateManifestFetchData(kUpdateUrl));
    fetch->AddExtension("1111", "1.0", &zeroDays, kEmptyUpdateUrlData,
                        std::string(), std::string(),
                        ManifestFetchData::FetchPriority::BACKGROUND);

    // This will start the first fetcher.
    downloader.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    EXPECT_CALL(delegate, OnExtensionDownloadFailed(
        "1111", ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED, _, _));

    // The first fetch will fail, and require retrying.
    {
      auto* request = GetPendingRequest(0);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateResourceResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
          "");
    }
    RunUntilIdle();

    // The second fetch will fail with response 400 and should not cause
    // ExtensionDownloader to retry.
    {
      auto* request = GetPendingRequest(0);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateResourceResponseHead(net::HTTP_BAD_REQUEST), "");
    }
    RunUntilIdle();

    Mock::VerifyAndClearExpectations(&delegate);
  }

  void TestSingleExtensionDownloading(bool pending, bool retry, bool fail) {
    std::unique_ptr<ServiceForDownloadTests> service(
        new ServiceForDownloadTests(prefs_.get(),
                                    test_shared_url_loader_factory_));
    ExtensionUpdater updater(service.get(),
                             service->extension_prefs(),
                             service->pref_service(),
                             service->profile(),
                             kUpdateFrequencySecs,
                             NULL,
                             service->GetDownloaderFactory());
    MockExtensionDownloaderDelegate delegate;
    delegate.DelegateTo(&updater);
    service->OverrideDownloaderDelegate(&delegate);
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(
        &kNoBackoffPolicy);

    GURL test_url("http://localhost/extension.crx");

    const std::string id(32, 'a');
    std::string hash;
    CRXFileInfo crx_file_info;
    base::Version version("0.0.1");
    std::set<int> requests;
    requests.insert(0);
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch(
        new ExtensionDownloader::ExtensionFetch(id, test_url, hash,
                                                version.GetString(), requests));
    updater.downloader_->FetchUpdatedExtension(std::move(fetch));

    if (pending) {
      const bool kIsFromSync = true;
      const bool kMarkAcknowledged = false;
      const bool kRemoteInstall = false;
      PendingExtensionManager* pending_extension_manager =
          service->pending_extension_manager();
      pending_extension_manager->AddForTesting(
          PendingExtensionInfo(id,
                               std::string(),
                               test_url,
                               version,
                               &ShouldAlwaysInstall,
                               kIsFromSync,
                               Manifest::INTERNAL,
                               Extension::NO_FLAGS,
                               kMarkAcknowledged,
                               kRemoteInstall));
    }

    if (retry) {
      EXPECT_CALL(delegate, OnExtensionDownloadRetryForTests())
          .WillOnce(DoAll(
              InvokeWithoutArgs(&delegate,
                                &MockExtensionDownloaderDelegate::Quit),
              InvokeWithoutArgs(
                  this,
                  &ExtensionUpdaterTest::ClearURLLoaderFactoryResponses)));
      test_url_loader_factory_.AddResponse(test_url.spec(), "",
                                           net::HTTP_INTERNAL_SERVER_ERROR);
      delegate.Wait();
      EXPECT_TRUE(updater.downloader_->extension_loader_);
    }

    if (fail) {
      EXPECT_CALL(delegate, OnExtensionDownloadFailed(id, _, _, requests))
          .WillOnce(DoAll(
              InvokeWithoutArgs(&delegate,
                                &MockExtensionDownloaderDelegate::Quit),
              InvokeWithoutArgs(
                  this,
                  &ExtensionUpdaterTest::ClearURLLoaderFactoryResponses)));
      test_url_loader_factory_.AddResponse(test_url.spec(),
                                           "Any content. It is irrelevant.",
                                           net::HTTP_NOT_FOUND);
      delegate.Wait();
    } else {
      EXPECT_TRUE(updater.downloader_->extension_loader_);
      EXPECT_CALL(delegate, OnExtensionDownloadFinished(
                                _, _, _, version.GetString(), _, requests, _))
          .WillOnce(
              DoAll(testing::SaveArg<0>(&crx_file_info),
                    InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit)));
      test_url_loader_factory_.AddResponse(test_url.spec(),
                                           "Any content. It is irrelevant.");
      delegate.Wait();
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

  void ClearURLLoaderFactoryResponses() {
    test_url_loader_factory_.ClearResponses();
  }

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
    std::unique_ptr<ServiceForDownloadTests> service(
        new ServiceForDownloadTests(prefs_.get(),
                                    test_shared_url_loader_factory_));
    const ExtensionDownloader::Factory& downloader_factory =
        enable_oauth2 ? service->GetAuthenticatedDownloaderFactory()
            : service->GetDownloaderFactory();
    ExtensionUpdater updater(
        service.get(),
        service->extension_prefs(),
        service->pref_service(),
        service->profile(),
        kUpdateFrequencySecs,
        NULL,
        downloader_factory);

    MockExtensionDownloaderDelegate delegate;
    delegate.DelegateTo(&updater);
    service->OverrideDownloaderDelegate(&delegate);

    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(
        &kNoBackoffPolicy);

    GURL test_url(base::StringPrintf("%s/extension.crx", url_prefix.c_str()));

    const std::string id(32, 'a');
    std::string hash;
    base::Version version("0.0.1");
    std::set<int> requests;
    requests.insert(0);
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch(
        new ExtensionDownloader::ExtensionFetch(id, test_url, hash,
                                                version.GetString(), requests));
    updater.downloader_->FetchUpdatedExtension(std::move(fetch));

    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    // Fake a 403 response.
    EXPECT_CALL(delegate, OnExtensionDownloadRetryForTests())
        .WillOnce(DoAll(
            InvokeWithoutArgs(&delegate,
                              &MockExtensionDownloaderDelegate::Quit),
            InvokeWithoutArgs(
                this, &ExtensionUpdaterTest::ClearURLLoaderFactoryResponses)));
    test_url_loader_factory_.AddResponse(test_url.spec(), "",
                                         net::HTTP_FORBIDDEN);
    delegate.Wait();

    if (service->fake_token_service()) {
      service->fake_token_service()->IssueAllTokensForAccount(
          service->fake_account_id(),
          OAuth2AccessTokenConsumer::TokenResponse(
              kFakeOAuth2Token, base::Time::Now(), std::string()));
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
        std::string actual_header_value;
        fetch_headers.GetHeader(net::HttpRequestHeaders::kAuthorization,
                                &actual_header_value);
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
            .WillOnce(DoAll(
                InvokeWithoutArgs(&delegate,
                                  &MockExtensionDownloaderDelegate::Quit),
                InvokeWithoutArgs(
                    this,
                    &ExtensionUpdaterTest::ClearURLLoaderFactoryResponses)));
        test_url_loader_factory_.AddResponse(test_url.spec(), "",
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
            .WillOnce(DoAll(
                InvokeWithoutArgs(&delegate,
                                  &MockExtensionDownloaderDelegate::Quit),
                InvokeWithoutArgs(
                    this,
                    &ExtensionUpdaterTest::ClearURLLoaderFactoryResponses)));
        test_url_loader_factory_.AddResponse(fetch.url.spec(), "whatever",
                                             net::HTTP_FORBIDDEN);
        delegate.Wait();
      }

      // Simulate exhaustion of all available authusers.
      if (!success && user_index > max_authuser) {
        const ExtensionDownloader::ExtensionFetch& fetch =
            *updater.downloader_->extensions_queue_.active_request();
        EXPECT_TRUE(updater.downloader_->extension_loader_);
        test_url_loader_factory_.AddResponse(fetch.url.spec(), std::string(),
                                             net::HTTP_UNAUTHORIZED);
        EXPECT_CALL(delegate, OnExtensionDownloadFailed(_, _, _, _))
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
      EXPECT_CALL(delegate, OnExtensionDownloadFinished(_, _, _, _, _, _, _))
          .WillOnce(
              DoAll(testing::SaveArg<0>(&crx_file_info),
                    InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit)));
      test_url_loader_factory_.AddResponse(fetch.url.spec(), "whatever");
      delegate.Wait();

      // Verify installation would proceed as normal.
      EXPECT_EQ(id, crx_file_info.extension_id);
      base::FilePath tmpfile_path = crx_file_info.path;
      EXPECT_FALSE(tmpfile_path.empty());
    }
  }

  // Two extensions are updated.  If |updates_start_running| is true, the
  // mock extensions service has UpdateExtension(...) return true, and
  // the test is responsible for creating fake CrxInstallers.  Otherwise,
  // UpdateExtension() returns false, signaling install failures.
  void TestMultipleExtensionDownloading(bool updates_start_running) {
    ServiceForDownloadTests service(prefs_.get(),
                                    test_shared_url_loader_factory_);
    ExtensionUpdater updater(&service,
                             service.extension_prefs(),
                             service.pref_service(),
                             service.profile(),
                             kUpdateFrequencySecs,
                             NULL,
                             service.GetDownloaderFactory());
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(
        &kNoBackoffPolicy);

    EXPECT_FALSE(updater.crx_install_is_running_);

    GURL url1("http://localhost/extension1.crx");
    GURL url2("http://localhost/extension2.crx");

    const std::string id1(32, 'a');
    const std::string id2(32, 'b');

    std::string hash1;
    std::string hash2;

    std::string version1 = "0.1";
    std::string version2 = "0.1";
    std::set<int> requests;
    requests.insert(0);
    // Start two fetches
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch1(
        new ExtensionDownloader::ExtensionFetch(id1, url1, hash1, version1,
                                                requests));
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch2(
        new ExtensionDownloader::ExtensionFetch(id2, url2, hash2, version2,
                                                requests));
    updater.downloader_->FetchUpdatedExtension(std::move(fetch1));
    updater.downloader_->FetchUpdatedExtension(std::move(fetch2));

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

    scoped_refptr<CrxInstaller> fake_crx1(
        CrxInstaller::CreateSilent(extension_service));
    scoped_refptr<CrxInstaller> fake_crx2(
        CrxInstaller::CreateSilent(extension_service));

    if (updates_start_running) {
      // Add fake CrxInstaller to be returned by service.UpdateExtension().
      service.AddFakeCrxInstaller(id1, fake_crx1.get());
      service.AddFakeCrxInstaller(id2, fake_crx2.get());
    } else {
      // If we don't add fake CRX installers, the mock service fakes a failure
      // starting the install.
    }

    test_url_loader_factory_.AddResponse(
        url1.spec(), "Any content. This is irrelevant.", net::HTTP_OK);
    RunUntilIdle();

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

    test_url_loader_factory_.AddResponse(
        url2.spec(), "Any other content. This is irrelevant.", net::HTTP_OK);
    content::RunAllTasksUntilIdle();

    if (updates_start_running) {
      EXPECT_TRUE(updater.crx_install_is_running_);

      // The second install should not have run, because the first has not
      // sent a notification that it finished.
      EXPECT_EQ(id1, service.extension_id());

      // Fake install notice.  This should start the second installation,
      // which will be checked below.
      fake_crx1->NotifyCrxInstallComplete(CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE));

      EXPECT_TRUE(updater.crx_install_is_running_);
    }

    EXPECT_EQ(id2, service.extension_id());
    EXPECT_FALSE(service.install_path().empty());

    if (updates_start_running) {
      EXPECT_TRUE(updater.crx_install_is_running_);
      fake_crx2->NotifyCrxInstallComplete(CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE));
    }
    EXPECT_FALSE(updater.crx_install_is_running_);
  }

  void TestGalleryRequestsWithBrand(bool use_organic_brand_code) {
    google_brand::BrandForTesting brand_for_testing(
        use_organic_brand_code ? "GGLS" : "TEST");

    // We want to test a variety of combinations of expected ping conditions for
    // rollcall and active pings.
    int ping_cases[] = { ManifestFetchData::kNeverPinged, 0, 1, 5 };

    for (size_t i = 0; i < arraysize(ping_cases); i++) {
      for (size_t j = 0; j < arraysize(ping_cases); j++) {
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
    prefs_.reset(new TestExtensionPrefs(base::ThreadTaskRunnerHandle::Get()));
    ServiceForManifestTests service(prefs_.get(),
                                    test_shared_url_loader_factory_);
    ExtensionList tmp;
    GURL url1("http://clients2.google.com/service/update2/crx");
    GURL url2("http://www.somewebsite.com");
    service.CreateTestExtensions(1, 1, &tmp, &url1.possibly_invalid_spec(),
                                 Manifest::INTERNAL);
    service.CreateTestExtensions(2, 1, &tmp, &url2.possibly_invalid_spec(),
                                 Manifest::INTERNAL);
    EXPECT_EQ(2u, tmp.size());
    service.set_extensions(tmp, ExtensionList());

    ExtensionPrefs* prefs = service.extension_prefs();
    const std::string& id = tmp[0]->id();
    Time now = Time::Now();
    if (rollcall_ping_days == 0) {
      prefs->SetLastPingDay(id, now - TimeDelta::FromSeconds(15));
    } else if (rollcall_ping_days > 0) {
      Time last_ping_day = now -
                           TimeDelta::FromDays(rollcall_ping_days) -
                           TimeDelta::FromSeconds(15);
      prefs->SetLastPingDay(id, last_ping_day);
    }

    // Store a value for the last day we sent an active ping.
    if (active_ping_days == 0) {
      prefs->SetLastActivePingDay(id, now - TimeDelta::FromSeconds(15));
    } else if (active_ping_days > 0) {
      Time last_active_ping_day = now -
                                  TimeDelta::FromDays(active_ping_days) -
                                  TimeDelta::FromSeconds(15);
      prefs->SetLastActivePingDay(id, last_active_ping_day);
    }
    if (active_bit)
      prefs->SetActiveBit(id, true);

    ExtensionUpdater updater(&service,
                             service.extension_prefs(),
                             service.pref_service(),
                             service.profile(),
                             kUpdateFrequencySecs,
                             NULL,
                             service.GetDownloaderFactory());
    updater.Start();
    updater.CheckNow(ExtensionUpdater::CheckParams());

    // Make the updater do manifest fetching, and note the urls it tries to
    // fetch.
    std::vector<GURL> fetched_urls;
    ASSERT_TRUE(updater.downloader_->manifest_loader_);
    const ManifestFetchData& fetch =
        *updater.downloader_->manifests_queue_.active_request();
    fetched_urls.push_back(fetch.full_url());
    test_url_loader_factory_.AddResponse(fetched_urls[0].spec(), std::string(),
                                         net::HTTP_INTERNAL_SERVER_ERROR);
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
      NOTREACHED();
    }

    std::map<std::string, ParamsMap> url1_ping_data =
        GetPingDataFromURL(url1_fetch_url);
    ParamsMap url1_params = ParamsMap();
    if (!url1_ping_data.empty() && base::ContainsKey(url1_ping_data, id))
      url1_params = url1_ping_data[id];

    // First make sure the non-google query had no ping parameter.
    EXPECT_TRUE(GetPingDataFromURL(url2_fetch_url).empty());

    // Now make sure the google query had the correct ping parameter.
    bool did_rollcall = false;
    if (rollcall_ping_days != 0) {
      ASSERT_TRUE(base::ContainsKey(url1_params, "r"));
      ASSERT_EQ(1u, url1_params["r"].size());
      EXPECT_EQ(base::IntToString(rollcall_ping_days),
                *url1_params["r"].begin());
      did_rollcall = true;
    }
    if (active_bit && active_ping_days != 0 && did_rollcall) {
      ASSERT_TRUE(base::ContainsKey(url1_params, "a"));
      ASSERT_EQ(1u, url1_params["a"].size());
      EXPECT_EQ(base::IntToString(active_ping_days),
                *url1_params["a"].begin());
    }

    // Make sure the non-google query has no brand parameter.
    const std::string brand_string = "brand%3D";
    EXPECT_TRUE(url2_query.find(brand_string) == std::string::npos);

#if defined(GOOGLE_CHROME_BUILD)
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
    ServiceForManifestTests service(prefs_.get(),
                                    test_shared_url_loader_factory_);
    GURL update_url("http://www.google.com/manifest");
    ExtensionList tmp;
    service.CreateTestExtensions(1, 1, &tmp, &update_url.spec(),
                                 Manifest::INTERNAL);
    service.set_extensions(tmp, ExtensionList());

    ExtensionUpdater updater(&service,
                             service.extension_prefs(),
                             service.pref_service(),
                             service.profile(),
                             kUpdateFrequencySecs,
                             nullptr,
                             service.GetDownloaderFactory());
    updater.Start();
    updater.EnsureDownloaderCreated();

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(update_url));
    const Extension* extension = tmp[0].get();
    fetch_data->AddExtension(extension->id(), extension->VersionString(),
                             &kNeverPingedData, kEmptyUpdateUrlData,
                             std::string(), std::string(),
                             ManifestFetchData::FetchPriority::BACKGROUND);
    auto results = std::make_unique<UpdateManifestResults>();
    constexpr int kDaystartElapsedSeconds = 750;
    results->daystart_elapsed_seconds = kDaystartElapsedSeconds;

    updater.downloader_->HandleManifestResults(
        std::move(fetch_data), std::move(results),
        /*error=*/base::Optional<std::string>());
    Time last_ping_day =
        service.extension_prefs()->LastPingDay(extension->id());
    EXPECT_FALSE(last_ping_day.is_null());
    int64_t seconds_diff = (Time::Now() - last_ping_day).InSeconds();
    EXPECT_LT(seconds_diff - kDaystartElapsedSeconds, 5);
  }

  // This lets us run a test with some enabled and some disabled
  // extensions. The |num_enabled| value specifies how many enabled extensions
  // to have, and |disabled| is a vector of DisableReason bitmasks for each
  // disabled extension we want.
  void TestPingMetrics(int num_enabled,
                       const std::vector<int>& disabled) {
    ServiceForManifestTests service(prefs_.get(),
                                    test_shared_url_loader_factory_);
    ExtensionList enabled_extensions;
    ExtensionList disabled_extensions;

    std::string update_url = extension_urls::GetWebstoreUpdateUrl().spec();
    if (num_enabled > 0)
      service.CreateTestExtensions(
          1, num_enabled, &enabled_extensions, &update_url, Manifest::INTERNAL);
    if (disabled.size() > 0)
      service.CreateTestExtensions(2,
                                   disabled.size(),
                                   &disabled_extensions,
                                   &update_url,
                                   Manifest::INTERNAL);

    service.set_extensions(enabled_extensions, disabled_extensions);

    ExtensionPrefs* prefs = prefs_->prefs();

    for (size_t i = 0; i < disabled.size(); i++)
      prefs->SetExtensionDisabled(disabled_extensions[i]->id(), disabled[i]);

    // Create the extension updater, make it issue an update, and capture the
    // URL that it tried to fetch.
    ExtensionUpdater updater(&service,
                             service.extension_prefs(),
                             service.pref_service(),
                             service.profile(),
                             kUpdateFrequencySecs,
                             nullptr,
                             service.GetDownloaderFactory());
    updater.Start();
    SimulateTimerFired(&updater);
    ASSERT_NE(nullptr, updater.downloader_->manifest_loader_);
    const ManifestFetchData& fetch =
        *updater.downloader_->manifests_queue_.active_request();
    const GURL& url = fetch.full_url();
    EXPECT_FALSE(url.is_empty());
    EXPECT_TRUE(url.is_valid());
    EXPECT_TRUE(url.has_query());

    std::map<std::string, ParamsMap> all_pings = GetPingDataFromURL(url);

    // Make sure that all the enabled extensions have "e=1" in their ping
    // parameter.
    for (const auto& ext : enabled_extensions) {
      ASSERT_TRUE(base::ContainsKey(all_pings, ext->id()));
      ParamsMap& ping = all_pings[ext->id()];
      EXPECT_FALSE(base::ContainsKey(ping, "dr"));
      ASSERT_TRUE(base::ContainsKey(ping, "e")) << url;
      std::set<std::string> e = ping["e"];
      ASSERT_EQ(1u, e.size()) << url;
      EXPECT_EQ(std::string("1"), *e.begin()) << url;
      EXPECT_FALSE(base::ContainsKey(ping, "dr"));
    }

    // Make sure that all the disable extensions have the appropriate
    // "dr=<num>" values in their ping parameter if metrics are on, or omit
    // it otherwise.
    ASSERT_EQ(disabled_extensions.size(), disabled.size());
    for (size_t i = 0; i < disabled.size(); i++) {
      scoped_refptr<const Extension>& ext = disabled_extensions[i];
      int disable_reasons = disabled[i];
      ASSERT_TRUE(base::ContainsKey(all_pings, ext->id())) << url;
      ParamsMap& ping = all_pings[ext->id()];

      ASSERT_TRUE(base::ContainsKey(ping, "e")) << url;
      std::set<std::string> e = ping["e"];
      ASSERT_EQ(1u, e.size()) << url;
      EXPECT_EQ(std::string("0"), *e.begin()) << url;

      if (disable_reasons == 0) {
        EXPECT_FALSE(base::ContainsKey(ping, "dr"));
      } else {
        ASSERT_TRUE(base::ContainsKey(ping, "dr"));
        int found_reasons = 0;
        for (const auto& reason_string : ping["dr"]) {
          int reason = 0;
          ASSERT_TRUE(base::StringToInt(reason_string, &reason));
          // Make sure it's a power of 2.
          ASSERT_TRUE(reason < 2 || !(reason & (reason - 1))) << reason;
          found_reasons |= reason;
        }
        EXPECT_EQ(disable_reasons, found_reasons);
      }
    }
  }

  void TestManifestAddExtension(
      ManifestFetchData::FetchPriority data_priority,
      ManifestFetchData::FetchPriority extension_priority,
      ManifestFetchData::FetchPriority expected_priority) {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo"), data_priority));
    ASSERT_TRUE(fetch_data->AddExtension(id, version, &kNeverPingedData,
                                         std::string(), std::string(),
                                         std::string(), extension_priority));
    ASSERT_EQ(expected_priority, fetch_data->fetch_priority());
  }

  void TestManifestMerge(ManifestFetchData::FetchPriority data_priority,
                         ManifestFetchData::FetchPriority other_priority,
                         ManifestFetchData::FetchPriority expected_priority) {
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo"), data_priority));

    std::unique_ptr<ManifestFetchData> fetch_other(
        CreateManifestFetchData(GURL("http://localhost/foo"), other_priority));

    fetch_data->Merge(*fetch_other);
    ASSERT_EQ(expected_priority, fetch_data->fetch_priority());
  }

 protected:
  std::unique_ptr<TestExtensionPrefs> prefs_;
  content::TestBrowserThreadBundle thread_bundle_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  ManifestFetchData* CreateManifestFetchData(
      const GURL& update_url,
      ManifestFetchData::FetchPriority fetch_priority) {
    return new ManifestFetchData(update_url, 0, "",
                                 UpdateQueryParams::Get(UpdateQueryParams::CRX),
                                 ManifestFetchData::PING, fetch_priority);
  }

  ManifestFetchData* CreateManifestFetchData(const GURL& update_url) {
    return CreateManifestFetchData(
        update_url, ManifestFetchData::FetchPriority::BACKGROUND);
  }

  service_manager::Connector* data_decoder_service_connector() const {
    return test_data_decoder_service_.connector();
  }

 private:
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  ScopedTestingLocalState testing_local_state_;
  data_decoder::TestDataDecoderService test_data_decoder_service_;

#if defined OS_CHROMEOS
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
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
  TestUpdateUrlDataFromUrl(
      gallery_url_spec, ManifestFetchData::FetchPriority::BACKGROUND, 1, true);
  TestUpdateUrlDataFromUrl(
      gallery_url_spec, ManifestFetchData::FetchPriority::FOREGROUND, 1, true);
  TestUpdateUrlDataFromUrl(
      gallery_url_spec, ManifestFetchData::FetchPriority::BACKGROUND, 2, true);
  TestUpdateUrlDataFromUrl(
      gallery_url_spec, ManifestFetchData::FetchPriority::FOREGROUND, 4, true);
  TestUpdateUrlDataFromUrl("http://example.com/update",
                           ManifestFetchData::FetchPriority::FOREGROUND, 4,
                           false);
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

TEST_F(ExtensionUpdaterTest, TestGalleryRequestsWithOrganicBrand) {
  TestGalleryRequestsWithBrand(true);
}

TEST_F(ExtensionUpdaterTest, TestGalleryRequestsWithNonOrganicBrand) {
  TestGalleryRequestsWithBrand(false);
}

TEST_F(ExtensionUpdaterTest, TestHandleManifestResults) {
  TestHandleManifestResults();
}

TEST_F(ExtensionUpdaterTest, TestNonAutoUpdateableLocations) {
  ServiceForManifestTests service(prefs_.get(),
                                  test_shared_url_loader_factory_);
  ExtensionUpdater updater(&service,
                           service.extension_prefs(),
                           service.pref_service(),
                           service.profile(),
                           kUpdateFrequencySecs,
                           NULL,
                           service.GetDownloaderFactory());
  MockExtensionDownloaderDelegate delegate;
  service.OverrideDownloaderDelegate(&delegate);

  // Non-internal non-external extensions should be rejected.
  ExtensionList extensions;
  service.CreateTestExtensions(1, 1, &extensions, NULL,
                               Manifest::INVALID_LOCATION);
  service.CreateTestExtensions(2, 1, &extensions, NULL, Manifest::INTERNAL);
  ASSERT_EQ(2u, extensions.size());
  const std::string& updateable_id = extensions[1]->id();

  // These expectations fail if the delegate's methods are invoked for the
  // first extension, which has a non-matching id.
  EXPECT_CALL(delegate,
              GetUpdateUrlData(updateable_id)).WillOnce(Return(""));
  EXPECT_CALL(delegate, GetPingDataForExtension(updateable_id, _));

  service.set_extensions(extensions, ExtensionList());
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

TEST_F(ExtensionUpdaterTest, TestUpdatingDisabledExtensions) {
  ServiceForManifestTests service(prefs_.get(),
                                  test_shared_url_loader_factory_);
  ExtensionUpdater updater(&service,
                           service.extension_prefs(),
                           service.pref_service(),
                           service.profile(),
                           kUpdateFrequencySecs,
                           NULL,
                           service.GetDownloaderFactory());
  MockExtensionDownloaderDelegate delegate;
  service.OverrideDownloaderDelegate(&delegate);

  // Non-internal non-external extensions should be rejected.
  ExtensionList enabled_extensions;
  ExtensionList disabled_extensions;
  service.CreateTestExtensions(1, 1, &enabled_extensions, NULL,
      Manifest::INTERNAL);
  service.CreateTestExtensions(2, 1, &disabled_extensions, NULL,
      Manifest::INTERNAL);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(1u, disabled_extensions.size());
  const std::string& enabled_id = enabled_extensions[0]->id();
  const std::string& disabled_id = disabled_extensions[0]->id();

  // We expect that both enabled and disabled extensions are auto-updated.
  EXPECT_CALL(delegate, GetUpdateUrlData(enabled_id)).WillOnce(Return(""));
  EXPECT_CALL(delegate, GetPingDataForExtension(enabled_id, _));
  EXPECT_CALL(delegate,
              GetUpdateUrlData(disabled_id)).WillOnce(Return(""));
  EXPECT_CALL(delegate, GetPingDataForExtension(disabled_id, _));

  service.set_extensions(enabled_extensions, disabled_extensions);
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchesBuilderAddExtension) {
  MockService service(prefs_.get(), test_shared_url_loader_factory_);
  MockExtensionDownloaderDelegate delegate;
  std::unique_ptr<ExtensionDownloader> downloader(
      new ExtensionDownloader(&delegate, service.url_loader_factory(),
                              data_decoder_service_connector()));
  EXPECT_EQ(0u, ManifestFetchersCount(downloader.get()));

  // First, verify that adding valid extensions does invoke the callbacks on
  // the delegate.
  std::string id = crx_file::id_util::GenerateId("foo");
  EXPECT_CALL(delegate, GetPingDataForExtension(id, _)).WillOnce(Return(false));
  EXPECT_TRUE(downloader->AddPendingExtension(
      id, GURL("http://example.com/update"), Manifest::INTERNAL, false, 0,
      ManifestFetchData::FetchPriority::BACKGROUND));
  downloader->StartAllPending(NULL);
  Mock::VerifyAndClearExpectations(&delegate);
  EXPECT_EQ(1u, ManifestFetchersCount(downloader.get()));

  // Extensions with invalid update URLs should be rejected.
  id = crx_file::id_util::GenerateId("foo2");
  EXPECT_FALSE(downloader->AddPendingExtension(
      id, GURL("http:google.com:foo"), Manifest::INTERNAL, false, 0,
      ManifestFetchData::FetchPriority::BACKGROUND));
  downloader->StartAllPending(NULL);
  EXPECT_EQ(1u, ManifestFetchersCount(downloader.get()));

  // Extensions with empty IDs should be rejected.
  EXPECT_FALSE(downloader->AddPendingExtension(
      std::string(), GURL(), Manifest::INTERNAL, false, 0,
      ManifestFetchData::FetchPriority::BACKGROUND));
  downloader->StartAllPending(NULL);
  EXPECT_EQ(1u, ManifestFetchersCount(downloader.get()));

  // TODO(akalin): Test that extensions with empty update URLs
  // converted from user scripts are rejected.

  // Reset the ExtensionDownloader so that it drops the current fetcher.
  downloader.reset(new ExtensionDownloader(&delegate,
                                           service.url_loader_factory(),
                                           data_decoder_service_connector()));
  EXPECT_EQ(0u, ManifestFetchersCount(downloader.get()));

  // Extensions with empty update URLs should have a default one
  // filled in.
  id = crx_file::id_util::GenerateId("foo3");
  EXPECT_CALL(delegate, GetPingDataForExtension(id, _)).WillOnce(Return(false));
  EXPECT_TRUE(downloader->AddPendingExtension(
      id, GURL(), Manifest::INTERNAL, false, 0,
      ManifestFetchData::FetchPriority::BACKGROUND));
  downloader->StartAllPending(NULL);
  EXPECT_EQ(1u, ManifestFetchersCount(downloader.get()));

  RunUntilIdle();
  auto* request = &(*test_url_loader_factory_.pending_requests())[1];
  ASSERT_TRUE(request);
  EXPECT_FALSE(request->request.url.is_empty());
}

TEST_F(ExtensionUpdaterTest, TestStartUpdateCheckMemory) {
  MockService service(prefs_.get(), test_shared_url_loader_factory_);
  MockExtensionDownloaderDelegate delegate;
  ExtensionDownloader downloader(&delegate, service.url_loader_factory(),
                                 data_decoder_service_connector());

  StartUpdateCheck(&downloader,
                   CreateManifestFetchData(GURL("http://localhost/foo")));
  // This should delete the newly-created ManifestFetchData.
  StartUpdateCheck(&downloader,
                   CreateManifestFetchData(GURL("http://localhost/foo")));
  // This should add into |manifests_pending_|.
  StartUpdateCheck(&downloader,
                   CreateManifestFetchData(GURL("http://www.google.com")));
  // The dtor of |downloader| should delete the pending fetchers.
}

TEST_F(ExtensionUpdaterTest, TestCheckSoon) {
  ServiceForManifestTests service(prefs_.get(),
                                  test_shared_url_loader_factory_);
  ExtensionUpdater updater(&service,
                           service.extension_prefs(),
                           service.pref_service(),
                           service.profile(),
                           kUpdateFrequencySecs,
                           NULL,
                           service.GetDownloaderFactory());
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.Start();
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

TEST_F(ExtensionUpdaterTest, TestDisabledReasons1) {
  TestPingMetrics(1, {disable_reason::DISABLE_USER_ACTION,
                      disable_reason::DISABLE_PERMISSIONS_INCREASE |
                          disable_reason::DISABLE_CORRUPTED});
}

TEST_F(ExtensionUpdaterTest, TestDisabledReasons2) {
  TestPingMetrics(1, {});
}

TEST_F(ExtensionUpdaterTest, TestDisabledReasons3) {
  TestPingMetrics(0, {0});
}

TEST_F(ExtensionUpdaterTest, TestUninstallWhileUpdateCheck) {
  ServiceForManifestTests service(prefs_.get(),
                                  test_shared_url_loader_factory_);
  ExtensionList tmp;
  service.CreateTestExtensions(1, 1, &tmp, nullptr, Manifest::INTERNAL);
  service.set_extensions(tmp, ExtensionList());

  ASSERT_EQ(1u, tmp.size());
  ExtensionId id = tmp.front()->id();
  ASSERT_TRUE(service.GetExtensionById(id, false));

  ExtensionUpdater updater(&service,
                           service.extension_prefs(),
                           service.pref_service(),
                           service.profile(),
                           kUpdateFrequencySecs,
                           NULL,
                           service.GetDownloaderFactory());
  ExtensionUpdater::CheckParams params;
  params.ids = {id};
  updater.Start();
  updater.CheckNow(std::move(params));

  service.set_extensions(ExtensionList(), ExtensionList());
  ASSERT_FALSE(service.GetExtensionById(id, false));

  // RunUntilIdle is needed to make sure that the UpdateService instance that
  // runs the extension update process has a chance to exit gracefully; without
  // it, the test would crash.
  RunUntilIdle();
}

// Tests that we don't get a DCHECK failure when the next check time saved in
// prefs happens to be within one second of startup.
TEST_F(ExtensionUpdaterTest, TestPersistedNextCheckTime) {
  base::Time next_check_time =
      base::Time::Now() + base::TimeDelta::FromMilliseconds(500);
  prefs_->pref_service()->SetInt64(pref_names::kNextUpdateCheck,
                                   next_check_time.ToInternalValue());
  ServiceForManifestTests service(prefs_.get(),
                                  test_shared_url_loader_factory_);
  ExtensionUpdater updater(&service, service.extension_prefs(),
                           service.pref_service(), service.profile(),
                           kDefaultUpdateFrequencySeconds, nullptr,
                           service.GetDownloaderFactory());
  updater.Start();
  updater.Stop();
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchDataAddExtension) {
  TestManifestAddExtension(ManifestFetchData::FetchPriority::BACKGROUND,
                           ManifestFetchData::FetchPriority::BACKGROUND,
                           ManifestFetchData::FetchPriority::BACKGROUND);

  TestManifestAddExtension(ManifestFetchData::FetchPriority::BACKGROUND,
                           ManifestFetchData::FetchPriority::FOREGROUND,
                           ManifestFetchData::FetchPriority::FOREGROUND);

  TestManifestAddExtension(ManifestFetchData::FetchPriority::FOREGROUND,
                           ManifestFetchData::FetchPriority::BACKGROUND,
                           ManifestFetchData::FetchPriority::FOREGROUND);

  TestManifestAddExtension(ManifestFetchData::FetchPriority::FOREGROUND,
                           ManifestFetchData::FetchPriority::FOREGROUND,
                           ManifestFetchData::FetchPriority::FOREGROUND);
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchDataMerge) {
  TestManifestMerge(ManifestFetchData::FetchPriority::BACKGROUND,
                    ManifestFetchData::FetchPriority::BACKGROUND,
                    ManifestFetchData::FetchPriority::BACKGROUND);

  TestManifestMerge(ManifestFetchData::FetchPriority::BACKGROUND,
                    ManifestFetchData::FetchPriority::FOREGROUND,
                    ManifestFetchData::FetchPriority::FOREGROUND);

  TestManifestMerge(ManifestFetchData::FetchPriority::FOREGROUND,
                    ManifestFetchData::FetchPriority::BACKGROUND,
                    ManifestFetchData::FetchPriority::FOREGROUND);

  TestManifestMerge(ManifestFetchData::FetchPriority::FOREGROUND,
                    ManifestFetchData::FetchPriority::FOREGROUND,
                    ManifestFetchData::FetchPriority::FOREGROUND);
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
