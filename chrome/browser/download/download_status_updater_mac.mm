// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/memory/scoped_policy.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#import "chrome/browser/ui/cocoa/dock_icon.h"
#include "components/download/public/common/download_item.h"
#import "net/base/apple/url_conversions.h"

namespace {

const char kCrNSProgressUserDataKey[] = "CrNSProgressUserData";

class CrNSProgressUserData : public base::SupportsUserData::Data {
 public:
  CrNSProgressUserData(NSProgress* progress, const base::FilePath& target)
      : target_(target) {
    progress_ = progress;
  }
  ~CrNSProgressUserData() override { [progress_ unpublish]; }

  NSProgress* progress() const { return progress_; }
  base::FilePath target() const { return target_; }
  void setTarget(const base::FilePath& target) { target_ = target; }

 private:
  NSProgress* __strong progress_;
  base::FilePath target_;
};

void UpdateAppDockIcon(int download_count,
                       bool progress_known,
                       float progress) {
  DockIcon* dock_icon = [DockIcon sharedDockIcon];
  [dock_icon setDownloads:download_count];
  [dock_icon setIndeterminate:!progress_known];
  [dock_icon setProgress:progress];
  [dock_icon updateIcon];
}

CrNSProgressUserData* CreateOrGetNSProgress(download::DownloadItem* download) {
  CrNSProgressUserData* progress_data = static_cast<CrNSProgressUserData*>(
      download->GetUserData(&kCrNSProgressUserDataKey));
  if (progress_data)
    return progress_data;

  base::FilePath destination_path = download->GetFullPath();
  NSURL* destination_url = base::apple::FilePathToNSURL(destination_path);

  NSProgress* progress = [NSProgress progressWithTotalUnitCount:-1];
  progress.kind = NSProgressKindFile;
  progress.fileOperationKind = NSProgressFileOperationKindDownloading;
  progress.fileURL = destination_url;

  // Don't publish a pause/resume handler. The only users of `NSProgress` are
  // outside of Chromium, and none currently implement pausing published
  // progresses. Because there is no way to test pausing, do not implement or
  // ship it.
  progress.pausable = NO;

  // Do publish a cancellation handler. In icon view, the Finder provides a
  // little (X) button on the icon, and using it will cause this callback.
  progress.cancellable = YES;
  progress.cancellationHandler = ^{
    dispatch_async(dispatch_get_main_queue(), ^{
      download->Cancel(/*user_cancel=*/true);
    });
  };

  [progress publish];

  download->SetUserData(
      &kCrNSProgressUserDataKey,
      std::make_unique<CrNSProgressUserData>(progress, destination_path));

  return static_cast<CrNSProgressUserData*>(
      download->GetUserData(&kCrNSProgressUserDataKey));
}

void UpdateNSProgress(download::DownloadItem* download) {
  CrNSProgressUserData* progress_data = CreateOrGetNSProgress(download);

  NSProgress* progress = progress_data->progress();
  progress.totalUnitCount = download->GetTotalBytes();
  progress.completedUnitCount = download->GetReceivedBytes();
  progress.throughput = @(download->CurrentSpeed());

  base::TimeDelta time_remaining;
  NSNumber* ns_time_remaining = nil;
  if (download->TimeRemaining(&time_remaining))
    ns_time_remaining = @(time_remaining.InSeconds());
  progress.estimatedTimeRemaining = ns_time_remaining;

  base::FilePath download_path = download->GetFullPath();
  if (progress_data->target() != download_path) {
    progress_data->setTarget(download_path);
    NSURL* download_url = base::apple::FilePathToNSURL(download_path);
    progress.fileURL = download_url;
  }
}

void DestroyNSProgress(download::DownloadItem* download) {
  download->RemoveUserData(&kCrNSProgressUserDataKey);
}

}  // namespace

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  // Always update overall progress in the Dock icon.

  float progress = 0;
  int download_count = 0;
  bool progress_known = GetProgress(&progress, &download_count);
  UpdateAppDockIcon(download_count, progress_known, progress);

  // Update `NSProgress`-based indicators. Only show progress:
  //   - if the download is IN_PROGRESS, and
  //   - it has not yet saved all the data, and
  //   - it hasn't been renamed to its final name.
  //
  // There's a race condition in macOS code where unpublishing an `NSProgress`
  // object for a file that was renamed will sometimes leave a progress
  // indicator visible in the Finder (https://crbug.com/1304233). Therefore, as
  // soon as `DownloadItem::AllDataSaved()` returns true, do the unpublish.
  // As an additional bug to avoid (http://crbug.com/166683), never update the
  // data of an `NSProgress` after the file name has changed, as that can result
  // in the file being stuck in an in-progress state in the Dock.
  if (download->GetState() == download::DownloadItem::IN_PROGRESS &&
      !download->AllDataSaved() && !download->GetFullPath().empty() &&
      download->GetFullPath() != download->GetTargetFilePath()) {
    UpdateNSProgress(download);
  } else {
    DestroyNSProgress(download);
  }

  // Handle downloads that ended.
  if (download->GetState() != download::DownloadItem::IN_PROGRESS &&
      !download->GetTargetFilePath().empty()) {
    NSString* download_path =
        base::apple::FilePathToNSString(download->GetTargetFilePath());
    if (download->GetState() == download::DownloadItem::COMPLETE) {
      // Bounce the dock icon.
      [NSDistributedNotificationCenter.defaultCenter
          postNotificationName:@"com.apple.DownloadFileFinished"
                        object:download_path];
    }

    // Notify the Finder.
    [NSWorkspace.sharedWorkspace noteFileSystemChanged:download_path];
  }
}
