// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.handoff;

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

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;

/**
 * Orchestrates Handoff integration for a {@link Activity}. This class manages the opt-in state for
 * the Activity and handles data requests from the Android platform.
 */
@NullMarked
@SuppressLint("NewApi")
public class HandoffController implements TabModelSelectorObserver {
    private final Activity mActivity;
    private final TabModelSelector mTabModelSelector;
    private final ActivityTabProvider mActivityTabProvider;
    private final Delegate mDelegate;

    /** Delegate interface for Android Handoff system APIs. */
    interface Delegate {
        void setHandoffEnabled(Activity activity, boolean enabled);

        @Nullable Object buildHandoffActivityData(Activity activity, String url);
    }

    private static class DelegateImpl implements Delegate {
        @Override
        public void setHandoffEnabled(Activity activity, boolean enabled) {
            // TODO(crbug.com/444503472): Verify with xfn whether we should allow handoff to be
            //  opened in default browser if Chrome is not installed.
            HandoffActivityParams params =
                    new HandoffActivityParams.Builder()
                            .setAllowHandoffWithoutPackageInstalled(true)
                            .build();
            activity.setHandoffEnabled(enabled, params);
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

        // TODO(crbug.com/444503472): implement updates for url navigation and tab switches.
        updateHandoffState();
    }

    public void destroy() {
        mTabModelSelector.removeObserver(this);
    }

    // TabModelSelectorObserver implementation.
    @Override
    public void onChange() {
        updateHandoffState();
    }

    /**
     * Updates the handoff enablement state for the activity. Handoff is disabled if the user is in
     * Incognito mode.
     */
    private void updateHandoffState() {
        boolean isIncognito = mTabModelSelector.isIncognitoBrandedModelSelected();

        // Check enterprise policy / user restrictions.
        UserManager userManager = (UserManager) mActivity.getSystemService(Context.USER_SERVICE);
        boolean isDisallowedByPolicy = false;
        if (userManager != null) {
            Bundle restrictions = userManager.getUserRestrictions();
            // TODO(crbug.com/444503472): Change "disallow_handoff" to UserManager#DISALLOW_HANDOFF,
            // once it is integrated into the Chrome build.
            isDisallowedByPolicy = restrictions.getBoolean("disallow_handoff", false);
        }

        // Opt-out if in incognito or disallowed by policy to protect privacy/comply with
        // enterprise.
        boolean handoffEnabled = !isIncognito && !isDisallowedByPolicy;

        // Update handoff state via delegate.
        mDelegate.setHandoffEnabled(mActivity, handoffEnabled);
    }

    public @Nullable HandoffActivityData onHandoffActivityDataRequested(
            HandoffActivityDataRequestInfo requestInfo) {
        // 1. Get the active tab.
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isOffTheRecord()) {
            return null;
        }

        // 2. Build the handoff data via delegate.
        return (HandoffActivityData)
                mDelegate.buildHandoffActivityData(mActivity, tab.getUrl().getSpec());
    }
}
