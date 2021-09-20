// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.os.Handler;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator class for the component. */
public class MerchantTrustBottomSheetMediator {
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = 50;

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final MerchantTrustMetrics mMetrics;
    private final int mTopControlsHeightDp;

    private PropertyModel mToolbarModel;
    private WebContents mWebContents;
    private ContentView mWebContentView;
    private WebContentsDelegateAndroid mWebContentsDelegate;
    private WebContentsObserver mWebContentsObserver;
    private WebContents mWebContentsForTesting;

    /** Creates a new instance. */
    MerchantTrustBottomSheetMediator(
            Context context, WindowAndroid windowAndroid, MerchantTrustMetrics metrics) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mMetrics = metrics;
        mTopControlsHeightDp = (int) (mContext.getResources().getDimensionPixelSize(
                                              R.dimen.toolbar_height_no_shadow)
                / mWindowAndroid.getDisplay().getDipScale());
    }

    void setupSheetWebContents(ThinWebView thinWebView, PropertyModel toolbarModel) {
        assert mWebContentsObserver == null && mWebContentsDelegate == null
                && mToolbarModel == null;
        mToolbarModel = toolbarModel;

        createWebContents();

        mWebContentsObserver = new WebContentsObserver(mWebContents) {
            @Override
            public void loadProgressChanged(float progress) {
                if (mToolbarModel != null) {
                    mToolbarModel.set(BottomSheetToolbarProperties.LOAD_PROGRESS, progress);
                }
            }

            @Override
            public void didStartNavigation(NavigationHandle navigation) {
                mMetrics.recordNavigateLinkOnBottomSheet();
            }

            @Override
            public void titleWasSet(String title) {
                if (!MerchantViewerConfig.doesTrustSignalsSheetUsePageTitle()) return;
                mToolbarModel.set(BottomSheetToolbarProperties.TITLE, title);
            }

            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                if (navigation.isInPrimaryMainFrame() && navigation.hasCommitted()) {
                    mToolbarModel.set(
                            BottomSheetToolbarProperties.URL, mWebContents.get().getVisibleUrl());
                }
            }
        };

        mWebContentsDelegate = new WebContentsDelegateAndroid() {
            @Override
            public void visibleSSLStateChanged() {
                if (mToolbarModel == null) return;
                int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
                mToolbarModel.set(BottomSheetToolbarProperties.SECURITY_ICON,
                        getSecurityIconResource(securityLevel));
                mToolbarModel.set(BottomSheetToolbarProperties.URL, mWebContents.getVisibleUrl());
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
                    if (mToolbarModel == null) return;
                    mToolbarModel.set(BottomSheetToolbarProperties.LOAD_PROGRESS, 0);
                    mToolbarModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, true);
                } else {
                    // Make sure the progress bar is visible for a few frames.
                    new Handler().postDelayed(() -> {
                        if (mToolbarModel != null) {
                            mToolbarModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, false);
                        }
                    }, HIDE_PROGRESS_BAR_DELAY_MS);
                }
            }

            @Override
            public int getTopControlsHeight() {
                return mTopControlsHeightDp;
            }
        };
        if ((mWebContentView != null) && (mWebContentView.getParent() != null)) {
            ((ViewGroup) mWebContentView.getParent()).removeView(mWebContentView);
        }
        thinWebView.attachWebContents(mWebContents, mWebContentView, mWebContentsDelegate);
    }

    void navigateToUrl(GURL url, String title) {
        assert isValidUrl(url) && mWebContents != null && mToolbarModel != null;

        loadUrl(url);
        mToolbarModel.set(BottomSheetToolbarProperties.TITLE, title);
    }

    int getVerticalScrollOffset() {
        return mWebContents == null
                ? 0
                : RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
    }

    private void createWebContents() {
        assert mWebContents == null;
        if (mWebContentsForTesting != null) {
            mWebContents = mWebContentsForTesting;
            return;
        }
        mWebContents = WebContentsHelpers.createWebContents(false, false);
        mWebContentView = ContentView.createContentView(mContext, null, mWebContents);
        final ViewAndroidDelegate delegate =
                ViewAndroidDelegate.createBasicDelegate(mWebContentView);
        mWebContents.initialize(ChromeVersionInfo.getProductVersion(), delegate, mWebContentView,
                mWindowAndroid, WebContents.createDefaultInternalsHolder());
        WebContentsHelpers.setUserAgentOverride(mWebContents);
    }

    void destroyWebContents() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mWebContentView = null;
        }
        mWebContentsDelegate = null;
        mToolbarModel = null;
    }

    private void loadUrl(GURL url) {
        if (mWebContents != null) {
            mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url.getSpec()));
        }
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

    @VisibleForTesting
    void setWebContentsForTesting(WebContents webContents) {
        mWebContentsForTesting = webContents;
    }
}
