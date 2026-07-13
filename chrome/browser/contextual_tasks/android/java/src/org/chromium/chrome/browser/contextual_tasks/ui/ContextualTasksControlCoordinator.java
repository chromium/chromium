// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.ui;

import android.content.res.Resources;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.PeekViewManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator to manage the Contextual Tasks peek view in the tab bottom sheet. */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksControlCoordinator implements PeekViewManager {
    private final TabBottomSheetManager mTabBottomSheetManager;
    private final PropertyModel mModel;
    private final ContextualTasksControlMediator mMediator;
    private long mNativeContextualTasksControlCoordinator;

    private static final String EMPTY_TASK_ID = "";
    private static final String EMPTY_TITLE = "";

    private String mActiveTaskId = EMPTY_TASK_ID;
    private String mActiveTaskTitle = EMPTY_TITLE;

    /**
     * Constructs the coordinator.
     *
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} to use.
     * @param profile The {@link Profile} to use.
     */
    public ContextualTasksControlCoordinator(
            TabBottomSheetManager tabBottomSheetManager, Profile profile) {
        mTabBottomSheetManager = tabBottomSheetManager;

        mModel =
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(TabBottomSheetPeekProperties.TITLE_TEXT, "")
                        .with(
                                TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                                R.style.TextAppearance_TextMediumThick_Primary)
                        .with(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID, Resources.ID_NULL)
                        .with(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.GONE)
                        .with(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID, Resources.ID_NULL)
                        .with(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.GONE)
                        .with(
                                TabBottomSheetPeekProperties.PEEK_ICON_ID,
                                R.drawable.ic_logo_googleg_24dp)
                        .with(
                                TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED,
                                this::onActionClicked)
                        .with(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED, this::onCloseClicked)
                        .with(
                                TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED,
                                this::onPeekViewClicked)
                        .build();

        mMediator = new ContextualTasksControlMediator(mModel);

        if (mNativeContextualTasksControlCoordinator == 0) {
            mNativeContextualTasksControlCoordinator =
                    ContextualTasksControlCoordinatorJni.get().init(this, profile);
        }
    }

    /**
     * Called when the title of a task changes.
     *
     * @param taskId The ID of the task that changed.
     * @param title The new title of the task.
     */
    @CalledByNative
    void onTaskTitleChanged(
            @JniType("std::string") String taskId, @JniType("std::string") String title) {
        if (taskId.equals(mActiveTaskId)) {
            mActiveTaskTitle = title;
            updatePeekView();
        }
    }

    /**
     * Called when the active task changes.
     *
     * @param oldTaskId The ID of the previous active task.
     * @param newTaskId The ID of the new active task.
     */
    @CalledByNative
    void onTaskChanged(
            @JniType("std::string") String oldTaskId, @JniType("std::string") String newTaskId) {
        mActiveTaskId = newTaskId;
        if (newTaskId.isEmpty()) {
            clearActiveTask();
        } else {
            updatePeekView();
        }
    }

    /**
     * Called when a task is removed.
     *
     * @param taskId The ID of the task that was removed.
     */
    @CalledByNative
    void onTaskRemoved(@JniType("std::string") String taskId) {
        if (taskId.equals(mActiveTaskId)) {
            clearActiveTask();
        }
    }

    private void clearActiveTask() {
        mActiveTaskId = EMPTY_TASK_ID;
        mActiveTaskTitle = EMPTY_TITLE;
        updatePeekView();
    }

    private void updatePeekView() {
        mMediator.setTitle(mActiveTaskTitle);
    }

    private void onActionClicked() {
        mTabBottomSheetManager.setSheetExpanded(true);
    }

    private void onCloseClicked() {
        mTabBottomSheetManager.tryToCloseBottomSheet(/* animate= */ true);
    }

    private void onPeekViewClicked() {
        mTabBottomSheetManager.setSheetExpanded(true);
    }

    @Override
    public PropertyModel getModel() {
        return mModel;
    }

    @Override
    public void destroy() {
        if (mNativeContextualTasksControlCoordinator != 0) {
            ContextualTasksControlCoordinatorJni.get()
                    .destroy(mNativeContextualTasksControlCoordinator);
            mNativeContextualTasksControlCoordinator = 0;
        }
        clearActiveTask();
    }

    @NativeMethods
    interface Natives {
        long init(ContextualTasksControlCoordinator caller, @JniType("Profile*") Profile profile);

        void destroy(long nativeContextualTasksControlCoordinatorAndroid);
    }
}
