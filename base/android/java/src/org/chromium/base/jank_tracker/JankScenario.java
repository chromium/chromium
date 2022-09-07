// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A list of jank scenarios to be tracked, each scenario corresponds to a specific user journey
 * except by PERIODIC_REPORTING, which runs constantly and is uploaded every 30s.
 */
@IntDef({JankScenario.PERIODIC_REPORTING, JankScenario.OMNIBOX_FOCUS, JankScenario.NEW_TAB_PAGE,
        JankScenario.STARTUP, JankScenario.TAB_SWITCHER, JankScenario.OPEN_LINK_IN_NEW_TAB,
        JankScenario.START_SURFACE_HOMEPAGE, JankScenario.START_SURFACE_TAB_SWITCHER})
@Retention(RetentionPolicy.SOURCE)
public @interface JankScenario {
    int PERIODIC_REPORTING = 1;
    int OMNIBOX_FOCUS = 2;
    int NEW_TAB_PAGE = 3;
    int STARTUP = 4;
    int TAB_SWITCHER = 5;
    int OPEN_LINK_IN_NEW_TAB = 6;
    int START_SURFACE_HOMEPAGE = 7;
    int START_SURFACE_TAB_SWITCHER = 8;
}
