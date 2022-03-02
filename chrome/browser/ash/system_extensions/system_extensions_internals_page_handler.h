// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INTERNALS_PAGE_HANDLER_H_

#include "ash/webui/system_extensions_internals_ui/mojom/system_extensions_internals_ui.mojom.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_status.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"

class SystemExtensionsInternalsPageHandler
    : public ash::mojom::system_extensions_internals::PageHandler {
 public:
  explicit SystemExtensionsInternalsPageHandler(Profile* profile);
  ~SystemExtensionsInternalsPageHandler() override;

  // mojom::system_extensions_internals::PageHandler
  void InstallSystemExtensionFromDownloadsDir(
      const base::SafeBaseName& system_extension_dir_name,
      InstallSystemExtensionFromDownloadsDirCallback callback) override;

 private:
  void OnInstallFinished(
      InstallSystemExtensionFromDownloadsDirCallback callback,
      InstallStatusOrSystemExtensionId result);

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<SystemExtensionsInternalsPageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INTERNALS_PAGE_HANDLER_H_
