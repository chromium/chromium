// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.text.TextUtils;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content_public.browser.NavigationHandle;

/**
 * Responsible for detecting candidate events for fetching the merchant trust signal and publishing
 * the merchant trust message.
 */
class MerchantTrustSignalsMediator {
    /** Callback interface to communicate with the owning object. */
    interface MerchantTrustSignalsCallback {
        /**
         * Called when the mediator has detected a candidate event for fetching the merchant
         * trust signal and scheduling the merchant trust message.
         */
        void onFinishEligibleNavigation(MerchantTrustMessageContext item);
    }

    private final CurrentTabObserver mCurrentTabObserver;

    MerchantTrustSignalsMediator(
            ObservableSupplier<Tab> tabSupplier,
            MerchantTrustSignalsCallback delegate,
            MerchantTrustMetrics metrics) {
        mCurrentTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onDidFinishNavigationInPrimaryMainFrame(
                                    Tab tab, NavigationHandle navigation) {
                                if (tab.isIncognito()
                                        || !navigation.hasCommitted()
                                        || navigation.isPrimaryMainFrameFragmentNavigation()
                                        || navigation.isErrorPage()
                                        || (navigation.getUrl() == null)
                                        || TextUtils.isEmpty(navigation.getUrl().getHost())) {
                                    return;
                                }

                                metrics.updateRecordingMessageImpact(navigation.getUrl().getHost());
                                delegate.onFinishEligibleNavigation(
                                        new MerchantTrustMessageContext(
                                                navigation, tab.getWebContents()));
                            }

                            @Override
                            public void onHidden(Tab tab, @TabHidingType int type) {
                                metrics.finishRecordingMessageImpact();
                            }

                            @Override
                            public void onDestroyed(Tab tab) {
                                metrics.finishRecordingMessageImpact();
                            }
                        });
    }

    void destroy() {
        mCurrentTabObserver.destroy();
    }
}
