// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;

/**
 * The Save Password infobar asks the user whether they want to save the password for the site.
 */
public class SavePasswordInfoBar extends ConfirmInfoBar {
    private final String mDetailsMessage;

    @CalledByNative
    private static InfoBar show(int enumeratedIconId, String message, String detailsMessage,
            String primaryButtonText, String secondaryButtonText) {
        return new SavePasswordInfoBar(ResourceId.mapToDrawableId(enumeratedIconId), message,
                detailsMessage, primaryButtonText, secondaryButtonText);
    }

    private SavePasswordInfoBar(int iconDrawbleId, String message, String detailsMessage,
            String primaryButtonText, String secondaryButtonText) {
        super(iconDrawbleId, R.color.infobar_icon_drawable_color, null, message, null,
                primaryButtonText, secondaryButtonText);
        mDetailsMessage = detailsMessage;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (!TextUtils.isEmpty(mDetailsMessage)) {
            InfoBarControlLayout detailsMessageLayout = layout.addControlLayout();
            detailsMessageLayout.addDescription(mDetailsMessage);
        }
    }
}
