// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_APP_ORDER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_APP_ORDER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

namespace chromeos {
namespace default_app_order {

// ExternalLoader checks FILE_DEFAULT_APP_ORDER and loads it if the file
// exists. Otherwise, it uses the default built-in order. The file loading runs
// asynchronously on start up except for the browser restart path, in which
// case, start up will wait for the file check to finish because user profile
// might need to access the ordinals data.
class ExternalLoader {
 public:
  // Constructs an ExternalLoader and starts file loading. |async| is true to
  // load the file asynchronously on the blocking pool.
  explicit ExternalLoader(bool async);

  ExternalLoader(const ExternalLoader&) = delete;
  ExternalLoader& operator=(const ExternalLoader&) = delete;

  ~ExternalLoader();

  const std::vector<std::string>& GetAppIds();
  const std::string& GetOemAppsFolderName();

 private:
  void Load();

  // A vector of app id strings that defines the default order of apps.
  std::vector<std::string> app_ids_;

  std::string oem_apps_folder_name_;

  base::WaitableEvent loaded_;
};

// Gets the ordered list of app ids.
void Get(std::vector<std::string>* app_ids);

// Gets the default ordered list of LauncherItems (PackageIds or folders) to be
// used with AppPreloadService when apps::kAppPreloadServiceEnableLauncherOrder
// is enabled.
base::span<const apps::LauncherItem> GetAppPreloadServiceDefaults();

// Get the name of OEM apps folder in app launcher.
std::string GetOemAppsFolderName();

// Number of apps in hard-coded apps order.
size_t DefaultAppCount();

}  // namespace default_app_order
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_APP_ORDER_H_
