// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxDataProvider;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

/** Implementation of {@link ContextualTasksFuseboxManager}. */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksFuseboxManagerImpl implements ContextualTasksFuseboxManager {
    private final Activity mActivity;
    private final Supplier<ContextualTasksFusebox.ContextualTasksFuseboxConfig>
            mFuseboxConfigSupplier;
    private final WindowAndroid mWindowAndroid;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final MonotonicObservableSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private final CurrentTabObserver mCurrentTabObserver;

    // The fusebox instance. Shared across all tabs. Lazily initialized.
    private @Nullable ContextualTasksFusebox mFusebox;
    private final ContextualTasksFuseboxDataProvider mFuseboxDataProvider;
    private final Map<String, FuseboxSessionState> mTaskSessionMap = new HashMap<>();

    public static @Nullable ContextualTasksFuseboxManagerImpl from(WindowAndroid windowAndroid) {
        ContextualTasksFuseboxManager manager = ContextualTasksFuseboxManager.from(windowAndroid);
        if (manager instanceof ContextualTasksFuseboxManagerImpl managerImpl) {
            return managerImpl;
        }
        return null;
    }

    public ContextualTasksFuseboxManagerImpl(
            Activity activity,
            Supplier<ContextualTasksFusebox.ContextualTasksFuseboxConfig> fuseboxConfigSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            MonotonicObservableSupplier<SnackbarManager> snackbarManagerSupplier) {
        mActivity = activity;
        mFuseboxConfigSupplier = fuseboxConfigSupplier;
        mWindowAndroid = windowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;
        mProfileSupplier = profileSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mFuseboxDataProvider =
                new ContextualTasksFuseboxDataProvider(
                        mActivity,
                        mProfileSupplier.get() == null
                                ? false
                                : mProfileSupplier.get().isOffTheRecord());

        mCurrentTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                                updateFuseboxVisibility(tab);
                            }

                            @Override
                            public void onDidFinishNavigationInPrimaryMainFrame(
                                    Tab tab, NavigationHandle navigation) {
                                updateFuseboxVisibility(tab);
                            }
                        },
                        this::updateFuseboxVisibility);

        KEY.attachToHost(mWindowAndroid.getUnownedUserDataHost(), this);
    }

    /**
     * Called when the WebUI is ready. Triggers the initialization of the fusebox session state.
     *
     * @param taskId The ID of the task.
     * @param webContents The WebContents of the contextual tasks WebUI.
     */
    @Override
    public void onWebUIReady(String taskId, WebContents webContents) {
        ensureFuseboxSessionState(taskId, webContents);
    }

    /**
     * Called when the WebUI controller is destroyed.
     *
     * @param taskId The ID of the task.
     */
    @Override
    public void onWebUIDestroyed(String taskId) {
        FuseboxSessionState sessionState = mTaskSessionMap.remove(taskId);
        if (sessionState != null) {
            if (mFuseboxDataProvider.getFuseboxSessionState() == sessionState) {
                mFuseboxDataProvider.setFuseboxSessionState(null);
            }
            ComposeboxQueryControllerBridge bridge =
                    sessionState.getComposeboxQueryControllerBridge();
            if (bridge != null) {
                bridge.onWebUIDestroyed();
            }
            sessionState.destroy();
        }
    }

    /**
     * Called when the task ID is updated. Re-keys the session map.
     *
     * @param oldTaskId The old ID of the task.
     * @param newTaskId The new ID of the task.
     */
    @Override
    public void onTaskChanged(String oldTaskId, String newTaskId) {
        FuseboxSessionState sessionState = mTaskSessionMap.remove(oldTaskId);
        if (sessionState != null) {
            mTaskSessionMap.put(newTaskId, sessionState);
        }
    }

    /**
     * Called to initialize the {@ link FuseboxSessionState} associated with a task. Ignored if the
     * task is already associated with a {@link FuseboxSessionState}.
     *
     * @param taskId The ID of the task.
     * @param webContents The WebContents of the contextual tasks WebUI associated with the fusebox.
     */
    public void ensureFuseboxSessionState(String taskId, WebContents webContents) {
        FuseboxSessionState fuseboxSessionState = mTaskSessionMap.get(taskId);
        if (fuseboxSessionState == null) {
            fuseboxSessionState = createSessionState(webContents);
            mTaskSessionMap.put(taskId, fuseboxSessionState);
        }
        mFuseboxDataProvider.setFuseboxSessionState(fuseboxSessionState);
    }

    @VisibleForTesting
    FuseboxSessionState createSessionState(WebContents webContents) {
        return new FuseboxSessionState(webContents);
    }

    @Override
    public @Nullable View getFuseboxView() {
        return mFusebox != null ? mFusebox.getFuseboxView() : null;
    }

    @Override
    public ContextualTasksFuseboxDataProvider getFuseboxDataProvider() {
        return mFuseboxDataProvider;
    }

    private @Nullable String getTaskIdForTab(@Nullable Tab tab) {
        if (tab == null || tab.getWebContents() == null) return null;
        return ContextualTasksFuseboxManagerImplJni.get().getTaskIdForTab(tab.getWebContents());
    }

    private void updateFuseboxVisibility(@Nullable Tab currentTab) {
        String taskId = getTaskIdForTab(currentTab);
        if (currentTab != null
                && currentTab.getWebContents() != null
                && !TextUtils.isEmpty(taskId)
                && isContextualTasksUrl(currentTab.getUrl())) {

            ensureFuseboxSessionState(taskId, currentTab.getWebContents());

            ensureFuseboxInitialized();
            setFuseboxVisible(true);
        } else {
            mFuseboxDataProvider.setFuseboxSessionState(/* fuseboxSessionState= */ null);
            setFuseboxVisible(false);
        }
    }

    private boolean isContextualTasksUrl(GURL url) {
        return ContextualTasksFuseboxManagerImplJni.get().isContextualTasksUrl(url);
    }

    private void setFuseboxVisible(boolean visible) {
        if (mFusebox == null) return;
        mFusebox.getFuseboxView().setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    private void ensureFuseboxInitialized() {
        if (mFusebox != null) return;

        SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
        if (snackbarManager == null) return;

        ContextualTasksFusebox.ContextualTasksFuseboxConfig config = mFuseboxConfigSupplier.get();

        mFusebox =
                new ContextualTasksFusebox(
                        mActivity,
                        config.contentView,
                        config,
                        mProfileSupplier,
                        mWindowAndroid,
                        mLifecycleDispatcher,
                        /* loadUrlCallback= */ CallbackUtils.emptyCallback(),
                        snackbarManager,
                        mFuseboxDataProvider);
    }

    @Override
    public void destroy() {
        KEY.detachFromHost(mWindowAndroid.getUnownedUserDataHost());
        mCurrentTabObserver.destroy();
        if (mFusebox != null) {
            mFusebox.destroy();
        }
        for (FuseboxSessionState fuseboxSessionState : mTaskSessionMap.values()) {
            fuseboxSessionState.destroy();
        }
        mTaskSessionMap.clear();
        mFuseboxDataProvider.destroy();
    }

    @NativeMethods
    public interface Natives {
        @Nullable
        @JniType("std::string")
        String getTaskIdForTab(@JniType("content::WebContents*") WebContents webContents);

        boolean isContextualTasksUrl(@JniType("GURL") GURL url);
    }
}
