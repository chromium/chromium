// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.url.GURL;

/**
 * Coordinates toggling link-sharing on and off on the share sheet.
 */
public class ShareSheetLinkToggleCoordinator {
    @IntDef({LinkToggleState.LINK, LinkToggleState.NO_LINK, LinkToggleState.MAX})
    public @interface LinkToggleState {
        int LINK = 0;
        int NO_LINK = 1;
        int MAX = 2;
    }

    private final long mShareStartTime;
    private final LinkToTextCoordinator mLinkToTextCoordinator;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;

    private ShareParams mShareParams;
    private ChromeShareExtras mChromeShareExtras;
    private GURL mUrl;
    private boolean mShouldEnableLinkToTextToggle;
    private boolean mShouldEnableGenericToggle;

    /**
     * Constructs a new ShareSheetLinkToggleCoordinator.
     *
     * @param shareParams The original {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param shareStartTime The start time for the current share.
     * @param linkToTextCoordinator The {@link LinkToTextCoordinator} to share highlighted text.
     * @param chromeOptionShareCallback The {@link ChromeOptionShareCallback} to open the share
     *         sheet.
     */
    ShareSheetLinkToggleCoordinator(ShareParams shareParams, ChromeShareExtras chromeShareExtras,
            long shareStartTime, LinkToTextCoordinator linkToTextCoordinator,
            ChromeOptionShareCallback chromeOptionShareCallback) {
        mShareStartTime = shareStartTime;
        mLinkToTextCoordinator = linkToTextCoordinator;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        setShareParamsAndExtras(shareParams, chromeShareExtras);

        showShareSheet();
    }

    /**
     * Sets the {@link ShareParams} and {@link ChromeShareExtras}.
     */
    void setShareParamsAndExtras(ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        mShareParams = shareParams;
        mChromeShareExtras = chromeShareExtras;
        mUrl = chromeShareExtras.getContentUrl();
        mShouldEnableLinkToTextToggle =
                (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                        || ChromeFeatureList.isEnabled(ChromeFeatureList.SHARING_HUB_LINK_TOGGLE))
                && mLinkToTextCoordinator != null && chromeShareExtras.isUserHighlightedText();
        mShouldEnableGenericToggle =
                ChromeFeatureList.isEnabled(ChromeFeatureList.SHARING_HUB_LINK_TOGGLE)
                && TextUtils.isEmpty(shareParams.getUrl()) && mUrl != null && !mUrl.isEmpty();
    }

    /**
     * Returns the {@link ShareParams} associated with the {@link LinkToggleState}.
     */
    ShareParams getShareParams(@LinkToggleState int linkToggleState) {
        if (mShouldEnableLinkToTextToggle) {
            return mLinkToTextCoordinator.getShareParams(linkToggleState);
        }
        if (mShouldEnableGenericToggle) {
            if (linkToggleState == LinkToggleState.LINK) {
                mShareParams.setUrl(mUrl.getSpec());
            } else {
                mShareParams.setUrl(null);
            }
        }
        return mShareParams;
    }

    private void showShareSheet() {
        if (mShouldEnableLinkToTextToggle) {
            mLinkToTextCoordinator.shareLinkToText();
        } else if (mShouldEnableGenericToggle) {
            mChromeOptionShareCallback.showShareSheet(
                    getShareParams(LinkToggleState.LINK), mChromeShareExtras, mShareStartTime);
        } else {
            mChromeOptionShareCallback.showShareSheet(
                    mShareParams, mChromeShareExtras, mShareStartTime);
        }
    }
}
