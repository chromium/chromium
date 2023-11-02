// Copyright 2021 The Chromium Authors
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
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class SystemExtensionsInternalsPageHandler
    : public ash::mojom::system_extensions_internals::PageHandler {
 public:
  SystemExtensionsInternalsPageHandler(
      Profile* profile,
      mojo::PendingReceiver<mojom::system_extensions_internals::PageHandler>
          receiver);
  ~SystemExtensionsInternalsPageHandler() override;

  // mojom::system_extensions_internals::PageHandler
  void InstallSystemExtensionFromDownloadsDir(
      const base::SafeBaseName& system_extension_dir_name,
      InstallSystemExtensionFromDownloadsDirCallback callback) override;
  void IsSystemExtensionInstalled(
      IsSystemExtensionInstalledCallback callback) override;
  void UninstallSystemExtension(
      UninstallSystemExtensionCallback callback) override;

 private:
  void OnInstallFinished(
      InstallSystemExtensionFromDownloadsDirCallback callback,
      InstallStatusOrSystemExtensionId result);

  raw_ptr<Profile> profile_;
  mojo::Receiver<mojom::system_extensions_internals::PageHandler> receiver_;

  base::WeakPtrFactory<SystemExtensionsInternalsPageHandler> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INTERNALS_PAGE_HANDLER_H_
