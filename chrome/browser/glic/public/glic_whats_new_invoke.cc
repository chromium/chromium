// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_whats_new_invoke.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {
namespace {

// Prepares `GlicInvokeOptions` when `Invoke` / `InvokeWithAutoSubmit` should be
// used, or handles the fallback (no-op or `ToggleUI`) and returns
// `std::nullopt`.
std::optional<GlicInvokeOptions> PrepareWhatsNewInvokeOrFallback(
    GlicKeyedService& glic_service,
    BrowserWindowInterface* browser_window) {
  if (!GlicEnabling::ShouldShowGlicButton(glic_service.profile())) {
    return std::nullopt;
  }

  // Use ToggleUI if !IsEnabledForProfile; this is needed for previously
  // onboarded profiles that are not now enabled.
  if (!base::FeatureList::IsEnabled(features::kGlicBrowserCommandInvoke) ||
      !GlicEnabling::IsEnabledForProfile(glic_service.profile())) {
    glic_service.ToggleUI(browser_window, /*prevent_close=*/false,
                          mojom::InvocationSource::kWhatsNew);
    return std::nullopt;
  }

  GlicInvokeOptions options(mojom::InvocationSource::kWhatsNew);
  options.target.surface = DefaultSurface{browser_window};
  options.fre_override = mojom::FreOverride::kTrustFirstInline;
  return options;
}

}  // namespace

void InvokeFromWhatsNew(InvokeFromWhatsNewPasskey,
                        GlicKeyedService& glic_service,
                        BrowserWindowInterface* browser_window) {
  std::optional<GlicInvokeOptions> options =
      PrepareWhatsNewInvokeOrFallback(glic_service, browser_window);
  if (!options.has_value()) {
    return;
  }

  glic_service.Invoke(std::move(*options));
}

void InvokeFromWhatsNewForQuizzes(InvokeFromWhatsNewPasskey,
                                  GlicKeyedService& glic_service,
                                  BrowserWindowInterface* browser_window) {
  std::optional<GlicInvokeOptions> options =
      PrepareWhatsNewInvokeOrFallback(glic_service, browser_window);
  if (!options.has_value()) {
    return;
  }

  // Only explicitly approved What's New modules may auto-submit prompts; adding
  // a new auto-submitting handler requires chrome/browser/glic/API_OWNERS
  // review.
  if (base::FeatureList::IsEnabled(features::kGlicWhatsNewQuizzes)) {
    std::string prompt =
        l10n_util::GetStringUTF8(IDS_GLIC_WHATS_NEW_QUIZZES_PROMPT);
    if (!prompt.empty()) {
      options->prompts.push_back(std::move(prompt));
      glic_service.InvokeWithAutoSubmit(
          InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
          std::move(*options));
      return;
    }
  }

  glic_service.Invoke(std::move(*options));
}

}  // namespace glic
