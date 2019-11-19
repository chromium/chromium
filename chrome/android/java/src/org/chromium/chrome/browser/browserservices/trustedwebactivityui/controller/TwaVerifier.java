// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.content_public.browser.WebContents;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.inject.Inject;

import androidx.browser.customtabs.CustomTabsService;

/**
 * Provides Trusted Web Activity specific behaviour for the {@link CurrentPageVerifier}.
 */
@ActivityScope
public class TwaVerifier implements Verifier, Destroyable {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private static final int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final OriginVerifier mOriginVerifier;

    // These origins need to be verified via OriginVerifier#start, bypassing cache.
    private Set<Origin> mOriginsPendingVerification;

    @Inject
    public TwaVerifier(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabIntentDataProvider intentDataProvider,
            OriginVerifier.Factory originVerifierFactory,
            CustomTabActivityTabProvider tabProvider,
            ClientPackageNameProvider clientPackageNameProvider) {
        mIntentDataProvider = intentDataProvider;

        // TODO(peconn): See if we can get rid of the dependency on Web Contents.
        WebContents webContents =
                tabProvider.getTab() != null ? tabProvider.getTab().getWebContents() : null;
        mOriginVerifier = originVerifierFactory.create(
                clientPackageNameProvider.get(), RELATIONSHIP, webContents);

        lifecycleDispatcher.register(this);
    }

    @Override
    public void destroy() {
        // Verification may finish after activity is destroyed.
        mOriginVerifier.removeListener();
    }

    @Override
    public Promise<Boolean> verify(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return Promise.fulfilled(false);

        collectTrustedOriginsIfNeeded();
        Promise<Boolean> promise = new Promise<>();
        if (mOriginsPendingVerification.contains(origin)) {

            mOriginVerifier.start((packageName, unused, verified, online) -> {
                mOriginsPendingVerification.remove(origin);
                promise.fulfill(verified);
            }, origin);

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
    public boolean wasPreviouslyVerified(String url) {
        return mOriginVerifier.wasPreviouslyVerified(Origin.createOrThrow(url));
    }

    private void collectTrustedOriginsIfNeeded() {
        if (mOriginsPendingVerification != null) return;

        mOriginsPendingVerification = new HashSet<>();

        Origin initialOrigin = Origin.create(mIntentDataProvider.getUrlToLoad());
        if (initialOrigin != null) mOriginsPendingVerification.add(initialOrigin);

        List<String> additionalOrigins =
                mIntentDataProvider.getTrustedWebActivityAdditionalOrigins();
        if (additionalOrigins != null) {
            for (String originAsString : additionalOrigins) {
                Origin origin = Origin.create(originAsString);
                if (origin == null) continue;

                mOriginsPendingVerification.add(origin);
            }
        }
    }
}
