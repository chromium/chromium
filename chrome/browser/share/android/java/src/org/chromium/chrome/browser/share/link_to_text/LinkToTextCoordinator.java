// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.content.Context;
import android.net.Uri;
import android.view.LayoutInflater;
import android.widget.TextView;

import org.chromium.blink.mojom.TextFragmentSelectorProducer;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.services.service_manager.InterfaceProvider;
import org.chromium.ui.widget.Toast;

/**
 * Handles the Link To Text action in the Sharing Hub.
 */
public class LinkToTextCoordinator extends EmptyTabObserver {
    private static final String SHARE_TEXT_TEMPLATE = "\"%s\"\n";
    private static final String TEXT_FRAGMENT_PREFIX = ":~:text=";
    private static final String INVALID_SELECTOR = "";
    private final Context mContext;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mVisibleUrl;
    private final String mSelectedText;
    private final Tab mTab;

    private TextFragmentSelectorProducer mProducer;
    private boolean mCancelRequest;

    public LinkToTextCoordinator(Context context, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback, String visibleUrl,
            String selectedText) {
        mContext = context;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mVisibleUrl = visibleUrl;
        mSelectedText = selectedText;
        mTab = tab;
        mTab.addObserver(this);
        mCancelRequest = false;

        requestSelector();
    }

    public void onSelectorReady(String selector) {
        if (mCancelRequest) return;

        ShareParams params =
                new ShareParams
                        .Builder(mTab.getWindowAndroid(), /*title=*/"", getUrlToShare(selector))
                        .setText(String.format(SHARE_TEXT_TEMPLATE, mSelectedText))
                        .build();
        mChromeOptionShareCallback.showThirdPartyShareSheet(
                params, new ChromeShareExtras.Builder().build(), System.currentTimeMillis());

        if (selector.isEmpty()) {
            // TODO(gayane): Android toast should be replace by another toast like UI which allows
            // custom positioning as |setView| and |setGravity| are deprecated starting API 30.
            String toastMessage =
                    mContext.getResources().getString(R.string.link_to_text_failure_toast_message);
            LayoutInflater inflater = LayoutInflater.from(mContext);
            TextView text = (TextView) inflater.inflate(R.layout.custom_toast_layout, null);
            text.setText(toastMessage);
            text.announceForAccessibility(toastMessage);

            Toast toast = new Toast(mContext);
            toast.setView(text);
            toast.setDuration(Toast.LENGTH_SHORT);
            toast.setGravity(toast.getGravity(), toast.getXOffset(),
                    mContext.getResources().getDimensionPixelSize(R.dimen.y_offset));
            toast.show();
        }
    }

    public void requestSelector() {
        if (mTab.getWebContents().getMainFrame() != mTab.getWebContents().getFocusedFrame()) {
            LinkToTextMetricsBridge.logGenerateErrorIFrame();
            onSelectorReady(INVALID_SELECTOR);
            return;
        }

        InterfaceProvider interfaces = mTab.getWebContents().getMainFrame().getRemoteInterfaces();
        mProducer = interfaces.getInterface(TextFragmentSelectorProducer.MANAGER);
        mProducer.generateSelector(new TextFragmentSelectorProducer.GenerateSelectorResponse() {
            @Override
            public void call(String selector) {
                onSelectorReady(selector);
                cleanup();
            }
        });
    }

    public String getUrlToShare(String selector) {
        String url = mVisibleUrl;
        if (!selector.isEmpty()) {
            // Set the fragment which will also remove existing fragment, including text fragments.
            Uri uri = Uri.parse(url);
            url = uri.buildUpon().encodedFragment(TEXT_FRAGMENT_PREFIX + selector).toString();
        }
        return url;
    }

    // Discard results if tab is not on foreground anymore.
    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        LinkToTextMetricsBridge.logGenerateErrorTabHidden();
        cleanup();
    }

    // Discard results if tab content is changed by typing new URL in omnibox.
    @Override
    public void onUpdateUrl(Tab tab, String url) {
        LinkToTextMetricsBridge.logGenerateErrorOmniboxNavigation();
        cleanup();
    }

    // Discard results if tab content crashes.
    @Override
    public void onCrash(Tab tab) {
        LinkToTextMetricsBridge.logGenerateErrorTabCrash();
        cleanup();
    }

    private void cleanup() {
        // TODO(gayane): Consider canceling request in renderer.
        if (mProducer != null) mProducer.close();
        mCancelRequest = true;
        mTab.removeObserver(this);
    }
}