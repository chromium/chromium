// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_entropy_provider.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/content/browser/content_visit_delegate.h"
#include "components/history/content/browser/download_conversions.h"
#include "components/history/content/browser/history_database_helper.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A testing factory that creates a HistoryService for a TestingProfile.
std::unique_ptr<KeyedService> BuildHistoryService(
    content::BrowserContext* context) {
  TestingProfile* profile = static_cast<TestingProfile*>(context);

  // Delete the file before creating the service.
  base::FilePath history_path(
      profile->GetPath().Append(history::kHistoryFilename));
  if (!base::DeleteFile(history_path) || base::PathExists(history_path)) {
    ADD_FAILURE() << "failed to delete history db file "
                  << history_path.value();
    return nullptr;
  }

  std::unique_ptr<history::HistoryService> history_service(
      new history::HistoryService(
          std::make_unique<ChromeHistoryClient>(
              BookmarkModelFactory::GetForBrowserContext(profile)),
          std::unique_ptr<history::VisitDelegate>()));
  if (history_service->Init(history::HistoryDatabaseParamsForPath(
          profile->GetPath(), version_info::Channel::UNKNOWN))) {
    return std::move(history_service);
  }

  ADD_FAILURE() << "failed to initialize history service";
  return nullptr;
}

#if BUILDFLAG(IS_WIN)
static const base::FilePath::CharType kBinaryFileName[] =
    FILE_PATH_LITERAL("spam.exe");
static const base::FilePath::CharType kBinaryFileNameForOtherOS[] =
    FILE_PATH_LITERAL("spam.dmg");
#elif BUILDFLAG(IS_MAC)
static const base::FilePath::CharType kBinaryFileName[] =
    FILE_PATH_LITERAL("spam.dmg");
static const base::FilePath::CharType kBinaryFileNameForOtherOS[] =
    FILE_PATH_LITERAL("spam.apk");
#elif BUILDFLAG(IS_ANDROID)
static const base::FilePath::CharType kBinaryFileName[] =
    FILE_PATH_LITERAL("spam.apk");
static const base::FilePath::CharType kBinaryFileNameForOtherOS[] =
    FILE_PATH_LITERAL("spam.dmg");
#else
static const base::FilePath::CharType kBinaryFileName[] =
    FILE_PATH_LITERAL("spam.exe");
#endif

static const base::FilePath::CharType kTxtFileName[] =
    FILE_PATH_LITERAL("download.txt");

}  // namespace

namespace safe_browsing {

class LastDownloadFinderTest : public testing::Test {
 public:
  void NeverCalled(
      std::unique_ptr<ClientIncidentReport_DownloadDetails> download,
      std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
          non_binary_download) {
    FAIL();
  }

  // Creates a new profile that participates in safe browsing extended reporting
  // and adds a download to its history.
  void CreateProfileWithDownload() {
    TestingProfile* profile =
        CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    history_service->CreateDownload(
        CreateTestDownloadRow(kBinaryFileName, profile),
        base::BindOnce(&LastDownloadFinderTest::OnDownloadCreated,
                       base::Unretained(this)));
  }

  // LastDownloadFinder::LastDownloadCallback implementation that
  // passes the found download to |result| and then runs a closure.
  void OnLastDownload(
      std::unique_ptr<ClientIncidentReport_DownloadDetails>* result,
      std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>*
          non_binary_result,
      base::OnceClosure quit_closure,
      std::unique_ptr<ClientIncidentReport_DownloadDetails> download,
      std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
          non_binary_download) {
    *result = std::move(download);
    *non_binary_result = std::move(non_binary_download);
    std::move(quit_closure).Run();
  }

 protected:
  // A type for specifying whether a profile created by CreateProfile
  // participates in safe browsing and safe browsing extended reporting.
  enum SafeBrowsingDisposition {
    OPT_OUT,
    SAFE_BROWSING_ONLY,
    EXTENDED_REPORTING_ONLY,
    SAFE_BROWSING_AND_EXTENDED_REPORTING,
  };

