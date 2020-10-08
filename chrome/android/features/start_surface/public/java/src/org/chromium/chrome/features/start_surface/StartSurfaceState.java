// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The internal state of the StartSurface.
 */
@IntDef({StartSurfaceState.NOT_SHOWN, StartSurfaceState.SHOWN_HOMEPAGE,
        StartSurfaceState.SHOWN_TABSWITCHER, StartSurfaceState.SHOWN_TABSWITCHER_TWO_PANES,
        StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY,
        StartSurfaceState.SHOWN_TABSWITCHER_OMNIBOX_ONLY, StartSurfaceState.DISABLED,
        StartSurfaceState.SHOWING_TABSWITCHER, StartSurfaceState.SHOWING_START,
        StartSurfaceState.SHOWING_HOMEPAGE, StartSurfaceState.SHOWING_PREVIOUS,
        StartSurfaceState.SHOWN_TABSWITCHER_TRENDY_TERMS})
@Retention(RetentionPolicy.SOURCE)
public @interface StartSurfaceState {
    int NOT_SHOWN = 0;

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, let's audit all the state here.

    // When overview is visible, it will be in one of the SHOWN states.
    int SHOWN_HOMEPAGE = 1;
    int SHOWN_TABSWITCHER = 2;
    int SHOWN_TABSWITCHER_TWO_PANES = 3;
    int SHOWN_TABSWITCHER_TASKS_ONLY = 4;
    int SHOWN_TABSWITCHER_OMNIBOX_ONLY = 5;
    int SHOWN_TABSWITCHER_TRENDY_TERMS = 6;

    int DISABLED = 7;

    // SHOWING states are intermediary states that will immediately transition
    // to one of the SHOWN states when overview is/becomes visible.
    int SHOWING_TABSWITCHER = 8;
    int SHOWING_START = 9;
    int SHOWING_HOMEPAGE = 10;
    int SHOWING_PREVIOUS = 11;
}
