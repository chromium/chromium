// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.VISIBLE;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
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
    static final String NOTICE_INTERACTIONS_HISTOGRAM =
            "PersonalContext.AmbientAutofill.NoticeInteractions";

    private final Delegate mDelegate;
    private final PropertyModel mModel;
    private final BottomSheetFocusHelper mBottomSheetFocusHelper;
    private boolean mWasDismissed;

    TouchToFillAutofillMediator(Delegate delegate, BottomSheetFocusHelper bottomSheetFocusHelper) {
        mDelegate = delegate;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
        mModel = createModel();
    }

    void show() {
        mWasDismissed = false;

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        sheetItems.add(new ListItem(ItemType.HEADER, createHeader()));
        sheetItems.add(new ListItem(ItemType.FILL_BUTTON, createFillButton()));
        sheetItems.add(new ListItem(ItemType.TEXT_BUTTON, createSettingsButton()));

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);
        recordNoticeInteraction(PopupNoticeInteractions.SHOWN);
    }

    void hide() {
        onDismissed();
    }

    private PropertyModel createHeader() {
        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.IMAGE_DRAWABLE_ID, R.drawable.fre_product_logo)
                .with(HeaderProperties.TITLE_ID, R.string.autofill_personal_context_notice_title)
                .with(
                        HeaderProperties.TITLE_BOTTOM_MARGIN,
                        R.dimen.ttf_notice_header_title_bottom_margin)
                .with(
                        HeaderProperties.SUBTITLE_ID,
                        R.string.autofill_personal_context_notice_description)
                .with(
                        HeaderProperties.SUBTITLE_BOTTOM_MARGIN,
                        R.dimen.ttf_notice_header_subtitle_bottom_margin)
                .build();
    }

    private PropertyModel createFillButton() {
        return new PropertyModel.Builder(ButtonProperties.ALL_KEYS)
                .with(ButtonProperties.TEXT_ID, R.string.autofill_personal_context_notice_ok_button)
                .with(ButtonProperties.ON_CLICK_ACTION, this::onNoticeAcknowledged)
                .build();
    }

    private PropertyModel createSettingsButton() {
        return new PropertyModel.Builder(ButtonProperties.ALL_KEYS)
                .with(
                        ButtonProperties.TEXT_ID,
                        R.string.autofill_personal_context_notice_settings_link)
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
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_INTERACTIONS_HISTOGRAM, interaction, PopupNoticeInteractions.MAX_VALUE + 1);
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
