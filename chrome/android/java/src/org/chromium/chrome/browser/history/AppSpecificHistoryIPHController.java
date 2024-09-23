// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controls showing IPH for App Specific history. */
public class AppSpecificHistoryIPHController {
    private final Activity mActivity;
    private final Supplier<Profile> mProfileSupplier;
    private UserEducationHelper mUserEducationHelper;

    /**
     * Constructs the controller.
     *
     * @param activity The {@link Activity}.
     * @param profileSupplier The {@link Supplier} for the current {@link Profile}.
     */
    public AppSpecificHistoryIPHController(Activity activity, Supplier<Profile> profileSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
    }

    public void notifyUserEngaged() {
        if (!mProfileSupplier.hasValue()) return;

        var tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.addOnInitializedCallback(
                success ->
                        tracker.notifyEvent(
                                EventConstants.HISTORY_TOOLBAR_SEARCH_MENU_ITEM_CLICKED));
    }

    void maybeShowIPH() {
        if (!shouldShowIPH()) {
            return;
        }
        View historyToolbarSearchMenuItem = mActivity.findViewById(R.id.search_menu_id);
        if (mUserEducationHelper == null) {
            mUserEducationHelper =
                    new UserEducationHelper(
                            mActivity, mProfileSupplier, new Handler(Looper.getMainLooper()));
        }
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.APP_SPECIFIC_HISTORY_FEATURE,
                                R.string.history_iph_bubble_text,
                                R.string.history_iph_bubble_text)
                        .setAnchorView(historyToolbarSearchMenuItem)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .build());
    }

    private boolean shouldShowIPH() {
        if (!HistoryManager.isAppSpecificHistoryEnabled()) return false;

        var tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        return (tracker.isInitialized()
                && tracker.wouldTriggerHelpUI(FeatureConstants.APP_SPECIFIC_HISTORY_FEATURE));
    }

    void setUserEducationHelperForTesting(UserEducationHelper userEducationHelper) {
        mUserEducationHelper = userEducationHelper;
    }
}