  LastDownloadFinderTest() : profile_number_(), download_id_(1) {}

  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  TestingProfile* CreateProfile(SafeBrowsingDisposition safe_browsing_opt_in) {
    std::string profile_name("profile");
    profile_name.append(base::NumberToString(++profile_number_));

    // Set up keyed service factories.
    TestingProfile::TestingFactories factories;
    // Build up a custom history service.
    factories.emplace_back(HistoryServiceFactory::GetInstance(),
                           base::BindRepeating(&BuildHistoryService));
    // Suppress WebHistoryService since it makes network requests.
    factories.emplace_back(WebHistoryServiceFactory::GetInstance(),
                           BrowserContextKeyedServiceFactory::TestingFactory());

    // Create prefs for the profile with safe browsing enabled or not.
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs(
        new sync_preferences::TestingPrefServiceSyncable);
    RegisterUserProfilePrefs(prefs->registry());
    prefs->SetBoolean(
        prefs::kSafeBrowsingEnabled,
        safe_browsing_opt_in == SAFE_BROWSING_ONLY ||
            safe_browsing_opt_in == SAFE_BROWSING_AND_EXTENDED_REPORTING);
    safe_browsing::SetExtendedReportingPrefForTests(
        prefs.get(),
        safe_browsing_opt_in == EXTENDED_REPORTING_ONLY ||
            safe_browsing_opt_in == SAFE_BROWSING_AND_EXTENDED_REPORTING);

    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        profile_name, std::move(prefs),
        base::UTF8ToUTF16(profile_name),  // user_name
        0,                                // avatar_id
        std::move(factories));

    return profile;
  }

  LastDownloadFinder::DownloadDetailsGetter GetDownloadDetailsGetter() {
    return base::BindRepeating(&LastDownloadFinderTest::GetDownloadDetails,
                               base::Unretained(this));
  }

