// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.text.TextUtils;

import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialCoordinator;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialCoordinatorFactory;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;

import javax.inject.Inject;

/**
 * A {@link TabObserver} that determines whether a custom tab navigation should show the new
 * download UI.
 */
@ActivityScope
public class CustomTabDownloadObserver extends EmptyTabObserver {
    private final Activity mActivity;
    private final TabObserverRegistrar mTabObserverRegistrar;

    @Inject
    public CustomTabDownloadObserver(Activity activity, TabObserverRegistrar tabObserverRegistrar) {
        mActivity = activity;
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabObserverRegistrar.registerTabObserver(this);
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        // For a navigation from page A to page B, there can be any number of redirects in between.
        // The first navigation which opens the custom tab will have a transition of type FROM_API.
        // Each redirect can then be treated as its own navigation with a separate call to this
        // method. This creates a mask which keeps this observer alive during the first chain of
        // navigations only. After that, this observer is unregistered.
        if ((navigation.pageTransition()
                        & (PageTransition.FROM_API
                                | PageTransition.SERVER_REDIRECT
                                | PageTransition.CLIENT_REDIRECT))
                == 0) {
            unregister();
            return;
        }
        if (navigation.isDownload()) {
            // If this is an inline pdf page, don't show interstitial.
            if (PdfUtils.shouldOpenPdfInline(tab.isIncognito())
                    && TextUtils.equals(navigation.getMimeType(), MimeTypeUtils.PDF_MIME_TYPE)) {
                unregister();
                return;
            }
            Runnable urlRegistration =
                    () -> {
                        if (mActivity.isFinishing()
                                || mActivity.isDestroyed()
                                || tab.isDestroyed()) {
                            return;
                        }
                        DownloadManagerService.getDownloadManagerService()
                                .getMessageUiController(/* otrProfileID= */ null)
                                .addDownloadInterstitialSource(tab.getOriginalUrl());
                    };

            DownloadInterstitialCoordinator coordinator =
                    DownloadInterstitialCoordinatorFactory.create(
                            tab::getContext,
                            tab.getOriginalUrl().getSpec(),
                            tab.getWindowAndroid(),
                            () -> {
                                tab.reload();
                                urlRegistration.run();
                            });
            // Register the download's original URL to prevent messages UI showing in
            // interstitial.
            DeferredStartupHandler.getInstance().addDeferredTask(urlRegistration);
            NewDownloadTab.from(tab, coordinator, mActivity).show();
        }
    }

    private void unregister() {
        mTabObserverRegistrar.unregisterTabObserver(this);
    }
}
