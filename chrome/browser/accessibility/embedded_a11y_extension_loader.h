// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_EMBEDDED_A11Y_EXTENSION_LOADER_H_
#define CHROME_BROWSER_ACCESSIBILITY_EMBEDDED_A11Y_EXTENSION_LOADER_H_

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"

///////////////////////////////////////////////////////////////////////////////
// EmbeddedA11yExtensionLoader
//
// A class that manages the installation and uninstallation of the
// Accessibility helper extension on every profile (including guest and
// incognito) for Chrome Accessibility services and features on all platforms
// except Lacros, where it just informs EmbeddedA11yHelperLacros.
//
class EmbeddedA11yExtensionLoader {
 public:
  // Gets the current instance of EmbeddedA11yExtensionLoader. There
  // should be one of these across all profiles.
  static EmbeddedA11yExtensionLoader* GetInstance();

  EmbeddedA11yExtensionLoader();
  virtual ~EmbeddedA11yExtensionLoader();
  EmbeddedA11yExtensionLoader(EmbeddedA11yExtensionLoader&) = delete;
  EmbeddedA11yExtensionLoader& operator=(EmbeddedA11yExtensionLoader&) = delete;

  // TODO(crbug.com/324143642): Observe the reading mode enabled/disabled state
  // in this class instead of informing EmbeddedA11yManagerLacros to
  // enable/disable reading mode.
  virtual void InstallA11yHelperExtensionForReadingMode();
  virtual void RemoveA11yHelperExtensionForReadingMode();

 private:
  base::WeakPtrFactory<EmbeddedA11yExtensionLoader> weak_ptr_factory_{this};

  friend struct base::DefaultSingletonTraits<EmbeddedA11yExtensionLoader>;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_EMBEDDED_A11Y_EXTENSION_LOADER_H_
