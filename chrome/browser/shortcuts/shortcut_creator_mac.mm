// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#import <AppKit/AppKit.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/functional/function_ref.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/shortcuts/chrome_webloc_file.h"
#include "chrome/browser/shortcuts/platform_util_mac.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace shortcuts {

void CreateShortcutOnUserDesktop(ShortcutMetadata shortcut_metadata,
                                 ShortcutCreatorCallback complete) {
  CHECK(shortcut_metadata.IsValid());
  using Result = ShortcutCreatorResult;
  const GURL& shortcut_url = shortcut_metadata.shortcut_url;

  base::FilePath desktop_path;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path)) {
    std::move(complete).Run(/*created_shortcut_path=*/base::FilePath(),
                            Result::kError);
    return;
  }

  std::optional<base::SafeBaseName> base_name = SanitizeTitleForFileName(
      base::UTF16ToUTF8(shortcut_metadata.shortcut_title));
  if (!base_name.has_value()) {
    base_name = SanitizeTitleForFileName(shortcut_url.spec());
  }
  CHECK(base_name.has_value());

  base::FilePath target_path = base::GetUniquePath(
      desktop_path.Append(*base_name)
          .AddExtensionASCII(ChromeWeblocFile::kFileExtension));
  if (target_path.empty()) {
    std::move(complete).Run(/*created_shortcut_path=*/base::FilePath(),
                            Result::kError);
    return;
  }

  auto profile_path_name =
      base::SafeBaseName::Create(shortcut_metadata.profile_path);
  if (!profile_path_name.has_value() ||
      !ChromeWeblocFile(shortcut_url, *profile_path_name)
           .SaveToFile(target_path)) {
    std::move(complete).Run(/*created_shortcut_path=*/base::FilePath(),
                            Result::kError);
    return;
  }

  // None of the remaining operations are considered fatal; i.e. shortcut
  // creation is still considered a success if any of these fail as the
  // created shortcut should work just fine even without any of this in the
  // vast majority of cases.
  base::ConcurrentCallbacks<bool> concurrent;

  SetDefaultApplicationToOpenFile(
      base::apple::FilePathToNSURL(target_path), base::apple::MainBundleURL(),
      base::BindOnce([](NSError* error) {
        if (error) {
          LOG(ERROR) << "Failed to set default application for shortcut.";
        }
        return !error;
      }).Then(concurrent.CreateCallback()));

  NSImage* icon_image = [[NSImage alloc] init];
  for (const gfx::Image& image : shortcut_metadata.shortcut_images) {
    NSArray<NSImageRep*>* image_reps = image.AsNSImage().representations;
    DCHECK_GE(image_reps.count, 1u);
    for (NSImageRep* rep in image_reps) {
      [icon_image addRepresentation:rep];
    }
  }
  SetIconForFile(icon_image, target_path,
                 base::BindOnce([](bool success) {
                   if (!success) {
                     LOG(ERROR) << "Failed to set icon for shortcut.";
                   }
                   return success;
                 }).Then(concurrent.CreateCallback()));

  std::move(concurrent)
      .Done(
          base::BindOnce(
              [](const base::FilePath& path, std::vector<bool> step_successes) {
                bool success = base::mac::RemoveQuarantineAttribute(path);
                step_successes.push_back(success);
                if (!success) {
                  LOG(ERROR) << "Failed to remove quarantine attribute "
                                "from shortcut.";
                }
                return base::Contains(step_successes, false)
                           ? Result::kSuccessWithErrors
                           : Result::kSuccess;
              },
              target_path)
              .Then(base::BindOnce(std::move(complete), target_path)));
}

scoped_refptr<base::SequencedTaskRunner> GetShortcutsTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

}  // namespace shortcuts
