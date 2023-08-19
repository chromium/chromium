// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_EXTENSIONS_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_EXTENSIONS_EXTERNAL_LOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/external_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace base {
class Value;
}

namespace chromeos {
class ExternalCache;
}

namespace ash {

// External loader for extensions to be loaded into demo mode sessions. The CRX
// files are loaded from preinstalled demo mode resources image mounted by
// image loader service (from the device's stateful partition).
// The loader reads external extensions prefs from the mounted demo resource
// image, and converts crx paths, which are expected to be relative to the
// mounted demo resources root to absolute paths that can be used by external
// extensions provider.
// NOTE: The class is expected to be used on the UI thread exclusively.
//
// TODO(b/290844778): Delete this class. It was used in olden times to launch
// the Demo Mode Chrome Apps from Chromium, but the Chrome Apps have been
// replaced by the Demo Mode SWA. Even for 1-2 yearly releases before the
// SWA migration, the Chrome Apps were installed via policy and self-launched
// via background script on installation. There's no need for this class
// anymore.
class DemoExtensionsExternalLoader : public extensions::ExternalLoader,
                                     public chromeos::ExternalCacheDelegate {
 public:
  // Whether demo apps should be loaded for the profile - i.e. whether the
  // profile is the primary profile in a demo session.
  static bool SupportedForProfile(Profile* profile);

  explicit DemoExtensionsExternalLoader(const base::FilePath& cache_dir);

  DemoExtensionsExternalLoader(const DemoExtensionsExternalLoader&) = delete;
  DemoExtensionsExternalLoader& operator=(const DemoExtensionsExternalLoader&) =
      delete;

  // Loads the app with `app_id` and installs it from the update url or cache.
  void LoadApp(const std::string& app_id);

  // extensions::ExternalLoader:
  void StartLoading() override;

  // chromeos::ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::Value::Dict& prefs) override;

 protected:
  ~DemoExtensionsExternalLoader() override;

 private:
  // Starts loading the external extensions prefs. Passed as callback to
  // DemoSession::EnsureOfflineResourcesLoaded() in StartLoading() - it
  // will get called when offline demo resources have finished loading.
  void StartLoadingFromOfflineDemoResources();

  // Called when the external extensions prefs are read from the disk.
  // `prefs` - demo extensions prefs.
  void DemoExternalExtensionsPrefsLoaded(
      absl::optional<base::Value::Dict> prefs);

  std::unique_ptr<chromeos::ExternalCache> external_cache_;

  const base::FilePath cache_dir_;

  // The list of app ids that should be cached by `external_cache_`.
  std::vector<std::string> app_ids_;

  base::WeakPtrFactory<DemoExtensionsExternalLoader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_EXTENSIONS_EXTERNAL_LOADER_H_
