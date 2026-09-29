// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_CLOSED_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_FULL_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.VISIBLE;

import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.R;
import org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.ButtonProperties;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties;
import org.chromium.components.autofill.PopupNoticeInteractions;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the business logic for the TouchToFillAutofill MVC component. It is responsible for
 * binding the user interaction events to the delegate and controlling the model.
 */
@NullMarked
class TouchToFillAutofillMediator {
    /**
     * Immutable description of a notice that the bottom sheet can display.
     *
     * <p>The notices share the same layout and behavior and differ only in the strings they show
     * and the histogram they record to. Each notice is described by exactly one constant below, so
     * adding a new notice means adding a constant rather than extending a switch at every point of
     * use.
     */
    static final class NoticeContent {
        static final NoticeContent PERSONAL_CONTEXT =
                new NoticeContent(
                        R.string.autofill_personal_context_notice_title,
                        R.string.autofill_personal_context_notice_description,
                        R.string.autofill_personal_context_notice_ok_button,
                        R.string.autofill_personal_context_notice_settings_link,
                        R.string.autofill_personal_context_notice_sheet_full_height,
                        R.string.autofill_personal_context_notice_sheet_closed,
                        "PersonalContext.AmbientAutofill.NoticeInteractions");

        static final NoticeContent PRIVATE_INFERENCE =
                new NoticeContent(
                        R.string.autofill_ai_private_inference_notice_sheet_title,
                        R.string.autofill_ai_private_inference_notice_sheet_description,
                        R.string.autofill_ai_private_inference_notice_sheet_ok_button,
                        R.string.autofill_ai_private_inference_notice_sheet_settings_link,
                        R.string.autofill_ai_private_inference_notice_sheet_full_height,
                        R.string.autofill_ai_private_inference_notice_sheet_closed,
                        "Autofill.Ai.PrivateInferenceNoticeInteractions");

        /** Headline shown at the top of the sheet. */
        public final @StringRes int titleId;

        /** Body text explaining the notice. */
        public final @StringRes int descriptionId;

        /** Label of the button that acknowledges the notice. */
        public final @StringRes int acknowledgeButtonTextId;

        /** Label of the button that opens settings. */
        public final @StringRes int settingsButtonTextId;

        /** Announced by screen readers when the sheet opens at full height. */
        public final @StringRes int fullHeightDescriptionId;

        /** Announced by screen readers when the sheet closes. */
        public final @StringRes int closedDescriptionId;

        /** Histogram recording how the user interacted with this notice. */
        public final String interactionsHistogram;

        private NoticeContent(
                @StringRes int titleId,
                @StringRes int descriptionId,
                @StringRes int acknowledgeButtonTextId,
                @StringRes int settingsButtonTextId,
                @StringRes int fullHeightDescriptionId,
                @StringRes int closedDescriptionId,
                String interactionsHistogram) {
            this.titleId = titleId;
            this.descriptionId = descriptionId;
            this.acknowledgeButtonTextId = acknowledgeButtonTextId;
            this.settingsButtonTextId = settingsButtonTextId;
            this.fullHeightDescriptionId = fullHeightDescriptionId;
            this.closedDescriptionId = closedDescriptionId;
            this.interactionsHistogram = interactionsHistogram;
        }
    }

    private final Delegate mDelegate;
    private final PropertyModel mModel;
    private final BottomSheetFocusHelper mBottomSheetFocusHelper;
    private boolean mWasDismissed;
    private @Nullable NoticeContent mNotice;

    TouchToFillAutofillMediator(Delegate delegate, BottomSheetFocusHelper bottomSheetFocusHelper) {
        mDelegate = delegate;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
        mModel = createModel();
    }

    void showPersonalContextNotice() {
        show(NoticeContent.PERSONAL_CONTEXT);
    }

    void showPrivateInferenceNotice() {
        show(NoticeContent.PRIVATE_INFERENCE);
    }

    void hide() {
        onDismissed();
    }

    private void show(NoticeContent notice) {
        mWasDismissed = false;
        mNotice = notice;

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        sheetItems.add(new ListItem(ItemType.HEADER, createHeader()));
        sheetItems.add(new ListItem(ItemType.FILL_BUTTON, createAcknowledgeButton()));
        sheetItems.add(new ListItem(ItemType.TEXT_BUTTON, createSettingsButton()));

        mModel.set(SHEET_FULL_HEIGHT_DESCRIPTION_ID, notice.fullHeightDescriptionId);
        mModel.set(SHEET_CLOSED_DESCRIPTION_ID, notice.closedDescriptionId);

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);
        recordNoticeInteraction(PopupNoticeInteractions.SHOWN);
    }

    private PropertyModel createHeader() {
        assumeNonNull(mNotice);
        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.IMAGE_DRAWABLE_ID, R.drawable.fre_product_logo)
                .with(HeaderProperties.TITLE_ID, mNotice.titleId)
                .with(
                        HeaderProperties.TITLE_BOTTOM_MARGIN,
                        R.dimen.ttf_notice_header_title_bottom_margin)
                .with(HeaderProperties.SUBTITLE_ID, mNotice.descriptionId)
                .with(
                        HeaderProperties.SUBTITLE_BOTTOM_MARGIN,
                        R.dimen.ttf_notice_header_subtitle_bottom_margin)
                .build();
    }

    private PropertyModel createAcknowledgeButton() {
        assumeNonNull(mNotice);
        return new PropertyModel.Builder(ButtonProperties.ALL_KEYS)
                .with(ButtonProperties.TEXT_ID, mNotice.acknowledgeButtonTextId)
                .with(ButtonProperties.ON_CLICK_ACTION, this::onNoticeAcknowledged)
                .build();
    }

    private PropertyModel createSettingsButton() {
        assumeNonNull(mNotice);
        return new PropertyModel.Builder(ButtonProperties.ALL_KEYS)
                .with(ButtonProperties.TEXT_ID, mNotice.settingsButtonTextId)
                .with(ButtonProperties.ON_CLICK_ACTION, this::onSettingsLinkClicked)
                .build();
    }

    private boolean dismiss() {
        if (mWasDismissed) return false;
        mWasDismissed = true;
        mModel.set(VISIBLE, false);
        return true;
    }

    private void onNoticeAcknowledged() {
        if (!dismiss()) return;
        mDelegate.onNoticeAcknowledged();
        recordNoticeInteraction(PopupNoticeInteractions.ACKNOWLEDGED);
    }

    private void onSettingsLinkClicked() {
        if (!dismiss()) return;
        mDelegate.onSettingsLinkClicked();
        recordNoticeInteraction(PopupNoticeInteractions.LINK_BUTTON_CLICKED);
    }

    private void onDismissed() {
        if (!dismiss()) return;
        mDelegate.onDismissed();
        recordNoticeInteraction(PopupNoticeInteractions.DISMISSED);
    }

    private void recordNoticeInteraction(@PopupNoticeInteractions int interaction) {
        if (mNotice == null) {
            // The sheet can be dismissed before a notice was ever shown, e.g. when the component
            // is torn down. There is no notice to attribute the interaction to.
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                mNotice.interactionsHistogram, interaction, PopupNoticeInteractions.MAX_VALUE + 1);
    }

    private PropertyModel createModel() {
        return new PropertyModel.Builder(TouchToFillAutofillProperties.ALL_KEYS)
                .with(DISMISS_HANDLER, this::onDismissed)
                .with(SHEET_ITEMS, new ModelList())
                .with(VISIBLE, false)
                .build();
    }

    PropertyModel getModel() {
        return mModel;
    }
}
