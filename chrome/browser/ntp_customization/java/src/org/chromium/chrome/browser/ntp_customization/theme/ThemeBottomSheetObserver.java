// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for updating the content of theme-related bottom sheets. Implementations should define
 * how their respective bottom sheet's UI or content state is updated in response to a NTP
 * background type change.
 */
@NullMarked
public interface ThemeBottomSheetObserver {
    /**
     * Called when a NTP background type selection occurs. Observers should use this to clear stale
     * UI states based on the new background type.
     */
    default void onBackgroundTypeChanged() {}
}
