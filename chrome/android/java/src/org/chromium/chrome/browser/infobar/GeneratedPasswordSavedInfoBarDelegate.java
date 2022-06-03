// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.infobars.InfoBar;

/**
 * Provides JNI methods for the infobar to notify that the generated password was saved.
 */
public class GeneratedPasswordSavedInfoBarDelegate {
    /**
     * Creates and shows the infobar to notify that the generated password was saved.
     * @param iconId Enum ID corresponding to the icon that the infobar will show.
     * @param messageText Message to display in the infobar.
     * @param detailsMessageText Message containing additional details to be displayed in the
     * infobar.
     * @param inlineLinkRangeStart The start of the range of the messageText that should be a link.
     * @param inlineLinkRangeEnd The end of the range of the messageText that should be a link.
     * @param buttonLabel String to display on the button.
     */
    @CalledByNative
    private static InfoBar show(int iconId, String messageText, String detailsMessageText,
            int inlineLinkRangeStart, int inlineLinkRangeEnd, String buttonLabel) {
        return new GeneratedPasswordSavedInfoBar(iconId, messageText, detailsMessageText,
                inlineLinkRangeStart, inlineLinkRangeEnd, buttonLabel);
    }
}
