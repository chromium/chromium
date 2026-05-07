// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxDataProvider;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxManager;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksSessionState;
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
        var session = setTaskContext(taskId, webContents);

        // Manual Activation to pre-initialize native controllers.
        session.activate(mActivity, webContents, mProfileSupplier, /* onFullyActivated= */ null);

        // UI is now fully functional, so show it.
        ensureFuseboxInitialized();
        setFuseboxVisible(true);
        if (mFusebox != null) {
            mFusebox.beginInput();
        }
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
                clearTaskContext();
                setFuseboxVisible(false);
                if (mFusebox != null) {
                    mFusebox.endInput();
                }
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
     * Set the task context for the fusebox. Ensures the session exists and the data provider is
     * correctly configured.
     *
     * @param taskId The ID of the task.
     * @param webContents The WebContents of the contextual tasks WebUI.
     * @return The {@link FuseboxSessionState} for the task.
     */
    private FuseboxSessionState setTaskContext(String taskId, WebContents webContents) {
        var session = mTaskSessionMap.computeIfAbsent(taskId, k -> createSessionState());
        mFuseboxDataProvider.setFuseboxSessionState(session);
        mFuseboxDataProvider.setWebContents(webContents);
        return session;
    }

    /** Clears the task context from the data provider. */
    private void clearTaskContext() {
        mFuseboxDataProvider.setFuseboxSessionState(null);
        mFuseboxDataProvider.setWebContents(null);
    }

    @VisibleForTesting
    FuseboxSessionState createSessionState() {
        return new ContextualTasksSessionState();
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
        return ContextualTasksBridge.getTaskIdForTab(tab);
    }

    /**
     * Central method to configure the correct session state after an event. Used by both full tab
     * mode and bottom sheet / side panel mode. Normally called after tab switch / navigation events
     * upon which it will 1. Find the task ID associated with the tab (regardless of full tab or
     * bottom sheet mode). 2. Updates the LocationBarDataProvider with the correct
     * FuseboxSessionState and WebContents. 3. Updates visibility of the full tab fusebox, i.e.
     * visible if AIM page is showing, hidden otherwise. Hidden for bottom sheet mode as well since
     * bottom sheet has its own fusebox.
     */
    @VisibleForTesting
    void updateFuseboxVisibility(@Nullable Tab currentTab) {
        String taskId = getTaskIdForTab(currentTab);

        // Check if the WebUI is already ready for this task.
        var session = !TextUtils.isEmpty(taskId) ? mTaskSessionMap.get(taskId) : null;
        WebContents contextualTasksWebContents =
                (session != null) ? session.getContextualTasksWebContents() : null;

        if (!TextUtils.isEmpty(taskId) && contextualTasksWebContents != null) {
            // 1. Plumbing Sync: Connection between fusebox and WebUI.
            // This ensures follow-up queries from browsing contexts are sent to the AI bridge.
            // Applies to both full tab and bottom sheet.
            setTaskContext(taskId, contextualTasksWebContents);

            // 2. Visibility Sync: Show the physical fusebox UI only on full tab AIM page.
            boolean isOnAimPage = currentTab != null && isContextualTasksUrl(currentTab.getUrl());
            setFuseboxVisible(isOnAimPage);

            // 3. Input State: Focus fusebox if on the AIM page.
            if (isOnAimPage) {
                ensureFuseboxInitialized();
            } else {
                if (mFusebox != null) {
                    mFusebox.endInput();
                }
            }
        } else {
            // No task or WebUI not ready yet.
            clearTaskContext();
            setFuseboxVisible(false);
            if (mFusebox != null) {
                mFusebox.endInput();
            }
        }
    }

    private boolean isContextualTasksUrl(GURL url) {
        return ContextualTasksBridge.isContextualTasksUrl(url);
    }

    private void setFuseboxVisible(boolean visible) {
        if (mFusebox == null) return;
        mFusebox.getFuseboxView().setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setFuseboxForTesting(ContextualTasksFusebox fusebox) {
        mFusebox = fusebox;
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
}
