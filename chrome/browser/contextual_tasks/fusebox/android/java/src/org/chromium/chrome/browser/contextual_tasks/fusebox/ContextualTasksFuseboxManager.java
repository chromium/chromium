// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.fusebox;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.CallbackUtils;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
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

/**
 * Manages the lifecycle and visibility of a single {@link ContextualTasksFusebox} instance overlaid
 * on regular tabs.
 */
@NullMarked
public class ContextualTasksFuseboxManager {
    private static final UnownedUserDataKey<ContextualTasksFuseboxManager> KEY =
            new UnownedUserDataKey<>();

    private final Activity mActivity;
    private final Supplier<ContextualTasksFusebox.ContextualTasksFuseboxConfig>
            mFuseboxConfigSupplier;
    private final WindowAndroid mWindowAndroid;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;
    private final CurrentTabObserver mCurrentTabObserver;

    // The fusebox instance. Shared across all tabs. Lazily initialized.
    private @Nullable ContextualTasksFusebox mFusebox;
    private final ContextualTasksFuseboxDataProvider mFuseboxDataProvider;
    private final Map<String, FuseboxSessionState> mTaskSessionMap = new HashMap<>();

    public ContextualTasksFuseboxManager(
            Activity activity,
            Supplier<ContextualTasksFusebox.ContextualTasksFuseboxConfig> fuseboxConfigSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<@Nullable SnackbarManager> snackbarManagerSupplier) {
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
     * Helper method to retrieve the {@link ContextualTasksFuseboxManager} instance from a given
     * {@link WindowAndroid}.
     *
     * @param windowAndroid The window to retrieve the manager from.
     * @return The manager for the given window, or null if none exists.
     */
    public static @Nullable ContextualTasksFuseboxManager from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Called to initialize the {@ link FuseboxSessionState} associated with a task. Ignored if the
     * task is already associated with a {@link FuseboxSessionState}.
     *
     * @param taskId The ID of the task.
     * @param webContents The WebContents of the contextual tasks WebUI associated with the fusebox.
     */
    public void ensureFuseboxSessionState(String taskId, @Nullable WebContents webContents) {
        FuseboxSessionState fuseboxSessionState = mTaskSessionMap.get(taskId);
        if (fuseboxSessionState == null) {
            fuseboxSessionState = new FuseboxSessionState(webContents);
            mTaskSessionMap.put(taskId, fuseboxSessionState);
        }
        mFuseboxDataProvider.setFuseboxSessionState(fuseboxSessionState);
    }

    /** Returns the {@link ContextualTasksFuseboxDataProvider}. One per activity. */
    public ContextualTasksFuseboxDataProvider getFuseboxDataProvider() {
        return mFuseboxDataProvider;
    }

    private @Nullable String getTaskIdForTab(@Nullable Tab tab) {
        if (tab == null || tab.getUrl().isEmpty()) return null;
        return null;
    }

    private void updateFuseboxVisibility(@Nullable Tab currentTab) {
        String taskId = getTaskIdForTab(currentTab);
        if (currentTab != null
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
        if (url == null || url.isEmpty() || !url.isValid()) return false;
        // TODO(crbug.com/491504815): Do an exact check, or better yet call native.
        return url.getSpec().startsWith("chrome://contextual-tasks");
    }

    private void setFuseboxVisible(boolean visible) {
        if (mFusebox == null) return;
        // TODO(crbug.com/491504815): Create a new fusebox every time, and pass WebContents info via
        // the LocationBarDataProvider.
        mFusebox.getFuseboxView().setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    private void ensureFuseboxInitialized() {
        if (mFusebox != null) return;

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
                        SupplierUtils.asNonNull(mSnackbarManagerSupplier).get(),
                        mFuseboxDataProvider);
    }

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
