// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;

/**
 * This InfoBar is shown to let the user know when the browser took action to
 * reduce the content of the page using too much memory, potentially breaking it
 * and offers them a way to decline the intervention.
 * The native caller can handle user action through {@code InfoBar::ProcessButton(int action)}
 */
public class NearOomReductionInfoBar extends InfoBar {
    @VisibleForTesting
    public NearOomReductionInfoBar() {
        super(R.drawable.infobar_chrome, R.color.infobar_icon_drawable_color, null, null);
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        new InfoBarCompactLayout.MessageBuilder(layout)
                .withText(R.string.near_oom_reduction_message)
                .withLink(R.string.near_oom_reduction_decline, view -> onLinkClicked())
                .buildAndInsert();
    }

    @CalledByNative
    private static NearOomReductionInfoBar create() {
        return new NearOomReductionInfoBar();
    }
}
