// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/download_controller_client_lacros.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"

// Helpers ---------------------------------------------------------------------

std::unique_ptr<download::DownloadItem> CreateDownloadItemWithStartTimeOffset(
    Profile* profile,
    base::TimeDelta start_time_offset) {
  auto download = std::make_unique<content::FakeDownloadItem>();
  download->SetState(download::DownloadItem::IN_PROGRESS);
  download->SetStartTime(base::Time::Now() + start_time_offset);
  content::DownloadItemUtils::AttachInfoForTesting(download.get(), profile,
                                                   nullptr);
  return download;
}

bool IsSortedChronologicallyByStartTime(
    const std::vector<crosapi::mojom::DownloadItemPtr>& downloads) {
  for (size_t i = 1; i < downloads.size(); ++i) {
    if (downloads[i]->start_time.value_or(base::Time()) <
        downloads[i - 1]->start_time.value_or(base::Time())) {
      return false;
    }
  }
  return true;
}

// DownloadControllerClientLacrosBrowserTest -----------------------------------

class DownloadControllerClientLacrosBrowserTest : public InProcessBrowserTest {
 public:
  crosapi::mojom::DownloadControllerClient* download_controller_client() {
    return download_controller_client_.get();
  }

  testing::NiceMock<content::MockDownloadManager>* download_manager() {
    return static_cast<testing::NiceMock<content::MockDownloadManager>*>(
        browser()->profile()->GetDownloadManager());
  }

  Profile* profile() { return browser()->profile(); }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Download manager.
    auto download_manager =
        std::make_unique<testing::NiceMock<content::MockDownloadManager>>();
    profile()->GetDownloadManager()->Shutdown();
    profile()->SetDownloadManagerForTesting(std::move(download_manager));

    // Download controller client.
    download_controller_client_ =
        std::make_unique<DownloadControllerClientLacros>();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    download_controller_client_.reset();
  }

  std::unique_ptr<crosapi::mojom::DownloadControllerClient>
      download_controller_client_;
};

// Tests -----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(DownloadControllerClientLacrosBrowserTest,
                       GetAllDownloads) {
  // Create a few `downloads`.
  std::vector<std::unique_ptr<download::DownloadItem>> downloads;
  downloads.push_back(
      CreateDownloadItemWithStartTimeOffset(profile(), base::Minutes(10)));
  downloads.push_back(
      CreateDownloadItemWithStartTimeOffset(profile(), -base::Minutes(10)));

  // Mock `download_manager()` response for `GetAllDownloads()`.
  EXPECT_CALL(*download_manager(), GetAllDownloads)
      .WillRepeatedly(testing::Invoke(
          [&](std::vector<download::DownloadItem*>* download_ptrs) {
            for (auto& download : downloads)
              download_ptrs->push_back(download.get());
          }));

  // Initially indicate that `download_manager()` is uninitialized.
  ON_CALL(*download_manager(), IsManagerInitialized())
      .WillByDefault(testing::Return(false));

  {
    // Invoke `GetAllDownloads()`.
    base::RunLoop run_loop;
    download_controller_client()->GetAllDownloads(base::BindLambdaForTesting(
        [&](std::vector<crosapi::mojom::DownloadItemPtr> downloads) {
          EXPECT_EQ(downloads.size(), 0u);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Now indicate that `download_manager()` is initialized.
  ON_CALL(*download_manager(), IsManagerInitialized())
      .WillByDefault(testing::Return(true));

  {
    // Invoke `GetAllDownloads()`.
    base::RunLoop run_loop;
    download_controller_client()->GetAllDownloads(base::BindLambdaForTesting(
        [&](std::vector<crosapi::mojom::DownloadItemPtr> downloads) {
          EXPECT_EQ(downloads.size(), 2u);
          EXPECT_TRUE(IsSortedChronologicallyByStartTime(downloads));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}
