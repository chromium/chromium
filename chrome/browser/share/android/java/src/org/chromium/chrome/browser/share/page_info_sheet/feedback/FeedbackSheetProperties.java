// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet.feedback;

import android.view.View.OnClickListener;
import android.widget.RadioGroup;

import org.chromium.chrome.browser.share.page_info_sheet.feedback.FeedbackSheetCoordinator.FeedbackOption;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

import java.util.List;

final class FeedbackSheetProperties {

    static final ReadableObjectPropertyKey<RadioGroup.OnCheckedChangeListener>
            OPTION_SELECTED_CALLBACK = new ReadableObjectPropertyKey();
    static final ReadableObjectPropertyKey<List<FeedbackOption>> AVAILABLE_OPTIONS =
            new ReadableObjectPropertyKey();
    static final ReadableObjectPropertyKey<OnClickListener> ON_ACCEPT_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_CANCEL_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey IS_ACCEPT_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    private FeedbackSheetProperties() {}

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                OPTION_SELECTED_CALLBACK,
                AVAILABLE_OPTIONS,
                ON_ACCEPT_CLICKED,
                ON_CANCEL_CLICKED,
                IS_ACCEPT_BUTTON_ENABLED,
            };

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(ALL_KEYS);
    }
}
