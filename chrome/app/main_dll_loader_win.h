// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a class to load the main DLL of a Chrome process.

#ifndef CHROME_APP_MAIN_DLL_LOADER_WIN_H_
#define CHROME_APP_MAIN_DLL_LOADER_WIN_H_

#include <windows.h>  // NOLINT

#include <string>

#include "base/time/time.h"

namespace base {
class CommandLine;
class FilePath;
enum class PrefetchResultCode;
}  // namespace base

// Implements the common aspects of loading the main dll for both chrome and
// chromium scenarios, which are in charge of implementing one abstract
// method: OnBeforeLaunch()
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
  // invoked. Derived classes can implement custom actions here. |cmd_line| is
  // the process command line. |process_type| is the argument to the --type
  // command line argument (e.g., "renderer" or "watcher"). |dll_path| refers
  // to the path of the Chrome dll being loaded.
  virtual void OnBeforeLaunch(const base::CommandLine& cmd_line,
                              const std::string& process_type,
                              const base::FilePath& dll_path) = 0;

 private:
  struct LoadResult {
    HMODULE handle;
    base::PrefetchResultCode prefetch_result_code;
  };

  // Prefetches and loads the appropriate DLL for the process type
  // |process_type_|. Populates |module| with the path of the loaded DLL, and
  // returns a struct containing a handle to the loaded DLL (or nullptr on
  // failure), and a prefetch result code.
  static LoadResult Load(base::FilePath* module);

  // Prefetches and loads |module| after setting the CWD to |module|'s
  // directory. Returns a struct containing a handle to the loaded module on
  // success (or nullptr on error) and a prefetch result code.
  static LoadResult LoadModuleWithDirectory(const base::FilePath& module);

  HMODULE dll_;
  std::string process_type_;
};

// Factory for the MainDllLoader. Caller owns the pointer and should call
// delete to free it.
MainDllLoader* MakeMainDllLoader();

#endif  // CHROME_APP_MAIN_DLL_LOADER_WIN_H_
