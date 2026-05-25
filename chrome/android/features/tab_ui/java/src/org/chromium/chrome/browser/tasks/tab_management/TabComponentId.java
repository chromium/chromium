// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Identifiers for parent components hosting a Tab List. */
@NullMarked
@Target(ElementType.TYPE_USE)
@IntDef({
    TabComponentId.GRID_TAB_SWITCHER,
    TabComponentId.TAB_STRIP,
    TabComponentId.TAB_GRID_DIALOG_FROM_STRIP,
    TabComponentId.TAB_GRID_DIALOG_IN_SWITCHER,
    TabComponentId.TAB_LIST_EDITOR,
    TabComponentId.ARCHIVED_TABS_DIALOG,
    TabComponentId.VERTICAL_TABS
})
@Retention(RetentionPolicy.SOURCE)
public @interface TabComponentId {
    int GRID_TAB_SWITCHER = 0;
    int TAB_STRIP = 1;
    int TAB_GRID_DIALOG_FROM_STRIP = 2;
    int TAB_GRID_DIALOG_IN_SWITCHER = 3;
    int TAB_LIST_EDITOR = 4;
    int ARCHIVED_TABS_DIALOG = 5;
    int VERTICAL_TABS = 6;
}
