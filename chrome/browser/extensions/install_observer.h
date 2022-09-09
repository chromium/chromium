// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_

#include <string>

#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

class Extension;

class InstallObserver {
 public:
  struct ExtensionInstallParams {
    ExtensionInstallParams(
        std::string extension_id,
        std::string extension_name,
        gfx::ImageSkia installing_icon,
        bool is_app,
        bool is_platform_app);

    std::string extension_id;
    std::string extension_name;
    gfx::ImageSkia installing_icon;
    bool is_app;
    bool is_platform_app;
  };

  // Called at the beginning of the complete installation process, i.e., this
  // is called before the extension download begins.
  virtual void OnBeginExtensionInstall(const ExtensionInstallParams& params) {}

  // Called when the Extension begins the download process. This typically
  // happens right after OnBeginExtensionInstall(), unless the extension has
  // already been downloaded.
  virtual void OnBeginExtensionDownload(const std::string& extension_id) {}

  // Called whenever the extension download is updated.
  // Note: Some extensions have multiple modules, so the percent included here
  // is a simple calculation of:
  // (finished_files * 100 + current_file_progress) / (total files * 100).
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) {}

  // Called when the necessary downloads have completed, and the crx
  // installation is due to start.
  virtual void OnBeginCrxInstall(const std::string& extension_id) {}

  // Called when installation of a crx has completed (either successfully or
  // not).
  virtual void OnFinishCrxInstall(const std::string& extension_id,
                                  bool success) {}

  // Called if the extension fails to install.
  virtual void OnInstallFailure(const std::string& extension_id) {}

  // Called when the app list is reordered. If |extension_id| is set, it
  // indicates the extension ID that was re-ordered.
  virtual void OnAppsReordered(
      const absl::optional<ExtensionId>& extension_id) {}

  // Notifies observers that the observed object is going away.
  virtual void OnShutdown() {}

 protected:
  virtual ~InstallObserver() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_
