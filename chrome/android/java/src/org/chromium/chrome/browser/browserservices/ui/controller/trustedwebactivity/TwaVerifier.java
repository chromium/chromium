// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;

/** Provides Trusted Web Activity specific behaviour for the {@link CurrentPageVerifier}. */
@ActivityScope
public class TwaVerifier implements Verifier, DestroyObserver {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private static final int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final ChromeOriginVerifier mOriginVerifier;

    /**
     * Origins that we have yet to call OriginVerifier#start on.
     *
     * This value will be {@code null} until {@link #getPendingOrigins} is called (you can just use
     * getPendingOrigins to get a ensured non-null value).
     */
    @Nullable private Set<Origin> mPendingOrigins;

    private boolean mDestroyed;

    /** All the origins that have been successfully verified. */
    private Set<Origin> mVerifiedOrigins = new HashSet<>();

    @Inject
    public TwaVerifier(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider,
            ChromeOriginVerifierFactory originVerifierFactory,
            CustomTabActivityTabProvider tabProvider,
            ClientPackageNameProvider clientPackageNameProvider,
            ExternalAuthUtils externalAuthUtils) {
        mIntentDataProvider = intentDataProvider;

        // TODO(peconn): See if we can get rid of the dependency on Web Contents.
        WebContents webContents =
                tabProvider.getTab() != null ? tabProvider.getTab().getWebContents() : null;
        mOriginVerifier =
                originVerifierFactory.create(
                        clientPackageNameProvider.get(),
                        RELATIONSHIP,
                        webContents,
                        externalAuthUtils);

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        mDestroyed = true;
    }

    @Override
    public Promise<Boolean> verify(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return Promise.fulfilled(false);

        Promise<Boolean> promise = new Promise<>();
        if (getPendingOrigins().contains(origin)) {
            mOriginVerifier.start(
                    (packageName, unused, verified, online) -> {
                        if (mDestroyed) return;

                        getPendingOrigins().remove(origin);
                        if (verified) mVerifiedOrigins.add(origin);

                        promise.fulfill(verified);
                    },
                    origin);

        } else {
            promise.fulfill(mOriginVerifier.wasPreviouslyVerified(origin));
        }

        return promise;
    }

    @Override
    public String getVerifiedScope(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return null;
        return origin.toString();
    }

    @Override
    public boolean shouldIgnoreExternalIntentHandlers(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return false;

        return getPendingOrigins().contains(origin) || mVerifiedOrigins.contains(origin);
    }

    @Override
    public boolean wasPreviouslyVerified(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return false;
        return mOriginVerifier.wasPreviouslyVerified(origin);
    }

    private Set<Origin> getPendingOrigins() {
        // mPendingOrigins isn't populated in the constructor because
        // mIntentDataProvider.getUrlToLoad requires native to be loaded.

        if (mPendingOrigins == null) {
            Set<Origin> trustedOrigins = mIntentDataProvider.getAllTrustedWebActivityOrigins();
            // This should not be null, since there should be at least one trusted origin for the
            // TWA's url.
            assert (trustedOrigins != null && trustedOrigins.size() > 0);
            // Make a copy of the list since we modify it.
            mPendingOrigins = new HashSet(trustedOrigins);
        }

        return mPendingOrigins;
    }
}
