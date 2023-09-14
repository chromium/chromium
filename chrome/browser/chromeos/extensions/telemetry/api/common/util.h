// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_UTIL_H_

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace chromeos {

// Finds a WebContents hosting a app UI of a ChromeOSSystemExtension. The
// security level of the WebContents must be secure. If `focused_ui_required` is
// true, the WebContents must be focused. A focused UI will always be preferred
// regardless of `focused_ui_required`.
content::WebContents* FindTelemetryExtensionOpenAndSecureAppUi(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    bool focused_ui_required = false);

// Same as above but returns whether there is a valid WebContents or not.
bool IsTelemetryExtensionAppUiOpenAndSecure(
    content::BrowserContext* context,
    const extensions::Extension* extension);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_UTIL_H_
