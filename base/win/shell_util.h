// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SHELL_UTIL_H_
#define BASE_WIN_SHELL_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/win/windows_types.h"

namespace base::win {

struct BASE_EXPORT ShellExecuteOptions {
  // Specifies the verb (e.g. "open") to use when executing the object
  // specified. If left empty then a default verb will be used.
  std::wstring verb;

  // Current directory to use for ShellExecute. If empty then the current
  // directory of the current process is used.
  std::wstring current_directory;

  // Specifies that the object being launched should start hidden.
  bool start_hidden = false;
};

// Runs `path` out-of-process via the Explorer COM object. This ensures that
// the process is launched as the user at the same elevation level as explorer.
//
// `path` specifies the file or object on which to execute. If `path` specifies
// an executable file, `parameters` specifies the parameters to be passed to
// the executable.
//
// N.B. this function requires COM to be initialized.
BASE_EXPORT HRESULT
RunShellExecuteViaExplorer(const std::wstring& path,
                           const std::wstring& parameters,
                           const ShellExecuteOptions& options = {});

}  // namespace base::win

#endif  // BASE_WIN_SHELL_UTIL_H_
