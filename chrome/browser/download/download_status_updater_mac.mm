// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#import "chrome/browser/ui/cocoa/dock_icon.h"
#include "components/download/public/common/download_item.h"
#include "url/gurl.h"

namespace {

// These are not the keys themselves; they are the names for dynamic lookup via
// the ProgressString() function.

// Public keys, SPI in 10.8, API in 10.9:
NSString* const kNSProgressEstimatedTimeRemainingKeyName =
    @"NSProgressEstimatedTimeRemainingKey";
NSString* const kNSProgressFileOperationKindDownloadingName =
    @"NSProgressFileOperationKindDownloading";
NSString* const kNSProgressFileOperationKindKeyName =
    @"NSProgressFileOperationKindKey";
NSString* const kNSProgressFileURLKeyName =
    @"NSProgressFileURLKey";
NSString* const kNSProgressKindFileName =
    @"NSProgressKindFile";
NSString* const kNSProgressThroughputKeyName =
    @"NSProgressThroughputKey";

// Private keys, SPI in 10.8 and 10.9:
// TODO(avi): Are any of these actually needed for the NSProgress integration?
NSString* const kNSProgressFileDownloadingSourceURLKeyName =
    @"NSProgressFileDownloadingSourceURLKey";
NSString* const kNSProgressFileLocationCanChangeKeyName =
    @"NSProgressFileLocationCanChangeKey";

// Given an NSProgress string name (kNSProgress[...]Name above), looks up the
// real symbol of that name from Foundation and returns it.
NSString* ProgressString(NSString* string) {
  static NSMutableDictionary* cache;
  static CFBundleRef foundation;
  if (!cache) {
    cache = [[NSMutableDictionary alloc] init];
    foundation = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Foundation"));
  }

  NSString* result = cache[string];
  if (!result) {
    NSString** ref = static_cast<NSString**>(
        CFBundleGetDataPointerForName(foundation,
                                      base::mac::NSToCFCast(string)));
    if (ref) {
      result = *ref;
      cache[string] = result;
    }
  }

  if (!result && string == kNSProgressEstimatedTimeRemainingKeyName) {
    // Perhaps this is 10.8; try the old name of this key.
    NSString** ref = static_cast<NSString**>(
        CFBundleGetDataPointerForName(foundation,
                                      CFSTR("NSProgressEstimatedTimeKey")));
    if (ref) {
      result = *ref;
      cache[string] = result;
    }
  }

  if (!result) {
    // Huh. At least return a local copy of the expected string.
    result = string;
    NSString* const kKeySuffix = @"Key";
    if ([result hasSuffix:kKeySuffix])
      result = [result substringToIndex:[result length] - [kKeySuffix length]];
  }

  return result;
}

bool NSProgressSupported() {
  static bool supported;
  static bool valid;
  if (!valid) {
    supported = NSClassFromString(@"NSProgress");
    valid = true;
  }

  return supported;
}

const char kCrNSProgressUserDataKey[] = "CrNSProgressUserData";

class CrNSProgressUserData : public base::SupportsUserData::Data {
 public:
  CrNSProgressUserData(NSProgress* progress, const base::FilePath& target)
      : target_(target) {
    progress_.reset(progress);
  }
  ~CrNSProgressUserData() override { [progress_.get() unpublish]; }

  NSProgress* progress() const { return progress_.get(); }
  base::FilePath target() const { return target_; }
  void setTarget(const base::FilePath& target) { target_ = target; }

