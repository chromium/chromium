// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarStateProvider;

/**
 * An observer class that listens for changes in UI components that are attached to the bottom of
 * the screen, bordering the OS navigation bar. This class then aggregates that information and
 * notifies its own observers of properties of the UI currently bordering ("attached to") the
 * navigation bar.
 */
public class BottomAttachedUiObserver
        implements BrowserControlsStateProvider.Observer, SnackbarStateProvider.Observer {
    /**
     * An observer to be notified of changes to what kind of UI is currently bordering the bottom of
     * the screen.
     */
    public interface Observer {
        void onBottomAttachedColorChanged(@Nullable @ColorInt Integer color);
    }

    private final ObserverList<Observer> mObservers;
    private @Nullable @ColorInt Integer mBottomAttachedColor;

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private int mBottomControlsHeight;
    private @Nullable @ColorInt Integer mBottomControlsColor;
    private boolean mBottomControlsAreVisible;

    private final SnackbarStateProvider mSnackbarStateProvider;
    private @Nullable @ColorInt Integer mSnackbarColor;
    private boolean mSnackbarVisible;

    /**
     * Build the observer that listens to changes in the UI bordering the bottom.
     *
     * @param browserControlsStateProvider Supplies a {@link BrowserControlsStateProvider} for the
     *     browser controls.
     * @param snackbarStateProvider Supplies a {@link SnackbarStateProvider} to watch for snackbars
     *     being shown.
     */
    public BottomAttachedUiObserver(
            BrowserControlsStateProvider browserControlsStateProvider,
            SnackbarStateProvider snackbarStateProvider) {
        mObservers = new ObserverList<>();

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);

        mSnackbarStateProvider = snackbarStateProvider;
        mSnackbarStateProvider.addObserver(this);
    }

    /**
     * @param observer The observer to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    public void destroy() {
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
        if (mSnackbarStateProvider != null) {
            mSnackbarStateProvider.removeObserver(this);
        }
    }

    private void updateBottomAttachedColor() {
        @Nullable
        @ColorInt
        Integer bottomAttachedColor = calculateBottomAttachedColor();
        if (mBottomAttachedColor == null && bottomAttachedColor == null) {
            return;
        }
        if (mBottomAttachedColor != null && mBottomAttachedColor.equals(bottomAttachedColor)) {
            return;
        }
        mBottomAttachedColor = bottomAttachedColor;
        for (Observer observer : mObservers) {
            observer.onBottomAttachedColorChanged(mBottomAttachedColor);
        }
    }

    private @Nullable @ColorInt Integer calculateBottomAttachedColor() {
        if (mBottomControlsAreVisible) {
            return mBottomControlsColor;
        }
        if (mSnackbarVisible) {
            return mSnackbarColor;
        }
        return null;
    }

    // Browser Controls (Tab group UI, Read Aloud)

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate) {
        updateBrowserControlsVisibility(
                // MiniPlayerMediator#shrinkBottomControls() sets the height to 1 and minHeight to 0
                // when hiding, instead of setting the height to 0.
                // TODO(b/320750931): Clean up once the MiniPlayerMediator has been improved.
                mBottomControlsHeight > 1 && bottomOffset < mBottomControlsHeight);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        mBottomControlsHeight = bottomControlsHeight;
        // MiniPlayerMediator#shrinkBottomControls() sets the height to 1 and minHeight to 0 when
        // hiding, instead of setting the height to 0.
        // TODO(b/320750931): Clean up once the MiniPlayerMediator has been improved.
        updateBrowserControlsVisibility(bottomControlsHeight > 1);
    }

    @Override
    public void onBottomControlsBackgroundColorChanged(@ColorInt int color) {
        mBottomControlsColor = color;
        updateBottomAttachedColor();
    }

    private void updateBrowserControlsVisibility(boolean bottomControlsAreVisible) {
        if (bottomControlsAreVisible == mBottomControlsAreVisible) {
            return;
        }
        mBottomControlsAreVisible = bottomControlsAreVisible;
        updateBottomAttachedColor();
    }

    // Snackbar

    @Override
    public void onSnackbarStateChanged(boolean isShowing, Integer color) {
        mSnackbarVisible = isShowing;
        mSnackbarColor = color;
        updateBottomAttachedColor();
    }
}
