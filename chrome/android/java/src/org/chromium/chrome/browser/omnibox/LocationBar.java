// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Container that holds the {@link UrlBar} and SSL state related with the current {@link Tab}.
 */
public interface LocationBar extends UrlBarDelegate, FakeboxDelegate {
    /** A means of tracking which mechanism is being used to focus the omnibox. */
    @IntDef({OmniboxFocusReason.OMNIBOX_TAP, OmniboxFocusReason.OMNIBOX_LONG_PRESS,
            OmniboxFocusReason.FAKE_BOX_TAP, OmniboxFocusReason.FAKE_BOX_LONG_PRESS,
            OmniboxFocusReason.ACCELERATOR_TAP, OmniboxFocusReason.TAB_SWITCHER_OMNIBOX_TAP,
            OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP,
            OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS,
            OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD, OmniboxFocusReason.SEARCH_QUERY,
            OmniboxFocusReason.LAUNCH_NEW_INCOGNITO_TAB, OmniboxFocusReason.MENU_OR_KEYBOARD_ACTION,
            OmniboxFocusReason.UNFOCUS, OmniboxFocusReason.QUERY_TILES_NTP_TAP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OmniboxFocusReason {
        int OMNIBOX_TAP = 0;
        int OMNIBOX_LONG_PRESS = 1;
        int FAKE_BOX_TAP = 2;
        int FAKE_BOX_LONG_PRESS = 3;
        int ACCELERATOR_TAP = 4;
        // TAB_SWITCHER_OMNIBOX_TAP has not been used anymore, keep it for record for now.
        int TAB_SWITCHER_OMNIBOX_TAP = 5;
        int TASKS_SURFACE_FAKE_BOX_TAP = 6;
        int TASKS_SURFACE_FAKE_BOX_LONG_PRESS = 7;
        int DEFAULT_WITH_HARDWARE_KEYBOARD = 8;
        int SEARCH_QUERY = 9;
        int LAUNCH_NEW_INCOGNITO_TAB = 10;
        int MENU_OR_KEYBOARD_ACTION = 11;
        int UNFOCUS = 12;
        int QUERY_TILES_NTP_TAP = 13;
        int NUM_ENTRIES = 14;
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    void destroy();

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
     */
    default void onDeferredStartup() {}

    /**
     * Handles native dependent initialization for this class.
     */
    void onNativeLibraryReady();

    /**
     * Triggered when the current tab has changed to a {@link NewTabPage}.
     */
    void onTabLoadingNTP(NewTabPage ntp);

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    void updateVisualsForState();

    /**
     * Updates progress of current the URL focus change animation.
     *
     * @param fraction 1.0 is 100% focused, 0 is completely unfocused.
     */
    void setUrlFocusChangeFraction(float fraction);

    /**
     * Sets the displayed URL to be the URL of the page currently showing.
     *
     * <p>The URL is converted to the most user friendly format (removing HTTP:// for example).
     *
     * <p>If the current tab is null, the URL text will be cleared.
     */
    void setUrlToPageUrl();

    /**
     * Sets the displayed title to the page title.
     */
    void setTitleToPageTitle();

    /**
     * Sets whether the location bar should have a layout showing a title.
     * @param showTitle Whether the title should be shown.
     */
    void setShowTitle(boolean showTitle);

    /**
     * Update the visuals based on a loading state change.
     * @param updateUrl Whether to update the URL as a result of the this call.
     */
    void updateLoadingState(boolean updateUrl);

    /**
     * Sets the {@link ToolbarDataProvider} to be used for accessing {@link Toolbar} state.
     */
    void setToolbarDataProvider(ToolbarDataProvider model);

    /**
     * Sets the {@link OverviewModeBehavior}.
     */
    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior);

    /**
     * Gets the {@link ToolbarDataProvider} to be used for accessing {@link Toolbar} state.
     */
    ToolbarDataProvider getToolbarDataProvider();

    /**
     * Initialize controls that will act as hooks to various functions.
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param activityTabProvider An {@link ActivityTabProvider} to access the activity's current
     *         tab.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} object.
     * @param shareDelegateSupplier A supplier for {@link ShareDelegate} object.
     * @param incognitoStateProvider An {@link IncognitoStateProvider} to access the current
     *         incognito state.
     */
    void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider);

    /**
     * Triggers the cursor to be visible in the UrlBar without triggering any of the focus animation
     * logic.
     * <p>
     * Only applies to devices with a hardware keyboard attached.
     */
    void showUrlBarCursorWithoutFocusAnimations();

    /**
     * Selects all of the editable text in the UrlBar.
     */
    void selectAll();

    /**
     * Reverts any pending edits of the location bar and reset to the page state. This does not
     * change the focus state of the location bar.
     */
    void revertChanges();

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    void updateStatusIcon();

    /**
     * @return The {@link ViewGroup} that this container holds.
     */
    View getContainerView();

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    View getSecurityIconView();

    /**
     * Updates the state of the mic button if there is one.
     */
    void updateMicButtonState();

    /**
     * Sets the callback to be used by default for text editing action bar.
     * @param callback The callback to use.
     */
    void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback);

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     *
     * Immediately after the animation to transition the URL bar from focused to unfocused finishes,
     * the layout width returned from #getMeasuredWidth() can differ from the final unfocused width
     * (e.g. this value) until the next layout pass is complete.
     *
     * This value may be used to determine whether optional child views should be visible in the
     * unfocused location bar.
     *
     * @param unfocusedWidth The unfocused location bar width.
     */
    void setUnfocusedWidth(int unfocusedWidth);

