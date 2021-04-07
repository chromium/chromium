// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

/**
 * Handles the Link To Text action in the Sharing Hub.
 */
public class LinkToTextCoordinator extends EmptyTabObserver {
    @IntDef({LinkGeneration.TEXT, LinkGeneration.LINK, LinkGeneration.FAILURE})
    public @interface LinkGeneration {
        int TEXT = 0;
        int LINK = 1;
        int FAILURE = 2;
    }

    private static final String SHARE_TEXT_TEMPLATE = "\"%s\"\n";
    private static final String TEXT_FRAGMENT_PREFIX = ":~:text=";
    private static final String INVALID_SELECTOR = "";
    private static final long TIMEOUT_MS = 50;
    private final Context mContext;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mVisibleUrl;
    private final String mSelectedText;
    private final Tab mTab;
    private final ChromeShareExtras mChromeShareExtras;
    private final long mShareStartTime;
    private final ShareParams mShareTextParams;
    private final long mRequestSelectorStartTime;

    private ShareParams mShareLinkParams;
    private TextFragmentReceiver mProducer;
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
        mChromeShareExtras = null;
        mShareStartTime = 0;
        mShareTextParams = null;
        mRequestSelectorStartTime = System.currentTimeMillis();

        requestSelector();
    }

    // Constructor for preemptive link-to-text generation.
    public LinkToTextCoordinator(ShareParams shareParams, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ChromeShareExtras chromeShareExtras, long shareStartTime, String visibleUrl) {
        mShareTextParams = shareParams;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mChromeShareExtras = chromeShareExtras;
        mShareStartTime = shareStartTime;
        mVisibleUrl = visibleUrl;
        mSelectedText = shareParams.getText();
        mTab.addObserver(this);
        mCancelRequest = false;
        mContext = null;

        mRequestSelectorStartTime = System.currentTimeMillis();
        requestSelector();
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> timeout(), TIMEOUT_MS);
    }

    public ShareParams getShareParams(@LinkGeneration int linkGeneration) {
        switch (linkGeneration) {
            case LinkGeneration.LINK:
                return mShareLinkParams;
            default:
                return mShareTextParams;
        }
    }

    public void onSelectorReady(String selector) {
        RecordHistogram.recordTimesHistogram(
                "Sharing.SharingHubAndroid.SharedHighlights.TimeToGetLinkToText",
                System.currentTimeMillis() - mRequestSelectorStartTime);
        if (mCancelRequest) return;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            mShareLinkParams = selector.isEmpty()
                    ? null
                    : new ShareParams
                              .Builder(mTab.getWindowAndroid(), /*title=*/"",
                                      getUrlToShare(selector))
                              .setText(String.format(SHARE_TEXT_TEMPLATE, mSelectedText))
                              .setLinkToTextSuccessful(true)
                              .build();

            mChromeOptionShareCallback.showShareSheet(
                    getShareParams(
                            selector.isEmpty() ? LinkGeneration.FAILURE : LinkGeneration.LINK),
                    mChromeShareExtras, mShareStartTime);
            cleanup();
            return;
        }

        ShareParams params =
                new ShareParams
                        .Builder(mTab.getWindowAndroid(), /*title=*/"", getUrlToShare(selector))
                        .setText(String.format(SHARE_TEXT_TEMPLATE, mSelectedText))
                        .build();

        mChromeOptionShareCallback.showThirdPartyShareSheet(params,
                new ChromeShareExtras.Builder().setIsUserHighlightedText(true).build(),
                System.currentTimeMillis());

        if (selector.isEmpty()) {
            // TODO(gayane): Android toast should be replace by another toast like UI which allows
            // custom positioning as |setView| and |setGravity| are deprecated starting API 30.
            String toastMessage =
                    mContext.getResources().getString(R.string.link_to_text_failure_toast_message);
            Toast toast = Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT);
            toast.setGravity(toast.getGravity(), toast.getXOffset(),
                    mContext.getResources().getDimensionPixelSize(
                            R.dimen.y_offset_thirdparty_sharesheet));
            toast.show();
        }
        // After generation results are communicated to users, cleanup to remove tab listener.
        cleanup();
    }

    public void requestSelector() {
        if (!LinkToTextBridge.shouldOfferLinkToText(new GURL(mVisibleUrl))) {
            LinkToTextBridge.logGenerateErrorBlockList();
            onSelectorReady(INVALID_SELECTOR);
            return;
        }

        if (mTab.getWebContents().getMainFrame() != mTab.getWebContents().getFocusedFrame()) {
            LinkToTextBridge.logGenerateErrorIFrame();
            onSelectorReady(INVALID_SELECTOR);
            return;
        }

        mProducer = mTab.getWebContents().getMainFrame().getInterfaceToRendererFrame(
                TextFragmentReceiver.MANAGER);
        mProducer.requestSelector(new TextFragmentReceiver.RequestSelectorResponse() {
            @Override
            public void call(String selector) {
                onSelectorReady(selector);
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
        LinkToTextBridge.logGenerateErrorTabHidden();
        cleanup();
    }

    // Discard results if tab content is changed by typing new URL in omnibox.
    @Override
    public void onUpdateUrl(Tab tab, GURL url) {
        LinkToTextBridge.logGenerateErrorOmniboxNavigation();
        cleanup();
    }

    // Discard results if tab content crashes.
    @Override
    public void onCrash(Tab tab) {
        LinkToTextBridge.logGenerateErrorTabCrash();
        cleanup();
    }

    private void cleanup() {
        if (mProducer != null) {
            mProducer.cancel();
            mProducer.close();
        }
        mCancelRequest = true;
        mTab.removeObserver(this);
    }

    private void timeout() {
        if (!mCancelRequest) {
            LinkToTextBridge.logGenerateErrorTimeout();
            onSelectorReady(INVALID_SELECTOR);
        }
    }
}
