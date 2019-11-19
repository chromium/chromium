// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * An implementation of {@link LogoView.Delegate}.
 */
public class LogoDelegateImpl implements LogoView.Delegate {
    // UMA enum constants. CTA means the "click-to-action" icon.
    private static final String LOGO_SHOWN_UMA_NAME = "NewTabPage.LogoShown";
    private static final String LOGO_SHOWN_FROM_CACHE_UMA_NAME = "NewTabPage.LogoShown.FromCache";
    private static final String LOGO_SHOWN_FRESH_UMA_NAME = "NewTabPage.LogoShown.Fresh";
    private static final int STATIC_LOGO_SHOWN = 0;
    private static final int CTA_IMAGE_SHOWN = 1;
    private static final int LOGO_SHOWN_COUNT = 2;

    private static final String LOGO_SHOWN_TIME_UMA_NAME = "NewTabPage.LogoShownTime2";

    private static final String LOGO_CLICK_UMA_NAME = "NewTabPage.LogoClick";
    private static final int STATIC_LOGO_CLICKED = 0;
    private static final int CTA_IMAGE_CLICKED = 1;
    private static final int ANIMATED_LOGO_CLICKED = 2;

    private final SuggestionsNavigationDelegate mNavigationDelegate;
    private LogoView mLogoView;

    private LogoBridge mLogoBridge;
    private ImageFetcher mImageFetcher;

    private String mOnLogoClickUrl;
    private String mAnimatedLogoUrl;

    private boolean mShouldRecordLoadTime = true;
    private boolean mIsDestroyed;

    /**
     * Construct a new {@link LogoDelegateImpl}.
     * @param navigationDelegate The delegate for loading the URL when the logo is clicked.
     * @param logoView The view that shows the search provider logo.
     * @param profile The profile to show the logo for.
     */
    public LogoDelegateImpl(
            SuggestionsNavigationDelegate navigationDelegate, LogoView logoView, Profile profile) {
        mNavigationDelegate = navigationDelegate;
        mLogoView = logoView;
        mLogoBridge = new LogoBridge(profile);
        mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY);
    }

    public void destroy() {
        mIsDestroyed = true;
        mImageFetcher.destroy();
        mImageFetcher = null;
    }

    @Override
    public void onLogoClicked(boolean isAnimatedLogoShowing) {
        if (mIsDestroyed) return;

        if (!isAnimatedLogoShowing && mAnimatedLogoUrl != null) {
            RecordHistogram.recordSparseHistogram(LOGO_CLICK_UMA_NAME, CTA_IMAGE_CLICKED);
            mLogoView.showLoadingView();
            mImageFetcher.fetchGif(mAnimatedLogoUrl, ImageFetcher.NTP_ANIMATED_LOGO_UMA_CLIENT_NAME,
                    (BaseGifImage animatedLogoImage) -> {
                        if (mIsDestroyed || animatedLogoImage == null) return;
                        mLogoView.playAnimatedLogo(animatedLogoImage);
                    });
        } else if (mOnLogoClickUrl != null) {
            RecordHistogram.recordSparseHistogram(LOGO_CLICK_UMA_NAME,
                    isAnimatedLogoShowing ? ANIMATED_LOGO_CLICKED : STATIC_LOGO_CLICKED);
            mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams(mOnLogoClickUrl, PageTransition.LINK));
        }
    }

    public void getSearchProviderLogo(final LogoObserver logoObserver) {
        assert !mIsDestroyed;

        final long loadTimeStart = System.currentTimeMillis();

        LogoObserver wrapperCallback = new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (mIsDestroyed) return;

                if (logo != null) {
                    int logoType =
                            logo.animatedLogoUrl == null ? STATIC_LOGO_SHOWN : CTA_IMAGE_SHOWN;
                    RecordHistogram.recordEnumeratedHistogram(
                            LOGO_SHOWN_UMA_NAME, logoType, LOGO_SHOWN_COUNT);
                    if (fromCache) {
                        RecordHistogram.recordEnumeratedHistogram(
                                LOGO_SHOWN_FROM_CACHE_UMA_NAME, logoType, LOGO_SHOWN_COUNT);
                    } else {
                        RecordHistogram.recordEnumeratedHistogram(
                                LOGO_SHOWN_FRESH_UMA_NAME, logoType, LOGO_SHOWN_COUNT);
                    }
                    if (mShouldRecordLoadTime) {
                        long loadTime = System.currentTimeMillis() - loadTimeStart;
                        RecordHistogram.recordMediumTimesHistogram(
                                LOGO_SHOWN_TIME_UMA_NAME, loadTime);
                        // Only record the load time once per NTP, for the first logo we got,
                        // whether that came from cache or not.
                        mShouldRecordLoadTime = false;
                    }
                } else if (!fromCache) {
                    // If we got a fresh (i.e. not from cache) null logo, don't record any load
                    // time even if we get another update later.
                    mShouldRecordLoadTime = false;
                }

                mOnLogoClickUrl = logo != null ? logo.onClickUrl : null;
                mAnimatedLogoUrl = logo != null ? logo.animatedLogoUrl : null;

                logoObserver.onLogoAvailable(logo, fromCache);
            }
        };

        mLogoBridge.getCurrentLogo(wrapperCallback);
    }
}
