// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.facilitated_payments.core.ui_utils.UiEvent;
import org.chromium.ui.base.LocalizationUtils;

/**
 * This class is responsible for rendering the various screens of a Facilitated Payments flow in a
 * bottom sheet. It is a View in the Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
@NullMarked
class FacilitatedPaymentsPaymentMethodsView implements BottomSheetContent {
    // Contains everything to be shown in the bottom sheet. Includes the drag handler.
    private final LinearLayout mView;
    // Holds the screens to be displayed in the bottom sheet. To show different screens, simply swap
    // the view contained. Does not include the drag handler.
    private final FrameLayout mScreenHolder;
    private final BottomSheetController mBottomSheetController;
    // The screen currently being shown.
    private @Nullable FacilitatedPaymentsSequenceView mCurrentScreen;
    // The new screen to be shown replacing {@link #mCurrentScreen}.
    private @Nullable FacilitatedPaymentsSequenceView mNextScreen;
    private @Nullable Callback<Integer> mUiEventListener;
    private boolean mHasCustomLifecycle;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    assert mUiEventListener != null;
                    switch (reason) {
                        case StateChangeReason.BACK_PRESS: // Intentional fallthrough.
                        case StateChangeReason.SWIPE: // Intentional fallthrough.
                        case StateChangeReason.OMNIBOX_FOCUS: // Intentional fallthrough.
                        case StateChangeReason.TAP_SCRIM:
                            mUiEventListener.onResult(UiEvent.SCREEN_CLOSED_BY_USER);
                            break;
                        case StateChangeReason.NAVIGATION: // Intentional fallthrough.
                        case StateChangeReason.COMPOSITED_UI: // Intentional fallthrough.
                        case StateChangeReason.VR: // Intentional fallthrough.
                        case StateChangeReason.PROMOTE_TAB: // Intentional fallthrough.
                        case StateChangeReason.INTERACTION_COMPLETE: // Intentional fallthrough.
                        case StateChangeReason.NONE:
                            mUiEventListener.onResult(UiEvent.SCREEN_CLOSED_NOT_BY_USER);
                            break;
                    }
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    /**
     * Constructs a FacilitatedPaymentsPaymentMethodsView which creates, modifies, and shows the
     * bottom sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    FacilitatedPaymentsPaymentMethodsView(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.facilitated_payments_sequence_view, null);
        mScreenHolder = (FrameLayout) mView.findViewById(R.id.screen_holder);

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mView.setLayoutDirection(layoutDirection);
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     *
     * @param isVisible A boolean describing whether to show or hide the sheet.
     */
    void setVisible(boolean isVisible) {
        if (isVisible) {
            assert mUiEventListener != null && mNextScreen != null;
            // If the bottom sheet is already showing a screen, replace it with {@link
            // #mNextScreen}. Else, open the bottom sheet and show the {@link mNextScreen}.
            if (mBottomSheetController.isSheetOpen()) {
                assert mCurrentScreen != null;
                mScreenHolder.addView(mNextScreen.getView());
                mScreenHolder.removeView(mCurrentScreen.getView());
            } else {
                assert mCurrentScreen == null;
                mBottomSheetController.addObserver(mBottomSheetObserver);
                mScreenHolder.addView(mNextScreen.getView());
                if (!mBottomSheetController.requestShowContent(this, /* animate= */ true)) {
                    mUiEventListener.onResult(UiEvent.SCREEN_CLOSED_NOT_BY_USER);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                    mNextScreen = null;
                    return;
                }
            }
            // Update the reference for {@link mCurrentScreen} to the current screen being shown.
            mCurrentScreen = mNextScreen;
            mNextScreen = null;
            mUiEventListener.onResult(UiEvent.NEW_SCREEN_SHOWN);
        } else {
            mBottomSheetController.hideContent(this, true);
            mCurrentScreen = null;
        }
    }

    /**
     * Sets a new listener that reacts to bottom sheet UI events.
     *
     * @param uiEventListener A {@link Callback<Integer>}.
     */
    void setUiEventListener(Callback<Integer> uiEventListener) {
        mUiEventListener = uiEventListener;
    }

    /**
     * Sets a bit informing whether or not the bottom sheet closes on page navigations.
     *
     * @param survivesNavigation A boolean which if set to true prevents the bottom sheet from
     *     closing during page navigations.
     */
    void setSurvivesNavigation(boolean survivesNavigation) {
        mHasCustomLifecycle = survivesNavigation;
    }

    /**
     * @return {@link #mScreenHolder}, the parent view where the screen to be shown is added.
     */
    FrameLayout getScreenHolder() {
        return mScreenHolder;
    }

    /**
     * Sets the screen to be shown in the bottom sheet.
     *
     * @param nextScreen The screen to be shown.
     */
    void setNextScreen(FacilitatedPaymentsSequenceView nextScreen) {
        mNextScreen = nextScreen;
    }

    @Override
    public View getContentView() {
        return mView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return mHasCustomLifecycle;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public int getVerticalScrollOffset() {
        return assumeNonNull(mCurrentScreen).getVerticalScrollOffset();
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(
                R.string.facilitated_payments_payment_methods_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.facilitated_payments_payment_methods_bottom_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.facilitated_payments_payment_methods_bottom_sheet_closed;
    }
}
