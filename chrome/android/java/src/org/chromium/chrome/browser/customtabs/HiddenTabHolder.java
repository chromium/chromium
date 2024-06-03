// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

/**
 * Holds a hidden tab which may be used to preload pages before a CustomTabActivity is launched.
 *
 * Lifecycle: 1:1 relationship between this and {@link CustomTabsConnection}.
 * Thread safety: Only access on UI Thread.
 * Native: This class needs native to be loaded (since it creates Tabs).
 */
public class HiddenTabHolder {
    /** Holds the parameters for the current hidden tab speculation. */
    @VisibleForTesting
    static final class SpeculationParams {
        public final CustomTabsSessionToken session;
        public final String url;
        public final Tab tab;
        public final String referrer;

        private SpeculationParams(
                CustomTabsSessionToken session, String url, Tab tab, String referrer) {
            this.session = session;
            this.url = url;
            this.tab = tab;
            this.referrer = referrer;
        }
    }

    private class HiddenTabObserver extends EmptyTabObserver {
        // This WindowAndroid is "owned" by the Tab and should be destroyed when it is no longer
        // needed by the Tab or when the Tab is destroyed.
        private WindowAndroid mOwnedWindowAndroid;

        public HiddenTabObserver(WindowAndroid ownedWindowAndroid) {
            mOwnedWindowAndroid = ownedWindowAndroid;
        }

        @Override
        public void onCrash(Tab tab) {
            destroyHiddenTab(null);
        }

        @Override
        public void onDestroyed(Tab tab) {
            destroyOwnedWindow(tab);
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
            destroyOwnedWindow(tab);
        }

        private void destroyOwnedWindow(Tab tab) {
            assert mOwnedWindowAndroid != null;
            mOwnedWindowAndroid.destroy();
            mOwnedWindowAndroid = null;
            tab.removeObserver(this);
        }
    }

    @Nullable private SpeculationParams mSpeculation;

    /**
     * Creates a hidden tab and initiates a navigation.
     *
     * @param tabCreatedCallback Callback run with the tab that is created. This is run before the
     *     url is loaded.
     * @param session The {@link CustomTabsSessionToken} for the Tab to be associated with.
     * @param profile The Profile the tab is associated with.
     * @param clientManager The {@link ClientManager} to get referrer information and link
     *     PostMessage.
     * @param url The URL to load into the Tab.
     * @param extras Extras to be passed that may contain referrer information.
     * @param webContents The {@link WebContents} to use in the hidden tab. If null the default is
     *     used.
     */
    void launchUrlInHiddenTab(
            Callback<Tab> tabCreatedCallback,
            CustomTabsSessionToken session,
            Profile profile,
            ClientManager clientManager,
            String url,
            @Nullable Bundle extras,
            @Nullable WebContents webContents) {
        assert mSpeculation == null;
        Intent extrasIntent = new Intent();
        if (extras != null) extrasIntent.putExtras(extras);

        // Ensures no Browser.EXTRA_HEADERS were in the Intent.
        if (IntentHandler.getExtraHeadersFromIntent(extrasIntent) != null) return;

        WarmupManager warmupManager = WarmupManager.getInstance();
        if (warmupManager.hasSpareTab(profile) && webContents != null) {
            warmupManager.destroySpareTab();
        }
        warmupManager.createRegularSpareTab(profile, webContents);
        // In case creating the tab fails for some reason.
        if (!warmupManager.hasSpareTab(profile)) return;
        Tab tab =
                warmupManager.takeSpareTab(
                        profile, TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION);

        tabCreatedCallback.onResult(tab);

        HiddenTabObserver observer = new HiddenTabObserver(tab.getWindowAndroid());
        tab.addObserver(observer);

        // Updating post message as soon as we have a valid WebContents.
        clientManager.resetPostMessageHandlerForSession(session, tab.getWebContents());

        LoadUrlParams loadParams = new LoadUrlParams(url);
        String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(extrasIntent);
        if (referrer == null && clientManager.getDefaultReferrerForSession(session) != null) {
            referrer = clientManager.getDefaultReferrerForSession(session).getUrl();
        }
        if (referrer == null) referrer = "";
        if (!referrer.isEmpty()) {
            loadParams.setReferrer(new Referrer(referrer, ReferrerPolicy.DEFAULT));
        }
        // The sender of an intent can't be trusted, so we navigate from an opaque Origin to
        // avoid sending same-site cookies.
        loadParams.setInitiatorOrigin(Origin.createOpaqueOrigin());

        loadParams.setTransitionType(PageTransition.LINK | PageTransition.FROM_API);
        RedirectHandlerTabHelper.getOrCreateHandlerFor(tab).setIsPrefetchLoadForIntent(true);
        mSpeculation = new SpeculationParams(session, url, tab, referrer);
        mSpeculation.tab.loadUrl(loadParams);
    }

    /**
     * Returns the preloaded {@link Tab} if it matches the given |url| and |referrer|. Null if no
     * such {@link Tab}. If a {@link Tab} is preloaded but it does not match, it is discarded.
     *
     * @param session The Binder object identifying a session the hidden tab was created for.
     * @param ignoreFragments Whether to ignore fragments while matching the url.
     * @param url The URL the tab is for.
     * @param referrer The referrer to use for |url|.
     * @return The hidden tab, or null.
     */
    @Nullable
    Tab takeHiddenTab(
            @Nullable CustomTabsSessionToken session,
            boolean ignoreFragments,
            String url,
            @Nullable String referrer) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.takeHiddenTab")) {
            if (mSpeculation == null || session == null) return null;
            if (!session.equals(mSpeculation.session)) return null;

            Tab tab = mSpeculation.tab;
            String speculatedUrl = mSpeculation.url;
            String speculationReferrer = mSpeculation.referrer;

            mSpeculation = null;

            boolean urlsMatch =
                    ignoreFragments
                            ? UrlUtilities.urlsMatchIgnoringFragments(speculatedUrl, url)
                            : TextUtils.equals(speculatedUrl, url);

            if (referrer == null) referrer = "";

            if (urlsMatch && TextUtils.equals(speculationReferrer, referrer)) {
                return tab;
            } else {
                tab.destroy();
                return null;
            }
        }
    }

    /** Cancels the speculation for a given session, or any session if null. */
    void destroyHiddenTab(@Nullable CustomTabsSessionToken session) {
        if (mSpeculation == null) return;
        if (session != null && !session.equals(mSpeculation.session)) return;

        mSpeculation.tab.destroy();
        mSpeculation = null;
    }

    /** Gets the url of the current hidden tab, if it exists. */
    @Nullable
    String getSpeculatedUrl(CustomTabsSessionToken session) {
        if (mSpeculation == null || !mSpeculation.session.equals(session)) {
            return null;
        }
        return mSpeculation.url;
    }

    /** Returns whether there currently is a hidden tab. */
    boolean hasHiddenTab() {
        return mSpeculation != null;
    }

    public Tab getHiddenTabForTesting() {
        return mSpeculation != null ? mSpeculation.tab : null;
    }

    @Nullable
    SpeculationParams getSpeculationParamsForTesting() {
        return mSpeculation;
    }
}
