// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.version_info.VersionInfo;

/**
 * This class is responsible for rendering the bottom sheet that shows the passwords
 * migration warning.
 */
class PasswordMigrationWarningView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private Callback<Integer> mDismissHandler;
    private PasswordMigrationWarningOnClickHandler mOnClickHandler;
    private FragmentManager mFragmentManager;
    private final RelativeLayout mContentView;
    private Context mContext;
    private String mAccountDisplayName;
    private @ScreenType int mScreenType = ScreenType.NONE;

    private Runnable mOnResumeExportFlowCallback;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
            super.onSheetClosed(reason);
            if (mBottomSheetController.getCurrentSheetContent()
                    != PasswordMigrationWarningView.this) {
                return;
            }
            assert mDismissHandler != null;
            mDismissHandler.onResult(reason);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }

        @Override
        public void onSheetStateChanged(int newState, int reason) {
            super.onSheetStateChanged(newState, reason);
            if (mBottomSheetController.getCurrentSheetContent()
                    != PasswordMigrationWarningView.this) {
                return;
            }
            if (newState != BottomSheetController.SheetState.HIDDEN) return;
            // This is a fail-safe for cases where onSheetClosed isn't triggered.
            mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }

        @Override
        public void onSheetOpened(@StateChangeReason int reason) {
            if (mBottomSheetController.getCurrentSheetContent()
                    != PasswordMigrationWarningView.this) {
                return;
            }
            if (mScreenType != ScreenType.NONE && getContentView().isShown()) {
                setFragment();
            }
        }
    };

    PasswordMigrationWarningView(Context context, BottomSheetController bottomSheetController,
            Runnable onResumeExportFlowCallback) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mOnResumeExportFlowCallback = onResumeExportFlowCallback;
        mContentView = (RelativeLayout) LayoutInflater.from(context).inflate(
                R.layout.pwd_migration_warning, null);
        ImageView sheetHeaderImage =
                mContentView.findViewById(R.id.touch_to_fill_sheet_header_image);
        sheetHeaderImage.setImageDrawable(AppCompatResources.getDrawable(
                context, PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));
        mFragmentManager = ((AppCompatActivity) context).getSupportFragmentManager();
    }

    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    void setOnClickHandler(PasswordMigrationWarningOnClickHandler onClickHandler) {
        mOnClickHandler = onClickHandler;
    }

    boolean setVisible(boolean isVisible) {
        if (!isVisible) {
            mBottomSheetController.hideContent(this, true);
            return true;
        }
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(this, true)) {
            return false;
        }
        return true;
    }

    void setScreen(@ScreenType int screenType) {
        mScreenType = screenType;
        if (getContentView().isShown()) {
            setFragment();
        }
        // Makes sure the sheet is fully expanded.
        mBottomSheetController.expandSheet();
    }

    private void setFragment() {
        assert mScreenType != ScreenType.NONE;
        if (mScreenType == ScreenType.INTRO_SCREEN) {
            PasswordMigrationWarningIntroFragment introFragment =
                    new PasswordMigrationWarningIntroFragment(mContext,
                            ()
                                    -> mOnClickHandler.onAcknowledge(mBottomSheetController),
                            () -> mOnClickHandler.onMoreOptions(), getChannelString());
            mFragmentManager.beginTransaction()
                    .setReorderingAllowed(true)
                    .replace(R.id.fragment_container_view, introFragment)
                    .commit();
        } else if (mScreenType == ScreenType.OPTIONS_SCREEN) {
            PasswordMigrationWarningOptionsFragment optionsFragment =
                    new PasswordMigrationWarningOptionsFragment(mContext, mOnClickHandler,
                            ()
                                    -> mOnClickHandler.onCancel(mBottomSheetController),
                            getChannelString(), mAccountDisplayName, mFragmentManager,
                            mOnResumeExportFlowCallback);
            mFragmentManager.beginTransaction()
                    .setReorderingAllowed(true)
                    .replace(R.id.fragment_container_view, optionsFragment)
                    .commit();
        }
    }

    void setAccountDisplayName(String accountDisplayName) {
        mAccountDisplayName = accountDisplayName;
    }

    private String getChannelString() {
        if (VersionInfo.isCanaryBuild()) {
            return "Canary";
        }
        if (VersionInfo.isDevBuild()) {
            return "Dev";
        }
        if (VersionInfo.isBetaBuild()) {
            return "Beta";
        }
        assert !VersionInfo.isStableBuild();
        return "";
    }

    private @Px int getDesiredSheetHeightPx() {
        if (mScreenType == ScreenType.INTRO_SCREEN) {
            return getDimensionPixelSize(R.dimen.pwd_migration_warning_intro_fragment_height);
        }
        return getDimensionPixelSize(R.dimen.pwd_migration_warning_options_fragment_height);
    }

    private @Px int getDimensionPixelSize(int id) {
        return mContentView.getContext().getResources().getDimensionPixelSize(id);
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
    public float getFullHeightRatio() {
        return Math.min(getDesiredSheetHeightPx(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }
}
