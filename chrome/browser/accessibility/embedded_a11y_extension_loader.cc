// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// static
EmbeddedA11yExtensionLoader* EmbeddedA11yExtensionLoader::GetInstance() {
  return base::Singleton<
      EmbeddedA11yExtensionLoader,
      base::LeakySingletonTraits<EmbeddedA11yExtensionLoader>>::get();
}

EmbeddedA11yExtensionLoader::EmbeddedA11yExtensionLoader() = default;

EmbeddedA11yExtensionLoader::~EmbeddedA11yExtensionLoader() = default;

void EmbeddedA11yExtensionLoader::InstallA11yHelperExtensionForReadingMode() {
  // TODO(crbug.com/324143642): Install a11y helper extension for all platforms.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EmbeddedA11yManagerLacros::GetInstance()->SetReadingModeEnabled(true);
#endif
}

void EmbeddedA11yExtensionLoader::RemoveA11yHelperExtensionForReadingMode() {
  // TODO(crbug.com/324143642): Remove a11y helper extension for all platforms.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EmbeddedA11yManagerLacros::GetInstance()->SetReadingModeEnabled(false);
#endif
}
