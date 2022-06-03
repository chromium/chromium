// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.Locale;
import java.util.TimeZone;

/**
 * A single date/time value which directly corresponds to the proto definition in {@code
 * DateTimeProto}.
 *
 * Note that this class does not make any guarantees with respect to the validity of the represented
 * date/time.
 */
@JNINamespace("autofill_assistant")
public class AssistantDateTime {
    /** Year, e.g., 2019. */
    private int mYear;
    /** Month in [1-12]. */
    private int mMonth;
    /** Day in [1-31]. */
    private int mDay;
    /** Hour in [0-23]. */
    private int mHour;
    /** Minute in [0-59]. */
    private int mMinute;
    /** Second in [0-59]. */
    private int mSecond;

    @CalledByNative
    public AssistantDateTime(int year, int month, int day, int hour, int minute, int second) {
        set(year, month, day, hour, minute, second);
    }

    public AssistantDateTime(long millisUtc) {
        setFromUtcMillis(millisUtc);
    }

    public void setFromUtcMillis(long value) {
        GregorianCalendar calendar = new GregorianCalendar(TimeZone.getTimeZone("UTC"));
        calendar.setGregorianChange(new Date(Long.MIN_VALUE));
        calendar.setTimeInMillis(value);
        mYear = calendar.get(Calendar.YEAR);
        mMonth = calendar.get(Calendar.MONTH) + 1;
        mDay = calendar.get(Calendar.DAY_OF_MONTH);
        mHour = calendar.get(Calendar.HOUR_OF_DAY);
        mMinute = calendar.get(Calendar.MINUTE);
        mSecond = calendar.get(Calendar.SECOND);
    }

    public void set(int year, int month, int day, int hour, int minute, int second) {
        mYear = year;
        mMonth = month;
        mDay = day;
        mHour = hour;
        mMinute = minute;
        mSecond = second;
    }

    public long getTimeInMillis(Locale locale) {
        Calendar calendar = Calendar.getInstance(locale);
        calendar.clear();
        calendar.set(mYear, mMonth - 1, mDay, mHour, mMinute, mSecond);
        return calendar.getTimeInMillis();
    }

    public long getTimeInUtcMillis() {
        GregorianCalendar calendar = new GregorianCalendar(TimeZone.getTimeZone("UTC"));
        calendar.setGregorianChange(new Date(Long.MIN_VALUE));
        calendar.clear();
        calendar.set(mYear, mMonth - 1, mDay, mHour, mMinute, mSecond);
        return calendar.getTimeInMillis();
    }

    @CalledByNative
    public int getYear() {
        return mYear;
    }

    @CalledByNative
    public int getMonth() {
        return mMonth;
    }

    @CalledByNative
    public int getDay() {
        return mDay;
    }

    public int getHour() {
        return mHour;
    }

    public int getMinute() {
        return mMinute;
    }

    public int getSecond() {
        return mSecond;
    }
}