  void AddDownload(Profile* profile, const history::DownloadRow& download) {
    base::RunLoop run_loop;

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    history_service->CreateDownload(
        download,
        base::BindOnce(&LastDownloadFinderTest::ContinueOnDownloadCreated,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Runs the last download finder on all loaded profiles.
  void RunLastDownloadFinder(
      std::unique_ptr<ClientIncidentReport_DownloadDetails>*
          last_binary_download,
      std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>*
          last_non_binary_download) {
    base::RunLoop run_loop;

    std::unique_ptr<LastDownloadFinder> finder(LastDownloadFinder::Create(
        GetDownloadDetailsGetter(),
        base::BindOnce(&LastDownloadFinderTest::OnLastDownload,
                       base::Unretained(this), last_binary_download,
                       last_non_binary_download, run_loop.QuitClosure())));

    if (finder)
      run_loop.Run();
  }

  history::DownloadRow CreateTestDownloadRow(
      const base::FilePath::CharType* file_path,
      content::BrowserContext* browser_context) {
    base::Time now(base::Time::Now());

    auto* download_manager = browser_context->GetDownloadManager();
    content::StoragePartitionConfig storage_partition_config =
        download_manager->GetStoragePartitionConfigForSiteUrl(
            GURL("http://site-url.com/"));
    history::DownloadRow row;
    row.current_path = base::FilePath(file_path);
    row.target_path = base::FilePath(file_path);
    row.url_chain.push_back(GURL("http://www.google.com/"));
    row.referrer_url = GURL("http://referrer.example.com/");
    row.embedder_download_data =
        download_manager
            ->StoragePartitionConfigToSerializedEmbedderDownloadData(
                storage_partition_config);
    row.tab_url = GURL("http://tab-url.example.com/");
    row.tab_referrer_url = GURL("http://tab-referrer.example.com/");
    row.mime_type = "application/octet-stream";
    row.original_mime_type = "application/octet-stream";
    row.start_time = now - base::Minutes(10);
    row.end_time = now - base::Minutes(9);
    row.received_bytes = 47;
    row.total_bytes = 47;
    row.state = history::DownloadState::COMPLETE;
    row.danger_type = history::DownloadDangerType::NOT_DANGEROUS;
    row.interrupt_reason = history::ToHistoryDownloadInterruptReason(
        download::DOWNLOAD_INTERRUPT_REASON_NONE);
    row.id = download_id_++;
    row.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    row.opened = false;
    row.last_access_time = now - base::Minutes(5);
    row.transient = false;

    return row;
  }

 private:
  // A HistoryService::DownloadCreateCallback that asserts that the download was
  // created and runs |closure|.
  void ContinueOnDownloadCreated(base::OnceClosure closure, bool created) {
    ASSERT_TRUE(created);
    std::move(closure).Run();
  }

  // A HistoryService::DownloadCreateCallback that asserts that the download was
  // created.
  void OnDownloadCreated(bool created) { ASSERT_TRUE(created); }

  void GetDownloadDetails(
      content::BrowserContext* context,
      DownloadMetadataManager::GetDownloadDetailsCallback callback) {
    std::move(callback).Run(
        std::unique_ptr<ClientIncidentReport_DownloadDetails>());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  int profile_number_;

  // Incremented on every download addition to avoid downloads with the same id.
  int download_id_;
};

// Tests that nothing happens if there are no profiles at all.
TEST_F(LastDownloadFinderTest, NoProfiles) {
  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  RunLastDownloadFinder(&last_binary_download, &last_non_binary_download);
  EXPECT_FALSE(last_binary_download);
  EXPECT_FALSE(last_non_binary_download);
}

// Tests that nothing happens other than the callback being invoked if there are
// no profiles participating in safe browsing.
TEST_F(LastDownloadFinderTest, NoSafeBrowsingProfile) {
  // Create a profile with a history service that is opted-out
  TestingProfile* profile = CreateProfile(EXTENDED_REPORTING_ONLY);

  // Add a download.
  AddDownload(profile, CreateTestDownloadRow(kBinaryFileName, profile));

  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  RunLastDownloadFinder(&last_binary_download, &last_non_binary_download);
  EXPECT_FALSE(last_binary_download);
  EXPECT_FALSE(last_non_binary_download);
}

// Tests that nothing happens other than the callback being invoked if there are
// no profiles participating in safe browsing extended reporting.
TEST_F(LastDownloadFinderTest, NoExtendedReportingProfile) {
  // Create a profile with a history service that is opted-out
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_ONLY);

  // Add a download.
  AddDownload(profile, CreateTestDownloadRow(kBinaryFileName, profile));

  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  RunLastDownloadFinder(&last_binary_download, &last_non_binary_download);
  EXPECT_FALSE(last_binary_download);
  EXPECT_FALSE(last_non_binary_download);
}

// Tests that a download is found from a single profile.
TEST_F(LastDownloadFinderTest, SimpleEndToEnd) {
  // Create a profile with a history service that is opted-in.
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  // Add a binary and non-binary download.
  AddDownload(profile, CreateTestDownloadRow(kBinaryFileName, profile));
  AddDownload(profile, CreateTestDownloadRow(kTxtFileName, profile));

  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  RunLastDownloadFinder(&last_binary_download, &last_non_binary_download);
  EXPECT_TRUE(last_binary_download);
  EXPECT_TRUE(last_non_binary_download);
}

// Tests that a non-binary download is found
TEST_F(LastDownloadFinderTest, NonBinaryOnly) {
  // Create a profile with a history service that is opted-in.
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  // Add a non-binary download.
  AddDownload(profile, CreateTestDownloadRow(kTxtFileName, profile));

  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  RunLastDownloadFinder(&last_binary_download, &last_non_binary_download);
  EXPECT_FALSE(last_binary_download);
  EXPECT_TRUE(last_non_binary_download);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Tests that nothing happens if the binary is an executable for a different OS.
TEST_F(LastDownloadFinderTest, DownloadForDifferentOs) {
  // Create a profile with a history service that is opted-in.
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  // Add a download.
  AddDownload(profile,
              CreateTestDownloadRow(kBinaryFileNameForOtherOS, profile));

  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  RunLastDownloadFinder(&last_binary_download, &last_non_binary_download);
  EXPECT_FALSE(last_binary_download);
  EXPECT_FALSE(last_non_binary_download);
}
#endif

// Tests that there is no crash if the finder is deleted before results arrive.
TEST_F(LastDownloadFinderTest, DeleteBeforeResults) {
  // Create a profile with a history service that is opted-in.
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  // Add a download.
  AddDownload(profile, CreateTestDownloadRow(kBinaryFileName, profile));

  // Start a finder and kill it before the search completes.
  LastDownloadFinder::Create(
      GetDownloadDetailsGetter(),
      base::BindOnce(&LastDownloadFinderTest::NeverCalled,
                     base::Unretained(this)))
      .reset();
}

// Tests that a download in profile added after the search is begun is found.
TEST_F(LastDownloadFinderTest, AddProfileAfterStarting) {
  // Create a profile with a history service that is opted-in.
  CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      last_non_binary_download;
  base::RunLoop run_loop;

  // Post a task that will create a second profile once the main loop is run.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LastDownloadFinderTest::CreateProfileWithDownload,
                     base::Unretained(this)));

  // Create a finder that we expect will find a download in the second profile.
  std::unique_ptr<LastDownloadFinder> finder(LastDownloadFinder::Create(
      GetDownloadDetailsGetter(),
      base::BindOnce(&LastDownloadFinderTest::OnLastDownload,
                     base::Unretained(this), &last_binary_download,
                     &last_non_binary_download, run_loop.QuitClosure())));

  run_loop.Run();

  ASSERT_TRUE(last_binary_download);
}

}  // namespace safe_browsing
