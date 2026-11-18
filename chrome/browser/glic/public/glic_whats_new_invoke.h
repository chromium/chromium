// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_WHATS_NEW_INVOKE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_WHATS_NEW_INVOKE_H_

class BrowserWindowInterface;

namespace glic {

class GlicKeyedService;

// Opens Glic for the `kOpenGlic` browser command from the What's New page.
// Callers must ensure `GlicEnabling::ShouldShowGlicButton` is true for
// `glic_service.profile()` (fallback URL navigation is handled by
// `BrowserCommandHandler`).
// - Invokes Glic via `GlicKeyedService::Invoke` when
//   `kGlicBrowserCommandInvoke` is enabled and Glic is enabled for the profile,
//   pre-filling (without submitting) `kGlicBrowserCommandInvokePrompt` if
//   non-empty.
// - Otherwise (when `kGlicBrowserCommandInvoke` is disabled or the profile is
//   not enabled, e.g. showing an anchored button on an ineligible account),
//   falls back to `GlicKeyedService::ToggleUI`.
// `browser_window` may be null or a non-normal window. In that case the
// `ToggleUI` path uses the active Glic-eligible browser, while the `Invoke`
// path opens a new browser window.
void InvokeFromWhatsNew(GlicKeyedService& glic_service,
                        BrowserWindowInterface* browser_window);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_WHATS_NEW_INVOKE_H_
