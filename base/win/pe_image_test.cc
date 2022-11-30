// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <cfgmgr32.h>
#include <shlobj.h>

#pragma comment(linker, "/export:FwdExport=KERNEL32.CreateFileA")

extern "C" {

__declspec(dllexport) void ExportFunc1() {
  // Call into user32.dll.
  HWND dummy = GetDesktopWindow();
  SetWindowTextA(dummy, "dummy");
}

__declspec(dllexport) void ExportFunc2() {
  // Call into cfgmgr32.dll.
  CM_MapCrToWin32Err(CR_SUCCESS, ERROR_SUCCESS);

  // Call into shell32.dll.
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Public, 0, nullptr, &path)))
    CoTaskMemFree(path);

  // Call into kernel32.dll.
  HANDLE h = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  CloseHandle(h);
}

}  // extern "C"
