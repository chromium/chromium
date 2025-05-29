// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.util.SparseArray;
import android.view.View;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/**
 * Button visibility checker using the rule set based on priority. The rule checker works only when
 * {@link ChromeFeatureList#sCctToolbarRefactor} is disabled.
 */
@NullMarked
public class ButtonVisibilityRule {

    /** ID of the buttons from the highest priority (CLOSE) to lowest. */
    public @interface ButtonId {
        int CLOSE = 0;
        int MENU = 1;
        int SECURITY = 2;
        int CUSTOM_1 = 3;
        int CUSTOM_2 = 4;
        int MINIMIZE = 5;
        int EXPAND = 6;
        int MTB = 7;

        int MAX_ID = 7;
    }

    private final SparseArray<Button> mButtons = new SparseArray<>(ButtonId.MAX_ID + 1);

    // Minimum width of URL/Title bar in px.
    private final int mMinUrlWidthPx;

    // True if the visibility rule is activated. Otherwise the whole operation is
    // no-op, meaning the rule set will be enforced by some other logic.
    private final boolean mActivated;

    // The current toolbar width. It can change over time, for instance, due to
    // device rotation or the window width adjustment in multi-window mode.
    private int mToolbarWidth;

    static class Button {
        private final View mView;
        private boolean mVisible;

        // True if the button view is suppressed to hidden state by this rule checker.
        // Only the buttons suppressed get turned on later again.
        private boolean mSuppressed;

        Button(View view, boolean visible) {
            mView = view;
            mVisible = visible;
        }
    }

    /**
     * Constructor.
     *
     * @param minUrlWidthPx Minimum width of URL/Title bar in px.
     * @param activated {@code true} if the visibility rule checker will be in operation.
     */
    public ButtonVisibilityRule(@Px int minUrlWidthPx, boolean activated) {
        mMinUrlWidthPx = minUrlWidthPx;
        mActivated = activated;
    }

    /**
     * Set the new toolbar width and apply the update.
     *
     * @param width The updated width of the toolbar.
     */
    public void setToolbarWidth(int width) {
        int oldWidth = mToolbarWidth;
        mToolbarWidth = width;
        if (width == 0 || oldWidth == width) return;

        if (oldWidth == 0 || width < oldWidth) {
            refresh();
        } else {
            refreshOnExpandedToolbar();
        }
    }

    /**
     * Add a button that will be counted in for refreshing the overall visibility.
     *
     * @param index Index of the button.
     * @param view {@link View} of the button to which the visibility is applied.
     * @param visible {@code true} if the button is to be visible.
     */
    public void addButton(int index, View view, boolean visible) {
        if (!mActivated) return;

        mButtons.put(index, new Button(view, visible));
        if (mToolbarWidth > 0 && visible) refresh();
    }

    /**
     * Update individual button visibility and apply the update.
     *
     * @param index Index of the button.
     * @param visible {@code true} if the button is to be visible.
     */
    public void update(int index, boolean visible) {
        Button button = mButtons.get(index);
        if (button == null || visible == button.mVisible) return;

        button.mVisible = visible;
        if (mToolbarWidth == 0) return;

        if (visible) {
            refresh();
        } else {
            refreshOnExpandedToolbar();
        }
    }

    /** Refresh visibility of buttons with the state updated so far. */
    public void refresh() {
        if (!mActivated) return;

        int urlBarWidth = getUrlBarWidth();
        // Loop through visible buttons, and see if the title/url bar width can stay above 68dp.
        // If not, turn off the visibility of buttons from the lowest to highest.
        @ButtonId int buttonToHide = ButtonId.MAX_ID;
        while (urlBarWidth < mMinUrlWidthPx && buttonToHide >= 0) {
            Button button = mButtons.get(buttonToHide--);
            if (button == null || !button.mVisible) continue;

            button.mVisible = false;
            button.mView.setVisibility(View.GONE);
            button.mSuppressed = true;
            urlBarWidth = getUrlBarWidth();
        }
        assert urlBarWidth >= mMinUrlWidthPx || isAllButtonHidden()
                : "There is not enough space for URL bar!!!!";
    }

    /**
     * Refresh the visibility when the toolbar width expands. Make some of the suppressed buttons
     * visible again if the width allows.
     */
    private void refreshOnExpandedToolbar() {
        if (!mActivated) return;

        int urlBarWidth = getUrlBarWidth();
        @ButtonId int buttonToShow = 0;
        while (urlBarWidth > mMinUrlWidthPx && buttonToShow <= ButtonId.MAX_ID) {
            Button button = mButtons.get(buttonToShow++);
            if (button == null || button.mVisible || !button.mSuppressed) continue;

            button.mVisible = true;
            urlBarWidth = getUrlBarWidth();
            if (urlBarWidth < mMinUrlWidthPx) {
                // Turning on this button makes it fail to meet the url bar minimum width
                // requirement. Turn it off again and exit.
                button.mVisible = false;
                break;
            } else {
                button.mView.setVisibility(View.VISIBLE);
                button.mSuppressed = false;
            }
        }
    }

    // Returns the current URL/title bar width, which is the toolbar width minus the total
    // width occupied by the visible buttons.
    private int getUrlBarWidth() {
        int buttonsWidth = 0;
        for (@ButtonId int i = 0; i <= ButtonId.MAX_ID; ++i) {
            Button button = mButtons.get(i);
            if (button == null || !button.mVisible) continue;
            assert button.mView.getLayoutParams().width > 0
                    : "Button#LayoutParams must contain the actual width.";
            buttonsWidth += button.mView.getLayoutParams().width;
        }
        return mToolbarWidth - buttonsWidth;
    }

    private boolean isAllButtonHidden() {
        for (@ButtonId int i = 0; i <= ButtonId.MAX_ID; ++i) {
            Button button = mButtons.get(i);
            if (button != null && button.mVisible) return false;
        }
        return true;
    }
}
