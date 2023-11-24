// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.content.ContextWrapper;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

import java.util.concurrent.Semaphore;

/** Basic shared functionality for partner customization unit tests. */
public class BasePartnerBrowserCustomizationUnitTestRule implements TestRule {
    static final String PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER =
            TestPartnerBrowserCustomizationsProvider.class.getName();
    static final String PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER =
            TestPartnerBrowserCustomizationsProvider.class.getName() + "INVALID";
    static final String PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER =
            TestPartnerBrowserCustomizationsDelayedProvider.class.getName();
    static final long DEFAULT_TIMEOUT_MS = 500;

    private final Runnable mCallback =
            new Runnable() {
                @Override
                public void run() {
                    mCallbackLock.release();
                }
            };
    private final Semaphore mCallbackLock = new Semaphore(0);

    /**
     * Specifies the URI path that should be delayed when querying the delayed provider.
     * <p>
     * This will override the provider authority in the PartnerBrowserCustomizations, so be
     * sure to reset it if you are not using the delayed provider.
     *
     * @param uriPath The path to be delayed.
     */
    void setDelayProviderUriPathForDelay(String uriPath) {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        Uri uri = CustomizationProviderDelegateUpstreamImpl.buildQueryUri(uriPath);
        getContextWrapper().getContentResolver().call(uri, "setUriPathToDelay", uriPath, null);
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                base.evaluate();
            }
        };
    }

    Context getContextWrapper() {
        return new ContextWrapper(ApplicationProvider.getApplicationContext()) {
            @Override
            public Context getApplicationContext() {
                return getBaseContext();
            }
        };
    }

    public Runnable getCallback() {
        return mCallback;
    }

    Semaphore getCallbackLock() {
        return mCallbackLock;
    }
}
