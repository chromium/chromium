// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

/**
 * Holds information controlling whether to show image clickthrough dialog and
 * how to customize the dialog.
 */
/*package*/ class ImageClickthroughData {
    private final boolean mAllowClickthrough;
    private final String mDescription;
    private final String mPositiveText;
    private final String mNegativeText;
    private final String mClickthroughUrl;

    ImageClickthroughData(boolean allowClickthrough, String description, String positiveText,
            String negativeText, String clickthroughUrl) {
        mAllowClickthrough = allowClickthrough;
        mDescription = (description == null) ? "" : description;
        mPositiveText = (positiveText == null) ? "" : positiveText;
        mNegativeText = (negativeText == null) ? "" : negativeText;
        mClickthroughUrl = (clickthroughUrl == null) ? "" : clickthroughUrl;
    }

    boolean getAllowClickthrough() {
        return mAllowClickthrough;
    }

    /**
     * The description text in the clickthrough dialog.
     */
    String getDescription() {
        return mDescription;
    }

    /**
     * The text appear on positive button of clickthrough dialog.
     */
    String getPositiveText() {
        return mPositiveText;
    }

    /**
     * The text appear on negative button of clickthrough dialog.
     */
    String getNegativeText() {
        return mNegativeText;
    }

    /**
     * The url to present when user did choose to click through.
     */
    String getClickthroughUrl() {
        return mClickthroughUrl;
    }
}
