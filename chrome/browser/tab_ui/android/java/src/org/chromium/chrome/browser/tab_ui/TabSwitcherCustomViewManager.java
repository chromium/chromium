// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** A class that supplies custom view to TabSwitcher from other non tab switcher clients. */
public class TabSwitcherCustomViewManager {
    /**
     * An interface for tab switcher, via which it can listen for signals concerning
     * addition and removal of custom views.
     */
    public interface Delegate {
        /**
         * This is fired when a client has requested a view to be shown.
         *
         * @param customView        The {@link View} that is requested to be added.
         * @param backPressRunnable A {@link Runnable} which can be supplied if clients also wish to
         *                          handle back presses while the custom view is shown. A null
         *                          value can be passed to
         *                          not intercept back presses.
         * @param clearTabList      A boolean to indicate whether we should clear the tab list when
         *                          showing the custom view.
         */
        void addCustomView(
                @NonNull View customView,
                @Nullable Runnable backPressRunnable,
                boolean clearTabList);

        /**
         * This is fired when the same client has made the view unavailable for it to be shown
         * any longer.
         *
         * @param customView The {@link View} that is requested to be removed.
         */
        void removeCustomView(@NonNull View customView);
    }

    // The {@link Delegate} that relays the events concerning the availability of the
    // custom view.
    private @Nullable Delegate mDelegate;

    private @Nullable View mCustomView;
    private @Nullable Runnable mBackPressRunnable;
    private boolean mClearTabList;

    // Whether a request to show custom view is in-flight.
    private boolean mIsCustomViewRequested;

    /**
     * @param delegate The {@link Delegate} that is responsible for relaying signals from clients to
     *     tab switcher, may be null if reset.
     */
    public void setDelegate(@NonNull Delegate delegate) {
        assert mDelegate == null || delegate == null;
        unbindDelegate(mDelegate);
        bindDelegate(delegate);
        mDelegate = delegate;
    }

    /**
     * A method to request showing a custom view.
     *
     * @param customView        The {@link View} that is being requested by the client to be shown.
     * @param backPressRunnable A {@link Runnable} which can be supplied if clients also wish to
     *                          handle back presses while the custom view is shown. A null value
     *                          can be passed to not
     *                          intercept back presses.
     * @param clearTabList      A boolean to indicate whether we should clear the tab list when
     *                          showing the custom view.
     *
     * @return true, if the request to show custom view was relayed successfully, false otherwise.
     */
    public boolean requestView(
            @NonNull View customView, @Nullable Runnable backPressRunnable, boolean clearTabList) {
        if (mIsCustomViewRequested) {
            assert false : "Previous request view is in-flight.";
            // assert statements are removed in release builds.
            return false;
        }
        mIsCustomViewRequested = true;
        mCustomView = customView;
        mBackPressRunnable = backPressRunnable;
        mClearTabList = clearTabList;
        bindDelegate(mDelegate);
        return true;
    }

    /**
     * A method to release the custom view from showing.
     *
     * @return true, if the request to release custom view was relayed successfully, false
     *         otherwise.
     */
    public boolean releaseView() {
        if (!mIsCustomViewRequested) {
            assert false : "Can't release a view without requesting it before.";
            return false;
        }
        mIsCustomViewRequested = false;
        unbindDelegate(mDelegate);
        mCustomView = null;
        mBackPressRunnable = null;
        mClearTabList = false;
        return true;
    }

    private void bindDelegate(@Nullable Delegate delegate) {
        if (delegate == null || mCustomView == null) return;

        delegate.addCustomView(mCustomView, mBackPressRunnable, mClearTabList);
    }

    private void unbindDelegate(@Nullable Delegate delegate) {
        if (delegate == null || mCustomView == null) return;

        delegate.removeCustomView(mCustomView);
    }
}
