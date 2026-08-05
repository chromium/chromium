// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.VISIBLE;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains the business logic for the TouchToFillAutofill MVC component. It is responsible for
 * binding the user interaction events to the delegate and controlling the model.
 */
@NullMarked
class TouchToFillAutofillMediator {
    static final String NOTICE_INTERACTIONS_HISTOGRAM =
            "PersonalContext.AmbientAutofill.NoticeInteractions";

    // Interactions with the Ambient Autofill notice.
    // LINT.IfChange(NoticeInteraction)
    @IntDef({
        NoticeInteraction.SHOWN,
        NoticeInteraction.ACKNOWLEDGED,
        NoticeInteraction.DISMISSED,
        NoticeInteraction.LINK_BUTTON_CLICKED,
        NoticeInteraction.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface NoticeInteraction {
        int SHOWN = 0;
        int ACKNOWLEDGED = 1;
        int DISMISSED = 2;
        int LINK_BUTTON_CLICKED = 3;
        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/personal_context/enums.xml:PopupNoticeInteractions)

    private @Nullable Delegate mDelegate;
    private @Nullable PropertyModel mModel;
    private @Nullable BottomSheetFocusHelper mBottomSheetFocusHelper;
    private boolean mWasDismissed;

    void initialize(
            Delegate delegate, PropertyModel model, BottomSheetFocusHelper bottomSheetFocusHelper) {
        mDelegate = delegate;
        mModel = model;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void show() {
        mWasDismissed = false;
        assumeNonNull(mBottomSheetFocusHelper).registerForOneTimeUse();
        assumeNonNull(mModel).set(VISIBLE, true);
        recordNoticeInteraction(NoticeInteraction.SHOWN);
    }

    void hide() {
        onDismissed();
    }

    private boolean dismiss() {
        if (mWasDismissed) return false;
        mWasDismissed = true;
        assumeNonNull(mModel).set(VISIBLE, false);
        return true;
    }

    void onNoticeAcknowledged() {
        if (!dismiss()) return;
        assumeNonNull(mDelegate).onNoticeAcknowledged();
        recordNoticeInteraction(NoticeInteraction.ACKNOWLEDGED);
    }

    void onSettingsLinkClicked() {
        if (!dismiss()) return;
        assumeNonNull(mDelegate).onSettingsLinkClicked();
        recordNoticeInteraction(NoticeInteraction.LINK_BUTTON_CLICKED);
    }

    void onDismissed() {
        if (!dismiss()) return;
        if (mDelegate != null) {
            mDelegate.onDismissed();
        }
        recordNoticeInteraction(NoticeInteraction.DISMISSED);
    }

    void destroy() {
        mDelegate = null;
        mModel = null;
        mBottomSheetFocusHelper = null;
    }

    private void recordNoticeInteraction(@NoticeInteraction int interaction) {
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_INTERACTIONS_HISTOGRAM, interaction, NoticeInteraction.COUNT);
    }
}
