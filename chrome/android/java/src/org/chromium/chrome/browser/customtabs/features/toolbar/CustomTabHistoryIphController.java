// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;

/** Controls showing IPH for Custom Tabs history. */
public class CustomTabHistoryIphController {
    private final Activity mActivity;
    private final ActivityTabProvider mTabProvider;
    private final Supplier<Profile> mProfileSupplier;
    private final AppMenuHandler mAppMenuHandler;
    private ActivityTabTabObserver mTabObserver;
    private UserEducationHelper mUserEducationHelper;

    /**
     * Constructs the controller.
     *
     * @param activity The {@link Activity}.
     * @param activityTabProvider The {@link ActivityTabProvider} for this Activity.
     * @param profileSupplier The {@link Supplier} for the current {@link Profile}.
     * @param appMenuHandler The {@link AppMenuHandler} for the tab.
     */
    public CustomTabHistoryIphController(
            Activity activity,
            ActivityTabProvider activityTabProvider,
            Supplier<Profile> profileSupplier,
            AppMenuHandler appMenuHandler) {
        mActivity = activity;
        mTabProvider = activityTabProvider;
        mProfileSupplier = profileSupplier;
        mAppMenuHandler = appMenuHandler;
        mTabObserver =
                new ActivityTabTabObserver(mTabProvider) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        maybeShowIph();
                    }
                };
    }

    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }
    }

    public void notifyUserEngaged() {
        if (!mProfileSupplier.hasValue()) return;

        var tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.addOnInitializedCallback(
                success -> tracker.notifyEvent(EventConstants.CCT_HISTORY_MENU_ITEM_CLICKED));
    }

    private void maybeShowIph() {
        if (!shouldShowIph()) return;
        if (mUserEducationHelper == null) {
            mUserEducationHelper =
                    new UserEducationHelper(
                            mActivity, mProfileSupplier, new Handler(Looper.getMainLooper()));
        }

        View ctOverflowMenu = mActivity.findViewById(R.id.menu_button_wrapper);
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.CCT_HISTORY_FEATURE,
                                R.string.custom_tab_history_iph_bubble_text,
                                R.string.custom_tab_history_iph_bubble_text)
                        .setAnchorView(ctOverflowMenu)
                        .setOnShowCallback(this::turnOnHighlightForMenuItem)
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .build());
    }

    private boolean shouldShowIph() {
        if (!ChromeFeatureList.sAppSpecificHistory.isEnabled()) return false;

        if (mProfileSupplier.get().isOffTheRecord()) return false;

        var tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        return (tracker.isInitialized()
                && tracker.wouldTriggerHelpUi(FeatureConstants.CCT_HISTORY_FEATURE));
    }

    private void turnOnHighlightForMenuItem() {
        mAppMenuHandler.setMenuHighlight(R.id.open_history_menu_id);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }

    ActivityTabTabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    void setHighlightMenuItemForTesting(boolean on) {
        if (on) {
            turnOnHighlightForMenuItem();
        } else {
            turnOffHighlightForMenuItem();
        }
    }

    void setUserEducationHelperForTesting(UserEducationHelper userEducationHelper) {
        mUserEducationHelper = userEducationHelper;
    }
}
