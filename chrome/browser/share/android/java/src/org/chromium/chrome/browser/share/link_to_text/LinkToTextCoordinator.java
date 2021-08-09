// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Handles the Link To Text action in the Sharing Hub.
 */
public class LinkToTextCoordinator extends EmptyTabObserver {
    @IntDef({LinkGeneration.TEXT, LinkGeneration.LINK, LinkGeneration.FAILURE, LinkGeneration.MAX})
    public @interface LinkGeneration {
        int TEXT = 0;
        int LINK = 1;
        int FAILURE = 2;
        int MAX = 3;
    }

    private static final String SHARE_TEXT_TEMPLATE = "\"%s\"\n";
    private static final String TEXT_FRAGMENT_PREFIX = ":~:text=";
    private static final String INVALID_SELECTOR = "";
    private static final long TIMEOUT_MS = 100;
    private static final long AMP_TIMEOUT_MS = 200;
    private static final Set<String> AMP_VIEWER_DOMAINS =
            new HashSet<>(Arrays.asList("google.com/amp/", "bing.com/amp"));
    private static final int LENGTH_AMP_DOMAIN = 15;
    private static final int PREVIEW_MAX_LENGTH = 35;
    private static final int PREVIEW_SELECTED_TEXT_CUTOFF_LENGTH = 32;
    private static final String PREVIEW_ELLIPSIS = "...";
    private final Context mContext;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final Tab mTab;
    private final ChromeShareExtras mChromeShareExtras;
    private final long mShareStartTime;

    private String mShareUrl;
    private TextFragmentReceiver mProducer;
    private boolean mCancelRequest;
    private String mSelectedText;
    private ShareParams mShareLinkParams;
    private ShareParams mShareTextParams;

    /**
     * Constructor for selected link-to-text.
     */
    public LinkToTextCoordinator(Context context, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback, String visibleUrl,
            String selectedText) {
        this(context, tab, chromeOptionShareCallback, /*chromeShareExtras=*/null,
                /*shareStartTime=*/0, visibleUrl, selectedText);
    }

    /**
     * Constructor for preemptive link-to-text generation.
     */
    public LinkToTextCoordinator(ShareParams shareParams, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ChromeShareExtras chromeShareExtras, long shareStartTime, String visibleUrl) {
        this(/*context=*/null, tab, chromeOptionShareCallback, chromeShareExtras, shareStartTime,
                visibleUrl, shareParams.getText());
    }

    private LinkToTextCoordinator(Context context, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ChromeShareExtras chromeShareExtras, long shareStartTime, String visibleUrl,
            String selectedText) {
        mContext = context;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mChromeShareExtras = chromeShareExtras;
        mShareStartTime = shareStartTime;
        mShareUrl = visibleUrl;
        mSelectedText = selectedText;

        mTab.addObserver(this);
        mCancelRequest = false;
    }

    public ShareParams getShareParams(@LinkToggleState int linkToggleState) {
        if (linkToggleState == LinkToggleState.LINK) {
            return mShareLinkParams;
        }
        return mShareTextParams;
    }

    public void shareLinkToText() {
        if (mChromeShareExtras.isReshareHighlightedText()) {
            reshareHighlightedText();
        } else {
            startRequestSelector();
        }
    }

    @VisibleForTesting
    void onSelectorReady(String selector) {
        if (mCancelRequest) return;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            showPreemptiveShareSheet(selector);
            return;
        }

        ShareParams params =
                new ShareParams
                        .Builder(mTab.getWindowAndroid(), /*title=*/"", getUrlToShare(selector))
                        .setText(mSelectedText, SHARE_TEXT_TEMPLATE)
                        .setPreviewText(getPreviewText(), SHARE_TEXT_TEMPLATE)
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

    @VisibleForTesting
    String getPreviewText() {
        if (mSelectedText.length() <= PREVIEW_MAX_LENGTH) {
            return mSelectedText;
        }

        StringBuilder sb = new StringBuilder();
        sb.append(mSelectedText.substring(0, PREVIEW_SELECTED_TEXT_CUTOFF_LENGTH));
        sb.append(PREVIEW_ELLIPSIS);
        return sb.toString();
    }

