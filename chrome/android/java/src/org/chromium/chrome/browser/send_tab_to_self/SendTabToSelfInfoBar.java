// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarCompactLayout;

/**
 * This infobar is shown to let users know they have a shared tab from another
 * device that can be opened on this one.
 */
public class SendTabToSelfInfoBar extends InfoBar {
    public SendTabToSelfInfoBar() {
        // TODO(crbug.com/949233): Update this to the right icon
        super(R.drawable.infobar_chrome, R.color.default_icon_color_blue, null, null);
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        new InfoBarCompactLayout.MessageBuilder(layout)
                .withText(R.string.send_tab_to_self_infobar_message)
                .withLink(R.string.send_tab_to_self_infobar_message_url, view -> onLinkClicked())
                .buildAndInsert();
    }

    @CalledByNative
    private static SendTabToSelfInfoBar create() {
        return new SendTabToSelfInfoBar();
    }

    @Override
    public void onLinkClicked() {
        // TODO(crbug.com/944602): Add support for opening the link. Figure out
        // whether the logic should live here or in the delegate.
    }
}
