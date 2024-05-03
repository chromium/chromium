// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/platform_util_win.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace shortcuts {

namespace {

constexpr base::FilePath::CharType kChromeProxyExecutable[] =
    FILE_PATH_LITERAL("chrome_proxy.exe");

}  // namespace

base::FilePath GetChromeProxyPath() {
  base::FilePath chrome_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &chrome_dir));
  return chrome_dir.Append(kChromeProxyExecutable);
}

}  // namespace shortcuts
