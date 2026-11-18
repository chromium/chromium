// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_whats_new_invoke.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/glic_enums.mojom.h"

namespace glic {

void InvokeFromWhatsNew(GlicKeyedService& glic_service,
                        BrowserWindowInterface* browser_window) {
  if (!GlicEnabling::ShouldShowGlicButton(glic_service.profile())) {
    return;
  }

  // Use ToggleUI if !IsEnabledForProfile; this is needed for previously
  // onboarded profiles that are not now enabled.
  if (!base::FeatureList::IsEnabled(features::kGlicBrowserCommandInvoke) ||
      !GlicEnabling::IsEnabledForProfile(glic_service.profile())) {
    glic_service.ToggleUI(browser_window, /*prevent_close=*/false,
                          mojom::InvocationSource::kWhatsNew);
    return;
  }

  GlicInvokeOptions options(mojom::InvocationSource::kWhatsNew);
  options.target.surface = DefaultSurface{browser_window};
  options.fre_override = mojom::FreOverride::kTrustFirstInline;

  std::string prompt = features::kGlicBrowserCommandInvokePrompt.Get();
  if (!prompt.empty()) {
    options.prompts.push_back(std::move(prompt));
  }

  glic_service.Invoke(std::move(options));
}

}  // namespace glic
