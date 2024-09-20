// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/accessibility_service_private/accessibility_service_private.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

ExtensionFunction::ResponseAction
AccessibilityServicePrivateSpeakSelectedTextFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Send to EmbeddedA11yManagerLacros which sends through
  // crosapi to Ash.
  EmbeddedA11yManagerLacros::GetInstance()->SpeakSelectedText();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // Send directly to the AccessibilityManager in Ash.
  ash::AccessibilityManager::Get()->OnSelectToSpeakContextMenuClick();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return RespondNow(NoArguments());
}

}  // namespace extensions
