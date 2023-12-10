// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Annotations for call sites of {@link TabLifecycle#show} or {@link TabImpl#loadIfNeeded}. */
@IntDef({
    TabLoadIfNeededCaller.SET_TAB,
    TabLoadIfNeededCaller.ON_ACTIVITY_SHOWN,
    TabLoadIfNeededCaller.ON_ACTIVITY_SHOWN_THEN_SHOW,
    TabLoadIfNeededCaller.REQUEST_TO_SHOW_TAB,
    TabLoadIfNeededCaller.REQUEST_TO_SHOW_TAB_THEN_SHOW,
    TabLoadIfNeededCaller.ON_FINISH_NATIVE_INITIALIZATION,
    TabLoadIfNeededCaller.MAYBE_SHOW_GLOBAL_SETTING_OPT_IN_MESSAGE,
    TabLoadIfNeededCaller.OTHER
})
@Retention(RetentionPolicy.SOURCE)
public @interface TabLoadIfNeededCaller {
    int SET_TAB = 0;
    int ON_ACTIVITY_SHOWN = 1;
    int ON_ACTIVITY_SHOWN_THEN_SHOW = 2;
    int REQUEST_TO_SHOW_TAB = 3;
    int REQUEST_TO_SHOW_TAB_THEN_SHOW = 4;
    int ON_FINISH_NATIVE_INITIALIZATION = 5;
    int MAYBE_SHOW_GLOBAL_SETTING_OPT_IN_MESSAGE = 6;
    int OTHER = 7;
}
