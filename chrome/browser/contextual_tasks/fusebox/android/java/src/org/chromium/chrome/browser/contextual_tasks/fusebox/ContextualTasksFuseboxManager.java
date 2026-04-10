// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.fusebox;

import android.app.Activity;
import android.view.View;

import org.chromium.base.CallbackUtils;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

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
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final CurrentTabObserver mCurrentTabObserver;

    // The fusebox instance. Shared across all tabs. Lazily initialized.
    private @Nullable ContextualTasksFusebox mFusebox;

    public ContextualTasksFuseboxManager(
            Activity activity,
            Supplier<ContextualTasksFusebox.ContextualTasksFuseboxConfig> fuseboxConfigSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<SnackbarManager> snackbarManagerSupplier) {
        mActivity = activity;
        mFuseboxConfigSupplier = fuseboxConfigSupplier;
        mWindowAndroid = windowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;
        mProfileSupplier = profileSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;

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

    private void updateFuseboxVisibility(@Nullable Tab currentTab) {
        if (currentTab != null && isContextualTasksUrl(currentTab.getUrl())) {
            ensureFuseboxInitialized();
            setFuseboxVisible(true);
        } else {
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
                        mSnackbarManagerSupplier.get());
    }

    public void destroy() {
        KEY.detachFromHost(mWindowAndroid.getUnownedUserDataHost());
        mCurrentTabObserver.destroy();
        if (mFusebox != null) {
            mFusebox.destroy();
        }
    }
}
