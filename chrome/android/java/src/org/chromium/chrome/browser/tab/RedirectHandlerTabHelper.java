// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;

/** This class glues RedirectHandler instances to Tabs. */
public class RedirectHandlerTabHelper extends EmptyTabObserver implements UserData {
    private static final Class<RedirectHandlerTabHelper> USER_DATA_KEY =
            RedirectHandlerTabHelper.class;

    private Tab mTab;
    private RedirectHandler mRedirectHandler;

    /**
     * Returns {@link RedirectHandler} that hangs on to a given {@link Tab}.
     * If not present, creates a new instance and associate it with the {@link UserDataHost}
     * that the {@link Tab} manages.
     * @param tab Tab instance that the RedirectHandler hangs on to.
     * @return RedirectHandler for a given Tab.
     */
    public static RedirectHandler getOrCreateHandlerFor(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        RedirectHandlerTabHelper helper = host.getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper = new RedirectHandlerTabHelper(tab);
            host.setUserData(USER_DATA_KEY, helper);
            tab.addObserver(helper);
        }
        return helper.mRedirectHandler;
    }

    /**
     * @return {@link RedirectHandler} hanging to the given {@link Tab},
     *     or {@code null} if there is no instance available.
     */
    public static @Nullable RedirectHandler getHandlerFor(Tab tab) {
        RedirectHandlerTabHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) return null;
        return helper.mRedirectHandler;
    }

    /**
     * Replace {@link RedirectHandler} instance for the Tab with the new one.
     * @return Old {@link RedirectHandler} associated with the Tab. Could be {@code null}.
     */
    public static RedirectHandler swapHandlerFor(Tab tab, RedirectHandler newHandler) {
        assert newHandler != null;
        RedirectHandlerTabHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) {
            getOrCreateHandlerFor(tab);
            helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        }
        RedirectHandler oldHandler = helper.mRedirectHandler;
        helper.mRedirectHandler = newHandler;
        return oldHandler;
    }

    private RedirectHandlerTabHelper(Tab tab) {
        mTab = tab;
        mRedirectHandler = RedirectHandler.create();
    }

    private RedirectHandlerTabHelper(Tab tab, RedirectHandler handler) {
        mTab = tab;
        mRedirectHandler = handler;
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
        mTab = null;
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        mRedirectHandler.clear();
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        if (navigation.isPageActivation()) {
            // Page Activations (e.g. for back/forward cache or Prerender) don't trigger
            // NavigationThrottles, so the RedirectHandler doesn't have insight into these
            // navigations, and we don't want to consider navigations after a Page Activation to be
            // part of the previous navigation chain.
            mRedirectHandler.clear();
        }
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    /** Wrapper around RedirectHandler#updateIntent() that supplies //chrome-level params. */
    public static void updateIntentInTab(Tab tab, @Nullable Intent intent) {
        boolean isCustomTab = false;
        boolean sendToExternalHandler = false;
        boolean startedTabbedChromeTask = false;
        if (intent != null) {
            isCustomTab = LaunchIntentDispatcher.isCustomTabIntent(intent);
            sendToExternalHandler = CustomTabsIntent.isSendToExternalDefaultHandlerEnabled(intent);
            startedTabbedChromeTask =
                    IntentUtils.safeGetBooleanExtra(
                            intent, IntentHandler.EXTRA_STARTED_TABBED_CHROME_TASK, false);
        }
        RedirectHandlerTabHelper.getOrCreateHandlerFor(tab)
                .updateIntent(intent, isCustomTab, sendToExternalHandler, startedTabbedChromeTask);
    }
}
