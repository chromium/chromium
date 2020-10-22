// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/platform_util_internal.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace platform_util {
namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  NOTIMPLEMENTED();
}

}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  auto* service = chromeos::LacrosChromeServiceImpl::Get();
  if (!service->IsFileManagerAvailable()) {
    DLOG(ERROR) << "Unsupported ash version.";
    return;
  }
  service->file_manager_remote()->ShowItemInFolder(full_path);
}

void OpenExternal(Profile* profile, const GURL& url) {
  // TODO(https://crbug.com/1140585): Add crosapi for opening links with
  // external protocol handlers.
  NOTIMPLEMENTED();
}

}  // namespace platform_util