    private void showPreemptiveShareSheet(String selector) {
        mShareLinkParams = selector.isEmpty()
                ? null
                : new ShareParams
                          .Builder(mTab.getWindowAndroid(), /*title=*/"", getUrlToShare(selector))
                          .setText(mSelectedText, SHARE_TEXT_TEMPLATE)
                          .setPreviewText(getPreviewText(), SHARE_TEXT_TEMPLATE)
                          .setLinkToTextSuccessful(true)
                          .build();
        mShareTextParams =
                new ShareParams.Builder(mTab.getWindowAndroid(), /*title=*/"", /*url=*/"")
                        .setText(mSelectedText)
                        .setLinkToTextSuccessful(!selector.isEmpty())
                        .build();
        mChromeOptionShareCallback.showShareSheet(
                getShareParams(selector.isEmpty() ? LinkToggleState.NO_LINK : LinkToggleState.LINK),
                mChromeShareExtras, mShareStartTime);
        cleanup();
    }

    private void startRequestSelector() {
        if (!LinkToTextBridge.shouldOfferLinkToText(new GURL(mShareUrl))) {
            LinkToTextBridge.logGenerateErrorBlockList();
            onSelectorReady(INVALID_SELECTOR);
            return;
        }

        if (mTab.getWebContents().getMainFrame() != mTab.getWebContents().getFocusedFrame()) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SHARED_HIGHLIGHTING_AMP)
                    && isAmpUrl(mShareUrl)) {
                PostTask.postDelayedTask(
                        UiThreadTaskTraits.DEFAULT, () -> timeout(), AMP_TIMEOUT_MS);
                requestSelectorForCanonicalUrl();
                return;
            }

            LinkToTextBridge.logGenerateErrorIFrame();
            onSelectorReady(INVALID_SELECTOR);
            return;
        }

        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> timeout(), TIMEOUT_MS);
        requestSelectorForCanonicalUrl();
    }

    private void reshareHighlightedText() {
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> timeout(), TIMEOUT_MS);
        setTextFragmentReceiver();
        mProducer.extractTextFragmentsMatches(
                new TextFragmentReceiver.ExtractTextFragmentsMatchesResponse() {
                    @Override
                    public void call(String[] matches) {
                        mSelectedText = String.join(",", matches);
                        onGetExistingSelectors();
                    }
                });
    }

    // Request text fragment selectors for existing highlights
    public void onGetExistingSelectors() {
        mProducer.getExistingSelectors(new TextFragmentReceiver.GetExistingSelectorsResponse() {
            @Override
            public void call(String[] text) {
                onSelectorReady(String.join("", text));
            }
        });
    }

    @VisibleForTesting
    String getUrlToShare(String selector) {
        String url = mShareUrl;
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

    private void requestSelector() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            LinkToTextMetricsHelper.recordLinkToTextDiagnoseStatus(
                    LinkToTextMetricsHelper.LinkToTextDiagnoseStatus.REQUEST_SELECTOR);
        }

        setTextFragmentReceiver();
        mProducer.requestSelector(new TextFragmentReceiver.RequestSelectorResponse() {
            @Override
            public void call(String selector) {
                if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
                    LinkToTextMetricsHelper.recordLinkToTextDiagnoseStatus(
                            LinkToTextMetricsHelper.LinkToTextDiagnoseStatus.SELECTOR_RECEIVED);
                }
                onSelectorReady(selector);
            }
        });
    }

    public boolean isAmpUrl(String url) {
        if (url.startsWith("www.", 8)) {
            if (url.length() - 12 < LENGTH_AMP_DOMAIN) return false;
            return AMP_VIEWER_DOMAINS.contains(url.substring(12, 12 + LENGTH_AMP_DOMAIN));
        } else if (url.startsWith("m.", 8)) {
            if (url.length() - 10 < LENGTH_AMP_DOMAIN) return false;
            return AMP_VIEWER_DOMAINS.contains(url.substring(10, 10 + LENGTH_AMP_DOMAIN));
        } else if (url.startsWith("mobile.", 8)) {
            if (url.length() - 15 < LENGTH_AMP_DOMAIN) return false;
            return AMP_VIEWER_DOMAINS.contains(url.substring(15, 15 + LENGTH_AMP_DOMAIN));
        }
        return false;
    }

    private void requestSelectorForCanonicalUrl() {
        mTab.getWebContents().getMainFrame().getCanonicalUrlForSharing(new Callback<GURL>() {
            @Override
            public void onResult(GURL result) {
                if (!result.isEmpty()) {
                    mShareUrl = result.getSpec();
                }
                requestSelector();
            }
        });
    }

    private void setTextFragmentReceiver() {
        mProducer = mChromeShareExtras.getRenderFrameHost() != null
                ? mChromeShareExtras.getRenderFrameHost().getInterfaceToRendererFrame(
                        TextFragmentReceiver.MANAGER)
                : mTab.getWebContents().getMainFrame().getInterfaceToRendererFrame(
                        TextFragmentReceiver.MANAGER);
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
