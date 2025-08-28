// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** Controls showing IPH for Minimized Custom Tabs. */
@NullMarked
public class MinimizedCustomTabIphController
        implements MinimizedCustomTabFeatureEngagementDelegate {
    // Ratio of the highlight circle diameter against the width of the containing view.
    private static final float HIGHLIGHT_CIRCLE_MIN_SCALE = 0.64f;
    private final Activity mActivity;
    private final ActivityTabProvider mTabProvider;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Profile> mProfileSupplier;
    private @Nullable ActivityTabTabObserver mTabObserver;

    /**
     * Constructs the controller.
     *
     * @param activity The {@link Activity}.
     * @param activityTabProvider The {@link ActivityTabProvider} for this Activity.
     * @param userEducationHelper The {@link UserEducationHelper} to show the IPH.
     * @param profileSupplier The {@link Supplier} for the current {@link Profile}.
     */
    public MinimizedCustomTabIphController(
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
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        var tracker = TrackerFactory.getTrackerForProfile(profile);
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
                        showIph(minimizeButton);
                    }
                };
    }

    private void showIph(View button) {
        Profile profile = mProfileSupplier.get();
        assert profile != null;
        var tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.isInitialized()) return;
        if (!tracker.wouldTriggerHelpUi(FeatureConstants.CCT_MINIMIZED_FEATURE)) return;
        float startingRadiusPx =
                button.getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_button_width)
                        / 2f
                        * HIGHLIGHT_CIRCLE_MIN_SCALE;

        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        PulseDrawable.Bounds circleBounds =
                new PulseDrawable.Bounds() {
                    @Override
                    public float getMaxRadiusPx(Rect bounds) {
                        // Radius of PulseDrawable circle when expanded. See {@link
                        // PulseDrawable#createCircle().
                        return startingRadiusPx * 1.2f;
                    }

                    @Override
                    public float getMinRadiusPx(Rect bounds) {
                        return startingRadiusPx;
                    }
                };
        params.setCircleRadius(circleBounds);
        params.setBoundsRespectPadding(true);
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.CCT_MINIMIZED_FEATURE,
                                R.string.custom_tab_minimize_button_iph_bubble_text,
                                R.string.custom_tab_minimize_button_iph_bubble_text)
                        .setAnchorView(button)
                        .setHighlightParams(params)
                        .build());
    }

    @Nullable ActivityTabTabObserver getTabObserverForTesting() {
        return mTabObserver;
    }
}
