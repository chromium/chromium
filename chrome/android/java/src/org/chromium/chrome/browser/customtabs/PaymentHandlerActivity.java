// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.res.Configuration;
import android.view.Gravity;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.display.DisplayUtil;

/**
 * Simple wrapper around CustomTabActivity to be used when launching a payment handler tab, which
 * uses a different theme.
 */
public class PaymentHandlerActivity extends CustomTabActivity {
    private static final double BOTTOM_SHEET_HEIGHT_RATIO = 0.7;
    private boolean mHaveNotifiedServiceWorker;
    private WebContents mWebContents;

    @Override
    public void performPreInflationStartup() {
        super.performPreInflationStartup();
        updateHeight();
        addObserverForPaymentsWhenTabReady();
    }

    private void addObserverForPaymentsWhenTabReady() {
        CustomTabActivityTabProvider tabProvider = getComponent().resolveTabProvider();
        Tab tab = tabProvider.getTab();
        if (tab != null) {
            ServiceWorkerPaymentAppBridge.addTabObserverForPaymentRequestTab(tab);
            mWebContents = tab.getWebContents();
        } else {
            tabProvider.addObserver(new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onInitialTabCreated(@NonNull Tab tab, int mode) {
                    tabProvider.removeObserver(this);
                    ServiceWorkerPaymentAppBridge.addTabObserverForPaymentRequestTab(tab);
                    mWebContents = tab.getWebContents();
                }
            });
        }
    }

    @Override
    protected void handleFinishAndClose() {
        // Notify the window is closing so as to abort invoking payment app early.
        if (!mHaveNotifiedServiceWorker && mWebContents != null) {
            mHaveNotifiedServiceWorker = true;
            ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContents);
        }

        super.handleFinishAndClose();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateHeight();
    }

    private void updateHeight() {
        int displayHeightInPixels = DisplayUtil.dpToPx(
                getWindowAndroid().getDisplay(), getResources().getConfiguration().screenHeightDp);
        int heightInPhysicalPixels = (int) (displayHeightInPixels * BOTTOM_SHEET_HEIGHT_RATIO);
        int minimumHeightInPhysicalPixels = getResources().getDimensionPixelSize(
                R.dimen.payments_handler_window_minimum_height);

        if (heightInPhysicalPixels < minimumHeightInPhysicalPixels) {
            heightInPhysicalPixels = minimumHeightInPhysicalPixels;
        }

        if (heightInPhysicalPixels > displayHeightInPixels) {
            heightInPhysicalPixels = WindowManager.LayoutParams.MATCH_PARENT;
        }

        WindowManager.LayoutParams attributes = getWindow().getAttributes();

        if (attributes.height == heightInPhysicalPixels) return;

        attributes.height = heightInPhysicalPixels;
        attributes.gravity = Gravity.BOTTOM;
        getWindow().setAttributes(attributes);
    }
}