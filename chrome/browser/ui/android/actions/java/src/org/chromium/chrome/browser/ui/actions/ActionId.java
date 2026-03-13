// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Defines IDs for actions that can be registered in the {@link ActionRegistry}. */
@IntDef({
    ActionId.HOME_BUTTON,
    ActionId.TAB_SWITCHER,
    ActionId.MENU,
    ActionId.BACK_BUTTON,
    ActionId.NEW_TAB,
    ActionId.GLIC,
})
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE_USE)
@NullMarked
public @interface ActionId {
    int HOME_BUTTON = 0;
    int TAB_SWITCHER = 1;
    int MENU = 2;
    int BACK_BUTTON = 3;
    int NEW_TAB = 4;
    int GLIC = 5;
}