    /**
     * Sets the (observable) supplier of the active profile. This supplier will notify observers of
     * changes to the active profile, e.g. when selecting an incognito tab model.
     * @param profileSupplier The supplier of the active profile.
     */
    void setProfileSupplier(ObservableSupplier<Profile> profileSupplier);

    /**
     * Public methods of LocationBar exclusive to smaller devices.
     */
    interface Phone {
        /**
         * Returns width of child views before the first view that would be visible when location
         * bar is focused. The first visible, focused view should be either url bar or status icon.
         */
        int getOffsetOfFirstVisibleFocusedView();

        /**
         * Populates fade animators of status icon for location bar focus change animation.
         *
         * @param animators The target list to add animators to.
         * @param startDelayMs Start delay of fade animation in milliseconds.
         * @param durationMs Duration of fade animation in milliseconds.
         * @param targetAlpha Target alpha value.
         */
        void populateFadeAnimations(
                List<Animator> animators, long startDelayMs, long durationMs, float targetAlpha);

        /**
         * Calculates the offset required for the focused LocationBar to appear as it's still
         * unfocused so it can animate to a focused state.
         *
         * @param hasFocus True if the LocationBar has focus, this will be true between the focus
         *         animation starting and the unfocus animation starting.
         * @return The offset for the location bar when showing the DSE/loupe icon.
         */
        int getLocationBarOffsetForFocusAnimation(boolean hasFocus);

        /**
         * Function used to position the URL bar inside the location bar during omnibox animation.
         *
         * @param urlExpansionFraction The current expansion progress, 1 is fully focused and 0 is
         *         completely unfocused.
         * @param hasFocus True if the LocationBar has focus, this will be true between the focus
         *         animation starting and the unfocus animation starting.
         * @return The number of pixels of horizontal translation for the URL bar, used in the
         *         toolbar animation.
         */
        float getUrlBarTranslationXForToolbarAnimation(
                float urlExpansionFraction, boolean hasFocus);

        /**
         * Handles any actions to be performed after all other actions triggered by the URL focus
         * change. This will be called after any animations are performed to transition from one
         * focus state to the other.
         *
         * @param hasFocus Whether the URL field has gained focus.
         */
        void finishUrlFocusChange(boolean hasFocus);

        /**
         * Sets whether the url bar should be focusable.
         */
        void setUrlBarFocusable(boolean focusable);

        /**
         * Returns {@link FrameLayout.LayoutParams} of the LocationBar view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getLayoutParams()
         */
        FrameLayout.LayoutParams getFrameLayoutParams();

        /**
         * The opacity of the view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getAlpha()
         */
        float getAlpha();

