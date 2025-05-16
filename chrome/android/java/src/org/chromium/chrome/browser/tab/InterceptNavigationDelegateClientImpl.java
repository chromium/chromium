// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private static Boolean sIsInDesktopWindowingModeForTesting;
    private final TabImpl mTab;
    private final TabObserver mTabObserver;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;

    public static InterceptNavigationDelegateClientImpl createForTesting(Tab tab) {
        return new InterceptNavigationDelegateClientImpl(tab);
    }

    InterceptNavigationDelegateClientImpl(Tab tab) {
        mTab = (TabImpl) tab;
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        mInterceptNavigationDelegate.associateWithWebContents(tab.getWebContents());
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (window != null) {
                            mInterceptNavigationDelegate.setExternalNavigationHandler(
                                    createExternalNavigationHandler());
                        }
                        mInterceptNavigationDelegate.onActivityAttachmentChanged(window != null);
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        mInterceptNavigationDelegate.onNavigationFinishedInPrimaryMainFrame(
                                navigation);
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
        return mTab.getDelegateFactory().createExternalNavigationHandler(mTab);
    }

    @Override
    public RedirectHandler getOrCreateRedirectHandler() {
        return RedirectHandlerTabHelper.getOrCreateHandlerFor(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognitoBranded();
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
        if (mTab.isClosing()) return;
        mTab.getActivity()
                .getTabModelSelector()
                .tryCloseTab(
                        TabClosureParams.closeTab(mTab).allowUndo(false).build(),
                        /* allowDialog= */ false);
    }

    public void initializeWithDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        mTab.addObserver(mTabObserver);
    }

    public void destroy() {
        assert mInterceptNavigationDelegate != null;
        mTab.removeObserver(mTabObserver);
        mInterceptNavigationDelegate = null;
    }

    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        if (mTab.isDestroyed() || mTab.isClosing()) return;
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public boolean isTabInPWA() {
        return mTab.isTabInPWA();
    }

    @Override
    public boolean isTabInBrowser() {
        return mTab.isTabInBrowser();
    }

    @Override
    public boolean isInDesktopWindowingMode() {
        if (sIsInDesktopWindowingModeForTesting != null) {
            return sIsInDesktopWindowingModeForTesting;
        }

        // TODO(crbug.com/417047079): replace multli-window mode check with desktop windowing mode
        // as soon as https://chromium-review.googlesource.com/c/chromium/src/+/6527788 is resolved.
        return MultiWindowUtils.getInstance().isInMultiWindowMode(getActivity());
    }

    @Override
    public void startReparentingTask() {
        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ReparentingTask.from(mTab)
                .begin(
                        ContextUtils.getApplicationContext(),
                        intent,
                        /* startActivityOptions= */ null,
                        /* finalizeCallback= */ null);
    }

    public static void setIsDesktopWindowingModeForTesting(boolean v) {
        sIsInDesktopWindowingModeForTesting = v;
        ResettersForTesting.register(() -> sIsInDesktopWindowingModeForTesting = null);
    }
}
