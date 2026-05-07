// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Java bridge for Contextual Tasks. Owned by the activity's TabbedRootUiCoordinator. Owns its
 * native JNI counterpart.
 */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksBridge implements ChromeAndroidTaskFeature {
    private static final UnownedUserDataKey<
                    SettableMonotonicObservableSupplier<ContextualTasksBridge>>
            SUPPLIER_KEY = new UnownedUserDataKey<>();

    private final Profile mProfile;
    private final ActivityWindowAndroid mWindowAndroid;
    private long mNativeContextualTasksBridge;
    private @Nullable SnackbarManager mSnackbarManager;
    private final SnackbarController mSnackbarController =
            new SnackbarController() {
                @Override
                public void onAction(@Nullable Object actionData) {
                    if (mNativeContextualTasksBridge != 0) {
                        ContextualTasksBridgeJni.get().undoClose(mNativeContextualTasksBridge);
                    }
                }
            };

    public ContextualTasksBridge(Profile profile, ActivityWindowAndroid windowAndroid) {
        mProfile = profile;
        mWindowAndroid = windowAndroid;
    }

    /**
     * Returns the {@link MonotonicObservableSupplier} for {@link ContextualTasksBridge} from the
     * given {@link WindowAndroid}.
     */
    public static MonotonicObservableSupplier<ContextualTasksBridge> getSupplier(
            WindowAndroid windowAndroid) {
        SettableMonotonicObservableSupplier<ContextualTasksBridge> supplier =
                SUPPLIER_KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
        if (supplier == null) {
            supplier = ObservableSuppliers.createMonotonic();
            SUPPLIER_KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), supplier);
        }
        return supplier;
    }

    @Override
    public void onAddedToTask(long nativeBrowserWindowPtr) {
        if (nativeBrowserWindowPtr == 0) return;
        mNativeContextualTasksBridge =
                ContextualTasksBridgeJni.get().init(this, nativeBrowserWindowPtr, mProfile);

        SettableMonotonicObservableSupplier<ContextualTasksBridge> supplier =
                (SettableMonotonicObservableSupplier<ContextualTasksBridge>)
                        getSupplier(mWindowAndroid);
        supplier.set(this);
    }

    @Override
    public void onFeatureRemoved() {
        if (mNativeContextualTasksBridge != 0) {
            ContextualTasksBridgeJni.get().destroy(mNativeContextualTasksBridge);
            mNativeContextualTasksBridge = 0;
        }
        if (mSnackbarManager != null) {
            mSnackbarManager.dismissSnackbars(mSnackbarController);
            mSnackbarManager = null;
        }
    }

    @CalledByNative
    void openFeedbackUi(String pageUrl) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return;

        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                .showFeedback(activity, pageUrl, "cobrowse");
    }

    @CalledByNative
    void onWebUIReady(String taskId, WebContents webContents) {
        ContextualTasksFuseboxManager fuseboxManager =
                ContextualTasksFuseboxManager.from(mWindowAndroid);
        if (fuseboxManager == null) return;
        fuseboxManager.onWebUIReady(taskId, webContents);
    }

    @CalledByNative
    void onWebUIDestroyed(String taskId) {
        ContextualTasksFuseboxManager fuseboxManager =
                ContextualTasksFuseboxManager.from(mWindowAndroid);
        if (fuseboxManager == null) return;
        fuseboxManager.onWebUIDestroyed(taskId);
    }

    @CalledByNative
    void onTaskChanged(String oldTaskId, String newTaskId) {
        ContextualTasksFuseboxManager fuseboxManager =
                ContextualTasksFuseboxManager.from(mWindowAndroid);
        if (fuseboxManager == null) return;
        fuseboxManager.onTaskChanged(oldTaskId, newTaskId);
    }

    @CalledByNative
    void showUndoSnackbar() {
        mSnackbarManager = SnackbarManagerProvider.from(mWindowAndroid);
        if (mSnackbarManager == null) return;

        Context context = ContextUtils.getApplicationContext();
        Snackbar snackbar =
                Snackbar.make(
                                context.getString(R.string.contextual_tasks_thread_closed),
                                mSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_CONTEXTUAL_TASKS_BOTTOM_SHEET_CLOSED_UNDO)
                        .setAction(context.getString(R.string.undo), null);
        mSnackbarManager.showSnackbar(snackbar);
    }

    /**
     * Returns the task ID associated with the given tab.
     *
     * @param tab The tab to check.
     * @return The task ID, or null if no task is associated.
     */
    public static @Nullable String getTaskIdForTab(@Nullable Tab tab) {
        if (tab == null || tab.getWebContents() == null) return null;
        return ContextualTasksBridgeJni.get().getTaskIdForTab(tab.getWebContents());
    }

    /**
     * Returns whether the given URL is a contextual tasks WebUI URL.
     *
     * @param url The URL to check.
     * @return True if it is a contextual tasks URL.
     */
    public static boolean isContextualTasksUrl(GURL url) {
        return ContextualTasksBridgeJni.get().isContextualTasksUrl(url);
    }

    @NativeMethods
    public interface Natives {
        long init(
                ContextualTasksBridge obj,
                long browserWindowPtr,
                @JniType("Profile*") Profile profile);

        void destroy(long nativeContextualTasksBridge);

        void undoClose(long nativeContextualTasksBridge);

        @JniType("std::string")
        String getTaskIdForTab(@JniType("content::WebContents*") WebContents webContents);

        boolean isContextualTasksUrl(@JniType("GURL") GURL url);
    }
}
