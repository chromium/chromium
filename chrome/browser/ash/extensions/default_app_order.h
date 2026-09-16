// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_APP_ORDER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_APP_ORDER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
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
  // Constructs an ExternalLoader and starts file loading.
  // `locale` is the application locale used to resolve localized folder names.
  // `async` is true to load the file asynchronously on the blocking thread
  // pool.
  // TODO(crbug.com/559471293): Introduce a completion callback or observer to
  // avoid returning an empty list before loading completes.
  explicit ExternalLoader(std::string locale, bool async);

  ExternalLoader(const ExternalLoader&) = delete;
  ExternalLoader& operator=(const ExternalLoader&) = delete;

  ~ExternalLoader();

  const std::vector<std::string>& GetAppIds();
  const std::string& GetOemAppsFolderName();

 private:
  struct ParsedAppOrder {
    std::vector<std::string> app_ids;
    std::string oem_apps_folder_name;
  };

  // Reads the app order file at `path` and parses it. Returns the built-in
  // default order if the file is missing or unreadable. `locale` is used to
  // resolve the localized OEM apps folder name. Runs on a blocking thread pool
  // sequence when loading asynchronously.
  static ParsedAppOrder ReadAndParseAppOrder(base::FilePath path,
                                             std::string locale);

  void OnLoadFinished(ParsedAppOrder parsed_order);

  ParsedAppOrder app_order_;
  bool is_loaded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExternalLoader> weak_ptr_factory_{this};
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
