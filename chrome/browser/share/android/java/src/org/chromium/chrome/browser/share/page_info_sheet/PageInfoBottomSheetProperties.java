// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import org.chromium.ui.modelutil.PropertyModel;

/** Data properties for the page info bottom sheet contents. */
final class PageInfoBottomSheetProperties {
    private PageInfoBottomSheetProperties() {}

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder();
    }
}
