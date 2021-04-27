// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.os.Handler;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;

/** Mediator class for the component. */
public class MerchantTrustDetailsTabMediator {
    private final BottomSheetController mBottomSheetController;
    private WebContents mWebContents;
    private MerchantTrustDetailsSheetContent mSheetContent;
    private final int mTopControlsHeightDp;
    private Profile mProfile;
    private WebContentsDelegateAndroid mWebContentsDelegate;
    private WebContentsObserver mWebContentsObserver;
    private final MerchantTrustMetrics mMetrics;
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = 50;

    /** Creates a new instance. */
    MerchantTrustDetailsTabMediator(BottomSheetController bottomSheetController,
            int topControlsHeightDp, MerchantTrustMetrics metrics) {
        mBottomSheetController = bottomSheetController;
        mTopControlsHeightDp = topControlsHeightDp;
        mMetrics = metrics;
    }

    /**
     * Starts the navigation to the provided URL and requests the bottom sheet to be shown.
     * @param url URL to navigate to. This is required to be a Google URL.
     * @param title title of the bottom sheet.
     */
    void requestShowContent(GURL url, String title) {
        assert isValidUrl(url) && mWebContents != null;

        loadUrl(url);
        mSheetContent.setTitle(title);
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }

    /**
     * Initializes internal state needed for {@link ThinWebView} and {@link WebContents}.
     */
    void init(WebContents webContents, ContentView contentView,
            MerchantTrustDetailsSheetContent sheetContent, Profile profile) {
        assert mProfile == null && mWebContentsObserver == null && mWebContentsDelegate == null;
        mProfile = profile;
        mWebContents = webContents;
        mSheetContent = sheetContent;
        mWebContentsObserver = new WebContentsObserver(mWebContents) {
            @Override
            public void loadProgressChanged(float progress) {
                if (mSheetContent != null) mSheetContent.setProgress(progress);
            }

            @Override
            public void didStartNavigation(NavigationHandle navigation) {
                mMetrics.recordNavigateLinkOnBottomSheet();
            }

            @Override
            public void titleWasSet(String title) {
                if (!MerchantViewerConfig.TRUST_SIGNALS_SHEET_USE_PAGE_TITLE.getValue()) return;
                mSheetContent.setTitle(title);
            }

            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                if (navigation.isInMainFrame() && navigation.hasCommitted()) {
                    mSheetContent.setUrl(mWebContents.get().getVisibleUrl());
                }
            }
        };

        mWebContentsDelegate = new WebContentsDelegateAndroid() {
            @Override
            public void visibleSSLStateChanged() {
                if (mSheetContent == null) return;
                int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
                mSheetContent.setSecurityIcon(getSecurityIconResource(securityLevel));
                mSheetContent.setUrl(mWebContents.getVisibleUrl());
            }

            @Override
            public void openNewTab(GURL url, String extraHeaders, ResourceRequestBody postData,
                    int disposition, boolean isRendererInitiated) {
                loadUrl(url);
            }

            @Override
            public boolean shouldCreateWebContents(GURL targetUrl) {
                loadUrl(targetUrl);
                return false;
            }

            @Override
            public void loadingStateChanged(boolean toDifferentDocument) {
                boolean isLoading = mWebContents != null && mWebContents.isLoading();
                if (isLoading) {
                    if (mSheetContent == null) return;
                    mSheetContent.setProgress(0);
                    mSheetContent.setProgressVisible(true);
                } else {
                    // Make sure the progress bar is visible for a few frames.
                    new Handler().postDelayed(() -> {
                        if (mSheetContent != null) mSheetContent.setProgressVisible(false);
                    }, HIDE_PROGRESS_BAR_DELAY_MS);
                }
            }

            @Override
            public int getTopControlsHeight() {
                return mTopControlsHeightDp;
            }
        };

        mSheetContent.attachWebContents(mWebContents, contentView, mWebContentsDelegate);
    }

    /**
     * Destroys the objects used for the current preview tab.
     */
    void destroyContent() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        mWebContentsDelegate = null;
        mWebContents = null;
        mSheetContent = null;
        mProfile = null;
    }

    private void loadUrl(GURL url) {
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url.getSpec()));
    }

    @DrawableRes
    private static int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel) {
        switch (securityLevel) {
            case ConnectionSecurityLevel.NONE:
            case ConnectionSecurityLevel.WARNING:
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.drawable.omnibox_not_secure_warning;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
                return R.drawable.omnibox_https_valid;
            default:
                assert false;
        }
        return 0;
    }

    private boolean isValidUrl(GURL url) {
        return UrlUtilitiesJni.get().isGoogleDomainUrl(url.getSpec(), true);
    }
}
