// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Activity;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Responsible for notes main UI and its subcomponents.
 */
public class LightweightReactionsCoordinatorImpl implements LightweightReactionsCoordinator {
    private final Activity mActivity;
    private final Tab mTab;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mShareUrl;

    public LightweightReactionsCoordinatorImpl(Activity activity, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback, String shareUrl) {
        mActivity = activity;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mShareUrl = shareUrl;
    }

    @Override
    public void showDialog() {
        // No-op for now
    }
}
