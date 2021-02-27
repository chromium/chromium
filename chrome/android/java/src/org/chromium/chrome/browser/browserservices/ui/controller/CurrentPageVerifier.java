// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;

/**
 * Checks whether the currently seen web page belongs to a verified origin and notifies any
 * observers.
 */
@ActivityScope
public class CurrentPageVerifier implements NativeInitObserver {
    private final CustomTabActivityTabProvider mTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Verifier mDelegate;

    @Nullable
    private VerificationState mState;

    private final ObserverList<Runnable> mObservers = new ObserverList<>();

    @IntDef({VerificationStatus.PENDING, VerificationStatus.SUCCESS, VerificationStatus.FAILURE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerificationStatus {
        int PENDING = 0;
        int SUCCESS = 1;
        int FAILURE = 2;
    }

    /** Represents the verification state of currently viewed web page. */
    public static class VerificationState {
        public final String scope;
        @VerificationStatus
        public final int status;

        public VerificationState(String scope, @VerificationStatus int status) {
            this.scope = scope;
            this.status = status;
        }
    }

    /** A {@link TabObserver} that checks whether we are on a verified page on navigation. */
    private final CustomTabTabObserver mVerifyOnPageLoadObserver = new CustomTabTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                    || navigation.isSameDocument()) {
                return;
            }
            verify(navigation.getUrl().getSpec());
        }

        @Override
        public void onObservingDifferentTab(@NonNull Tab tab) {
            // When a link with target="_blank" is followed and the user navigates back, we
            // don't get the onDidFinishNavigation event (because the original page wasn't
            // navigated away from, it was only ever hidden). https://crbug.com/942088
            verify(tab.getUrlString());
        }
    };

    @Inject
    public CurrentPageVerifier(ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar, CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider, Verifier delegate) {
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mDelegate = delegate;

        tabObserverRegistrar.registerActivityTabObserver(mVerifyOnPageLoadObserver);
        lifecycleDispatcher.register(this);
    }

    /**
     * @return the {@link VerificationState} of the page we are currently on.
     * Since verification may require native, may return null before native is loaded.
     */
    @Nullable
    public VerificationState getState() {
        return mState;
    }

    public void addVerificationObserver(Runnable observer) {
        mObservers.addObserver(observer);
    }

    public void removeVerificationObserver(Runnable observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void onFinishNativeInitialization() {
        verify(mIntentDataProvider.getUrlToLoad());
    }

    /**
     * Perform verification for the given page.
     */
    private void verify(String url) {
        Promise<Boolean> result = mDelegate.verify(url);
        String scope = mDelegate.getVerifiedScope(url);
        if (scope == null) return;

        if (result.isFulfilled()) {
            updateState(scope, statusFromBoolean(result.getResult()));
        } else {
            updateState(scope, VerificationStatus.PENDING);
            result.then(verified -> { onVerificationResult(scope, verified); });
        }
    }

    /**
     * Is called as a result of a verification request to OriginVerifier. Is not called if the
     * client called |validateRelationship| before launching the TWA and we found that verification
     * in the cache.
     */
    private void onVerificationResult(String scope, boolean verified) {
        Tab tab = mTabProvider.getTab();

        boolean resultStillApplies =
                tab != null && scope.equals(mDelegate.getVerifiedScope(tab.getUrlString()));
        if (resultStillApplies) {
            updateState(scope, verified ? VerificationStatus.SUCCESS : VerificationStatus.FAILURE);
        }
    }

    private void updateState(String scope, @VerificationStatus int status) {
        mState = new VerificationState(scope, status);
        for (Runnable observer : mObservers) {
            observer.run();
        }
    }

    private static @VerificationStatus int statusFromBoolean(boolean success) {
        return success ? VerificationStatus.SUCCESS : VerificationStatus.FAILURE;
    }
}
