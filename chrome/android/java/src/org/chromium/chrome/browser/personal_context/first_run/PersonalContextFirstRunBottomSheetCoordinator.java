// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.personal_context.first_run;

import android.content.Context;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsCustomTabLauncherImpl;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.SpanApplier;

/** Coordinator for the Personal Context first-run bottom sheet. */
@NullMarked
public class PersonalContextFirstRunBottomSheetCoordinator {
    /** Delegate interface for handling bottom sheet actions. */
    public interface Delegate {
        void onInfoAcknowledged();

        void onManageSettingsClicked();

        void onLearnMoreClicked();

        void onInfoDismissed();
    }

    private final PersonalContextFirstRunBottomSheetViewHolder mView;
    private final PersonalContextFirstRunBottomSheetMediator mMediator;
    private final PropertyModel mModel;

    public PersonalContextFirstRunBottomSheetCoordinator(
            Context context, BottomSheetController bottomSheetController, Delegate delegate) {
        mView = new PersonalContextFirstRunBottomSheetViewHolder(context);
        mModel =
                new PropertyModel.Builder(PersonalContextFirstRunBottomSheetProperties.ALL_KEYS)
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.TITLE,
                                context.getString(R.string.accessibility_annotator_info_title))
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.DESCRIPTION,
                                context.getString(
                                        R.string.accessibility_annotator_info_description_android))
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.LEARN_MORE_DESCRIPTION,
                                context.getString(
                                        R.string.accessibility_annotator_info_learn_more_android))
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.CARD_1_TEXT,
                                context.getString(
                                        R.string.accessibility_annotator_info_card_1_android))
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.CARD_2_TEXT,
                                context.getString(
                                        R.string.accessibility_annotator_info_card_2_android))
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.PRIMARY_BUTTON_LABEL,
                                context.getString(
                                        R.string.accessibility_annotator_info_primary_button))
                        .with(
                                PersonalContextFirstRunBottomSheetProperties.SECONDARY_BUTTON_LABEL,
                                context.getString(
                                        R.string.accessibility_annotator_info_secondary_button))
                        .build();

        ClickableSpan learnMoreSpan =
                new ClickableSpan() {
                    @Override
                    public void onClick(View widget) {
                        mMediator.onLearnMoreClicked();
                    }
                };

        SpannableString learnMoreDescription =
                SpanApplier.applySpans(
                        mModel.get(
                                        PersonalContextFirstRunBottomSheetProperties
                                                .LEARN_MORE_DESCRIPTION)
                                .toString(),
                        new SpanApplier.SpanInfo("<link>", "</link>", learnMoreSpan));

        mModel.set(
                PersonalContextFirstRunBottomSheetProperties.LEARN_MORE_DESCRIPTION,
                learnMoreDescription);

        PropertyModelChangeProcessor.create(
                mModel, mView, PersonalContextFirstRunBottomSheetViewBinder::bind);

        mMediator =
                new PersonalContextFirstRunBottomSheetMediator(
                        context,
                        bottomSheetController,
                        new PersonalContextFirstRunBottomSheetContent(
                                mView.mContentView, mView.mScrollView),
                        delegate,
                        new SettingsCustomTabLauncherImpl());

        mView.mPrimaryButton.setOnClickListener(v -> mMediator.onAcknowledgeClicked());
        mView.mSecondaryButton.setOnClickListener(v -> mMediator.onManageSettingsClicked());

        mView.setAnimation(R.raw.accessibility_annotator_first_run_animation);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param manageSettingsUrl The URL for the manage settings page.
     * @param learnMoreUrl The URL for the learn more page.
     * @return True if the content was shown, false if it was suppressed.
     */
    public boolean requestShowContent(String manageSettingsUrl, String learnMoreUrl) {
        mView.playAnimation();
        return mMediator.requestShowContent(manageSettingsUrl, learnMoreUrl);
    }

    /** Hides the bottom sheet. */
    public void hide(@StateChangeReason int hideReason) {
        mMediator.hide(hideReason);
    }
}
