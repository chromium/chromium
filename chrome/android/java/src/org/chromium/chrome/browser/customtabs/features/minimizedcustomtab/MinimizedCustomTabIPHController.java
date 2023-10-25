// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.app.Activity;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;

/** Controls showing IPH for Minimized Custom Tabs. */
public class MinimizedCustomTabIPHController
        implements MinimizedCustomTabFeatureEngagementDelegate {
    private final Activity mActivity;
    private final ActivityTabProvider mTabProvider;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Profile> mProfileSupplier;
    private ActivityTabTabObserver mTabObserver;

    /**
     * Constructs the controller.
     *
     * @param activity The {@link Activity}.
     * @param activityTabProvider The {@link ActivityTabProvider} for this Activity.
     * @param userEducationHelper The {@link UserEducationHelper} to show the IPH.
     * @param profileSupplier The {@link Supplier} for the current {@link Profile}.
     */
    public MinimizedCustomTabIPHController(
            Activity activity,
            ActivityTabProvider activityTabProvider,
            UserEducationHelper userEducationHelper,
            Supplier<Profile> profileSupplier) {
        mActivity = activity;
        mTabProvider = activityTabProvider;
        mUserEducationHelper = userEducationHelper;
        mProfileSupplier = profileSupplier;

        createTabObserver();
    }

    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }
    }

    @Override
    public void notifyUserEngaged() {
        if (!mProfileSupplier.hasValue()) return;

        var tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.addOnInitializedCallback(
                success -> tracker.notifyEvent(EventConstants.CCT_MINIMIZE_BUTTON_CLICKED));
    }

    private void createTabObserver() {
        mTabObserver =
                new ActivityTabTabObserver(mTabProvider) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        View minimizeButton =
                                mActivity.findViewById(R.id.custom_tabs_minimize_button);
                        if (minimizeButton == null
                                || minimizeButton.getVisibility() != View.VISIBLE) {
                            return;
                        }
                        showIPH(minimizeButton);
                    }
                };
    }

    private void showIPH(View button) {
        var tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        if (!tracker.isInitialized()) return;
        if (!tracker.wouldTriggerHelpUI(FeatureConstants.CCT_MINIMIZED_FEATURE)) return;
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.CCT_MINIMIZED_FEATURE,
                                R.string.custom_tab_minimize_button_iph_bubble_text,
                                R.string.custom_tab_minimize_button_iph_bubble_text)
                        .setAnchorView(button)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .build());
    }

    ActivityTabTabObserver getTabObserverForTesting() {
        return mTabObserver;
    }
}
