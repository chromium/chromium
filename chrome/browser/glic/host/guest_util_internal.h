// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_INTERNAL_H_
#define CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_INTERNAL_H_

// Utilities for the glic guest that should not be used external to
// chrome/browser/glic.

namespace content {
class WebContents;
}

namespace glic {

class GlicUI;
class Host;

// Returns the GlicUI for the given guest WebContents, or nullptr if not a Glic
// guest.
GlicUI* GetGlicUiForGuest(content::WebContents* guest_contents);

// Returns the Glic Host for the given guest WebContents, or nullptr if not a
// Glic guest.
Host* GetGlicHostForGuest(content::WebContents* guest_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_INTERNAL_H_
