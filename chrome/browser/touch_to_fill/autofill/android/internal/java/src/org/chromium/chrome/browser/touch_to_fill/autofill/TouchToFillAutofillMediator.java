// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ACKNOWLEDGE_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SETTINGS_LINK_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.VISIBLE;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.autofill.PopupNoticeInteractions;
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
        mModel =
                new PropertyModel.Builder(TouchToFillAutofillProperties.ALL_KEYS)
                        .with(DISMISS_HANDLER, this::onDismissed)
                        .with(ACKNOWLEDGE_HANDLER, this::onNoticeAcknowledged)
                        .with(SETTINGS_LINK_HANDLER, this::onSettingsLinkClicked)
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    void show() {
        mWasDismissed = false;
        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);
        recordNoticeInteraction(PopupNoticeInteractions.SHOWN);
    }

    void hide() {
        onDismissed();
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
}
