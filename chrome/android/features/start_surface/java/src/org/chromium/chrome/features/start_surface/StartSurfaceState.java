// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The internal state of the StartSurface.
 *
 * These values are persisted to logs. Entries should not be renumbered and numeric values should
 * never be reused. See tools/metrics/histograms/enums.xml.
 */
@IntDef({StartSurfaceState.NOT_SHOWN, StartSurfaceState.SHOWN_HOMEPAGE,
        StartSurfaceState.SHOWN_TABSWITCHER, StartSurfaceState.DISABLED,
        StartSurfaceState.SHOWING_TABSWITCHER, StartSurfaceState.SHOWING_START,
        StartSurfaceState.SHOWING_HOMEPAGE, StartSurfaceState.SHOWING_PREVIOUS,
        StartSurfaceState.NUM_ENTRIES})
@Retention(RetentionPolicy.SOURCE)
public @interface StartSurfaceState {
    int NOT_SHOWN = 0;

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, let's audit all the state here.

    // TODO(https://crbug.com/1315679): Replace this with {@link LayoutType} after the {@link
    // ChromeFeatureList.START_SURFACE_REFACTOR} is enabled by default.
    // When overview is visible, it will be in one of the SHOWN states.
    @Deprecated
    int SHOWN_HOMEPAGE = 1;
    @Deprecated
    int SHOWN_TABSWITCHER = 2;

    int DISABLED = 3;

    // SHOWING states are intermediary states that will immediately transition
    // to one of the SHOWN states when overview is/becomes visible.
    int SHOWING_TABSWITCHER = 4;
    int SHOWING_START = 5;
    int SHOWING_HOMEPAGE = 6;
    int SHOWING_PREVIOUS = 7;
    int NUM_ENTRIES = 8;
}
