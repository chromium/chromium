// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

/**
 * Interface used to generalize the creation of the OverlayPanelContent. This is so test cases
 * are able to offer a custom version of the OverlayPanelContent to be used in the tests.
 */
public interface OverlayPanelContentFactory {
    /** Create a new OverlayPanelContent object. This can be overridden for tests. */
    OverlayPanelContent createNewOverlayPanelContent();
}
