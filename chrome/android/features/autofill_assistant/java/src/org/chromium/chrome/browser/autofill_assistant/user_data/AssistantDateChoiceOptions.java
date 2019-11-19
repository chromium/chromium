// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

/**
 * Represents a request to let the user choose a single date/time value.
 */
public class AssistantDateChoiceOptions {
    private final AssistantDateTime mInitialValue;
    private final AssistantDateTime mMinValue;
    private final AssistantDateTime mMaxValue;

    /**
     *
     * @param initialValue The initial value for the date/time value.
     * @param minValue The minimum allowed value for the date/time value.
     * @param maxValue The maximum allowed value for the date/time value.
     */
    public AssistantDateChoiceOptions(AssistantDateTime initialValue, AssistantDateTime minValue,
            AssistantDateTime maxValue) {
        mInitialValue = initialValue;
        mMinValue = minValue;
        mMaxValue = maxValue;
    }

    AssistantDateTime getInitialValue() {
        return mInitialValue;
    }

    AssistantDateTime getMinValue() {
        return mMinValue;
    }

    AssistantDateTime getMaxValue() {
        return mMaxValue;
    }
}
