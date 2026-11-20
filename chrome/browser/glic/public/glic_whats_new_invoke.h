// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_WHATS_NEW_INVOKE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_WHATS_NEW_INVOKE_H_

#include "base/types/pass_key.h"

class BrowserWindowInterface;
class WhatsNewUI;

namespace glic {

class GlicKeyedService;
class GlicWhatsNewInvokeTest;

// Passkey for invoking Glic from What's New. Reach out to
// chrome/browser/glic/API_OWNERS before adding new callers.
class InvokeFromWhatsNewPasskeyProvider {
 public:
  using PassKey = base::PassKey<InvokeFromWhatsNewPasskeyProvider>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  friend class ::WhatsNewUI;
  friend class GlicWhatsNewInvokeTest;
};

using InvokeFromWhatsNewPasskey =
    base::PassKey<InvokeFromWhatsNewPasskeyProvider>;

// Opens Glic for the `kOpenGlic` browser command from the What's New page.
// Callers must ensure `GlicEnabling::ShouldShowGlicButton` is true for
// `glic_service.profile()` (fallback URL navigation is handled by
// `BrowserCommandHandler`).
// - When `kGlicBrowserCommandInvoke` is enabled and Glic is enabled for the
//   profile, invokes Glic via `GlicKeyedService::Invoke` without a prompt.
// - Otherwise (when `kGlicBrowserCommandInvoke` is disabled or the profile is
//   not enabled, e.g. showing an anchored button on an ineligible account),
//   falls back to `GlicKeyedService::ToggleUI`.
// `browser_window` may be null or a non-normal window. In that case the
// `ToggleUI` path uses the active Glic-eligible browser, while the `Invoke`
// path opens a new browser window.
void InvokeFromWhatsNew(InvokeFromWhatsNewPasskey,
                        GlicKeyedService& glic_service,
                        BrowserWindowInterface* browser_window);

// Opens Glic for the `kOpenGlicForQuizzes` browser command from the What's New
// page.
// Callers must ensure `GlicEnabling::ShouldShowGlicButton` is true for
// `glic_service.profile()` (fallback URL navigation is handled by
// `BrowserCommandHandler`).
// - When `kGlicBrowserCommandInvoke` is enabled and Glic is enabled for the
//   profile:
//   - If `kGlicWhatsNewQuizzes` is enabled, invokes Glic with
//     `IDS_GLIC_WHATS_NEW_QUIZZES_PROMPT` and auto-submits via
//     `GlicKeyedService::InvokeWithAutoSubmit`.
//   - Otherwise, invokes Glic via `GlicKeyedService::Invoke` without a prompt.
// - Otherwise (when `kGlicBrowserCommandInvoke` is disabled or the profile is
//   not enabled), falls back to `GlicKeyedService::ToggleUI`.
void InvokeFromWhatsNewForQuizzes(InvokeFromWhatsNewPasskey,
                                  GlicKeyedService& glic_service,
                                  BrowserWindowInterface* browser_window);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_WHATS_NEW_INVOKE_H_
