// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** This class glues RedirectHandler instances to Tabs. */
@NullMarked
public class RedirectHandlerTabHelper implements TabObserver, UserData {
    private static final Class<RedirectHandlerTabHelper> USER_DATA_KEY =
            RedirectHandlerTabHelper.class;

    private final Tab mTab;
    private RedirectHandler mRedirectHandler;

    /**
     * Returns {@link RedirectHandler} that hangs on to a given {@link Tab}. If not present, creates
     * a new instance and associates it with the {@link UserDataHost} that the {@link Tab} manages.
     *
     * @param tab Tab instance that the RedirectHandler hangs on to.
     * @return RedirectHandler for a given Tab.
     */
    public static RedirectHandler getOrCreateHandlerFor(Tab tab) {
        return getOrCreateFor(tab).mRedirectHandler;
    }

    static void maybeCreateForInitialNavigation(Tab tab, NavigationHandle navigation) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null
                && isInitialAboutNavigation(tab, navigation)) {
            getOrCreateFor(tab).recordInitialAboutNavigation(navigation);
        }
    }

    private static boolean isInitialAboutNavigation(Tab tab, NavigationHandle navigation) {
        @Nullable WebContents webContents = tab.getWebContents();
        return webContents != null
                && UrlConstants.ABOUT_SCHEME.equals(navigation.getUrl().getScheme())
                && navigation.isRendererInitiated()
                && !navigation.isRedirect()
                && webContents.getNavigationController().getLastCommittedEntryIndex() == 0;
    }

    private static RedirectHandlerTabHelper getOrCreateFor(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        RedirectHandlerTabHelper helper = host.getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper = new RedirectHandlerTabHelper(tab);
            host.setUserData(USER_DATA_KEY, helper);
            tab.addObserver(helper);
        }
        return helper;
    }

    /**
     * Returns {@link RedirectHandler} hanging to the given {@link Tab}, or {@code null} if there is
     * no instance available.
     */
    public static @Nullable RedirectHandler getHandlerFor(Tab tab) {
        RedirectHandlerTabHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) return null;
        return helper.mRedirectHandler;
    }

    /**
     * Replaces {@link RedirectHandler} instance for the Tab with the new one.
     *
     * @return Old {@link RedirectHandler} associated with the Tab.
     */
    public static RedirectHandler swapHandlerForTesting(Tab tab, RedirectHandler newHandler) {
        RedirectHandlerTabHelper helper = getOrCreateFor(tab);
        RedirectHandler oldHandler = helper.mRedirectHandler;
        helper.mRedirectHandler = newHandler;
        return oldHandler;
    }

    private RedirectHandlerTabHelper(Tab tab) {
        mTab = tab;
        mRedirectHandler = RedirectHandler.create();
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        mRedirectHandler.clear();
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        // The initial about:blank navigation in a new window is synchronously committed by the
        // renderer, and so is not visible to Navigation Throttles.
        if (!mRedirectHandler.isOnNavigation() && isInitialAboutNavigation(tab, navigation)) {
            recordInitialAboutNavigation(navigation);
        }

        if (navigation.isPageActivation()) {
            // Page Activations (e.g. for back/forward cache or Prerender) don't trigger
            // NavigationThrottles, so the RedirectHandler doesn't have insight into these
            // navigations, and we don't want to consider navigations after a Page Activation to be
            // part of the previous navigation chain.
            mRedirectHandler.clear();
        }
    }

    private void recordInitialAboutNavigation(NavigationHandle navigation) {
        assert !mRedirectHandler.isOnNavigation();
        mRedirectHandler.updateNewUrlLoading(
                navigation.pageTransition(),
                /* isRedirect= */ false,
                navigation.hasUserGesture(),
                /* lastCommittedEntryIndex= */ 0,
                /* isInitialNavigation= */ true,
                /* isRendererInitiated= */ true);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    /** Wrapper around RedirectHandler#updateIntent() that supplies //chrome-level params. */
    public static void updateIntentInTab(Tab tab, @Nullable Intent intent, boolean isCustomTab) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) {
            @Nullable RedirectHandler handler = getHandlerFor(tab);
            if (handler != null) {
                handler.updateIntent(
                        intent,
                        isCustomTab,
                        /* sendToExternalApps= */ false,
                        /* externalIntentStartedTask= */ false,
                        /* canInitialNavigationLeaveChrome= */ false);
            }
            return;
        }
        boolean sendToExternalHandler =
                CustomTabsIntent.isSendToExternalDefaultHandlerEnabled(intent);
        boolean startedTabbedChromeTask =
                IntentUtils.safeGetBooleanExtra(
                        intent, IntentHandler.EXTRA_STARTED_TABBED_CHROME_TASK, false);
        boolean canInitialNavigationLeaveChrome =
                CustomTabsIntent.isInitialNavigationAllowedToLeaveBrowser(intent);
        RedirectHandlerTabHelper.getOrCreateHandlerFor(tab)
                .updateIntent(
                        intent,
                        isCustomTab,
                        sendToExternalHandler,
                        startedTabbedChromeTask,
                        canInitialNavigationLeaveChrome);
    }
}
