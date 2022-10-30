// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.app.Activity;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.external_intents.AuthenticatorNavigationInterceptor;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class ArkInterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private final ArkTabImpl mTab;
    private final TabObserver mTabObserver;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;

    ArkInterceptNavigationDelegateClientImpl(ArkTabImpl tab) {
        mTab = tab;
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                mInterceptNavigationDelegate.associateWithWebContents(tab.getWebContents());
            }

//            @Override
//            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
//                if (window != null) {
//                    mInterceptNavigationDelegate.setExternalNavigationHandler(
//                            createExternalNavigationHandler());
//                }
//            }

            @Override
            public void onAttachToWindowAndroid(Tab tab, @NonNull WindowAndroid windowAndroid) {
                mInterceptNavigationDelegate.setExternalNavigationHandler(
                        createExternalNavigationHandler());
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                mInterceptNavigationDelegate.onNavigationFinished(navigation);
            }

            @Override
            public void onDestroyed(Tab tab) {
                mInterceptNavigationDelegate.associateWithWebContents(null);
            }
        };
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler() {

        return new ExternalNavigationHandler(new ArkExternalNavigationDelegateImpl(mTab));

//        if (mTab.getWindowAndroid() == null) {
//            return null;
//        }
//        return mTab.getWindowAndroid().getTabDelegateFactory().createExternalNavigationHandler(mTab);
    }

    @Override
    public long getLastUserInteractionTime() {
        AsyncInitializationActivity associatedActivity = mTab.getActivity();
        return (associatedActivity == null) ? -1 : associatedActivity.getLastUserInteractionTime();
    }

    @Override
    public RedirectHandler getOrCreateRedirectHandler() {
        return RedirectHandlerTabHelper.getOrCreateHandlerFor(mTab);
    }

    @Override
    public AuthenticatorNavigationInterceptor createAuthenticatorNavigationInterceptor() {
        return AppHooks.get().createAuthenticatorNavigationInterceptor(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognito();
    }

    @Override
    public boolean isHidden() {
        return mTab.isHidden();
    }

    @Override
    public boolean areIntentLaunchesAllowedInHiddenTabsForNavigation(NavigationHandle handle) {
        return false;
    }

    @Override
    public Activity getActivity() {
        return mTab.getActivity();
    }

    @Override
    public boolean wasTabLaunchedFromExternalApp() {
        return mTab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
    }

    @Override
    public boolean wasTabLaunchedFromLongPressInBackground() {
        return mTab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND;
    }

    @Override
    public void closeTab() {
        Toast.makeText(ContextUtils.getApplicationContext(), "TODO ArkInterceptNavigationDelegateClientImpl closeTab", Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onNavigationStarted(NavigationHandle handle) {}

    @Override
    public void onDecisionReachedForNavigation(
            NavigationHandle handle, OverrideUrlLoadingResult overrideUrlLoadingResult) {}

    public void initializeWithDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        mTab.addObserver(mTabObserver);
    }

    public void destroy() {
        assert mInterceptNavigationDelegate != null;
        mTab.removeObserver(mTabObserver);
        mInterceptNavigationDelegate = null;
    }
}
