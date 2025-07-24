// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;

/** Animation constants for the Hub. */
@NullMarked
public class HubAnimationConstants {
    // Copied from TabSwitcherLayout.
    /** Duration in milliseconds of translate animations for the Hub Layout.. */
    public static final long HUB_LAYOUT_TRANSLATE_DURATION_MS = 300L;

    /** Duration in milliseconds of shrink and expand tab animations for the Hub Layout. */
    public static final long HUB_LAYOUT_SHRINK_EXPAND_DURATION_MS = 325L;

    /** Duration in milliseconds of expand new tab animations for the Hub Layout. */
    public static final long HUB_LAYOUT_EXPAND_NEW_TAB_DURATION_MS = 300L;

    /** Duration in milliseconds of fade animations for the Hub Layout. */
    public static final long HUB_LAYOUT_FADE_DURATION_MS = 325L;

    /** Duration in milliseconds before the fade in animation for the Hub Layout on XR. */
    public static final long HUB_LAYOUT_XR_FADE_IN_DELAY_MS = 250L;

    /** Duration in milliseconds before the fade out animation for the Hub Layout on XR. */
    public static final long HUB_LAYOUT_XR_FADE_OUT_DELAY_MS = 250L;

    /** Duration in milliseconds before a fallback animation will occur for the Hub Layout. */
    public static final long HUB_LAYOUT_TIMEOUT_MS = 300L;

    /** Duration in milliseconds of tab list animation for the Hub Layout. */
    public static final long HUB_LAYOUT_TAB_LIST_FADE_DURATION_MS = 400L;

    /**
     * Duration in milliseconds of color blend animations for Hub Pane changes. Chosen to exactly
     * match the length of a consecutive fade-in and fade-out animation.
     */
    public static final long PANE_COLOR_BLEND_ANIMATION_DURATION_MS = 240L;

    /**
     * Duration in milliseconds of fade animations for Hub Pane changes. Chosen to exactly match the
     * default add/remove animation duration of RecyclerView.
     */
    public static final long PANE_FADE_ANIMATION_DURATION_MS =
            PANE_COLOR_BLEND_ANIMATION_DURATION_MS / 2;

    /** Duration in milliseconds of slide animations for Hub pane changes. */
    public static final long PANE_SLIDE_ANIMATION_DURATION_MS = 250;
}
