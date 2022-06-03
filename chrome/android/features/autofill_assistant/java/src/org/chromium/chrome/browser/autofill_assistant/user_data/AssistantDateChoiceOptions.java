// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import java.util.List;

/**
 * Represents a request to let the user choose a single date/time value.
 */
public class AssistantDateChoiceOptions {
    private final AssistantDateTime mMinDate;
    private final AssistantDateTime mMaxDate;
    private final List<String> mTimeSlots;

    /**
     * @param minDate The minimum allowed value for the date/time value.
     * @param maxDate The maximum allowed value for the date/time value.
     * @param timeSlots The list of allowed time slots to pick from.
     */
    public AssistantDateChoiceOptions(
            AssistantDateTime minDate, AssistantDateTime maxDate, List<String> timeSlots) {
        mMinDate = minDate;
        mMaxDate = maxDate;
        mTimeSlots = timeSlots;
    }

    AssistantDateTime getMinDate() {
        return mMinDate;
    }

    AssistantDateTime getMaxDate() {
        return mMaxDate;
    }

    List<String> getTimeSlots() {
        return mTimeSlots;
    }
}
