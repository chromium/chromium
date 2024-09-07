// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.ui.modelutil.PropertyModel;

/** A listener for individual feature content. */
public interface CommerceBottomSheetContentListener {

    /**
     * Called when the content is ready to show.
     *
     * @param model The PropertyModel contains all relevant content for the CommerceBottomSheet. The
     *     model should contain the following PropertyKey: TYPE, HAS_TITLE, TITLE_TEXT, and
     *     CONTENT_VIEW.
     */
    void onContentReady(PropertyModel model);
}
