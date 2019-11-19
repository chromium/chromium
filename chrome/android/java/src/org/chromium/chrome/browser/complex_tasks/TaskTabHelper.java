// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.complex_tasks;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.content_public.browser.WebContents;

/**
 * Used for maintaining Task ID (see NavigationTaskId) data about a Tab
 */
@JNINamespace("tasks")
public class TaskTabHelper {
    private static final long INVALID_ID = -1;

    private TaskTabHelper() {}

    /**
     * Creates the {@link TaskTabHelper} for the given {@link Tab}.
     * @param tab the Tab to attach the helper to.
     * @param parentTab corresponding parent Tab for the Tab
     */
    public static void createForTab(Tab tab, Tab parentTab) {
        if (parentTab == null) return;
        TabAttributes.from(tab).set(TabAttributeKeys.PARENT_TAB_TASK_ID,
                TaskTabHelperJni.get().getTaskId(parentTab.getWebContents()));
        TabAttributes.from(tab).set(TabAttributeKeys.PARENT_TAB_ROOT_TASK_ID,
                TaskTabHelperJni.get().getRootTaskId(parentTab.getWebContents()));
    }

    @CalledByNative
    private static long getParentTaskId(Tab tab) {
        Long parentTaskId = TabAttributes.from(tab).get(TabAttributeKeys.PARENT_TAB_TASK_ID);
        return parentTaskId == null ? INVALID_ID : parentTaskId;
    }

    @CalledByNative
    private static long getParentRootTaskId(Tab tab) {
        Long parentRootTaskId =
                TabAttributes.from(tab).get(TabAttributeKeys.PARENT_TAB_ROOT_TASK_ID);
        return parentRootTaskId == null ? INVALID_ID : parentRootTaskId;
    }

    @NativeMethods
    interface Natives {
        long getTaskId(WebContents webContents);
        long getRootTaskId(WebContents webContents);
    }
}
