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
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/shortcuts/chrome_webloc_file.h"
#include "chrome/browser/shortcuts/platform_util_mac.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace shortcuts {

void CreateShortcutOnUserDesktop(const std::string& shortcut_name,
                                 const GURL& shortcut_url,
                                 gfx::ImageFamily icon_images,
                                 const base::FilePath& profile_path,
                                 ShortcutCreatorCallback complete) {
  using Result = ShortcutCreatorResult;

  base::FilePath desktop_path;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path)) {
    std::move(complete).Run(Result::kError);
    return;
  }

  std::optional<base::SafeBaseName> base_name =
      SanitizeTitleForFileName(shortcut_name);
  if (!base_name.has_value()) {
    base_name = SanitizeTitleForFileName(shortcut_url.spec());
  }
  CHECK(base_name.has_value());

  base::FilePath target_path = base::GetUniquePath(
      desktop_path.Append(*base_name)
          .AddExtensionASCII(ChromeWeblocFile::kFileExtension));
  if (target_path.empty()) {
    std::move(complete).Run(Result::kError);
    return;
  }

  auto profile_path_name = base::SafeBaseName::Create(profile_path);
  if (!profile_path_name.has_value() ||
      !ChromeWeblocFile(shortcut_url, *profile_path_name)
           .SaveToFile(target_path)) {
    std::move(complete).Run(Result::kError);
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
  for (const gfx::Image& image : icon_images) {
    NSArray* image_reps = image.AsNSImage().representations;
    DCHECK_EQ(1u, image_reps.count);
    [icon_image addRepresentation:image_reps[0]];
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
              .Then(std::move(complete)));
}

}  // namespace shortcuts
