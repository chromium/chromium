// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/platform_util_internal.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace platform_util {
namespace {

void OnOpenResult(const base::FilePath& path,
                  crosapi::mojom::OpenResult result) {
  if (result == crosapi::mojom::OpenResult::kSucceeded)
    return;
  // TODO(https://crbug.com/1144316): Show error messages. This will require
  // refactoring the existing file manager string files, or introducing new
  // lacros strings.
  LOG(ERROR) << "Unable to open " << path.AsUTF8Unsafe() << " " << result;
}

}  // namespace

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  auto* service = chromeos::LacrosChromeServiceImpl::Get();
  if (service->GetInterfaceVersion(crosapi::mojom::FileManager::Uuid_) < 1) {
    LOG(ERROR) << "Unsupported ash version.";
    return;
  }
  switch (type) {
    case OPEN_FILE:
      service->file_manager_remote()->OpenFile(
          path, base::BindOnce(&OnOpenResult, path));
      break;
    case OPEN_FOLDER:
      service->file_manager_remote()->OpenFolder(
          path, base::BindOnce(&OnOpenResult, path));
      break;
  }
}

}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  auto* service = chromeos::LacrosChromeServiceImpl::Get();
  int interface_version =
      service->GetInterfaceVersion(crosapi::mojom::FileManager::Uuid_);
  if (interface_version < 0) {
    DLOG(ERROR) << "Unsupported ash version.";
    return;
  }
  if (interface_version < 1) {
    service->file_manager_remote()->DeprecatedShowItemInFolder(full_path);
    return;
  }
  service->file_manager_remote()->ShowItemInFolder(
      full_path, base::BindOnce(&OnOpenResult, full_path));
}

void OpenExternal(Profile* profile, const GURL& url) {
  // TODO(https://crbug.com/1140585): Add crosapi for opening links with
  // external protocol handlers.
  NOTIMPLEMENTED();
}

}  // namespace platform_util
