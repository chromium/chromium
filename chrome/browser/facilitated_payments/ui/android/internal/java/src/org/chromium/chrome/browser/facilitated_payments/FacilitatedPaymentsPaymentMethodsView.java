// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.LocalizationUtils;

/**
 * This class is responsible for rendering the various screens of a Facilitated Payments flow in a
 * bottom sheet. It is a View in the Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class FacilitatedPaymentsPaymentMethodsView implements BottomSheetContent {
    // Contains everything to be shown in the bottom sheet. Includes the drag handler.
    private final LinearLayout mView;
    // Holds the screens to be displayed in the bottom sheet. To show different screens, simply swap
    // the view contained. Does not include the drag handler.
    private final FrameLayout mScreenHolder;
    private final BottomSheetController mBottomSheetController;
    // The screen currently being shown.
    private FacilitatedPaymentsSequenceView mCurrentScreen;
    // The new screen to be shown replacing {@link #mCurrentScreen}.
    private FacilitatedPaymentsSequenceView mNextScreen;
    private Callback<Integer> mDismissHandler;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
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
     * @return True if the request was successful, false otherwise
     */
    boolean setVisible(boolean isVisible) {
        if (isVisible) {
            // If the bottom sheet is already showing a screen, replace it with {@link
            // #mNextScreen}. Else, open the bottom sheet and show the {@link mNextScreen}.
            if (mBottomSheetController.isSheetOpen()) {
                assert (mCurrentScreen != null && mNextScreen != null);
                mScreenHolder.addView(mNextScreen.getView());
                mScreenHolder.removeView(mCurrentScreen.getView());
            } else {
                assert mCurrentScreen == null;
                mBottomSheetController.addObserver(mBottomSheetObserver);
                mScreenHolder.addView(mNextScreen.getView());
                if (!mBottomSheetController.requestShowContent(this, /* animate= */ true)) {
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                    mNextScreen = null;
                    return false;
                }
            }
            // Update the reference for {@link mCurrentScreen} to the current screen being shown.
            mCurrentScreen = mNextScreen;
            mNextScreen = null;
        } else {
            mBottomSheetController.hideContent(this, true);
            mCurrentScreen = null;
        }
        return true;
    }

    /**
     * Sets a new listener that reacts to bottom sheet dismissal.
     *
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
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

    @Nullable
    @Override
    public View getToolbarView() {
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
    public int getPeekHeight() {
        return HeightMode.DISABLED;
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
        return mCurrentScreen.getVerticalScrollOffset();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.pix_payment_methods_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.pix_payment_methods_bottom_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.pix_payment_methods_bottom_sheet_closed;
    }
}
