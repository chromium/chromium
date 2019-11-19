// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;

/**
 * An InfoBar that lets the user know that a Preview page has been loaded, and gives the user
 * a link to reload the original page. This InfoBar will only be shown once per page load.
 */
public class PreviewsInfoBar extends ConfirmInfoBar {
    private final String mTimestampText;

    @CalledByNative
    private static InfoBar show(
            int enumeratedIconId, String message, String linkText, String timestampText) {
        return new PreviewsInfoBar(
                ResourceId.mapToDrawableId(enumeratedIconId), message, linkText, timestampText);
    }

    private PreviewsInfoBar(
            int iconDrawbleId, String message, String linkText, String timestampText) {
        super(iconDrawbleId, R.color.infobar_icon_drawable_color, null, message, linkText, null,
                null);
        mTimestampText = timestampText;
    }

    @Override
    protected CharSequence getAccessibilityMessage(CharSequence defaultMessage) {
        return getContext().getString(R.string.previews_infobar_accessibility_title);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (mTimestampText.isEmpty()) return;
        layout.getMessageLayout().addDescription(mTimestampText);
    }
}
