// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_HTML_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_HTML_TEST_UTILS_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace ash {

// Helper methods for browser tests that need to get HTML element bounds
// and execute Javascript.

// Gets the bounds of the element with ID `field_id` in the web contents,
// in density-independent pixels.
gfx::Rect GetControlBoundsInRoot(content::WebContents* web_contents,
                                 const std::string& field_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_HTML_TEST_UTILS_H_
