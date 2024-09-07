// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#include <windows.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/i18n/file_util_icu.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

void CreateShortcutOnUserDesktop(ShortcutMetadata shortcut_metadata,
                                 ShortcutCreatorCallback complete) {
  CHECK(shortcut_metadata.IsValid());
  const GURL& shortcut_url = shortcut_metadata.shortcut_url;

  base::FilePath chrome_proxy_path = GetChromeProxyPath();
  // Create a .ico file from the icon images, and put it in
  // <profile dir>/shortcuts/resources/<Url Hash>/icons.
  std::string url_hash =
      base::NumberToString(base::PersistentHash(shortcut_url.spec()));
  base::FilePath target_path =
      shortcut_metadata.profile_path.Append(kWebShortcutsIconDirName)
          .AppendASCII(url_hash);
  base::FilePath icon_path =
      CreateIconFileFromBitmap(target_path, shortcut_metadata.shortcut_images);

  // Create a desktop shortcut
  base::FilePath desktop;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop) ||
      desktop.empty()) {
    std::move(complete).Run(/*created_shortcut_path=*/base::FilePath(),
                            ShortcutCreatorResult::kError);
    return;
  }
  std::wstring shortcut_name =
      base::UTF16ToWide(shortcut_metadata.shortcut_title);
  base::i18n::ReplaceIllegalCharactersInPath(&shortcut_name, ' ');
  base::FilePath shortcut_path =
      GetUniquePath(desktop.Append(base::StrCat({shortcut_name, L".lnk"})));

  base::win::ShortcutProperties target_and_args_properties;
  target_and_args_properties.set_target(chrome_proxy_path);
  target_and_args_properties.set_arguments(base::StrCat(
      {L"--", base::ASCIIToWide(switches::kProfileDirectory), L"=\"",
       shortcut_metadata.profile_path.BaseName().value(), L"\" --",
       base::ASCIIToWide(switches::kIgnoreProfileDirectoryIfNotExists), L" ",
       base::ASCIIToWide(shortcut_url.spec())}));
  target_and_args_properties.set_icon(icon_path, /*icon_index_in=*/0);

  bool res =
      CreateOrUpdateShortcutLink(shortcut_path, target_and_args_properties,
                                 base::win::ShortcutOperation::kCreateAlways);

  auto final_result_callback =
      res ? base::BindOnce(std::move(complete), shortcut_path,
                           ShortcutCreatorResult::kSuccess)
          : base::BindOnce(std::move(complete),
                           /*created_shortcut_path=*/base::FilePath(),
                           ShortcutCreatorResult::kError);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(final_result_callback));
}

scoped_refptr<base::SequencedTaskRunner> GetShortcutsTaskRunner() {
  return base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

}  // namespace shortcuts
