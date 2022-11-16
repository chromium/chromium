// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/notreached.h"
namespace platform_util {

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  // TODO(crbug.com/1231928): Implement once Fuchsia supports opening folders.
  NOTIMPLEMENTED_LOG_ONCE();
}

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  // TODO(crbug.com/1231928): Implement once Fuchsia supports opening folders.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace internal

void OpenExternal(const GURL& url) {
  // TODO(crbug.com/1231928): Implement once Fuchsia supports opening folders.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace platform_util
