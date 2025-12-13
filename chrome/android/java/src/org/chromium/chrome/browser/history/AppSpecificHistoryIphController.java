// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.function.Supplier;

/** Controls showing IPH for App Specific history. */
@NullMarked
public class AppSpecificHistoryIphController {
    private final Activity mActivity;
    private final Supplier<Profile> mProfileSupplier;
    private @Nullable UserEducationHelper mUserEducationHelper;

    /**
     * Constructs the controller.
     *
     * @param activity The {@link Activity}.
     * @param profileSupplier The {@link Supplier} for the current {@link Profile}.
     */
    public AppSpecificHistoryIphController(Activity activity, Supplier<Profile> profileSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
    }

    public void notifyUserEngaged() {
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        var tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.addOnInitializedCallback(
                success ->
                        tracker.notifyEvent(
                                EventConstants.HISTORY_TOOLBAR_SEARCH_MENU_ITEM_CLICKED));
    }

    void maybeShowIph() {
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;
        if (!HistoryManager.isAppSpecificHistoryEnabled()) return;

        var tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.isInitialized()
                || !tracker.wouldTriggerHelpUi(FeatureConstants.APP_SPECIFIC_HISTORY_FEATURE)) {
            return;
        }
        View historyToolbarSearchMenuItem = mActivity.findViewById(R.id.search_menu_id);
        if (mUserEducationHelper == null) {
            mUserEducationHelper =
                    new UserEducationHelper(
                            mActivity, profile, new Handler(Looper.getMainLooper()));
        }
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.APP_SPECIFIC_HISTORY_FEATURE,
                                R.string.history_iph_bubble_text,
                                R.string.history_iph_bubble_text)
                        .setAnchorView(historyToolbarSearchMenuItem)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .build());
    }

    void setUserEducationHelperForTesting(UserEducationHelper userEducationHelper) {
        mUserEducationHelper = userEducationHelper;
    }
}
