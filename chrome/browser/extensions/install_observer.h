// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_

#include <optional>
#include <string>

#include "extensions/common/extension_id.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class CrxInstaller;
class Extension;

// An InstallObserver observes extension installation events coming from an InstallTracker.
// Since extension installs are scoped to a single Profile (represented here as a
// BrowserContext), InstallTrackers are as well. Instances of InstallObserver are passed the
// appropriate BrowserContext so that a single InstallObserver can observe multiple
// InstallTrackers.
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
  virtual void OnBeginExtensionInstall(content::BrowserContext* context,
                                       const ExtensionInstallParams& params) {}

  // Called when the Extension begins the download process. This typically
  // happens right after OnBeginExtensionInstall(), unless the extension has
  // already been downloaded.
  virtual void OnBeginExtensionDownload(content::BrowserContext* context,
                                        const std::string& extension_id) {}

  // Called whenever the extension download is updated.
  // Note: Some extensions have multiple modules, so the percent included here
  // is a simple calculation of:
  // (finished_files * 100 + current_file_progress) / (total files * 100).
  virtual void OnDownloadProgress(content::BrowserContext* context,
                                  const std::string& extension_id,
                                  int percent_downloaded) {}

  // Called when the necessary downloads have completed, and the crx
  // installation is due to start.
  virtual void OnBeginCrxInstall(content::BrowserContext* context,
                                 const CrxInstaller& installer,
                                 const std::string& extension_id) {}

  // Called when installation of a crx has completed (either successfully or
  // not).
  virtual void OnFinishCrxInstall(content::BrowserContext* context,
                                  const CrxInstaller& installer,
                                  const std::string& extension_id,
                                  bool success) {}

  // Called when the app list is reordered. If |extension_id| is set, it
  // indicates the extension ID that was re-ordered.
  virtual void OnAppsReordered(content::BrowserContext* context,
                               const std::optional<ExtensionId>& extension_id) {
  }

  // Notifies observers that the observed object is going away.
  virtual void OnShutdown() {}

 protected:
  virtual ~InstallObserver() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_OBSERVER_H_
