// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.url.GURL;

/** Handles the Link To Text action in the Sharing Hub. */
public class LinkToTextCoordinator extends EmptyTabObserver {
    @IntDef({LinkGeneration.TEXT, LinkGeneration.LINK, LinkGeneration.FAILURE, LinkGeneration.MAX})
    public @interface LinkGeneration {
        int TEXT = 0;
        int LINK = 1;
        int FAILURE = 2;
        int MAX = 3;
    }

    @IntDef({
        RemoteRequestStatus.NONE,
        RemoteRequestStatus.REQUESTED,
        RemoteRequestStatus.COMPLETED,
        RemoteRequestStatus.CANCELLED
    })
    public @interface RemoteRequestStatus {
        int NONE = 0;
        int REQUESTED = 1;
        int COMPLETED = 2;
        int CANCELLED = 3;
    }

    private static final String SHARE_TEXT_TEMPLATE = "\"%s\"\n";
    private static final String INVALID_SELECTOR = "";
    private static final int TIMEOUT_MS = 100;
    private static final int PREVIEW_MAX_LENGTH = 35;
    private static final int PREVIEW_SELECTED_TEXT_CUTOFF_LENGTH = 32;
    private static final String PREVIEW_ELLIPSIS = "...";

    private ChromeOptionShareCallback mChromeOptionShareCallback;
    private Tab mTab;
    private ChromeShareExtras mChromeShareExtras;
    private long mShareStartTime;

    private String mShareUrl;
    private TextFragmentReceiver mProducer;
    private String mSelectedText;
    private ShareParams mShareLinkParams;
    private ShareParams mShareTextParams;
    private boolean mIncludeOriginInTitle;
    public @RemoteRequestStatus int mRemoteRequestStatus;

    @VisibleForTesting
    LinkToTextCoordinator() {}

    public LinkToTextCoordinator(
            Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ChromeShareExtras chromeShareExtras,
            long shareStartTime,
            String visibleUrl,
            String selectedText,
            boolean includeOriginInTitle) {
        initLinkToTextCoordinator(
                tab,
                chromeOptionShareCallback,
                chromeShareExtras,
                shareStartTime,
                visibleUrl,
                selectedText,
                includeOriginInTitle);
    }

    @VisibleForTesting
    void initLinkToTextCoordinator(
            Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ChromeShareExtras chromeShareExtras,
            long shareStartTime,
            String visibleUrl,
            String selectedText,
            boolean includeOriginInTitle) {
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mChromeShareExtras = chromeShareExtras;
        mShareStartTime = shareStartTime;
        mShareUrl = visibleUrl;
        mSelectedText = selectedText;
        mIncludeOriginInTitle = includeOriginInTitle;

        mTab.addObserver(this);
        mRemoteRequestStatus = RemoteRequestStatus.NONE;
    }

    public ShareParams getShareParams(@LinkToggleState int linkToggleState) {
        if (linkToggleState == LinkToggleState.LINK && mShareLinkParams != null) {
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
    public void onSelectorReady(String selector) {
        boolean isSelectorEmpty = TextUtils.isEmpty(selector);
        mShareLinkParams =
                isSelectorEmpty
                        ? null
                        : new ShareParams.Builder(
                                        mTab.getWindowAndroid(),
                                        getTitle(),
                                        LinkToTextHelper.getUrlToShare(mShareUrl, selector))
                                .setText(mSelectedText, SHARE_TEXT_TEMPLATE)
                                .setPreviewText(getPreviewText(), SHARE_TEXT_TEMPLATE)
                                .setLinkToTextSuccessful(true)
                                .build();
        mShareTextParams =
                new ShareParams.Builder(mTab.getWindowAndroid(), mTab.getTitle(), /* url= */ "")
                        .setText(mSelectedText)
                        .setLinkToTextSuccessful(!isSelectorEmpty)
                        .build();
        mChromeOptionShareCallback.showShareSheet(
                getShareParams(isSelectorEmpty ? LinkToggleState.NO_LINK : LinkToggleState.LINK),
                mChromeShareExtras,
                mShareStartTime);
    }

    @VisibleForTesting
    String getPreviewText() {
        if (mSelectedText.length() <= PREVIEW_MAX_LENGTH) {
            return mSelectedText;
        }

        return mSelectedText.substring(0, PREVIEW_SELECTED_TEXT_CUTOFF_LENGTH) + PREVIEW_ELLIPSIS;
    }

    private void startRequestSelector() {
        if (!LinkToTextBridge.shouldOfferLinkToText(new GURL(mShareUrl))) {
            completeRequestWithFailure(LinkGenerationError.BLOCK_LIST);
            return;
        }

        if (mTab.getWebContents().getMainFrame() != mTab.getWebContents().getFocusedFrame()) {
            if (!LinkToTextBridge.supportsLinkGenerationInIframe(new GURL(mShareUrl))) {
                completeRequestWithFailure(LinkGenerationError.I_FRAME);
                return;
            }
        }

        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, () -> timeout(), TIMEOUT_MS);
        requestSelector();
    }

    @VisibleForTesting
    void reshareHighlightedText() {
        setTextFragmentReceiver();
        if (mProducer == null) {
            completeReshareWithFailure(LinkToTextReshareStatus.NO_REMOTE_CONNECTION);
            return;
        }

        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, () -> timeout(), TIMEOUT_MS);
        mRemoteRequestStatus = RemoteRequestStatus.REQUESTED;
        LinkToTextHelper.extractTextFragmentsMatches(
                mProducer,
                (matches) -> {
                    mSelectedText = String.join(",", matches);
                    LinkToTextHelper.getExistingSelectorsAllFrames(
                            mTab, this::onReshareSelectorsRemoteRequestCompleted);
                });
    }

