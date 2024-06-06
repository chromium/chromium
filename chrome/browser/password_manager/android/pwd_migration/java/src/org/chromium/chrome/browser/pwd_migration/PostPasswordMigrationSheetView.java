// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING;
import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.logPostPasswordMigrationOutcome;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.DialogTitle;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PostPasswordMigrationSheetOutcome;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.widget.TextViewWithLeading;

/**
 * This class is responsible for rendering the bottom sheet that shows the post password migration
 * sheet.
 */
class PostPasswordMigrationSheetView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private Callback<Integer> mDismissHandler;
    private final RelativeLayout mContentView;

    private boolean mAcknowledged;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);

                    logPostPasswordMigrationOutcome(
                            mAcknowledged
                                    ? PostPasswordMigrationSheetOutcome.GOT_IT
                                    : PostPasswordMigrationSheetOutcome.DISMISS);
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (newState != BottomSheetController.SheetState.HIDDEN) {
                        return;
                    }
                    // This is a fail-safe for cases where onSheetClosed isn't triggered.
                    mDismissHandler.onResult(StateChangeReason.NONE);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    PostPasswordMigrationSheetView(Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.post_pwd_migration_sheet, null);
        ImageView sheetHeaderImage = mContentView.findViewById(R.id.sheet_header_image);
        sheetHeaderImage.setImageDrawable(
                AppCompatResources.getDrawable(
                        context,
                        PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));
        String titleText;
        String baseSubtitleText;
        if (ChromeFeatureList.isEnabled(
                UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)) {
            titleText =
                    context.getString(R.string.post_password_migration_sheet_title_about_local_pwd);
            baseSubtitleText =
                    context.getString(R.string.post_pwd_migration_sheet_subtitle_about_local_pwd);
        } else {
            titleText = context.getString(R.string.post_password_migration_sheet_title);
            baseSubtitleText = context.getString(R.string.post_password_migration_sheet_subtitle);
        }
        DialogTitle titleView = mContentView.findViewById(R.id.sheet_title);
        titleView.setText(titleText);
        String subtitleText =
                baseSubtitleText.replace(
                        "%1$s", PasswordMigrationWarningUtil.getChannelString(context));
        TextViewWithLeading subtitleView = mContentView.findViewById(R.id.sheet_subtitle);
        subtitleView.setText(subtitleText);
        Button acknowledgeButton = mContentView.findViewById(R.id.acknowledge_button);
        acknowledgeButton.setOnClickListener(
                (unusedView) -> {
                    setVisible(false);
                    mAcknowledged = true;
                });
    }

    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    void setVisible(boolean isVisible) {
        if (!isVisible) {
            mBottomSheetController.hideContent(this, true, StateChangeReason.NAVIGATION);
            return;
        }
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(this, true)) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            assert mDismissHandler != null;
            mDismissHandler.onResult(StateChangeReason.NONE);
        }
    }

    @Nullable
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.password_migration_warning_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // The sheet doesn't have a half height state.
        assert false;
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.password_migration_warning_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.password_migration_warning_closed;
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
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}
