// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a class to load the main DLL of a Chrome process.

#ifndef CHROME_APP_MAIN_DLL_LOADER_WIN_H_
#define CHROME_APP_MAIN_DLL_LOADER_WIN_H_

#include <windows.h>

#include <string>

#include "base/time/time.h"

namespace base {
class FilePath;
}  // namespace base

// Implements the common aspects of loading the main dll for both chrome and
// chromium scenarios.
class MainDllLoader {
 public:
  MainDllLoader();
  virtual ~MainDllLoader();

  // Loads and calls the entry point of chrome.dll. |instance| is the exe
  // instance retrieved from wWinMain. |exe_entry_point_ticks| is the time
  // when wWinMain was entered.
  // The return value is what the main entry point of chrome.dll returns
  // upon termination.
  int Launch(HINSTANCE instance, base::TimeTicks exe_entry_point_ticks);

  // Launches a new instance of the browser if the current instance in
  // persistent mode an upgrade is detected.
  void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

 protected:
  // Called after chrome.dll has been loaded but before the entry point is
  // invoked. Derived classes can implement custom actions here. `process_type`
  // is the argument to the `--type` command line argument (e.g., `renderer` or
  // `watcher`). `dll_path` refers to the path of the Chrome dll being loaded.
  virtual void OnBeforeLaunch(const std::string& process_type,
                              const base::FilePath& dll_path) {}

 private:
  HMODULE dll_;
  std::string process_type_;
};

// Factory for the MainDllLoader. Caller owns the pointer and should call
// delete to free it.
MainDllLoader* MakeMainDllLoader();

#endif  // CHROME_APP_MAIN_DLL_LOADER_WIN_H_
