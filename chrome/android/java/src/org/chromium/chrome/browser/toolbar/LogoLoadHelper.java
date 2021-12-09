// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.LogoDelegateImpl;
import org.chromium.chrome.browser.ntp.LogoView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.features.start_surface.StartSurfaceState;

/**
 * Helper used to fetch and load logo image for Start surface.
 */
public class LogoLoadHelper {
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final TopToolbarCoordinator mToolbar;
    private final Context mContext;

    private CallbackController mCallbackController = new CallbackController();
    private LogoDelegateImpl mLogoDelegate;
    private boolean mHasLogoLoadedForCurrentSearchEngine;

    /**
     * Creates a LogoLoadHelper object.
     *
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param toolbar The {@link TopToolbarCoordinator}.
     * @param context The activity context.
     */
    public LogoLoadHelper(ObservableSupplier<Profile> profileSupplier,
            TopToolbarCoordinator toolbar, Context context) {
        mProfileSupplier = profileSupplier;
        mToolbar = toolbar;
        mContext = context;
    }

    /**
     * If it's on Start surface homepage, load search provider logo; If it's not on start surface
     * homepage, destroy mLogoDelegate.
     */
    void maybeLoadSearchProviderLogoOnHomepage(@StartSurfaceState int startSurfaceState) {
        assert ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(mContext);
        if (startSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
            if (mProfileSupplier.hasValue()) {
                loadSearchProviderLogo();
                return;
            }
            assert mCallbackController != null;
            new OneShotCallback<>(mProfileSupplier, mCallbackController.makeCancelable(profile -> {
                assert profile != null : "Unexpectedly null profile from TabModel.";
                loadSearchProviderLogo();
            }));
        } else if ((startSurfaceState == StartSurfaceState.NOT_SHOWN
                           || startSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                           || startSurfaceState == StartSurfaceState.DISABLED)
                && mLogoDelegate != null) {
            // Destroy |mLogoDelegate| when hiding Start surface homepage to save memory.
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }
    }

    /**
     * Update the logo based on default search engine changes.
     */
    void onDefaultSearchEngineChanged() {
        mHasLogoLoadedForCurrentSearchEngine = false;
        loadSearchProviderLogo();
    }

    /**
     * Cleans up any code as necessary.
     */
    void destroy() {
        if (mLogoDelegate != null) {
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    /**
     * Load the search provider logo on Start surface.
     */
    private void loadSearchProviderLogo() {
        assert ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(mContext);

        // If logo is already updated for the current search provider, or profile is null or off the
        // record, don't bother loading the logo image.
        if (mHasLogoLoadedForCurrentSearchEngine || !mProfileSupplier.hasValue()
                || mProfileSupplier.get().isOffTheRecord()) {
            return;
        }

        if (mLogoDelegate == null) {
            mLogoDelegate = new LogoDelegateImpl(
                    /* navigationDelegate = */ null, /* logoView = */ null, mProfileSupplier.get());
        }

        // If default search engine doesn't have logo, pass in null.
        if (!TemplateUrlServiceFactory.get().doesDefaultSearchEngineHaveLogo()) {
            mHasLogoLoadedForCurrentSearchEngine = true;
            mToolbar.onLogoAvailable(/*logoImage=*/null, /*contentDescription=*/null);
            return;
        }

        // TODO(crbug.com/1119467): Support Google doodles for Start surface.
        if (TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) {
            mHasLogoLoadedForCurrentSearchEngine = true;
            // TODO(crbug.com/1261631): Separate logo codes and use LogoView for Start surface
            // toolbar.
            mToolbar.onLogoAvailable(LogoView.getDefaultGoogleLogo(mContext.getResources()),
                    /*contentDescription=*/null);
            return;
        }

        mLogoDelegate.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                mHasLogoLoadedForCurrentSearchEngine = true;
                if (logo == null) {
                    mToolbar.onLogoAvailable(/*logoImage=*/null, /*contentDescription=*/null);
                } else {
                    // TODO(crbug.com/1119467): When supporting doodles, append
                    // R.string.accessibility_google_doodle before altText.
                    String contentDescription =
                            TextUtils.isEmpty(logo.altText) ? null : logo.altText;
                    mToolbar.onLogoAvailable(logo.image, contentDescription);
                }
            }

            @Override
            public void onCachedLogoRevalidated() {}
        });
    }

    @VisibleForTesting
    void setLogoDelegateForTesting(LogoDelegateImpl logoDelegate) {
        mLogoDelegate = logoDelegate;
    }

    @VisibleForTesting
    void setHasLogoLoadedForCurrentSearchEngineForTesting(
            boolean hasLogoLoadedForCurrentSearchEngine) {
        mHasLogoLoadedForCurrentSearchEngine = hasLogoLoadedForCurrentSearchEngine;
    }
}
