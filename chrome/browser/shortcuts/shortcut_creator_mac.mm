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
#include "base/functional/concurrent_closures.h"
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

namespace {

// Return a version of `title` that is safe to use as a filename on macOS.
// TODO(mek): Dedeuplicate this with the app shim code in
// chrome/browser/web_applications/os_integration/web_app_shortcut_mac.mm.
base::FilePath::StringType SanitizeTitle(std::string title) {
  // Strip all preceding '.'s from the path.
  size_t first_non_dot = 0;
  while (first_non_dot < title.size() && title[first_non_dot] == '.') {
    first_non_dot += 1;
  }
  title = title.substr(first_non_dot);
  if (title.empty()) {
    return {};
  }

  // Finder will display ':' as '/', so replace all '/' instances with ':'.
  std::replace(title.begin(), title.end(), '/', ':');

  return title;
}

}  // namespace

void CreateShortcutOnUserDesktop(const std::string& shortcut_name,
                                 const GURL& shortcut_url,
                                 gfx::ImageFamily icon_images,
                                 const base::FilePath& profile_path,
                                 ShortcutCreatorCallback complete) {
  base::FilePath desktop_path;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path)) {
    std::move(complete).Run(ShortcutCreatorResult::kError);
    return;
  }

  base::FilePath::StringType base_name = SanitizeTitle(shortcut_name);
  if (base_name.empty()) {
    base_name = SanitizeTitle(shortcut_url.spec());
  }

  base::FilePath target_path =
      base::GetUniquePath(desktop_path.Append(base_name).AddExtensionASCII(
          ChromeWeblocFile::kFileExtension));
  if (target_path.empty()) {
    std::move(complete).Run(ShortcutCreatorResult::kError);
    return;
  }

  auto profile_path_name = base::SafeBaseName::Create(profile_path);
  if (!profile_path_name.has_value() ||
      !ChromeWeblocFile(shortcut_url, *profile_path_name)
           .SaveToFile(target_path)) {
    std::move(complete).Run(ShortcutCreatorResult::kError);
    return;
  }

  // None of the remaining operations are considered fatal; i.e. shortcut
  // creation is still considered a success if any of these fail as the
  // created shortcut should work just fine even without any of this in the
  // vast majority of cases.
  base::ConcurrentClosures concurrent;

  SetDefaultApplicationToOpenFile(
      base::apple::FilePathToNSURL(target_path), base::apple::MainBundleURL(),
      base::BindOnce([](NSError* error) {
        if (error) {
          LOG(ERROR) << "Failed to set default application for shortcut.";
        }
      }).Then(concurrent.CreateClosure()));

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
                 }).Then(concurrent.CreateClosure()));

  std::move(concurrent)
      .Done(base::BindOnce(&base::mac::RemoveQuarantineAttribute, target_path)
                .Then(base::BindOnce([](bool success) {
                        if (!success) {
                          LOG(ERROR) << "Failed to remove quarantine attribute "
                                        "from shortcut.";
                        }
                      })
                          .Then(base::BindOnce(
                              std::move(complete),
                              ShortcutCreatorResult::kSuccess))));
}

}  // namespace shortcuts
