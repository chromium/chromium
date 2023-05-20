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
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet that shows the passwords
 * migration warning.
 */
class PasswordMigrationWarningView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private Callback<Integer> mDismissHandler;
    private FragmentManager mFragmentManager;
    private PasswordMigrationWarningIntroFragment mIntroFragment;
    private final RelativeLayout mContentView;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
            super.onSheetClosed(reason);
            assert mDismissHandler != null;
            mDismissHandler.onResult(reason);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }

        @Override
        public void onSheetStateChanged(int newState, int reason) {
            super.onSheetStateChanged(newState, reason);
            if (newState != BottomSheetController.SheetState.HIDDEN) return;
            // This is a fail-safe for cases where onSheetClosed isn't triggered.
            mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
    };

    PasswordMigrationWarningView(Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView = (RelativeLayout) LayoutInflater.from(context).inflate(
                R.layout.pwd_migration_warning, null);
        ImageView sheetHeaderImage =
                mContentView.findViewById(R.id.touch_to_fill_sheet_header_image);
        sheetHeaderImage.setImageDrawable(AppCompatResources.getDrawable(
                context, PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));

        mFragmentManager = ((AppCompatActivity) context).getSupportFragmentManager();
        mIntroFragment = new PasswordMigrationWarningIntroFragment(context);
    }

    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    boolean setVisible(boolean isVisible) {
        if (!isVisible) {
            mBottomSheetController.hideContent(this, true);
            return true;
        }
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(this, true)) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            return false;
        }
        mFragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, mIntroFragment,
                        "PasswordMigrationWarningFragment")
                .commit();
        return true;
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
        // TODO(crbug.com/1440104): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/1440104): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/1440104): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/1440104): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}
