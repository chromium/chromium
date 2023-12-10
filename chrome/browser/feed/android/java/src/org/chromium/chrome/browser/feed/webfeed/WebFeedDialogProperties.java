// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import org.chromium.ui.modelutil.PropertyModel;

/** Data properties for the WebFeed modal dialog. */
final class WebFeedDialogProperties {
    // Illustration drawable resource id for the WebFeed dialog.
    static final PropertyModel.ReadableIntPropertyKey ILLUSTRATION =
            new PropertyModel.ReadableIntPropertyKey();

    // Title that appears below the illustration.
    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    // Multiline explanation text displayed under the illustration.
    static final PropertyModel.ReadableObjectPropertyKey<String> DETAILS =
            new PropertyModel.ReadableObjectPropertyKey<>();

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(ILLUSTRATION, TITLE, DETAILS);
    }

    private WebFeedDialogProperties() {}
}
