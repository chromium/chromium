// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** An interface to handle platform specific implementations of ContextualTasksUiService. */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksUiServiceDelegate {
    private final Profile mProfile;
    private long mNativePtr;
    private @Nullable SnackbarManager mSnackbarManager;
    private final SnackbarController mSnackbarController =
            new SnackbarController() {
                @Override
                public void onAction(@Nullable Object actionData) {
                    if (actionData instanceof UndoActionData) {
                        UndoActionData data = (UndoActionData) actionData;
                        undoClose(data.windowAndroid, data.browserWindowPtr);
                    }
                }
            };

    private static class UndoActionData {
        public final WindowAndroid windowAndroid;
        public final long browserWindowPtr;

        public UndoActionData(WindowAndroid windowAndroid, long browserWindowPtr) {
            this.windowAndroid = windowAndroid;
            this.browserWindowPtr = browserWindowPtr;
        }
    }

    @CalledByNative
    @VisibleForTesting
    static ContextualTasksUiServiceDelegate create(
            long nativePtr, @JniType("Profile*") Profile profile) {
        return new ContextualTasksUiServiceDelegate(nativePtr, profile);
    }

    private ContextualTasksUiServiceDelegate(long nativePtr, Profile profile) {
        mNativePtr = nativePtr;
        mProfile = profile;
    }

    @CalledByNative
    @VisibleForTesting
    void clearNativePtr() {
        mNativePtr = 0;
        if (mSnackbarManager != null) {
            mSnackbarManager.dismissSnackbars(mSnackbarController);
            mSnackbarManager = null;
        }
    }

    @CalledByNative
    @VisibleForTesting
    void openFeedbackUi(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("std::string") String pageUrl) {
        Activity activity = windowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";
        if (activity == null) {
            return;
        }

        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                .showFeedback(activity, pageUrl, "cobrowse");
    }

    @CalledByNative
    @VisibleForTesting
    void onWebUIReady(
            @JniType("std::string") String taskId,
            @JniType("content::WebContents*") WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;

        ContextualTasksFuseboxManager fuseboxManager =
                ContextualTasksFuseboxManager.from(windowAndroid);
        if (fuseboxManager == null) return;

        fuseboxManager.onWebUIReady(taskId, webContents);
    }

    @CalledByNative
    @VisibleForTesting
    void onWebUIDestroyed(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("std::string") String taskId) {
        ContextualTasksFuseboxManager fuseboxManager =
                ContextualTasksFuseboxManager.from(windowAndroid);
        if (fuseboxManager != null) {
            fuseboxManager.onWebUIDestroyed(taskId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onTaskChanged(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("std::string") String oldTaskId,
            @JniType("std::string") String newTaskId) {
        ContextualTasksFuseboxManager fuseboxManager =
                ContextualTasksFuseboxManager.from(windowAndroid);
        if (fuseboxManager != null) {
            fuseboxManager.onTaskChanged(oldTaskId, newTaskId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    void showUndoSnackbar(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid, long browserWindowPtr) {
        mSnackbarManager = SnackbarManagerProvider.from(windowAndroid);
        if (mSnackbarManager == null) return;

        Context context = ContextUtils.getApplicationContext();
        Snackbar snackbar =
                Snackbar.make(
                                context.getString(R.string.contextual_tasks_thread_closed),
                                mSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_CONTEXTUAL_TASKS_BOTTOM_SHEET_CLOSED_UNDO)
                        .setAction(
                                context.getString(R.string.undo),
                                new UndoActionData(windowAndroid, browserWindowPtr));
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void undoClose(WindowAndroid windowAndroid, long browserWindowPtr) {
        if (mNativePtr == 0 || windowAndroid.isDestroyed()) return;

        // Reusing the browserWindowPtr is safe here in c++ because the WindowAndroid isn't
        // destroyed yet, and its lifecycle is tightly coupled with the BrowserWindowInterface.
        ContextualTasksUiServiceDelegateJni.get().undoClose(mNativePtr, browserWindowPtr);
    }

    @NativeMethods
    interface Natives {
        void undoClose(long nativeContextualTasksUiServiceDelegateAndroid, long browserWindowPtr);
    }
}
