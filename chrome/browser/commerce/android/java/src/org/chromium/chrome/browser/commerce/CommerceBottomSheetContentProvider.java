// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The interface to be implemented by the individual feature to show a View in the
 * CommerceBottomSheet.
 */
@NullMarked
public interface CommerceBottomSheetContentProvider {
    /** Supported content types, the content is prioritized based on this order. */
    @IntDef({ContentType.PRICE_TRACKING, ContentType.DISCOUNTS, ContentType.PRICE_INSIGHTS})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContentType {
        int PRICE_TRACKING = 0;
        int DISCOUNTS = 1;
        int PRICE_INSIGHTS = 2;
    }

    /**
     * Request the content to show in the CommerceBottomSheetContent.
     *
     * @param contentReadyCallback The callback that will run after the feature is finished
     *     gathering all content. PropertyModel can be null if there's nothing to show. Otherwise,
     *     the PropertyModel should contain the following PropertyKey: TYPE, HAS_TITLE, TITLE_TEXT,
     *     and CONTENT_VIEW.
     */
    void requestContent(Callback<@Nullable PropertyModel> contentReadyCallback);

    /** Called when the BottomSheet hides. */
    void hideContentView();
}
