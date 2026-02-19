// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.base.CallbackController;
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

/** Coordinator for the Save Passwords promo card. */
@NullMarked
public class SavePasswordsPromoCoordinator
        implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    private @Nullable SavePasswordsInstructionalBottomSheetContent mSavePasswordsBottomSheetContent;
    private @Nullable ButtonCompat mGotItButton;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param callbackController The instance of {@link CallbackController}.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public SavePasswordsPromoCoordinator(
            Runnable onModuleClickedCallback,
            CallbackController callbackController,
            EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = callbackController.makeCancelable(onModuleClickedCallback);
        mActionDelegate = actionDelegate;
    }

    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_save_passwords_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_save_passwords_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_save_passwords_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.save_passwords_promo_logo;
    }

    @Override
    public void onCardClicked() {
        showInstructionalBottomSheet();
        mOnModuleClickedCallback.run();
    }

    @Override
    public boolean isComplete() {
        return SetupListModuleUtils.isModuleCompleted(ModuleType.SAVE_PASSWORDS_PROMO);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.setup_list_completed_background_wavy_circle;
    }

    @Override
    public void destroy() {
        if (mGotItButton != null) {
            mGotItButton.setOnClickListener(null);
            mGotItButton = null;
        }

        if (mSavePasswordsBottomSheetContent != null) {
            mActionDelegate
                    .getBottomSheetController()
                    .hideContent(mSavePasswordsBottomSheetContent, /* animate= */ false);
            mSavePasswordsBottomSheetContent.destroy();
            mSavePasswordsBottomSheetContent = null;
        }
    }

    @Nullable SavePasswordsInstructionalBottomSheetContent getBottomSheetContent() {
        return mSavePasswordsBottomSheetContent;
    }

    private void showInstructionalBottomSheet() {
        View view =
                LayoutInflater.from(mActionDelegate.getContext())
                        .inflate(
                                R.layout.save_passwords_instructional_bottom_sheet,
                                /* root= */ null);

        LottieAnimationView animationView = view.findViewById(R.id.instructional_animation);
        animationView.setAnimation(R.raw.save_passwords_animation);
        animationView.playAnimation();

        ((TextView) view.findViewById(R.id.title))
                .setText(R.string.educational_tip_save_passwords_title);

        ((TextView) view.findViewById(R.id.step_1_content))
                .setText(R.string.educational_tip_save_passwords_bottom_sheet_first_step);

        TextView step2 = view.findViewById(R.id.step_2_content);
        step2.setText(R.string.educational_tip_save_passwords_bottom_sheet_second_step);
        step2.setContentDescription(
                mActionDelegate
                        .getContext()
                        .getString(
                                R.string.educational_tip_save_passwords_bottom_sheet_second_step));

        TextView step3 = view.findViewById(R.id.step_3_content);
        step3.setText(R.string.educational_tip_save_passwords_bottom_sheet_third_step);
        step3.setContentDescription(
                mActionDelegate
                        .getContext()
                        .getString(
                                R.string.educational_tip_save_passwords_bottom_sheet_third_step));

        mGotItButton = view.findViewById(R.id.button);
        assumeNonNull(mGotItButton).setText(R.string.got_it);

        mSavePasswordsBottomSheetContent =
                new SavePasswordsInstructionalBottomSheetContent(
                        view,
                        mActionDelegate
                                .getContext()
                                .getString(
                                        R.string
                                                .educational_tip_save_passwords_bottom_sheet_content_description),
                        R.string
                                .educational_tip_save_passwords_bottom_sheet_accessibility_opened_full,
                        R.string.educational_tip_save_passwords_bottom_sheet_accessibility_closed);

        BottomSheetController bottomSheetController = mActionDelegate.getBottomSheetController();
        bottomSheetController.requestShowContent(
                mSavePasswordsBottomSheetContent, /* animate= */ true);

        final SavePasswordsInstructionalBottomSheetContent content =
                mSavePasswordsBottomSheetContent;
        mGotItButton.setOnClickListener(
                (v) -> {
                    bottomSheetController.hideContent(
                            content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
                });
    }
}
