// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;

/**
 * Controls when the the Share Experiment IPH is shown.
 */
public class CrowIphController {
    private static final int DEFAULT_MIN_DAYS_VISITED = 3;
    private static final int DEFAULT_MIN_TOTAL_VISITS = 3;
    private static final int DEFAULT_NUM_HISTORY_LOOKBACK_DAYS = 5;
    private static final String MIN_DAYS_VISITED_PARAM = "min-days-visited";
    private static final String MIN_TOTAL_VISITS_PARAM = "min-total-visits";
    private static final String NUM_HISTORY_LOOKBACK_DAYS_PARAM = "num-history-lookback-days";

    private final Activity mActivity;
    private final AppMenuHandler mAppMenuHandler;
    private final CrowButtonDelegate mCrowButtonDelegate;
    private final CurrentTabObserver mPageLoadObserver;
    private final UserEducationHelper mUserEducationHelper;
    private final View mMenuButtonAnchorView;

    private final int mMinDaysVisited;
    private final int mMinTotalVisits;
    private final int mNumHistoryLookbackDays;

    /**
     * Constructs a {@link CrowIphController}.
     *
     * @param activity The current activity.
     * @param appMenuHandler The app menu containing the menu entry to highlight.
     * @param crowButtonDelegate The delegate that determines whether Crow is enabled for the site.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button view to anchor the bubble to.
     */
    public CrowIphController(Activity activity, AppMenuHandler appMenuHandler,
            CrowButtonDelegate crowButtonDelegate, ObservableSupplier<Tab> tabSupplier,
            View menuButtonAnchorView) {
        mActivity = activity;
        mAppMenuHandler = appMenuHandler;
        mCrowButtonDelegate = crowButtonDelegate;
        mMenuButtonAnchorView = menuButtonAnchorView;
        mUserEducationHelper = new UserEducationHelper(activity, new Handler());

        mMinDaysVisited = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.SHARE_CROW_BUTTON, MIN_DAYS_VISITED_PARAM,
                DEFAULT_MIN_DAYS_VISITED);
        mMinTotalVisits = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.SHARE_CROW_BUTTON, MIN_TOTAL_VISITS_PARAM,
                DEFAULT_MIN_TOTAL_VISITS);
        mNumHistoryLookbackDays = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.SHARE_CROW_BUTTON, NUM_HISTORY_LOOKBACK_DAYS_PARAM,
                DEFAULT_NUM_HISTORY_LOOKBACK_DAYS);

        mPageLoadObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                if (tab.isShowingErrorPage()) {
                    return;
                }
                mCrowButtonDelegate.isEnabledForSite(url, (enabled) -> {
                    if (enabled) maybeShowCrowIph(url);
                });
            }
        });
    }

    public void destroy() {
        mPageLoadObserver.destroy();
    }

    private void maybeShowCrowIph(GURL url) {
        CrowBridge.getVisitCountsToHost(url, mNumHistoryLookbackDays, result -> {
            if (result.dailyVisits >= mMinDaysVisited && result.visits >= mMinTotalVisits) {
                requestShowCrowIph();
            }
        });
    }

    private void requestShowCrowIph() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(), FeatureConstants.CROW_FEATURE,
                        R.string.crow_iph, R.string.crow_iph)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(this::turnOnHighlightForCrowMenuItem)
                        .setOnDismissCallback(this::turnOffHighlightForCrowMenuItem)
                        .build());
    }

    private void turnOnHighlightForCrowMenuItem() {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.setMenuHighlight(R.id.crow_chip_view);
        }
    }

    private void turnOffHighlightForCrowMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }
}
