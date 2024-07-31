// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
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
import org.chromium.ui.UiUtils;
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
    private final FaviconHelper mFaviconHelper;
    private final int mFaviconSize;
    private final ObservableSupplier<Profile> mProfileSupplier;

    private PropertyModel mToolbarModel;
    private WebContents mWebContents;
    private ContentView mWebContentView;
    private WebContentsDelegateAndroid mWebContentsDelegate;
    private WebContentsObserver mWebContentsObserver;
    private WebContents mWebContentsForTesting;
    private Drawable mFaviconDrawableForTesting;

    /** Creates a new instance. */
    MerchantTrustBottomSheetMediator(
            Context context,
            WindowAndroid windowAndroid,
            MerchantTrustMetrics metrics,
            ObservableSupplier<Profile> profileSupplier,
            FaviconHelper faviconHelper) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mMetrics = metrics;
        mTopControlsHeightDp =
                (int)
                        (mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                                / mWindowAndroid.getDisplay().getDipScale());
        mFaviconHelper = faviconHelper;
        mFaviconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.preview_tab_favicon_size);
        mProfileSupplier = profileSupplier;
    }

    void setupSheetWebContents(ThinWebView thinWebView, PropertyModel toolbarModel) {
        assert mWebContentsObserver == null
                && mWebContentsDelegate == null
                && mToolbarModel == null;
        mToolbarModel = toolbarModel;

        createWebContents();

        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    private GURL mCurrentUrl;

                    @Override
                    public void loadProgressChanged(float progress) {
                        if (mToolbarModel != null) {
                            mToolbarModel.set(BottomSheetToolbarProperties.LOAD_PROGRESS, progress);
                        }
                    }

                    @Override
                    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        mMetrics.recordNavigateLinkOnBottomSheet();
                        if (!navigation.isSameDocument() && (navigation.getUrl() != null)) {
                            GURL url = navigation.getUrl();
                            if (url.equals(mCurrentUrl)) return;
                            mCurrentUrl = url;
                            loadFavicon(url);
                        }
                    }

                    @Override
                    public void titleWasSet(String title) {
                        if (!MerchantViewerConfig.doesTrustSignalsSheetUsePageTitle()) return;
                        mToolbarModel.set(BottomSheetToolbarProperties.TITLE, title);
                    }

                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (navigation.hasCommitted()) {
                            mToolbarModel.set(
                                    BottomSheetToolbarProperties.URL,
                                    mWebContents.get().getVisibleUrl());
                        }
                    }
                };

        mWebContentsDelegate =
                new WebContentsDelegateAndroid() {
                    @Override
                    public void visibleSSLStateChanged() {
                        if (mToolbarModel == null) return;
                        int securityLevel =
                                SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
                        mToolbarModel.set(
                                BottomSheetToolbarProperties.SECURITY_ICON,
                                getSecurityIconResource(securityLevel));
                        mToolbarModel.set(
                                BottomSheetToolbarProperties.URL, mWebContents.getVisibleUrl());
                    }

                    @Override
                    public void openNewTab(
                            GURL url,
                            String extraHeaders,
                            ResourceRequestBody postData,
                            int disposition,
                            boolean isRendererInitiated) {
                        loadUrl(url);
                    }

                    @Override
                    public boolean shouldCreateWebContents(GURL targetUrl) {
                        loadUrl(targetUrl);
                        return false;
                    }

                    @Override
                    public void loadingStateChanged(boolean shouldShowLoadingUI) {
                        boolean isLoading = mWebContents != null && mWebContents.isLoading();
                        if (isLoading) {
                            if (mToolbarModel == null) return;
                            mToolbarModel.set(BottomSheetToolbarProperties.LOAD_PROGRESS, 0);
                            mToolbarModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, true);
                        } else {
                            // Make sure the progress bar is visible for a few frames.
                            Runnable runnable =
                                    () -> {
                                        if (mToolbarModel != null) {
                                            mToolbarModel.set(
                                                    BottomSheetToolbarProperties.PROGRESS_VISIBLE,
                                                    false);
                                        }
                                    };

                            PostTask.postDelayedTask(
                                    TaskTraits.UI_USER_VISIBLE,
                                    runnable,
                                    HIDE_PROGRESS_BAR_DELAY_MS);
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

    // This method should only be used for the first navigation before showing some content in the
    // bottom sheet.
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
        mWebContents = WebContentsFactory.createWebContents(mProfileSupplier.get(), false, false);
        mWebContentView = ContentView.createContentView(mContext, mWebContents);
        final ViewAndroidDelegate delegate =
                ViewAndroidDelegate.createBasicDelegate(mWebContentView);
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                delegate,
                mWebContentView,
                mWindowAndroid,
                WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents, false);
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

    private static @DrawableRes int getSecurityIconResource(
            @ConnectionSecurityLevel int securityLevel) {
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

    // This method is used to determine whether we want to show content in the bottom sheet and
    // whether we want to use a Google icon if no favicon found for the url. When the definition of
    // "valid" url changes, update the favicon rule if needed.
    private boolean isValidUrl(GURL url) {
        return UrlUtilitiesJni.get().isGoogleDomainUrl(url.getSpec(), true)
                || UrlUtilitiesJni.get().isGoogleSubDomainUrl(url.getSpec());
    }

    void setWebContentsForTesting(WebContents webContents) {
        mWebContentsForTesting = webContents;
    }

    /**
     * Generates a favicon for a given URL. If no favicon could be found or generated from the URL,
     * a default favicon will be shown.
     */
    private void loadFavicon(GURL url) {
        Profile profile = mProfileSupplier.get();
        // TODO(crbug.com/40204015): {@link FaviconHelper#getLocalFaviconImageForURL} may return
        // wrong non-null bitmap for the first navigation within bottom sheet, so we use Google icon
        // directly for valid urls.
        if (isValidUrl(url) || (profile == null)) {
            mToolbarModel.set(
                    BottomSheetToolbarProperties.FAVICON_ICON_DRAWABLE,
                    getDefaultFaviconDrawable(url));
            return;
        }
        mFaviconHelper.getLocalFaviconImageForURL(
                profile,
                url,
                mFaviconSize,
                (bitmap, iconUrl) -> {
                    Drawable drawable;
                    if (mFaviconDrawableForTesting != null) {
                        drawable = mFaviconDrawableForTesting;
                    } else if (bitmap != null) {
                        drawable =
                                FaviconUtils.createRoundedBitmapDrawable(
                                        mContext.getResources(), bitmap);
                    } else {
                        drawable = getDefaultFaviconDrawable(url);
                    }
                    mToolbarModel.set(BottomSheetToolbarProperties.FAVICON_ICON_DRAWABLE, drawable);
                });
    }

    // Used when we cannot find a favicon for the url. If url is valid, we use the Google icon.
    // Otherwise, we use the default icon.
    private Drawable getDefaultFaviconDrawable(GURL url) {
        if (isValidUrl(url)) {
            return AppCompatResources.getDrawable(mContext, R.drawable.ic_logo_googleg_24dp);
        } else {
            return UiUtils.getTintedDrawable(
                    mContext, R.drawable.ic_globe_24dp, R.color.default_icon_color_tint_list);
        }
    }

    void setFaviconDrawableForTesting(Drawable drawableForTesting) {
        mFaviconDrawableForTesting = drawableForTesting;
    }
}
