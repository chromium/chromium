// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.text.TextUtils;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/** Responsible for detecting candidate events for publishing the merchant trust message. */
class MerchantTrustSignalsMediator {
    /** Callback interface to communicate with the owning object. */
    interface MerchantTrustSignalsCallback {
        /**
         * Called when the mediator has detected a candidate event for displaying the merchant
         * trust message.
         */
        void maybeDisplayMessage(MerchantTrustMessageContext item);
    }

    private final MerchantTrustSignalsCallback mDelegate;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private WebContents mCurrentWebContents;
    private Tab mTab;

    private final WebContentsObserver mWebContentsObserver = new WebContentsObserver() {
        @Override
        public void didFinishNavigation(NavigationHandle navigation) {
            if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                    || navigation.isSameDocument()) {
                return;
            }

            if (navigation.getUrl() == null || TextUtils.isEmpty(navigation.getUrl().getHost())) {
                return;
            }

            mDelegate.maybeDisplayMessage(
                    new MerchantTrustMessageContext(navigation.getUrl(), mCurrentWebContents));
        }
    };

    MerchantTrustSignalsMediator(
            TabModelSelector tabModelSelector, MerchantTrustSignalsCallback delegate) {
        mTabModelSelector = tabModelSelector;
        mDelegate = delegate;

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (mTab != null && mTab.getWebContents() != null) {
                    mTab.getWebContents().removeObserver(mWebContentsObserver);
                }

                // Capture the current Tab and WebContents instances.
                mCurrentWebContents = tab.getWebContents();
                mTab = tab;
                mCurrentWebContents.addObserver(mWebContentsObserver);
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        // Listen on the initial tab's changes.
        mTab = tabModelSelector.getCurrentTab();
        if (mTab != null) {
            mCurrentWebContents = mTab.getWebContents();
            mCurrentWebContents.addObserver(mWebContentsObserver);
        }
    }

    void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
    }
}
