// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The interface to be implemented by the individual feature to show a View in the
 * CommerceBottomSheet.
 */
public interface CommerceBottomSheetContentProvider {

    /**
     * Request the content to show in the CommerceBottomSheetContent.
     *
     * @param contentReadyCallback The callback that will run after the feature is finished
     *     gathering all content. PropertyModel can be null if there's nothing to show. Otherwise,
     *     the PropertyModel should contain the following PropertyKey: TYPE, HAS_TITLE, TITLE_TEXT,
     *     and CONTENT_VIEW.
     */
    void requestContent(Callback<PropertyModel> contentReadyCallback);

    /** Called when the BottomSheet hides. */
    void hideContentView();
}
