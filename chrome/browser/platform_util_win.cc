// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <wrl/client.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace platform_util {

namespace {

void ShowItemInFolderOnWorkerThread(const base::FilePath& full_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath dir = full_path.DirName().AsEndingWithSeparator();
  // ParseDisplayName will fail if the directory is "C:", it must be "C:\\".
  if (dir.empty())
    return;

  Microsoft::WRL::ComPtr<IShellFolder> desktop;
  HRESULT hr = SHGetDesktopFolder(&desktop);
  if (FAILED(hr))
    return;

  base::win::ScopedCoMem<ITEMIDLIST> dir_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
                                 const_cast<wchar_t *>(dir.value().c_str()),
                                 NULL, &dir_item, NULL);
  if (FAILED(hr))
    return;

  base::win::ScopedCoMem<ITEMIDLIST> file_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
      const_cast<wchar_t *>(full_path.value().c_str()),
      NULL, &file_item, NULL);
  if (FAILED(hr))
    return;

  const ITEMIDLIST* highlight[] = {file_item};

  // Skip opening the folder during browser tests, to avoid leaving an open
  // file explorer window behind.
  if (!platform_util::internal::AreShellOperationsAllowed())
    return;

  hr = SHOpenFolderAndSelectItems(dir_item, std::size(highlight), highlight, 0);
  if (FAILED(hr)) {
    // On some systems, the above call mysteriously fails with "file not
    // found" even though the file is there.  In these cases, ShellExecute()
    // seems to work as a fallback (although it won't select the file).
    if (hr == ERROR_FILE_NOT_FOUND) {
      ShellExecute(NULL, L"open", dir.value().c_str(), NULL, NULL, SW_SHOW);
    } else {
      LOG(WARNING) << " " << __func__ << "(): Can't open full_path = \""
                   << full_path.value() << "\""
                   << " hr = " << logging::SystemErrorCodeToString(hr);
    }
  }
}

void OpenExternalOnWorkerThread(const GURL& url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Quote the input scheme to be sure that the command does not have
  // parameters unexpected by the external program. This url should already
  // have been escaped.
  std::string escaped_url = url.spec();
  escaped_url.insert(0, "\"");
  escaped_url += "\"";

  // According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
  // "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
  // ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
  // support URLS of 2083 chars in length, 2K is safe."
  //
  // It may be possible to increase this. https://crbug.com/727909
  const size_t kMaxUrlLength = 2048;
  if (escaped_url.length() > kMaxUrlLength)
    return;

  // Specify %windir%\system32 as the CWD so that any new proc spawned does not
  // inherit this proc's CWD. Without this, uninstalls may be broken by a
  // long-lived child proc that holds a handle to the browser's version
  // directory (the browser's CWD). A process's CWD is in the standard list of
  // directories to search when loading a DLL, and precedes the system directory
  // when safe DLL search mode is disabled (not the default). Setting the CWD to
  // the system directory is a nice way to mitigate a potential DLL search order
  // hijack for processes that don't implement their own mitigation.
  base::FilePath system_dir;
  base::PathService::Get(base::DIR_SYSTEM, &system_dir);
  if (reinterpret_cast<ULONG_PTR>(ShellExecuteA(
          NULL, "open", escaped_url.c_str(), NULL,
          system_dir.AsUTF8Unsafe().c_str(), SW_SHOWNORMAL)) <= 32) {
    // On failure, it may be good to display a message to the user.
    // https://crbug.com/727913
    return;
  }
}

}  // namespace

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&ShowItemInFolderOnWorkerThread, full_path));
}

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  // May result in an interactive dialog.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  switch (type) {
    case OPEN_FILE:
      ui::win::OpenFileViaShell(path);
      break;

    case OPEN_FOLDER:
      ui::win::OpenFolderViaShell(path);
      break;
  }
}

}  // namespace internal

void OpenExternal(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE, base::BindOnce(&OpenExternalOnWorkerThread, url));
}

}  // namespace platform_util
