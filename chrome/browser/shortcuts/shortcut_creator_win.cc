// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#include <windows.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shortcuts/platform_util_win.h"
#include "chrome/common/chrome_switches.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace shortcuts {
namespace {

base::FilePath CreateIconFileFromBitmap(const base::FilePath& icon_path,
                                        const gfx::ImageFamily& icon_images) {
  if (!base::CreateDirectory(icon_path)) {
    return base::FilePath();
  }
  EmitIconStorageCountMetric(icon_path);

  const base::FilePath icon_file = icon_path.Append(L"shortcut.ico");

  // Write the .ico file containing this new bitmap.
  if (!IconUtil::CreateIconFileFromImageFamily(icon_images, icon_file)) {
    // This can happen if the profile directory is deleted between the
    // beginning of this function and here.
    return base::FilePath();
  }
  return icon_file;
}

}  // namespace

void CreateShortcutOnUserDesktop(const std::string& shortcut_name,
                                 const GURL& shortcut_url,
                                 gfx::ImageFamily icon_images,
                                 const base::FilePath& profile_path,
                                 ShortcutCreatorCallback complete) {
  base::FilePath chrome_proxy_path = GetChromeProxyPath();
  // Create a .ico file from the icon images, and put it in
  // <profile dir>/shortcuts/resources/<Url Hash>/icons.
  std::string url_hash =
      base::NumberToString(base::PersistentHash(shortcut_url.spec()));
  base::FilePath target_path =
      profile_path.Append(kWebShortcutsIconDirName).AppendASCII(url_hash);
  base::FilePath icon_path = CreateIconFileFromBitmap(target_path, icon_images);

  // Create a desktop shortcut
  base::FilePath desktop;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop) ||
      desktop.empty()) {
    std::move(complete).Run(ShortcutCreatorResult::kError);
    return;
  }
  base::FilePath shortcut_path =
      desktop.Append(base::StrCat({base::ASCIIToWide(shortcut_name), L".lnk"}));

  base::win::ShortcutProperties target_and_args_properties;
  target_and_args_properties.set_target(chrome_proxy_path);
  target_and_args_properties.set_arguments(base::StrCat(
      {L"--", base::ASCIIToWide(switches::kProfileDirectory), L"=\"",
       profile_path.BaseName().value(), L"\" --",
       base::ASCIIToWide(switches::kIgnoreProfileDirectoryIfNotExists), L" ",
       base::ASCIIToWide(shortcut_url.spec())}));
  target_and_args_properties.set_icon(icon_path, /*icon_index_in=*/0);

  bool res =
      CreateOrUpdateShortcutLink(shortcut_path, target_and_args_properties,
                                 base::win::ShortcutOperation::kCreateAlways);
  std::move(complete).Run(res ? ShortcutCreatorResult::kSuccess
                              : ShortcutCreatorResult::kError);
}

}  // namespace shortcuts
