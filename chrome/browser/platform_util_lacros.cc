// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/notreached.h"
#include "chrome/browser/platform_util_internal.h"

namespace platform_util {
namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  NOTIMPLEMENTED();
}

}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  // TODO(https://crbug.com/1139128): Add crosapi to show item in file manager.
  NOTIMPLEMENTED();
}

void OpenExternal(Profile* profile, const GURL& url) {
  // TODO(https://crbug.com/1140585): Add crosapi for opening links with
  // external protocol handlers.
  NOTIMPLEMENTED();
}

}  // namespace platform_util
