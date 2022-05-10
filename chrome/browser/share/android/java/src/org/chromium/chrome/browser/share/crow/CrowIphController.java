// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
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
    // TODO(crbug/1314455): Adjust these numbers.
    private static final int MIN_DAYS = 1;
    private static final int MIN_VISITS = 1;
    private static final int NUM_HISTORY_DAYS = 7;

    private final Activity mActivity;
    private final AppMenuHandler mAppMenuHandler;
    private final CrowButtonDelegate mCrowButtonDelegate;
    private final CurrentTabObserver mPageLoadObserver;
    private final UserEducationHelper mUserEducationHelper;
    private final View mMenuButtonAnchorView;

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

        mPageLoadObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                if (tab.isShowingErrorPage() || !mCrowButtonDelegate.isEnabledForSite(url)) {
                    return;
                }
                maybeShowCrowIph(url);
            }
        });
    }

    public void destroy() {
        mPageLoadObserver.destroy();
    }

    private void maybeShowCrowIph(GURL url) {
        CrowBridge.getVisitCountsToHost(url, NUM_HISTORY_DAYS, result -> {
            if (result.dailyVisits >= MIN_DAYS && result.visits >= MIN_VISITS) {
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
