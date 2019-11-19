// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/downloads_counter.h"

#include <set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/download_row.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension.h"
#endif

namespace {

class DownloadsCounterTest : public InProcessBrowserTest,
                             public DownloadHistory::Observer {
 public:
  void SetUpOnMainThread() override {
    time_ = base::Time::Now();
    items_count_ = 0;
    manager_ =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    history_ =
        DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
            ->GetDownloadHistory();
    history_->AddObserver(this);

    otr_manager_ =
        content::BrowserContext::GetDownloadManager(
            browser()->profile()->GetOffTheRecordProfile());
    SetDownloadsDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void TearDownOnMainThread() override {
    history_->RemoveObserver(this);
  }

  // Adding and removing download items. ---------------------------------------

  std::string AddDownload() {
    std::string guid = AddDownloadInternal(
        download::DownloadItem::COMPLETE,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        download::DOWNLOAD_INTERRUPT_REASON_NONE, GURL(), std::string(), false);
    guids_to_add_.insert(guid);
    return guid;
  }

  std::string AddIncognitoDownload() {
    // Incognito downloads are not expected to be persisted. We don't need to
    // wait for a callback from them, so we don't add them to |guids_to_add_|.
    return AddDownloadInternal(download::DownloadItem::COMPLETE,
                               download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                               download::DOWNLOAD_INTERRUPT_REASON_NONE, GURL(),
                               std::string(), true);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::string AddExtensionDownload() {
    // Extension downloads are not expected to be persisted. We don't need to
    // wait for a callback from them, so we don't add them to |guids_to_add_|.
    return AddDownloadInternal(download::DownloadItem::COMPLETE,
                               download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                               download::DOWNLOAD_INTERRUPT_REASON_NONE, GURL(),
                               extensions::Extension::kMimeType, false);
  }

  std::string AddUserScriptDownload() {
    // User script downloads are not expected to be persisted. We don't need to
    // wait for a callback from them, so we don't add them to |guids_to_add_|.
    return AddDownloadInternal(download::DownloadItem::COMPLETE,
                               download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                               download::DOWNLOAD_INTERRUPT_REASON_NONE,
                               GURL("file:///download.user.js"),
                               "text/javascript", false);
  }
#endif

  std::string AddDownloadWithProperties(
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger,
      download::DownloadInterruptReason reason) {
    std::string guid = AddDownloadInternal(
        state, danger, reason, GURL(), std::string(), false);
    guids_to_add_.insert(guid);
    return guid;
  }

  std::string AddDownloadInternal(download::DownloadItem::DownloadState state,
                                  download::DownloadDangerType danger,
                                  download::DownloadInterruptReason reason,
                                  const GURL& url,
                                  std::string mime_type,
                                  bool incognito) {
    std::string guid = base::GenerateGUID();

    std::vector<GURL> url_chain;
    url_chain.push_back(url);

    content::DownloadManager* manager = incognito ? otr_manager_ : manager_;
    manager->CreateDownloadItem(
        guid, download::DownloadItem::kInvalidId + (++items_count_),
        base::FilePath(FILE_PATH_LITERAL("current/path")),
        base::FilePath(FILE_PATH_LITERAL("target/path")), url_chain, GURL(),
        GURL(), GURL(), GURL(), url::Origin(), mime_type, std::string(), time_,
        time_, std::string(), std::string(), 1, 1, std::string(), state, danger,
        reason, false, time_, false,
        std::vector<download::DownloadItem::ReceivedSlice>());

    return guid;
  }

  void RemoveDownload(const std::string& guid) {
    download::DownloadItem* item = manager_->GetDownloadByGuid(guid);
    ids_to_remove_.insert(item->GetId());
    item->Remove();
  }

  // Miscellaneous. ------------------------------------------------------------

  void SetDownloadsDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeleteDownloadHistory, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void RevertTimeInHours(int days) {
    time_ -= base::TimeDelta::FromHours(days);
  }

  // Waiting for downloads to be stored. ---------------------------------------

  void OnDownloadStored(download::DownloadItem* item,
                        const history::DownloadRow& info) override {
    // Ignore any updates on items that we have already processed.
    if (guids_to_add_.find(item->GetGuid()) == guids_to_add_.end())
      return;

    // DownloadHistory updates us before the item is actually written on
    // the history thread. Ignore this and wait until the item is actually
    // persisted.
    if (!DownloadHistory::IsPersisted(item))
      return;

    guids_to_add_.erase(item->GetGuid());

    if (run_loop_ && guids_to_add_.empty())
      run_loop_->Quit();
  }

  void OnDownloadsRemoved(const DownloadHistory::IdSet& ids) override {
    for (uint32_t id : ids)
      ASSERT_EQ(1u, ids_to_remove_.erase(id));

    if (run_loop_ && ids_to_remove_.empty())
      run_loop_->Quit();
  }

  void WaitForDownloadHistory() {
    if (guids_to_add_.empty() && ids_to_remove_.empty())
      return;

    DCHECK(!run_loop_ || !run_loop_->running());
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  // Retrieving the result. ----------------------------------------------------

  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void ResultCallback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      result_ =
          static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
              result.get())
              ->Value();
    }
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  // GUIDs of download items that were added and for which we expect
  // the OnDownloadStored() callback to be called.
  std::set<std::string> guids_to_add_;

  // IDs of download items that are being removed from the download service
  // and for which we expect the OnDownloadsRemoved() callback. Unlike in
  // |guids_to_add_|, we don't store GUIDs, because OnDownloadsRemoved() returns
  // a set of IDs.
  std::set<uint32_t> ids_to_remove_;

  content::DownloadManager* manager_;
  content::DownloadManager* otr_manager_;
  DownloadHistory* history_;
  base::Time time_;

  int items_count_;

  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;
};

// Tests that we count the total number of downloads correctly.
IN_PROC_BROWSER_TEST_F(DownloadsCounterTest, Count) {
  Profile* profile = browser()->profile();
  DownloadsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::Bind(&DownloadsCounterTest::ResultCallback,
                          base::Unretained(this)));
  counter.Restart();
  EXPECT_EQ(0u, GetResult());

  std::string first_download = AddDownload();
  AddDownload();
  std::string last_download = AddDownload();
  WaitForDownloadHistory();
  counter.Restart();
  EXPECT_EQ(3, GetResult());

  RemoveDownload(last_download);
  RemoveDownload(first_download);
  WaitForDownloadHistory();
  counter.Restart();
  EXPECT_EQ(1, GetResult());

  AddDownload();
  WaitForDownloadHistory();
  counter.Restart();
  EXPECT_EQ(2, GetResult());
}

// Tests that not just standard complete downloads are counted.
IN_PROC_BROWSER_TEST_F(DownloadsCounterTest, Types) {
  Profile* profile = browser()->profile();
  DownloadsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::Bind(&DownloadsCounterTest::ResultCallback,
                          base::Unretained(this)));

