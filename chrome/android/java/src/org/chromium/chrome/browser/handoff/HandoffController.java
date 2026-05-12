// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.handoff;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.HandoffActivityData;
import android.app.HandoffActivityDataRequestInfo;
import android.app.HandoffActivityParams;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.os.UserManager;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Orchestrates Handoff integration for a {@link Activity}. This class manages the opt-in state for
 * the Activity and handles data requests from the Android platform.
 */
@NullMarked
@SuppressLint("NewApi")
public class HandoffController implements TabModelSelectorObserver, Destroyable {
    @IntDef({
        HandoffEnableTrigger.TAB_SWITCH,
        HandoffEnableTrigger.URL_NAVIGATION,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HandoffEnableTrigger {
        int TAB_SWITCH = 0;
        int URL_NAVIGATION = 1;
    }

    private final Activity mActivity;
    private final TabModelSelector mTabModelSelector;
    private final ActivityTabProvider mActivityTabProvider;
    private final Delegate mDelegate;
    private final ActivityTabTabObserver mActivityTabTabObserver;

    private @Nullable GURL mTabLastUrlSeen;

    /** Delegate interface for Android Handoff system APIs. */
    interface Delegate {
        void setHandoffEnabled(Activity activity, boolean enabled);

        boolean isHandoffEnabled(Activity activity);

        @Nullable Object buildHandoffActivityData(Activity activity, String url);
    }

    private static class DelegateImpl implements Delegate {
        @Override
        public void setHandoffEnabled(Activity activity, boolean enabled) {
            // TODO(crbug.com/444503472): Re-enabling setAllowHandoffWithoutPackageInstalled(true)
            //  pending approval to open URLs in the receiver's default browser.
            HandoffActivityParams params =
                    new HandoffActivityParams.Builder()
                            .setAllowHandoffWithoutPackageInstalled(false)
                            .build();
            activity.setHandoffEnabled(enabled, params);
        }

        @Override
        public boolean isHandoffEnabled(Activity activity) {
            return activity.isHandoffEnabled();
        }

        @Override
        public Object buildHandoffActivityData(Activity activity, String url) {
            PersistableBundle extras = new PersistableBundle();
            extras.putBoolean(IntentHandler.EXTRA_INVOKED_FROM_HANDOFF, true);
            extras.putString(IntentHandler.EXTRA_HANDOFF_URL, url);
            extras.putString(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
            extras.putBoolean(Browser.EXTRA_CREATE_NEW_TAB, true);

            return new HandoffActivityData.Builder(activity.getComponentName())
                    .setFallbackUri(Uri.parse(url))
                    .setExtras(extras)
                    .build();
        }
    }

    public HandoffController(
            Activity activity,
            TabModelSelector tabModelSelector,
            ActivityTabProvider activityTabProvider) {
        this(activity, tabModelSelector, activityTabProvider, new DelegateImpl());
    }

    @VisibleForTesting
    HandoffController(
            Activity activity,
            TabModelSelector tabModelSelector,
            ActivityTabProvider activityTabProvider,
            Delegate delegate) {
        mActivity = activity;
        mTabModelSelector = tabModelSelector;
        mActivityTabProvider = activityTabProvider;
        mDelegate = delegate;
        mTabModelSelector.addObserver(this);

        Tab currentTab = activityTabProvider.get();
        mTabLastUrlSeen = (currentTab != null) ? currentTab.getUrl() : null;

        mActivityTabTabObserver =
                new ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        boolean isNormalTab = tab != null && !tab.isIncognitoBranded();
                        assumeNonNull(tab);
                        mTabLastUrlSeen = isNormalTab ? tab.getUrl() : null;

                        updateHandoffState(HandoffEnableTrigger.TAB_SWITCH);
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        // Ignore duplicate calls to #onUrlUpdate within the same navigation.
                        GURL currentUrl = tab.getUrl();
                        if (Objects.equals(currentUrl, mTabLastUrlSeen)) return;
                        if (tab.isIncognitoBranded()) return;
                        mTabLastUrlSeen = currentUrl;

                        updateHandoffState(HandoffEnableTrigger.URL_NAVIGATION);
                    }
                };
    }

    @Override
    public void destroy() {
        mActivityTabTabObserver.destroy();
        mTabModelSelector.removeObserver(this);
    }

    // TabModelSelectorObserver implementation.
    @Override
    public void onChange() {
        updateHandoffState(HandoffEnableTrigger.TAB_SWITCH);
    }

    ActivityTabTabObserver getActiveTabObserverForTesting() {
        return mActivityTabTabObserver;
    }

    /**
     * Updates the handoff enablement state for the activity. Handoff is disabled if the user is in
     * Incognito mode or if there is no active tab (e.g. in the tab switcher).
     */
    private void updateHandoffState(@HandoffEnableTrigger int updateType) {
        if (mActivityTabProvider == null) return;

        Tab tab = mActivityTabProvider.get();
        boolean isIncognito = mTabModelSelector.isIncognitoBrandedModelSelected();

        // 1. Check enterprise policy / user restrictions.
        UserManager userManager = (UserManager) mActivity.getSystemService(Context.USER_SERVICE);
        boolean isDisallowedByPolicy = false;
        if (userManager != null) {
            Bundle restrictions = userManager.getUserRestrictions();
            // TODO(crbug.com/444503472): Change "disallow_handoff" to UserManager#DISALLOW_HANDOFF,
            // once it is integrated into the Chrome build.
            isDisallowedByPolicy = restrictions.getBoolean("disallow_handoff", false);
        }

        // 2. Opt-out if in incognito, disallowed by policy, or no active tab to protect
        // privacy/comply with enterprise/reflect actual activity.
        boolean handoffEnabled = tab != null && !isIncognito && !isDisallowedByPolicy;
        boolean wasHandoffEnabled = mDelegate.isHandoffEnabled(mActivity);

        if (handoffEnabled && !wasHandoffEnabled) {
            String histogramName =
                    switch (updateType) {
                        case HandoffEnableTrigger.TAB_SWITCH -> "Android.Handoff.Enabled.TabSwitch";
                        case HandoffEnableTrigger.URL_NAVIGATION ->
                                "Android.Handoff.Enabled.UrlNavigation";
                        default -> null;
                    };
            if (histogramName != null) {
                RecordHistogram.recordBooleanHistogram(histogramName, true);
            }
        } else if (!handoffEnabled && wasHandoffEnabled) {
            RecordUserAction.record("HandoffDisabled");
        }

        // 3. Resets the handoff state to allow OS to refresh and resurface the handoff icon.
        if (handoffEnabled && wasHandoffEnabled) {
            mDelegate.setHandoffEnabled(mActivity, false);
        }

        // 4. Update handoff state via delegate.
        mDelegate.setHandoffEnabled(mActivity, handoffEnabled);
    }

    public @Nullable HandoffActivityData onHandoffActivityDataRequested(
            HandoffActivityDataRequestInfo requestInfo) {
        // 1. Get the active tab.
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isOffTheRecord()) {
            return null;
        }

        RecordUserAction.record("HandoffDataRequested");

        // 2. Build the handoff data via delegate.
        return (HandoffActivityData)
                mDelegate.buildHandoffActivityData(mActivity, tab.getUrl().getSpec());
    }
}
