// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import static androidx.browser.customtabs.CustomTabsCallback.NAVIGATION_FAILED;
import static androidx.browser.customtabs.CustomTabsCallback.NAVIGATION_FINISHED;
import static androidx.browser.customtabs.CustomTabsCallback.NAVIGATION_STARTED;
import static androidx.browser.customtabs.CustomTabsCallback.TAB_HIDDEN;
import static androidx.browser.customtabs.CustomTabsCallback.TAB_SHOWN;

import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsCallback;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.net.NetError;

import java.util.ArrayList;
import java.util.List;

/**
 * An observer for firing navigation events to the CCT dynamic module.
 */
public class DynamicModuleNavigationEventObserver extends EmptyTabObserver {
    @VisibleForTesting
    public static final String URL_KEY = "urlInfo";

    @VisibleForTesting
    public static final String PENDING_URL_KEY = "pendingUrl";

    private static final String TIMESTAMP_KEY = "timestampUptimeMillis";

    @Nullable
    private ActivityDelegate mActivityDelegate;

    private static class NavigationEvent {
        // The navigation event codes are defined in CustomTabsCallback class.
        final int mNavigationEvent;
        final Bundle mExtra;

        private NavigationEvent(int navigationEvent, Bundle extra) {
            mNavigationEvent = navigationEvent;
            mExtra = extra;
        }
    }

    // If module has not been loaded yet we need to enqueue events
    // and send it once module is loaded.
    private final List<NavigationEvent> mNavigationEvents = new ArrayList<>();

    private void notifyOnNavigationEvent(int navigationEvent, Bundle bundle) {
        if (mActivityDelegate == null) {
            mNavigationEvents.add(new NavigationEvent(navigationEvent, bundle));
            return;
        }

        mActivityDelegate.onNavigationEvent(navigationEvent, bundle);
    }

    /**
     * Set ActivityDelegate which will be notified on navigation events.
     */
    public void setActivityDelegate(ActivityDelegate activityDelegate) {
        assert activityDelegate != null && mActivityDelegate == null;
        mActivityDelegate = activityDelegate;
        for (NavigationEvent event: mNavigationEvents) {
            mActivityDelegate.onNavigationEvent(event.mNavigationEvent, event.mExtra);
        }
        mNavigationEvents.clear();
    }

    private Bundle getExtrasBundleForNavigationEvent(Tab tab) {
        Bundle bundle = new Bundle();
        bundle.putLong(TIMESTAMP_KEY, SystemClock.uptimeMillis());
        String url = tab.getUrl();
        if (!TextUtils.isEmpty(url)) bundle.putParcelable(URL_KEY, Uri.parse(url));

        NavigationEntry entry = tab.getWebContents().getNavigationController()
                .getPendingEntry();
        String pendingUrl =  entry != null ? entry.getUrl() : null;
        if (pendingUrl != null) bundle.putParcelable(PENDING_URL_KEY, Uri.parse(pendingUrl));

        return bundle;
    }

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        notifyOnNavigationEvent(NAVIGATION_STARTED, getExtrasBundleForNavigationEvent(tab));
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        notifyOnNavigationEvent(NAVIGATION_FINISHED, getExtrasBundleForNavigationEvent(tab));
    }

    @Override
    public void onPageLoadFailed(Tab tab, @NetError int errorCode) {
        int navigationEvent = errorCode == NetError.ERR_ABORTED
                ? CustomTabsCallback.NAVIGATION_ABORTED
                : CustomTabsCallback.NAVIGATION_FAILED;
        notifyOnNavigationEvent(navigationEvent, getExtrasBundleForNavigationEvent(tab));
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        notifyOnNavigationEvent(TAB_SHOWN, getExtrasBundleForNavigationEvent(tab));
    }

    @Override
    public void onHidden(Tab tab, @Tab.TabHidingType int type) {
        notifyOnNavigationEvent(TAB_HIDDEN, getExtrasBundleForNavigationEvent(tab));
    }

    @Override
    public void onDidAttachInterstitialPage(Tab tab) {
        if (tab.getSecurityLevel() != ConnectionSecurityLevel.DANGEROUS) return;
        notifyOnNavigationEvent(NAVIGATION_FAILED, getExtrasBundleForNavigationEvent(tab));
    }
}
