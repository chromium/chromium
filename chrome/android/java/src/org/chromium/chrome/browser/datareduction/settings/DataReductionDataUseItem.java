// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction.settings;

import android.content.Context;

import org.chromium.chrome.browser.util.FileSizeUtil;

/**
 * Stores the data used and saved by a hostname.
 */
public class DataReductionDataUseItem {
    private String mHostname;
    private long mDataUsed;
    private long mOriginalSize;

    /**
     * Constructor for a DataReductionDataUseItem which associates a hostname with its data usage
     * and savings.
     *
     * @param hostname The hostname associated with this data usage.
     * @param dataUsed The amount of data used by the host.
     * @param originalSize The original size of the data.
     */
    public DataReductionDataUseItem(String hostname, long dataUsed, long originalSize) {
        mHostname = hostname;
        mDataUsed = dataUsed;
        mOriginalSize = originalSize;
    }

    /**
     * Returns the hostname for this data use item.
     * @return The hostname.
     */
    public String getHostname() {
        return mHostname;
    }

    /**
     * Returns the amount of data used by the associated hostname.
     * @return The data used.
     */
    public long getDataUsed() {
        return mDataUsed;
    }

    /**
     * Returns the amount of data saved by the associated hostname. If the data used is greater than
     * data saved, return zero savings so that negative savings are not shown, which users found to
     * be confusing.
     * @return The data saved.
     */
    public long getDataSaved() {
        if (mDataUsed > mOriginalSize) return 0;
        return mOriginalSize - mDataUsed;
    }

    /**
     * Returns a formatted String of the data used by the associated hostname.
     * @param context An Android context.
     * @return A formatted string of the data used.
     */
    public CharSequence getFormattedDataUsed(Context context) {
        return FileSizeUtil.formatFileSize(context, mDataUsed);
    }

    /**
     * Returns a formatted String of the data saved by the associated hostname. If the data used is
     * greater than data saved, return zero savings so that negative savings are not shown, which
     * users found to be confusing.
     * @param context An Android context.
     * @return A formatted string of the data saved.
     */
    public CharSequence getFormattedDataSaved(Context context) {
        return FileSizeUtil.formatFileSize(context, Math.max(0, mOriginalSize - mDataUsed));
    }
}