        /**
         * Bottom position of this view relative to its parent.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getBottom()
         * @return The bottom of this view, in pixels.
         */
        int getBottom();

        /**
         * Returns the resolved layout direction for this view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getLayoutDirection()
         * @return {@link View#LAYOUT_DIRECTION_LTR}, or {@link View#LAYOUT_DIRECTION_RTL}.
         */
        int getLayoutDirection();

        /**
         * Returns the end padding of this view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getPaddingEnd()
         * @return The end padding in pixels.
         */
        int getPaddingEnd();

        /**
         * Returns the start padding of this view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getPaddingStart()
         * @return The start padding in pixels.
         */
        int getPaddingStart();

        /**
         * Top position of this view relative to its parent.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getTop()
         * @return The top of this view, in pixels.
         */
        int getTop();

        /**
         * The vertical location of this view relative to its top position, in pixels.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getTranslationY()
         */
        float getTranslationY();

        /**
         * Returns the visibility status for this view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getVisibility()
         */
        int getVisibility();

        /**
         * Returns true if this view has focus itself, or is the ancestor of the view that has
         * focus.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#hasFocus()
         */
        boolean hasFocus();

        /**
         * Invalidate the whole view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#invalidate()
         */
        void invalidate();

        /**
         * Sets the opacity of the view.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#setAlpha(float)
         */
        void setAlpha(float alpha);

        /**
         * Sets the padding.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#setPadding(int, int, int, int)
         */
        void setPadding(int left, int top, int right, int bottom);

        /**
         * Sets the horizontal location of this view relative to its left position.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#setTranslationX(float)
         */
        void setTranslationX(float translationX);

        /**
         * Sets the vertical location of this view relative to its top position.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#setTranslationY(float)
         */
        void setTranslationY(float translationY);

        /**
         * Returns the LocationBar view for use in drawing.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see ViewGroup#drawChild(Canvas, View, long)
         */
        View getViewForDrawing();
    }

    /**
     * Public methods of LocationBar exclusive to larger devices.
     */
    interface Tablet {
        /**
         * @param button The {@link View} of the button to hide.
         * @return An animator to run for the given view when hiding buttons in the unfocused
         *         location bar. This should also be used to create animators for hiding toolbar
         *         buttons.
         */
        ObjectAnimator createHideButtonAnimator(View button);

        /**
         * @param button The {@link View} of the button to show.
         * @return An animator to run for the given view when showing buttons in the unfocused
         *         location bar. This should also be used to create animators for showing toolbar
         *         buttons.
         */
        ObjectAnimator createShowButtonAnimator(View button);

        /**
         * Creates animators for hiding buttons in the unfocused location bar. The buttons fade out
         * while width of the location bar gets larger. There are toolbar buttons that also hide at
         * the same time, causing the width of the location bar to change.
         *
         * @param toolbarStartPaddingDifference The difference in the toolbar's start padding
         *         between the beginning and end of the animation.
         * @return A list of animators to run.
         */
        List<Animator> getHideButtonsWhenUnfocusedAnimators(int toolbarStartPaddingDifference);

        /**
         * Creates animators for showing buttons in the unfocused location bar. The buttons fade in
         * while width of the location bar gets smaller. There are toolbar buttons that also show at
         * the same time, causing the width of the location bar to change.
         *
         * @param toolbarStartPaddingDifference The difference in the toolbar's start padding
         *         between the beginning and end of the animation.
         * @return A list of animators to run.
         */
        List<Animator> getShowButtonsWhenUnfocusedAnimators(int toolbarStartPaddingDifference);

        /**
         * @param shouldShowButtons Whether buttons should be displayed in the URL bar when it's not
         *         focused.
         */
        void setShouldShowButtonsWhenUnfocused(boolean shouldShowButtons);

        /**
         * Updates the visibility of the buttons inside the location bar.
         */
        void updateButtonVisibility();

        /**
         * Gets the background drawable.
         *
         * <p>TODO(1133482): Hide this View interaction if possible.
         *
         * @see View#getBackground()
         */
        Drawable getBackground();
    }
}
