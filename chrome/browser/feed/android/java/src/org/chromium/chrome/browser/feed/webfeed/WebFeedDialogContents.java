// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/** Class containing all data that customizes the contents displayed in the dialog. */
class WebFeedDialogContents {
    final String mTitle;
    final String mDetails;
    final String mPrimaryButtonText;
    final @Nullable String mSecondaryButtonText;
    final @IdRes int mIllustrationId;
    final Callback<Integer> mButtonClickCallback;

    /**
     * Constructs an instance of {@link WebFeedDialogContents}.
     *
     * @param title The title of the dialog, to be displayed below the image.
     * @param details The details text to be displayed under the title.
     * @param illustrationId The resource id of the image displayed above the title.
     * @param primaryButtonText The text of the primary button.
     * @param secondaryButtonText The text of the secondary button, or null if there shouldn't be a
     *        secondary button.
     * @param buttonClickCallback The callback handling clicks on the primary and secondary buttons.
     *        It takes the type of the button as a parameter.
     */
    public WebFeedDialogContents(
            String title,
            String details,
            int illustrationId,
            String primaryButtonText,
            @Nullable String secondaryButtonText,
            Callback<Integer> buttonClickCallback) {
        mTitle = title;
        mDetails = details;
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mIllustrationId = illustrationId;
        mButtonClickCallback = buttonClickCallback;
    }
}
