// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.widget.ButtonCompat;

/** Coordinator for the default browser promo card. */
public class DefaultBrowserPromoCoordinator implements EducationalTipCardProvider {
    private final Context mContext;

    // For the default browser promo card specifically, it is triggered only when the user clicks on
    // the bottom sheet, directing them to the default app settings page.
    private final Runnable mOnModuleClickedCallback;

    private final BottomSheetController mBottomSheetController;
    private DefaultBrowserPromoBottomSheetContent mDefaultBrowserBottomSheetContent;

    public DefaultBrowserPromoCoordinator(
            @NonNull Context context,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mOnModuleClickedCallback = onModuleClickedCallback;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public String getCardTitle() {
        return mContext.getString(R.string.educational_tip_default_browser_title);
    }

    @Override
    public String getCardDescription() {
        return mContext.getString(R.string.educational_tip_default_browser_description);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.default_browser_promo_logo;
    }

    @Override
    public void onCardClicked() {
        View defaultBrowserBottomSheetView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.educational_tip_default_browser_bottom_sheet,
                                /* root= */ null);
        mDefaultBrowserBottomSheetContent =
                new DefaultBrowserPromoBottomSheetContent(defaultBrowserBottomSheetView);
        mBottomSheetController.requestShowContent(mDefaultBrowserBottomSheetContent, true);
        ButtonCompat bottomSheetButton =
                defaultBrowserBottomSheetView.findViewById(
                        R.id.default_browser_bottom_sheet_button);
        bottomSheetButton.setOnClickListener(
                (v) -> {
                    IntentUtils.safeStartActivity(mContext, createBottomSheetOnClickIntent());
                    mBottomSheetController.hideContent(
                            mDefaultBrowserBottomSheetContent,
                            /* animate= */ true,
                            StateChangeReason.INTERACTION_COMPLETE);
                    mOnModuleClickedCallback.run();
                });
    }

    private Intent createBottomSheetOnClickIntent() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public DefaultBrowserPromoBottomSheetContent getDefaultBrowserBottomSheetContent() {
        return mDefaultBrowserBottomSheetContent;
    }
}
