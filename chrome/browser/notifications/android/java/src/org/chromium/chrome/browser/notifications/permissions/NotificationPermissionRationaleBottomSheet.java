// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationRationaleResult;
import org.chromium.chrome.browser.notifications.R;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleDelegate;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleUiResult;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/** Bottom sheet to explain the advantages of Chrome notifications. */
public class NotificationPermissionRationaleBottomSheet
        implements RationaleDelegate, BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private Callback<Integer> mResponseCallback;
    private final BottomSheetObserver mBottomSheetObserver;
    private View mContentView;
    private boolean mWasSheetOpened;

    public NotificationPermissionRationaleBottomSheet(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContext = context;

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        // If the callback was already invoked then the user must have explicitly
                        // clicked one of the buttons.
                        if (mResponseCallback == null) return;

                        switch (reason) {
                            case StateChangeReason.BACK_PRESS:
                                executeResponseCallback(
                                        RationaleUiResult.REJECTED,
                                        NotificationRationaleResult.BOTTOM_SHEET_BACK_PRESS);
                                break;
                            case StateChangeReason.SWIPE:
                                executeResponseCallback(
                                        RationaleUiResult.REJECTED,
                                        NotificationRationaleResult.BOTTOM_SHEET_SWIPE);
                                break;
                            case StateChangeReason.TAP_SCRIM:
                                executeResponseCallback(
                                        RationaleUiResult.REJECTED,
                                        NotificationRationaleResult.BOTTOM_SHEET_TAP_SCRIM);
                                break;
                            default:
                                executeResponseCallback(
                                        RationaleUiResult.REJECTED,
                                        NotificationRationaleResult.BOTTOM_SHEET_CLOSED_UNKNOWN);
                                break;
                        }
                    }

                    @Override
                    public void onSheetOpened(int reason) {
                        if (mBottomSheetController.getCurrentSheetContent()
                                == NotificationPermissionRationaleBottomSheet.this) {
                            mWasSheetOpened = true;
                        }
                    }
                };
    }

    private void initializeContentView() {
        if (mContentView != null) return;

        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.notification_permission_rationale_bottom_sheet, null);
        // This view will be displayed inside a FrameLayout, if its LayoutParams are not set before
        // then FrameLayout will set a default value, which is LayoutParams.MATCH_PARENT for height
        // and width.
        mContentView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        Button positiveButton =
                mContentView.findViewById(R.id.notification_permission_rationale_positive_button);
        Button negativeButton =
                mContentView.findViewById(R.id.notification_permission_rationale_negative_button);

        positiveButton.setOnClickListener(
                (v) -> {
                    mBottomSheetController.hideContent(
                            this,
                            /* animate= */ true,
                            BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
                    executeResponseCallback(
                            RationaleUiResult.ACCEPTED,
                            NotificationRationaleResult.POSITIVE_BUTTON_CLICKED);
                });
        negativeButton.setOnClickListener(
                view -> {
                    mBottomSheetController.hideContent(
                            this,
                            /* animate= */ true,
                            BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
                    executeResponseCallback(
                            RationaleUiResult.REJECTED,
                            NotificationRationaleResult.NEGATIVE_BUTTON_CLICKED);
                });
    }

    private void executeResponseCallback(
            @RationaleUiResult int callbackResult,
            @NotificationRationaleResult int detailedResultForMetrics) {
        if (mResponseCallback == null) return;

        NotificationUmaTracker.getInstance()
                .onNotificationPermissionRationaleResult(detailedResultForMetrics);

        mResponseCallback.onResult(callbackResult);
        mResponseCallback = null;
    }

    /* RationaleDelegate implementation. */
    @Override
    public void showRationaleUi(Callback<Integer> callback) {
        assert mResponseCallback == null;
        assert !mBottomSheetController.isSheetOpen();

        initializeContentView();
        mResponseCallback = callback;
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(
                NotificationPermissionRationaleBottomSheet.this, /* animate= */ true);
    }

    /* BottomSheetContent implementation. */
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        if (!mWasSheetOpened) {
            // Some startup cases may destroy the action sheet before it's shown.
            executeResponseCallback(
                    RationaleUiResult.NOT_SHOWN,
                    NotificationRationaleResult.BOTTOM_SHEET_NEVER_OPENED);
        } else {
            executeResponseCallback(
                    RationaleUiResult.REJECTED, NotificationRationaleResult.BOTTOM_SHEET_DESTROYED);
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
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
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.notification_permission_rationale_content_description;
    }

    @Override
    public boolean hasCustomLifecycle() {
        // This is set to true to be able to display on startup without being removed by start
        // surface.
        // TODO(crbug.com/40254542): Get rid of this once we no longer show this on startup.
        return true;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.notification_permission_rationale_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.notification_permission_rationale_closed_description;
    }
}
