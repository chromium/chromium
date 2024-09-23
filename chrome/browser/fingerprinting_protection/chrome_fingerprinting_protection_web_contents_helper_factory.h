// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINGERPRINTING_PROTECTION_CHROME_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_FACTORY_H_
#define CHROME_BROWSER_FINGERPRINTING_PROTECTION_CHROME_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_FACTORY_H_

class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace privacy_sandbox {
class TrackingProtectionSettings;
}  // namespace privacy_sandbox

// Creates a FingerprintingProtectionWebContentsHelper object and attaches it
// to `web_contents`. This object manages the per-Page objects in a WebContents
// for a fingerprinting protection filter.
void CreateFingerprintingProtectionWebContentsHelper(
    content::WebContents* web_contents,
    PrefService* pref_service,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    bool is_incognito);

#endif  // CHROME_BROWSER_FINGERPRINTING_PROTECTION_CHROME_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_FACTORY_H_
