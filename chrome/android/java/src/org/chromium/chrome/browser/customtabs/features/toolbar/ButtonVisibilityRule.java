// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.util.SparseArray;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CustomTabsButtonState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Button visibility checker using the rule set based on priority. The rule checker works only when
 * {@link ChromeFeatureList#sCctToolbarRefactor} is disabled.
 */
@NullMarked
public class ButtonVisibilityRule {

    @IntDef({
        ButtonId.CLOSE,
        ButtonId.MENU,
        ButtonId.SECURITY,
        ButtonId.CUSTOM_1,
        ButtonId.CUSTOM_2,
        ButtonId.MINIMIZE,
        ButtonId.EXPAND,
        ButtonId.MTB,
        ButtonId.MAX_ID
    })
    @Retention(RetentionPolicy.SOURCE)
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

    @CustomTabsButtonState int mShareState;
    @CustomTabsButtonState int mOpenInBrowserState;

    // The current toolbar width. It can change over time, for instance, due to
    // device rotation or the window width adjustment in multi-window mode.
    private int mToolbarWidth;

    // Adjust minimum Title/URL bar width to have the optional button hidden/visible.
    // Used for Q/A testing, enabled only through feature flag.
    private boolean mHidingOptionalButton;

    static class Button {
        private final View mView;
        // Type of the button. Valid for CUSTOM_1 or CUSTOM_2 only; the rest of the buttons always
        // have ButtonType.OTHER.
        private final @ButtonType int mCustomType;

        private final @Nullable Callback<Boolean> mUpdateCallback;

        // Visibility of the button. It can be hidden either because there is no space (handled by
        // this class) or because of outside factors.
        private boolean mVisible;

        // True if the button view is suppressed to hidden state by this rule checker.
        // Only the buttons suppressed get turned on later again.
        private boolean mSuppressed;

        Button(
                View view,
                boolean visible,
                @ButtonType int customType,
                @Nullable Callback<Boolean> callback) {
            mUpdateCallback = callback;
            mView = view;
            mVisible = visible;
            mCustomType = customType;
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
     * Set the state (default/on/off) of Share/Open-in-Browser custom button. These states are used
     * to determine the relative priority of the custom action button and the minimize button.
     *
     * @param shareState State of Share action.
     * @param openInBrowserState State of Open-in-browser action.
     */
    public void setCustomButtonState(
            @CustomTabsButtonState int shareState, @CustomTabsButtonState int openInBrowserState) {
        mShareState = shareState;
        mOpenInBrowserState = openInBrowserState;
    }

    /**
     * Set the new toolbar width and apply the update.
     *
     * @param width The updated width of the toolbar.
     */
    public void setToolbarWidth(int width) {
        if (mHidingOptionalButton) return;
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
        addButtonInternal(index, view, visible, ButtonType.OTHER, null);
    }

    /**
     * Add a button for custom action with its button type.
     *
     * @param index Index of the button.
     * @param view {@link View} of the button to which the visibility is applied.
     * @param visible {@code true} if the button is to be visible.
     * @param customType Button type if this is custom action (CUSTOM_1 or CUSTOM_2). Otherwise
     *     {@code ButtonType.OTHER}.
     */
    public void addButtonForCustomAction(
            int index, View view, boolean visible, @ButtonType int customType) {
        addButtonInternal(index, view, visible, customType, null);
    }

    /**
     * Add a button with a callback to be invoked when the visibility changes.
     *
     * @param index Index of the button.
     * @param view {@link View} of the button to which the visibility is applied.
     * @param visible {@code true} if the button is to be visible.
     * @param callback {@link Callback} to be invoked when the visibility changes when the rule set
     *     is applied.
     */
    public void addButtonWithCallback(
            int index, View view, boolean visible, @Nullable Callback<Boolean> callback) {
        addButtonInternal(index, view, visible, ButtonType.OTHER, callback);
    }

    private static int getViewWidth(View view) {
        int width = view.getLayoutParams().width;
        return width > 0 ? width : view.getMeasuredWidth();
    }

    private void addButtonInternal(
            int index,
            View view,
            boolean visible,
            @ButtonType int customType,
            @Nullable Callback<Boolean> callback) {
        if (!mActivated) return;

        mButtons.put(index, new Button(view, visible, customType, callback));
        if (mToolbarWidth > 0 && visible) {
            if (getViewWidth(view) > 0) {
                refresh();
            } else {
                view.getViewTreeObserver()
                        .addOnPreDrawListener(
                                new ViewTreeObserver.OnPreDrawListener() {
                                    @Override
                                    public boolean onPreDraw() {
                                        view.getViewTreeObserver().removeOnPreDrawListener(this);
                                        refresh();
                                        return true;
                                    }
                                });
            }
        }
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

    /**
     * Removes a button from the visibility rule. The button will be treated as non-existent.
     *
     * @param index Index of the button to remove.
     */
    public void removeButton(@ButtonId int index) {
        if (!mActivated) return;

        // This is safe because mButtons is a SparseArray, and we always check whether .get()
        // results are null.
        mButtons.remove(index);
        // No refresh call is needed. This is equivalent to addButton, and we only refresh when the
        // button is visible. When we are removing a button, it's not visible.
    }

    /**
     * Return {@code true} if the given button was suppressed (hidden) by this rule checker.
     *
     * @param index Index of the button.
     */
    public boolean isSuppressed(int index) {
        Button button = mButtons.get(index);
        return button != null && !button.mVisible && button.mSuppressed;
    }

    /** Refresh visibility of buttons with the state updated so far. */
    public void refresh() {
        if (!mActivated || !allViewsHaveSize()) return;

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
            if (button.mUpdateCallback != null) button.mUpdateCallback.onResult(false);
            urlBarWidth = getUrlBarWidth();
        }
        adjustMinimizeButtonPriority();
        assert urlBarWidth >= mMinUrlWidthPx || isAllButtonHidden()
                : "There is not enough space for URL bar!!!!";
    }

    private boolean allViewsHaveSize() {
        for (@ButtonId int i = 0; i <= ButtonId.MAX_ID; ++i) {
            Button button = mButtons.get(i);
            if (button == null || !button.mVisible) continue;
            if (getViewWidth(button.mView) == 0) return false;
        }
        return true;
    }

    private void adjustMinimizeButtonPriority() {
        // Chrome-created custom buttons (Share, Open-in-Chrome) of state DEFAULT has a priority
        // lower than minimize button i.e. MINIMIZE > SHARE > OPEN-IN-CHROME > EXPAND. If MINIMIZE
        // was suppressed hidden and either SHARE or OPEN-IN-CHROME is visible, flip their state.
        Button minimize = mButtons.get(ButtonId.MINIMIZE);
        if (minimize != null && !minimize.mVisible && minimize.mSuppressed) {
            if (maybeSuppressCustomButtonOfType(
                            ButtonType.CCT_OPEN_IN_BROWSER_BUTTON, mOpenInBrowserState)
                    || maybeSuppressCustomButtonOfType(ButtonType.CCT_SHARE_BUTTON, mShareState)) {
                minimize.mVisible = true;
                minimize.mSuppressed = false;
                minimize.mView.setVisibility(View.VISIBLE);
                if (minimize.mUpdateCallback != null) minimize.mUpdateCallback.onResult(true);
            }
        }
    }

    private boolean maybeSuppressCustomButtonOfType(@ButtonType int buttonType, int buttonState) {
        return maybeSuppressSingleCustomButton(ButtonId.CUSTOM_2, buttonType, buttonState)
                || maybeSuppressSingleCustomButton(ButtonId.CUSTOM_1, buttonType, buttonState);
    }

    private boolean maybeSuppressSingleCustomButton(
            @ButtonId int id, @ButtonType int buttonType, @CustomTabsButtonState int buttonState) {
        Button button = mButtons.get(id);
        if (button != null
                && button.mVisible
                && button.mCustomType == buttonType
                && buttonState == CustomTabsButtonState.BUTTON_STATE_DEFAULT) {
            button.mVisible = false;
            button.mSuppressed = true;
            button.mView.setVisibility(View.GONE);
            if (button.mUpdateCallback != null) button.mUpdateCallback.onResult(false);
            return true;
        }
        return false;
    }

    /**
     * Refresh the visibility when the toolbar width expands. Make some of the suppressed buttons
     * visible again if the width allows.
     */
    private void refreshOnExpandedToolbar() {
        if (!mActivated || !allViewsHaveSize()) return;

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
                if (button.mUpdateCallback != null) button.mUpdateCallback.onResult(true);
            }
        }
        adjustMinimizeButtonPriority();
    }

    // Returns the current URL/title bar width, which is the toolbar width minus the total
    // width occupied by the visible buttons.
    private int getUrlBarWidth() {
        int buttonsWidth = 0;
        for (@ButtonId int i = 0; i <= ButtonId.MAX_ID; ++i) {
            Button button = mButtons.get(i);
            if (button == null || !button.mVisible) continue;
            int width = getViewWidth(button.mView);
            assert width > 0 : "All views must have the size ready.";
            buttonsWidth += width;
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

    public void setHidingOptionalButton() {
        Button button = mButtons.get(ButtonId.MTB);
        if (button == null || (!button.mVisible && button.mSuppressed)) return;

        // Set the toolbar width smaller than url bar and all the button widths combined.
        int buttonsWidth = mToolbarWidth - getUrlBarWidth();
        setToolbarWidth(mMinUrlWidthPx + (buttonsWidth - button.mView.getLayoutParams().width / 2));
        mHidingOptionalButton = true;
    }
}