    @VisibleForTesting
    void onReshareSelectorsRemoteRequestCompleted(String selectors) {
        if (mRemoteRequestStatus == RemoteRequestStatus.CANCELLED) return;
        if (selectors.isEmpty()) {
            completeReshareWithFailure(LinkToTextReshareStatus.EMPTY_SELECTOR);
            return;
        }

        LinkToTextHelper.requestCanonicalUrl(
                mTab,
                (canonicalUrl) -> {
                    if (!canonicalUrl.isEmpty()) {
                        mShareUrl = canonicalUrl;
                    }
                    reshareRequestCompleted(selectors);
                });
    }

    @VisibleForTesting
    void reshareRequestCompleted(String selectors) {
        if (mRemoteRequestStatus == RemoteRequestStatus.CANCELLED) return;

        mRemoteRequestStatus = RemoteRequestStatus.COMPLETED;
        completeRemoteRequestWithSuccess(selectors);
    }

    // Discard results if tab is not on foreground anymore.
    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        if (mChromeShareExtras.isReshareHighlightedText()) {
            completeReshareWithFailure(LinkToTextReshareStatus.TAB_HIDDEN);
        } else {
            completeRequestWithFailure(LinkGenerationError.TAB_HIDDEN);
        }
    }

    // Discard results if tab content is changed by typing new URL in omnibox.
    @Override
    public void onUpdateUrl(Tab tab, GURL url) {
        if (mChromeShareExtras.isReshareHighlightedText()) {
            completeReshareWithFailure(LinkToTextReshareStatus.OMNIBOX_NAVIGATION);
        } else {
            completeRequestWithFailure(LinkGenerationError.OMNIBOX_NAVIGATION);
        }
    }

    // Discard results if tab content crashes.
    @Override
    public void onCrash(Tab tab) {
        if (mChromeShareExtras.isReshareHighlightedText()) {
            completeReshareWithFailure(LinkToTextReshareStatus.TAB_CRASH);
        } else {
            completeRequestWithFailure(LinkGenerationError.TAB_CRASH);
        }
    }

    @VisibleForTesting
    void onRemoteRequestCompleted(String selector, Integer error, Integer readyStatus) {
        if (mRemoteRequestStatus == RemoteRequestStatus.CANCELLED) return;

        mRemoteRequestStatus = RemoteRequestStatus.COMPLETED;
        boolean success = !selector.isEmpty();
        assert error != null;

        if (success) {
            assert error == LinkGenerationError.NONE;

            // Request canonical url when we have a successful generation.
            LinkToTextHelper.requestCanonicalUrl(
                    mTab,
                    (canonicalUrl) -> {
                        if (!canonicalUrl.isEmpty()) {
                            mShareUrl = canonicalUrl;
                        }
                        completeRemoteRequestWithSuccess(selector);
                    });
        } else {
            assert error != LinkGenerationError.NONE;
            completeRequestWithFailure(error.intValue());
        }

        assert readyStatus != null;

        @LinkGenerationStatus
        int status = success ? LinkGenerationStatus.FAILURE : LinkGenerationStatus.SUCCESS;
        LinkToTextBridge.logLinkRequestedBeforeStatus(status, readyStatus.intValue());
    }

    @VisibleForTesting
    void requestSelector() {
        LinkToTextMetricsHelper.recordLinkToTextDiagnoseStatus(
                LinkToTextMetricsHelper.LinkToTextDiagnoseStatus.REQUEST_SELECTOR);

        setTextFragmentReceiver();

        if (mProducer == null) {
            completeRequestWithFailure(LinkGenerationError.NO_REMOTE_CONNECTION);
            return;
        }

        mRemoteRequestStatus = RemoteRequestStatus.REQUESTED;
        LinkToTextHelper.requestSelector(mProducer, this::onRemoteRequestCompleted);
    }

    private void setTextFragmentReceiver() {
        if (mChromeShareExtras.getRenderFrameHost() != null
                && mChromeShareExtras.getRenderFrameHost().isRenderFrameLive()) {
            mProducer =
                    mChromeShareExtras
                            .getRenderFrameHost()
                            .getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER);
            return;
        }

        if (mTab.getWebContents() != null && mTab.getWebContents().getMainFrame() != null) {
            mProducer =
                    mTab.getWebContents()
                            .getMainFrame()
                            .getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER);
        }
    }

    private void cancel() {
        // Cancel can be called before remote task was requested requested, for example, blocklist
        // case. Cancel only if remote request was requested.
        if (mRemoteRequestStatus == RemoteRequestStatus.REQUESTED) {
            mRemoteRequestStatus = RemoteRequestStatus.CANCELLED;
            // Cancelling remote request for reshare is not implemented. Cancelling only for
            // generated selector request.
            if (!mChromeShareExtras.isReshareHighlightedText() && mProducer != null) {
                mProducer.cancel();
            }
        }
    }

    private void cleanup() {
        if (mProducer != null) {
            mProducer.close();
        }
        mTab.removeObserver(this);
    }

    @VisibleForTesting
    void timeout() {
        assert (mRemoteRequestStatus == RemoteRequestStatus.REQUESTED
                || mRemoteRequestStatus == RemoteRequestStatus.COMPLETED);

        // If the request is already completed, then ignore the timeout.
        if (mRemoteRequestStatus == RemoteRequestStatus.REQUESTED) {
            if (mChromeShareExtras.isReshareHighlightedText()) {
                completeReshareWithFailure(LinkToTextReshareStatus.TIMEOUT);
            } else {
                completeRequestWithFailure(LinkGenerationError.TIMEOUT);
            }
        }
    }

    private void completeRequestWithFailure(@LinkGenerationError int error) {
        LinkToTextBridge.logFailureMetrics(mTab.getWebContents(), error);

        switch (error) {
            case LinkGenerationError.TAB_HIDDEN:
            case LinkGenerationError.OMNIBOX_NAVIGATION:
            case LinkGenerationError.TAB_CRASH:
                break;
            default:
                onSelectorReady(INVALID_SELECTOR);
        }

        cancel();
        cleanup();
    }

    private void completeRemoteRequestWithSuccess(String selector) {
        if (mChromeShareExtras.isReshareHighlightedText()) {
            LinkToTextBridge.logLinkToTextReshareStatus(LinkToTextReshareStatus.SUCCESS);
        } else {
            LinkToTextBridge.logSuccessMetrics(mTab.getWebContents());
        }
        onSelectorReady(selector);
        cleanup();
    }

    private void completeReshareWithFailure(@LinkToTextReshareStatus int status) {
        LinkToTextBridge.logLinkToTextReshareStatus(status);

        switch (status) {
            case LinkToTextReshareStatus.TAB_HIDDEN:
            case LinkToTextReshareStatus.OMNIBOX_NAVIGATION:
            case LinkToTextReshareStatus.TAB_CRASH:
                break;
            default:
                onSelectorReady(INVALID_SELECTOR);
        }
        cancel();
        cleanup();
    }

    @VisibleForTesting
    public String getTitle() {
        if (!mIncludeOriginInTitle) return mTab.getTitle();
        String origin = new GURL(mShareUrl).getOrigin().getSpec();
        return mTab.getContext().getString(R.string.sharing_including_link_title_template, origin);
    }
}
