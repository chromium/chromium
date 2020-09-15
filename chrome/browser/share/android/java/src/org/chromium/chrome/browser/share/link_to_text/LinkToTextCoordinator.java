// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.content.Context;
import android.net.Uri;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles the Link To Text action in the Sharing Hub.
 */
public class LinkToTextCoordinator {
    private static final String SHARE_TEXT_TEMPLATE = "\"%s\"\n%s";
    private static final String TEXT_FRAGMENT_PREFIX = ":~:text=";
    private final Context mContext;
    private final WindowAndroid mWindow;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mVisibleUrl;
    private final String mSelectedText;

    public LinkToTextCoordinator(Context context, WindowAndroid window,
            ChromeOptionShareCallback chromeOptionShareCallback, String visibleUrl,
            String selectedText) {
        mContext = context;
        mWindow = window;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mVisibleUrl = visibleUrl;
        mSelectedText = selectedText;

        // TODO(1102382): Replace following line with a request to create text fragment selector and
        // pass |OnSelectorReady| as callback.
        onSelectorReady("");
    }

    public void onSelectorReady(String selector) {
        String successMessage =
                mContext.getResources().getString(R.string.link_to_text_success_message);
        String failureMessage =
                mContext.getResources().getString(R.string.link_to_text_failure_message);

        // TODO(1102382): Consider creating SharedParams on sharesheet side. In that case there will
        // be no need to keep the WindowAndroid in this class.
        String textToShare = getTextToShare(selector);
        ShareParams params = new ShareParams.Builder(mWindow, /*title=*/"", /*url=*/"")
                                     .setText(textToShare)
                                     .build();

        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mChromeOptionShareCallback.showThirdPartyShareSheetWithMessage(
                !selector.isEmpty() ? successMessage : failureMessage, params, chromeShareExtras,
                System.currentTimeMillis());
    }

    public String getTextToShare(String selector) {
        String url = mVisibleUrl;
        if (!selector.isEmpty()) {
            // Set the fragment which will also remove existing fragment, including text fragments.
            Uri uri = Uri.parse(url);
            url = uri.buildUpon().encodedFragment(TEXT_FRAGMENT_PREFIX + selector).toString();
        }
        return String.format(SHARE_TEXT_TEMPLATE, mSelectedText, url);
    }
}