// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.StringDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of attributes used for {@link TabAttributes}. */
@StringDef({
    TabAttributeKeys.GROUPED_WITH_PARENT,
    TabAttributeKeys.MODAL_DIALOG_SHOWING,
    TabAttributeKeys.PARENT_TAB_TASK_ID,
    TabAttributeKeys.PARENT_TAB_ROOT_TASK_ID,
    TabAttributeKeys.ENTER_FULLSCREEN,
    TabAttributeKeys.FULLSCREEN_START_POSITION,
    TabAttributeKeys.FULLSCREEN_OPTIONS
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface TabAttributeKeys {
    /** Whether the tab should be grouped with its parent tab. True by default. */
    String GROUPED_WITH_PARENT = "isTabGroupedWithParent";

    /** Whether tab modal dialog is showing or not. */
    String MODAL_DIALOG_SHOWING = "isTabModalDialogShowing";

    /** Parent Tab Task Id. See NavigationTaskId (navigation_task_id.h) for definition */
    String PARENT_TAB_TASK_ID = "ParentTaskId";

    /** Parent Tab Root Task Id. See NavigationTaskId (navigation_task_id.h) for definition */
    String PARENT_TAB_ROOT_TASK_ID = "ParentRootTaskId";

    /** A runnable to delay the enabling of fullscreen mode if necessary. */
    String ENTER_FULLSCREEN = "EnterFullscreen";

    /**
     * A pair of display id (int) and window position (Rect) from where Fullscreen to screen
     * originated.
     */
    String FULLSCREEN_START_POSITION = "FullscreenStartPosition";

    /** FullscreenOptions when entering fullscreen to screen. */
    String FULLSCREEN_OPTIONS = "FullscreenOptions";
}
