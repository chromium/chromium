// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator;

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

/** Coordinator for the Accessibility Annotator bottom sheet. */
@NullMarked
public class AccessibilityAnnotatorBottomSheetCoordinator {
    /** Delegate interface for handling bottom sheet actions. */
    public interface Delegate {
        void onInfoAcknowledged();

        void onManageSettingsClicked();

        void onLearnMoreClicked();

        void onInfoDismissed();
    }

    private final AccessibilityAnnotatorBottomSheetViewHolder mView;
    private final AccessibilityAnnotatorBottomSheetMediator mMediator;
    private final PropertyModel mModel;

    public AccessibilityAnnotatorBottomSheetCoordinator(
            Context context, BottomSheetController bottomSheetController, Delegate delegate) {
        mView = new AccessibilityAnnotatorBottomSheetViewHolder(context);
        mModel =
                new PropertyModel.Builder(AccessibilityAnnotatorBottomSheetProperties.ALL_KEYS)
                        .with(
                                AccessibilityAnnotatorBottomSheetProperties.TITLE,
                                context.getString(
                                        R.string.accessibility_partial_custom_tab_bottom_sheet))
                        .with(
                                AccessibilityAnnotatorBottomSheetProperties.DESCRIPTION,
                                context.getString(R.string.ntp_learn_more_about_suggested_content))
                        .with(
                                AccessibilityAnnotatorBottomSheetProperties.PRIMARY_BUTTON_LABEL,
                                context.getString(R.string.got_it))
                        .with(
                                AccessibilityAnnotatorBottomSheetProperties.SECONDARY_BUTTON_LABEL,
                                context.getString(R.string.page_info_site_settings_button))
                        .build();

        ClickableSpan learnMoreSpan =
                new ClickableSpan() {
                    @Override
                    public void onClick(View widget) {
                        mMediator.onLearnMoreClicked();
                    }
                };

        SpannableString description =
                SpanApplier.applySpans(
                        mModel.get(AccessibilityAnnotatorBottomSheetProperties.DESCRIPTION)
                                .toString(),
                        new SpanApplier.SpanInfo("<link>", "</link>", learnMoreSpan));

        mModel.set(AccessibilityAnnotatorBottomSheetProperties.DESCRIPTION, description);

        PropertyModelChangeProcessor.create(
                mModel, mView, AccessibilityAnnotatorBottomSheetViewBinder::bind);

        mMediator =
                new AccessibilityAnnotatorBottomSheetMediator(
                        context,
                        bottomSheetController,
                        new AccessibilityAnnotatorBottomSheetContent(
                                mView.mContentView, mView.mScrollView),
                        delegate,
                        new SettingsCustomTabLauncherImpl());

        mView.mPrimaryButton.setOnClickListener(v -> mMediator.onAcknowledgeClicked());
        mView.mSecondaryButton.setOnClickListener(v -> mMediator.onManageSettingsClicked());

        mView.setAnimation(R.raw.chrome_finds_opt_in_animation);
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
