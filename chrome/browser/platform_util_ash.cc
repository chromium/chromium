// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_pin_util.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/platform_util_internal.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace platform_util {

namespace {

void ShowWarningOnOpenOperationResult(Profile* profile,
                                      const base::FilePath& path,
                                      OpenOperationResult result) {
  int message_id = IDS_FILE_BROWSER_ERROR_VIEWING_FILE;
  switch (result) {
    case OPEN_SUCCEEDED:
      return;

    case OPEN_FAILED_PATH_NOT_FOUND:
      message_id = IDS_FILE_BROWSER_ERROR_UNRESOLVABLE_FILE;
      break;

    case OPEN_FAILED_INVALID_TYPE:
      return;

    case OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE:
      if (path.MatchesExtension(FILE_PATH_LITERAL(".dmg")))
        message_id = IDS_FILE_BROWSER_ERROR_VIEWING_FILE_FOR_DMG;
      else if (path.MatchesExtension(FILE_PATH_LITERAL(".exe")) ||
               path.MatchesExtension(FILE_PATH_LITERAL(".msi")))
        message_id = IDS_FILE_BROWSER_ERROR_VIEWING_FILE_FOR_EXECUTABLE;
      else
        message_id = IDS_FILE_BROWSER_ERROR_VIEWING_FILE;
      break;

    case OPEN_FAILED_FILE_ERROR:
      message_id = IDS_FILE_BROWSER_ERROR_VIEWING_FILE;
      break;
  }

  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  chrome::ShowWarningMessageBox(
      browser ? browser->window()->GetNativeWindow() : nullptr,
      path.BaseName().AsUTF16Unsafe(),
      l10n_util::GetStringUTF16(message_id));
}

}  // namespace

namespace internal {

void DisableShellOperationsForTesting() {
  file_manager::util::DisableShellOperationsForTesting();
}

}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_manager::util::ShowItemInFolder(
      profile, full_path,
      base::BindOnce(&ShowWarningOnOpenOperationResult, profile, full_path));
}

void OpenItem(Profile* profile,
              const base::FilePath& full_path,
              OpenItemType item_type,
              OpenOperationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_manager::util::OpenItem(
      profile, full_path, item_type,
      callback.is_null() ? base::BindOnce(&ShowWarningOnOpenOperationResult,
                                          profile, full_path)
                         : std::move(callback));
}

void OpenExternal(Profile* profile, const GURL& url) {
  // This code is called either when:
  // 1. ChromeAppDelegate::NewWindowContentsDelegate::OpenURLFromTab determines
  // that the currently running chrome is not the system default browser. This
  // should not happen for Chrome OS (crrev.com/c/2454769).
  // 2. |url| uses a external protocol and either
  // ExternalProtocolDialog::OnDialogAccepted invokes this, or the dialog has
  // previously been accepted with "Always allow ..." and this is called from
  // ChromeContentBrowserClient::HandleExternalProtocol.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::optional<guest_os::GuestOsUrlHandler> handler =
      guest_os::GuestOsUrlHandler::GetForUrl(profile, url);
  if (handler) {
    handler->Handle(profile, url);
  }
}

bool IsBrowserLockedFullscreen(const Browser* browser) {
  aura::Window* window = browser->window()->GetNativeWindow();
  // |window| can be nullptr inside of unit tests.
  if (!window)
    return false;
  return GetWindowPinType(window) == chromeos::WindowPinType::kTrustedPinned;
}

}  // namespace platform_util
