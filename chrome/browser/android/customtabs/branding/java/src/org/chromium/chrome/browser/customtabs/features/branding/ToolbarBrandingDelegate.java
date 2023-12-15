// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

/** Delegate class for Toolbar strategy to control location bar UI elements. */
public interface ToolbarBrandingDelegate {
    /** Show the branding information on location bar, with start & end transition. */
    void showBrandingLocationBar();

    /**
     * Show an empty location bar. Used when waiting for whether to show branding.
     *
     * The implementation of this method should be fast so that it will not cause any UI janks when
     * switching state quickly into either branding location bar or regular toolbar.
     * */
    void showEmptyLocationBar();

    /** Show the regular location with URL and Title, with start transition. */
    void showRegularToolbar();
}
