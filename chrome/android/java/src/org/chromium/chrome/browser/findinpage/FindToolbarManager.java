// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.view.ActionMode;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.WindowAndroid;

/** Manages the interactions with the find toolbar. */
@NullMarked
public class FindToolbarManager {
    private @Nullable FindToolbar mFindToolbar;
    private final ViewStub mFindToolbarStub;
    private final TabModelSelector mTabModelSelector;
    private final WindowAndroid mWindowAndroid;
    private final ActionMode.Callback mCallback;
    private final ObserverList<FindToolbarObserver> mObservers;
    private final BackPressManager mBackPressManager;
    private final FrameLayout mSecondaryUiContainer;
    private final @Nullable View mAnchorView;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private @Nullable SideUiStateProvider mSideUiStateProvider;

    /**
     * Whether observers have been notified that the find toolbar is shown (and not yet notified
     * that it is hidden). This mirrors exactly what {@link FindToolbarObserver}s have been told, so
     * that observers registering late can be given the same notifications as the ones that
     * registered earlier. This is intentionally not {@link #isShowing()}, which is based on the
     * {@link View} visibility and can disagree with the notified state (e.g. while the tablet find
     * toolbar plays its exit animation the view is still visible although observers have already
     * been told that the toolbar is hidden).
     */
    private boolean mNotifiedShown;

    /**
     * Creates an instance of a {@link FindToolbarManager}.
     *
     * @param findToolbarStub The {@link ViewStub} where for the find toolbar.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param windowAndroid The {@link WindowAndroid} for the containing activity.
     * @param callback The ActionMode.Callback that will be used when selection occurs on the {@link
     *     FindToolbar}.
     * @param backPressManager The {@link BackPressManager} for intercepting back press.
     * @param secondaryUiContainer The {@link FrameLayout} that will hold the {@link FindResultBar}.
     * @param anchorView The {@link View} below which the find toolbar and result bar are
     *     positioned.
     * @param browserControlsStateProvider Provider for browser controls state.
     * @param sideUiStateProviderSupplier Supplier for {@link SideUiStateProvider}.
     */
    public FindToolbarManager(
            ViewStub findToolbarStub,
            TabModelSelector tabModelSelector,
            WindowAndroid windowAndroid,
            ActionMode.Callback callback,
            BackPressManager backPressManager,
            FrameLayout secondaryUiContainer,
            @Nullable View anchorView,
            BrowserControlsStateProvider browserControlsStateProvider,
            @Nullable OneshotSupplier<SideUiStateProvider> sideUiStateProviderSupplier) {
        mFindToolbarStub = findToolbarStub;
        mTabModelSelector = tabModelSelector;
        mWindowAndroid = windowAndroid;
        mCallback = callback;
        mBackPressManager = backPressManager;
        mSecondaryUiContainer = secondaryUiContainer;
        mAnchorView = anchorView;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mObservers = new ObserverList<>();
        if (sideUiStateProviderSupplier != null) {
            sideUiStateProviderSupplier.runSyncOrOnAvailable(this::setSideUiStateProvider);
        }
    }

    /**
     * @return Whether the find toolbar is currently showing.
     */
    public boolean isShowing() {
        return mFindToolbar != null && mFindToolbar.getVisibility() == View.VISIBLE;
    }

    /** Hides the toolbar and clears the selection on the screen. */
    public void hideToolbar() {
        hideToolbar(true);
    }

    /**
     * Hides the toolbar.
     * @param clearSelection Whether the selection on the page should be cleared.
     */
    public void hideToolbar(boolean clearSelection) {
        if (mFindToolbar == null) return;

        mFindToolbar.deactivate(clearSelection);
    }

    /**
     * Shows the toolbar if it's not already visible otherwise activates.
     *
     * TODO(crrev.com/959841): Return a boolean for whether the toolbar was actually shown.
     */
    public void showToolbar() {
        if (mFindToolbar == null) {
            mFindToolbar = (FindToolbar) mFindToolbarStub.inflate();
            mFindToolbar.setTabModelSelector(mTabModelSelector);
            mFindToolbar.setWindowAndroid(mWindowAndroid);
            mFindToolbar.setActionModeCallbackForTextEdit(mCallback);
            mFindToolbar.setSecondaryUiContainer(mSecondaryUiContainer);
            mFindToolbar.setAnchorView(mAnchorView);
            mFindToolbar.setBrowserControlsStateProvider(mBrowserControlsStateProvider);
            mFindToolbar.setSideUiStateProvider(mSideUiStateProvider);
            mFindToolbar.setObserver(
                    new FindToolbarObserver() {
                        @Override
                        public void onFindToolbarShown() {
                            mNotifiedShown = true;
                            for (FindToolbarObserver observer : mObservers) {
                                observer.onFindToolbarShown();
                            }
                        }

                        @Override
                        public void onFindToolbarHidden() {
                            mNotifiedShown = false;
                            for (FindToolbarObserver observer : mObservers) {
                                observer.onFindToolbarHidden();
                            }
                        }
                    });
        }
        if (mBackPressManager != null) {
            if (mBackPressManager.has(BackPressHandler.Type.FIND_TOOLBAR)) {
                mBackPressManager.removeHandler(BackPressHandler.Type.FIND_TOOLBAR);
            }
            mBackPressManager.addHandler(mFindToolbar, BackPressHandler.Type.FIND_TOOLBAR);
        }
        mFindToolbar.activate();
    }

    /**
     * Sets the {@link SideUiStateProvider} to observe side UI changes.
     *
     * @param sideUiStateProvider The {@link SideUiStateProvider} object.
     */
    @VisibleForTesting
    void setSideUiStateProvider(@Nullable SideUiStateProvider sideUiStateProvider) {
        mSideUiStateProvider = sideUiStateProvider;
        if (mFindToolbar != null) {
            mFindToolbar.setSideUiStateProvider(mSideUiStateProvider);
        }
    }

    /** Destroys the {@link FindToolbarManager} and cleans up observers. */
    public void destroy() {
        if (mFindToolbar != null) {
            mFindToolbar.destroy();
            mFindToolbar = null;
        }
        mNotifiedShown = false;
        mObservers.clear();
        mSideUiStateProvider = null;
    }

    /** Sets the find query text string. */
    public void setFindQuery(String findText) {
        assert mFindToolbar != null;
        mFindToolbar.setFindQuery(findText);
    }

    /**
     * Adds an observer for find in page changes.
     *
     * <p>If the find toolbar is already shown, the observer is immediately (and synchronously) told
     * about it, so that observers which subscribe after the toolbar was shown do not miss the
     * event. Only the newly added observer is notified.
     */
    public void addObserver(FindToolbarObserver observer) {
        if (mObservers.addObserver(observer) && mNotifiedShown) {
            observer.onFindToolbarShown();
        }
    }

    /** Remove an observer for find in page changes. */
    public void removeObserver(FindToolbarObserver observer) {
        mObservers.removeObserver(observer);
    }
}