 private:
  base::scoped_nsobject<NSProgress> progress_;
  base::FilePath target_;
};

void UpdateAppIcon(int download_count,
                   bool progress_known,
                   float progress) {
  DockIcon* dock_icon = [DockIcon sharedDockIcon];
  [dock_icon setDownloads:download_count];
  [dock_icon setIndeterminate:!progress_known];
  [dock_icon setProgress:progress];
  [dock_icon updateIcon];
}

void CreateNSProgress(download::DownloadItem* download) {
  NSURL* source_url = [NSURL URLWithString:
      base::SysUTF8ToNSString(download->GetURL().possibly_invalid_spec())];
  base::FilePath destination_path = download->GetFullPath();
  NSURL* destination_url = [NSURL fileURLWithPath:
      base::mac::FilePathToNSString(destination_path)];

  NSDictionary* user_info = @{
    ProgressString(kNSProgressFileLocationCanChangeKeyName) : @true,
    ProgressString(kNSProgressFileOperationKindKeyName) :
        ProgressString(kNSProgressFileOperationKindDownloadingName),
    ProgressString(kNSProgressFileURLKeyName) : destination_url
  };

  Class progress_class = NSClassFromString(@"NSProgress");
  NSProgress* progress = [progress_class performSelector:@selector(alloc)];
  progress = [progress performSelector:@selector(initWithParent:userInfo:)
                            withObject:nil
                            withObject:user_info];
  progress.kind = ProgressString(kNSProgressKindFileName);

  if (source_url) {
    [progress setUserInfoObject:source_url forKey:
        ProgressString(kNSProgressFileDownloadingSourceURLKeyName)];
  }

  progress.pausable = NO;
  progress.cancellable = YES;
  [progress setCancellationHandler:^{
      dispatch_async(dispatch_get_main_queue(), ^{
          download->Cancel(true);
      });
  }];

  progress.totalUnitCount = download->GetTotalBytes();
  progress.completedUnitCount = download->GetReceivedBytes();

  [progress publish];

  download->SetUserData(
      &kCrNSProgressUserDataKey,
      std::make_unique<CrNSProgressUserData>(progress, destination_path));
}

void UpdateNSProgress(download::DownloadItem* download,
                      CrNSProgressUserData* progress_data) {
  NSProgress* progress = progress_data->progress();
  progress.totalUnitCount = download->GetTotalBytes();
  progress.completedUnitCount = download->GetReceivedBytes();
  [progress setUserInfoObject:@(download->CurrentSpeed())
                       forKey:ProgressString(kNSProgressThroughputKeyName)];

  base::TimeDelta time_remaining;
  NSNumber* time_remaining_ns = nil;
  if (download->TimeRemaining(&time_remaining))
    time_remaining_ns = @(time_remaining.InSeconds());
  [progress setUserInfoObject:time_remaining_ns
               forKey:ProgressString(kNSProgressEstimatedTimeRemainingKeyName)];

  base::FilePath download_path = download->GetFullPath();
  if (progress_data->target() != download_path) {
    progress_data->setTarget(download_path);
    NSURL* download_url = [NSURL fileURLWithPath:
        base::mac::FilePathToNSString(download_path)];
    [progress setUserInfoObject:download_url
                         forKey:ProgressString(kNSProgressFileURLKeyName)];
  }
}

void DestroyNSProgress(download::DownloadItem* download,
                       CrNSProgressUserData* progress_data) {
  download->RemoveUserData(&kCrNSProgressUserDataKey);
}

}  // namespace

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  // Always update overall progress.

  float progress = 0;
  int download_count = 0;
  bool progress_known = GetProgress(&progress, &download_count);
  UpdateAppIcon(download_count, progress_known, progress);

  // Update NSProgress-based indicators.

  if (NSProgressSupported()) {
    CrNSProgressUserData* progress_data = static_cast<CrNSProgressUserData*>(
        download->GetUserData(&kCrNSProgressUserDataKey));

    // Only show progress if the download is IN_PROGRESS and it hasn't been
    // renamed to its final name. Setting the progress after the final rename
    // results in the file being stuck in an in-progress state on the dock. See
    // http://crbug.com/166683.
    if (download->GetState() == download::DownloadItem::IN_PROGRESS &&
        !download->GetFullPath().empty() &&
        download->GetFullPath() != download->GetTargetFilePath()) {
      if (!progress_data)
        CreateNSProgress(download);
      else
        UpdateNSProgress(download, progress_data);
    } else {
      DestroyNSProgress(download, progress_data);
    }
  }

  // Handle downloads that ended.
  if (download->GetState() != download::DownloadItem::IN_PROGRESS &&
      !download->GetTargetFilePath().empty()) {
    NSString* download_path =
        base::mac::FilePathToNSString(download->GetTargetFilePath());
    if (download->GetState() == download::DownloadItem::COMPLETE) {
      // Bounce the dock icon.
      [[NSDistributedNotificationCenter defaultCenter]
          postNotificationName:@"com.apple.DownloadFileFinished"
                        object:download_path];
    }

    // Notify the Finder.
    [[NSWorkspace sharedWorkspace] noteFileSystemChanged:download_path];
  }
}
