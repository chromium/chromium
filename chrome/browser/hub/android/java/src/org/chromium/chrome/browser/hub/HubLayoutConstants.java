// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Animation constants for {@link HubLayout}. */
public class HubLayoutConstants {
    // Copied from TabSwitcherLayout.
    /** Duration in milliseconds of translate animations. */
    public static final long TRANSLATE_DURATION_MS = 300L;

    /** Duration in milliseconds of shrink and expand tab animations. */
    public static final long SHRINK_EXPAND_DURATION_MS = 325L;

    /** Duration in milliseconds of expand new tab animations. */
    public static final long EXPAND_NEW_TAB_DURATION_MS = 300L;

    /** Duration in milliseconds of fade animations. */
    public static final long FADE_DURATION_MS = 325L;

    /** Duration in milliseconds before a fallback animation will occur. */
    public static final long TIMEOUT_MS = 300L;
}
