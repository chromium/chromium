// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_test_file_activity_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace download {
class DownloadItem;
}

// Test ChromeDownloadManagerDelegate that controls whether how file chooser
// dialogs are handled, and how files are opend.
// By default, file chooser dialogs are disabled.
class DownloadTestFileActivityObserver::MockDownloadManagerDelegate
    : public ChromeDownloadManagerDelegate {
 public:
  explicit MockDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile),
        file_chooser_enabled_(false),
        file_chooser_displayed_(false) {
    if (!profile->IsOffTheRecord())
      GetDownloadIdReceiverCallback().Run(download::DownloadItem::kInvalidId +
                                          1);
  }

  ~MockDownloadManagerDelegate() override {}

  void EnableFileChooser(bool enable) {
    file_chooser_enabled_ = enable;
  }

  bool TestAndResetDidShowFileChooser() {
    bool did_show = file_chooser_displayed_;
    file_chooser_displayed_ = false;
    return did_show;
  }

  base::WeakPtr<MockDownloadManagerDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  void ShowFilePickerForDownload(
      download::DownloadItem* download,
      const base::FilePath& suggested_path,
      const DownloadTargetDeterminerDelegate::ConfirmationCallback& callback)
      override {
    file_chooser_displayed_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MockDownloadManagerDelegate::OnConfirmationCallbackComplete,
            base::Unretained(this), callback,
            (file_chooser_enabled_ ? DownloadConfirmationResult::CONFIRMED
                                   : DownloadConfirmationResult::CANCELED),
            suggested_path));
  }

  void OpenDownload(download::DownloadItem* item) override {}

 private:
  bool file_chooser_enabled_;
  bool file_chooser_displayed_;
  base::WeakPtrFactory<MockDownloadManagerDelegate> weak_ptr_factory_{this};
};

DownloadTestFileActivityObserver::DownloadTestFileActivityObserver(
    Profile* profile) {
  std::unique_ptr<MockDownloadManagerDelegate> mock_delegate(
      new MockDownloadManagerDelegate(profile));
  test_delegate_ = mock_delegate->GetWeakPtr();
  DownloadCoreServiceFactory::GetForBrowserContext(profile)
      ->SetDownloadManagerDelegateForTesting(std::move(mock_delegate));
}

DownloadTestFileActivityObserver::~DownloadTestFileActivityObserver() {
}

void DownloadTestFileActivityObserver::EnableFileChooser(bool enable) {
  if (test_delegate_.get())
    test_delegate_->EnableFileChooser(enable);
}

bool DownloadTestFileActivityObserver::TestAndResetDidShowFileChooser() {
  return test_delegate_.get() &&
      test_delegate_->TestAndResetDidShowFileChooser();
}
