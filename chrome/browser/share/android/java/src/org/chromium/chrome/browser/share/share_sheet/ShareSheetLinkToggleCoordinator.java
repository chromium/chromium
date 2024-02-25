// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.url.GURL;

/** Coordinates toggling link-sharing on and off on the share sheet. */
public class ShareSheetLinkToggleCoordinator {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({LinkToggleState.LINK, LinkToggleState.NO_LINK, LinkToggleState.COUNT})
    public @interface LinkToggleState {
        int LINK = 0;
        int NO_LINK = 1;

        int COUNT = 2;
    }

    private final LinkToTextCoordinator mLinkToTextCoordinator;

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
     * @param linkToTextCoordinator The {@link LinkToTextCoordinator} to share highlighted text.
     */
    ShareSheetLinkToggleCoordinator(
            ShareParams shareParams,
            ChromeShareExtras chromeShareExtras,
            LinkToTextCoordinator linkToTextCoordinator) {
        mLinkToTextCoordinator = linkToTextCoordinator;
        setShareParamsAndExtras(shareParams, chromeShareExtras);
    }

    /** Sets the {@link ShareParams} and {@link ChromeShareExtras}. */
    void setShareParamsAndExtras(ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        mShareParams = shareParams;
        mChromeShareExtras = chromeShareExtras;
        mUrl = chromeShareExtras.getContentUrl();
        mShouldEnableLinkToTextToggle =
                mLinkToTextCoordinator != null
                        && chromeShareExtras.getDetailedContentType()
                                == DetailedContentType.HIGHLIGHTED_TEXT;
        mShouldEnableGenericToggle =
                mChromeShareExtras.getDetailedContentType() != DetailedContentType.NOT_SPECIFIED
                        && mUrl != null
                        && !mUrl.isEmpty();
    }

    /** Returns the {@link ShareParams} associated with the {@link LinkToggleState}. */
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

    /** Returns the {@link ShareParams} associated with the default toggle status. */
    ShareParams getDefaultShareParams() {
        return getShareParams(
                shouldEnableToggleByDefault() ? LinkToggleState.LINK : LinkToggleState.NO_LINK);
    }

    boolean shouldShowToggle() {
        return mShouldEnableLinkToTextToggle || mShouldEnableGenericToggle;
    }

    boolean shouldEnableToggleByDefault() {
        return DetailedContentType.HIGHLIGHTED_TEXT == mChromeShareExtras.getDetailedContentType();
    }
}
