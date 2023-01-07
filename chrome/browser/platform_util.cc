// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "ui/aura/window.h"
#endif

using content::BrowserThread;

namespace platform_util {

namespace {

bool shell_operations_allowed = true;

void VerifyAndOpenItemOnBlockingThread(const base::FilePath& path,
                                       OpenItemType type,
                                       OpenOperationCallback callback) {
  base::File target_item(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!base::PathExists(path)) {
    if (!callback.is_null())
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), OPEN_FAILED_PATH_NOT_FOUND));
    return;
  }
  if (base::DirectoryExists(path) != (type == OPEN_FOLDER)) {
    if (!callback.is_null())
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), OPEN_FAILED_INVALID_TYPE));
    return;
  }

  if (shell_operations_allowed)
    internal::PlatformOpenVerifiedItem(path, type);
  if (!callback.is_null())
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), OPEN_SUCCEEDED));
}

}  // namespace

namespace internal {

void DisableShellOperationsForTesting() {
  shell_operations_allowed = false;
}

bool AreShellOperationsAllowed() {
  return shell_operations_allowed;
}

}  // namespace internal

void OpenItem(Profile* profile,
              const base::FilePath& full_path,
              OpenItemType item_type,
              OpenOperationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TaskPriority::USER_BLOCKING because this is usually opened as a result of a
  // user action (e.g. open-downloaded-file or show-item-in-folder).
  // TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN because this doesn't need global
  // state and can hang shutdown without this trait as it may result in an
  // interactive dialog.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&VerifyAndOpenItemOnBlockingThread, full_path, item_type,
                     std::move(callback)));
}

bool IsBrowserLockedFullscreen(const Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  aura::Window* window = browser->window()->GetNativeWindow();
  // |window| can be nullptr inside of unit tests.
  if (!window)
    return false;

  return window->GetProperty(lacros::kWindowPinTypeKey) ==
         chromeos::WindowPinType::kTrustedPinned;
#else
  return false;
#endif
}

}  // namespace platform_util
