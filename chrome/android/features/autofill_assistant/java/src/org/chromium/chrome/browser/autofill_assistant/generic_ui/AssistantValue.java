// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantDateTime;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** The Java equivalent to {@code ValueProto}. */
@JNINamespace("autofill_assistant")
public class AssistantValue {
    private final String[] mStrings;
    private final boolean[] mBooleans;
    private final int[] mIntegers;
    private final List<AssistantDateTime> mDateTimes;

    AssistantValue() {
        mStrings = null;
        mBooleans = null;
        mIntegers = null;
        mDateTimes = null;
    }

    public AssistantValue(String[] strings) {
        mStrings = strings;
        mBooleans = null;
        mIntegers = null;
        mDateTimes = null;
    }

    public AssistantValue(boolean[] booleans) {
        mStrings = null;
        mBooleans = booleans;
        mIntegers = null;
        mDateTimes = null;
    }

    public AssistantValue(int[] integers) {
        mStrings = null;
        mBooleans = null;
        mIntegers = integers;
        mDateTimes = null;
    }

    public AssistantValue(List<AssistantDateTime> dateTimes) {
        mStrings = null;
        mBooleans = null;
        mIntegers = null;
        mDateTimes = dateTimes;
    }

    @CalledByNative
    public static AssistantValue create() {
        return new AssistantValue();
    }

    @CalledByNative
    public static AssistantValue createForStrings(String[] values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    public static AssistantValue createForBooleans(boolean[] values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    public static AssistantValue createForIntegers(int[] values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    public static AssistantValue createForDateTimes(List<AssistantDateTime> values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    private static List<AssistantDateTime> createDateTimeList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addDateTimeToList(List<AssistantDateTime> list, AssistantDateTime value) {
        list.add(value);
    }

    @CalledByNative
    public String[] getStrings() {
        return mStrings;
    }

    @CalledByNative
    public boolean[] getBooleans() {
        return mBooleans;
    }

    @CalledByNative
    public int[] getIntegers() {
        return mIntegers;
    }

    @CalledByNative
    public List<AssistantDateTime> getDateTimes() {
        return mDateTimes;
    }

    public static boolean isDateSingleton(AssistantValue value) {
        return value != null && value.mDateTimes != null && value.mDateTimes.size() == 1;
    }

    @CalledByNative
    private static int getListSize(List list) {
        return list.size();
    }

    @CalledByNative
    private static Object getListAt(List list, int index) {
        return list.get(index);
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == this) {
            return true;
        }
        if (!(obj instanceof AssistantValue)) {
            return false;
        }
        AssistantValue value = (AssistantValue) obj;

        return Arrays.equals(this.mStrings, value.mStrings)
                && Arrays.equals(this.mBooleans, value.mBooleans)
                && Arrays.equals(this.mIntegers, value.mIntegers);
    }
}
