// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.text.TextUtils;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;

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

    private final CurrentTabObserver mCurrentTabObserver;

    MerchantTrustSignalsMediator(
            ObservableSupplier<Tab> tabSupplier, MerchantTrustSignalsCallback delegate) {
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if ((tab.isIncognito()) || (!navigation.hasCommitted())
                        || (!navigation.isInPrimaryMainFrame())
                        || (navigation.isFragmentNavigation()) || (navigation.isErrorPage())
                        || (navigation.getUrl() == null)
                        || (TextUtils.isEmpty(navigation.getUrl().getHost()))) {
                    return;
                }

                delegate.maybeDisplayMessage(
                        new MerchantTrustMessageContext(navigation, tab.getWebContents()));
            }
        });
    }

    void destroy() {
        mCurrentTabObserver.destroy();
    }
}
