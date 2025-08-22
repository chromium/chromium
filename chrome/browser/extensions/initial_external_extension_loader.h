// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INITIAL_EXTERNAL_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_INITIAL_EXTERNAL_EXTENSION_LOADER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/external_loader.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace extensions {

// Manages the initial external extension to ensure that they persist after
// first run.
class InitialExternalExtensionLoader : public ExternalLoader {
 public:
  explicit InitialExternalExtensionLoader(PrefService& prefs);

  InitialExternalExtensionLoader(const InitialExternalExtensionLoader&) =
      delete;
  InitialExternalExtensionLoader& operator=(
      const InitialExternalExtensionLoader&) = delete;

  // ExternalLoader:
  void StartLoading() override;

 protected:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  ~InitialExternalExtensionLoader() override;

  // React to updates to kInitialExternalExtensions.
  void OnExtensionsPrefChanged();

  // Not owned. Lives as long as Profile.
  const raw_ref<PrefService> prefs_;

  // Pref registrar for managing the change observers.
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INITIAL_EXTERNAL_EXTENSION_LOADER_H_
