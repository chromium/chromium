// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_GUEST_URL_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_GUEST_URL_H_

class GURL;

namespace geic {

// Returns the Gemini Enterprise web app's Skills page URL for `guest_url`, the
// GEiC guest URL. Returns an empty `GURL` if `guest_url` is not valid.
GURL GetGeicSkillsUrlForGuestUrl(const GURL& guest_url);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_GUEST_URL_H_
