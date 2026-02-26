// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DrawableRes;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.widget.ButtonCompat;

/** Coordinator for the default browser promo card. */
@NullMarked
public class DefaultBrowserPromoCoordinator
        implements EducationalTipCardProvider, SetupListCompletable {
    // For the default browser promo card specifically, it is triggered only when the user clicks on
    // the bottom sheet, directing them to the default app settings page.
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    private @Nullable DefaultBrowserPromoBottomSheetContent mDefaultBrowserBottomSheetContent;

    /**
     * @param onModuleClickedCallback The callback to be called when the bottom sheet is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public DefaultBrowserPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate.getContext().getString(R.string.use_chrome_by_default);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_default_browser_description);
    }

    @Override
    public String getCardButtonText() {
        if (SetupListModuleUtils.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO)) {
            return mActionDelegate
                    .getContext()
                    .getString(R.string.setup_list_default_browser_promo_button);
        }
        return mActionDelegate.getContext().getString(R.string.educational_tip_module_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        if (SetupListModuleUtils.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO)) {
            return R.drawable.setup_list_default_browser_promo_logo;
        }
        return R.drawable.default_browser_promo_logo;
    }

    @Override
    public void onCardClicked() {
        if (SetupListModuleUtils.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO)) {
            SetupListModuleUtils.setModuleCompleted(
                    ModuleType.DEFAULT_BROWSER_PROMO, /* silent= */ false);
            if (mActionDelegate.maybeShowDefaultBrowserPromoWithRoleManager()) {
                mOnModuleClickedCallback.run();
                return;
            }
        }
        Context context = mActionDelegate.getContext();
        View defaultBrowserBottomSheetView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.educational_tip_default_browser_bottom_sheet,
                                /* root= */ null);
        mDefaultBrowserBottomSheetContent =
                new DefaultBrowserPromoBottomSheetContent(defaultBrowserBottomSheetView);
        BottomSheetController bottomSheetController = mActionDelegate.getBottomSheetController();
        bottomSheetController.requestShowContent(mDefaultBrowserBottomSheetContent, true);
        ButtonCompat bottomSheetButton = defaultBrowserBottomSheetView.findViewById(R.id.button);
        bottomSheetButton.setOnClickListener(
                (v) -> {
                    IntentUtils.safeStartActivity(context, createBottomSheetOnClickIntent());
                    bottomSheetController.hideContent(
                            assumeNonNull(mDefaultBrowserBottomSheetContent),
                            /* animate= */ true,
                            StateChangeReason.INTERACTION_COMPLETE);
                    mOnModuleClickedCallback.run();
                });
    }

    @Override
    public void destroy() {
        if (mDefaultBrowserBottomSheetContent != null) {
            mDefaultBrowserBottomSheetContent.destroy();
            mDefaultBrowserBottomSheetContent = null;
        }
    }

    // SetupListCompletable implementation.
    @Override
    public boolean isComplete() {
        return SetupListModuleUtils.isModuleCompleted(ModuleType.DEFAULT_BROWSER_PROMO);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.default_browser_promo_completed_logo;
    }

    private Intent createBottomSheetOnClickIntent() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public @Nullable DefaultBrowserPromoBottomSheetContent getDefaultBrowserBottomSheetContent() {
        return mDefaultBrowserBottomSheetContent;
    }
}