  AddDownload();
  AddDownloadWithProperties(download::DownloadItem::COMPLETE,
                            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
                            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  AddDownloadWithProperties(download::DownloadItem::COMPLETE,
                            download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED,
                            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  AddDownloadWithProperties(download::DownloadItem::CANCELLED,
                            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  AddDownloadWithProperties(download::DownloadItem::INTERRUPTED,
                            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                            download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  AddDownloadWithProperties(download::DownloadItem::INTERRUPTED,
                            download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                            download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);

  WaitForDownloadHistory();
  counter.Restart();
  EXPECT_EQ(6u, GetResult());
}

// Tests that downloads not persisted by DownloadHistory are not counted.
IN_PROC_BROWSER_TEST_F(DownloadsCounterTest, NotPersisted) {
  Profile* profile = browser()->profile();
  DownloadsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::Bind(&DownloadsCounterTest::ResultCallback,
                          base::Unretained(this)));

  // Extension and user scripts download are not persisted.
  AddDownload();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  AddUserScriptDownload();
  AddExtensionDownload();
#endif

  WaitForDownloadHistory();
  counter.Restart();
  EXPECT_EQ(1u, GetResult());

  // Neither are downloads in incognito mode.
  AddIncognitoDownload();

  WaitForDownloadHistory();
  counter.Restart();
  EXPECT_EQ(1u, GetResult());
}

// Tests that the counter takes time ranges into account.
// Flaky on Mac (crbug.com/736820)
#if defined(OS_MACOSX)
#define MAYBE_TimeRanges DISABLED_TimeRanges
#else
#define MAYBE_TimeRanges TimeRanges
#endif
IN_PROC_BROWSER_TEST_F(DownloadsCounterTest, MAYBE_TimeRanges) {
  AddDownload();
  AddDownload();  // 2 items

  RevertTimeInHours(12);
  AddDownload();
  AddDownload();
  AddDownload();  // 5 items

  RevertTimeInHours(2 * 24);
  AddDownload();
  AddDownload();  // 7 items

  RevertTimeInHours(10 * 24);
  AddDownload();  // 8 items

  RevertTimeInHours(30 * 24);
  AddDownload();
  AddDownload();
  AddDownload();  // 11 items

  WaitForDownloadHistory();

  Profile* profile = browser()->profile();
  DownloadsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::Bind(&DownloadsCounterTest::ResultCallback,
                          base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(2u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  EXPECT_EQ(5u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  EXPECT_EQ(7u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  EXPECT_EQ(8u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  EXPECT_EQ(11u, GetResult());
}

}  // namespace
