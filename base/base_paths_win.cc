// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <windows.h>

#include <KnownFolders.h>
#include <shlobj.h>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"

using base::FilePath;

namespace base {

bool PathProviderWin(int key, FilePath* result) {
  // We need to go compute the value. It would be nice to support paths with
  // names longer than MAX_PATH, but the system functions don't seem to be
  // designed for it either, with the exception of GetTempPath (but other
  // things will surely break if the temp path is too long, so we don't bother
  // handling it.
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;

  FilePath cur;
  switch (key) {
    case base::FILE_EXE:
      if (GetModuleFileName(NULL, system_buffer, MAX_PATH) == 0)
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::FILE_MODULE: {
      // the resource containing module is assumed to be the one that
      // this code lives in, whether that's a dll or exe
      if (GetModuleFileName(CURRENT_MODULE(), system_buffer, MAX_PATH) == 0)
        return false;
      cur = FilePath(system_buffer);
      break;
    }
    case base::DIR_WINDOWS:
      GetWindowsDirectory(system_buffer, MAX_PATH);
      cur = FilePath(system_buffer);
      break;
    case base::DIR_SYSTEM:
      GetSystemDirectory(system_buffer, MAX_PATH);
      cur = FilePath(system_buffer);
      break;
    case base::DIR_PROGRAM_FILESX86:
      if (win::OSInfo::GetArchitecture() != win::OSInfo::X86_ARCHITECTURE) {
        if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILESX86, NULL,
                                   SHGFP_TYPE_CURRENT, system_buffer)))
          return false;
        cur = FilePath(system_buffer);
        break;
      }
      // Fall through to base::DIR_PROGRAM_FILES if we're on an X86 machine.
      [[fallthrough]];
    case base::DIR_PROGRAM_FILES:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_PROGRAM_FILES6432:
#if !defined(_WIN64)
      if (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64() ||
          base::win::OSInfo::GetInstance()->IsWowX86OnARM64()) {
        std::unique_ptr<base::Environment> env(base::Environment::Create());
        std::string programfiles_w6432;
        // 32-bit process running in WOW64 sets ProgramW6432 environment
        // variable. See
        // https://msdn.microsoft.com/library/windows/desktop/aa384274.aspx.
        if (!env->GetVar("ProgramW6432", &programfiles_w6432))
          return false;
        // GetVar returns UTF8 - convert back to Wide.
        cur = FilePath(UTF8ToWide(programfiles_w6432));
        break;
      }
#endif
      if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_IE_INTERNET_CACHE:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_INTERNET_CACHE, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_COMMON_START_MENU:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_COMMON_PROGRAMS, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_START_MENU:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT,
                                 system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_COMMON_STARTUP:
      if (FAILED(SHGetFolderPath(nullptr, CSIDL_COMMON_STARTUP, nullptr,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_USER_STARTUP:
      if (FAILED(SHGetFolderPath(nullptr, CSIDL_STARTUP, nullptr,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_ROAMING_APP_DATA:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                                 system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_COMMON_APP_DATA:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_LOCAL_APP_DATA:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_SRC_TEST_DATA_ROOT: {
      FilePath executableDir;
      // On Windows, unit tests execute two levels deep from the source root.
      // For example:  chrome/{Debug|Release}/ui_tests.exe
      PathService::Get(base::DIR_EXE, &executableDir);
      cur = executableDir.DirName().DirName();
      break;
    }
    case base::DIR_APP_SHORTCUTS: {
      base::win::ScopedCoMem<wchar_t> path_buf;
      if (FAILED(SHGetKnownFolderPath(FOLDERID_ApplicationShortcuts, 0, NULL,
                                      &path_buf)))
        return false;

      cur = FilePath(path_buf.get());
      break;
    }
    case base::DIR_USER_DESKTOP:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer))) {
        return false;
      }
      cur = FilePath(system_buffer);
      break;
    case base::DIR_COMMON_DESKTOP:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer))) {
        return false;
      }
      cur = FilePath(system_buffer);
      break;
    case base::DIR_USER_QUICK_LAUNCH:
      if (!PathService::Get(base::DIR_ROAMING_APP_DATA, &cur))
        return false;
      // According to various sources, appending
      // "Microsoft\Internet Explorer\Quick Launch" to %appdata% is the only
      // reliable way to get the quick launch folder across all versions of
      // Windows.
      // http://stackoverflow.com/questions/76080/how-do-you-reliably-get-the-quick-
      // http://www.microsoft.com/technet/scriptcenter/resources/qanda/sept05/hey0901.mspx
      cur = cur.Append(FILE_PATH_LITERAL("Microsoft"))
                .Append(FILE_PATH_LITERAL("Internet Explorer"))
                .Append(FILE_PATH_LITERAL("Quick Launch"));
      break;
    case base::DIR_TASKBAR_PINS: {
      if (!PathService::Get(base::DIR_USER_QUICK_LAUNCH, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("User Pinned"))
                .Append(FILE_PATH_LITERAL("TaskBar"));
      break;
    }
    case base::DIR_IMPLICIT_APP_SHORTCUTS:
      if (!PathService::Get(base::DIR_USER_QUICK_LAUNCH, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("User Pinned"))
                .Append(FILE_PATH_LITERAL("ImplicitAppShortcuts"));
      break;
    case base::DIR_WINDOWS_FONTS:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT,
                                 system_buffer))) {
        return false;
      }
      cur = FilePath(system_buffer);
      break;
    case base::DIR_SYSTEM_TEMP:
      // Try C:\Windows\SystemTemp, which was introduced sometime before Windows
      // 10 build 19042. Do not use GetTempPath2, as it only appeared later and
      // will only return the path for processes running as SYSTEM.
      if (PathService::Get(DIR_WINDOWS, &cur)) {
        cur = cur.Append(FILE_PATH_LITERAL("SystemTemp"));
        if (PathIsWritable(cur)) {
          break;
        }
      }
      // Failing that, use C:\Program Files or C:\Program Files (x86) for older
      // versions of Windows 10.
      if (!PathService::Get(DIR_PROGRAM_FILES, &cur) || !PathIsWritable(cur)) {
        return false;
      }
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

}  // namespace base
