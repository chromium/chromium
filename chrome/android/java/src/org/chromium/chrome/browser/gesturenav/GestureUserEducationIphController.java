// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.View;
import android.view.Window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Controls showing IPH for Gesture User Education. */
@NullMarked
public class GestureUserEducationIphController {
    public static final int PAGE_HISTORY_MIN_OFFSET = -2;

    private final View mAnchorView;
    private final BackPressManager mBackPressManager;
    private final ScrimManager mScrimManager;
    private @Nullable ActivityTabTabObserver mTabObserver;
    private boolean mIsGestureNavModeForTesting;

    /**
     * Constructor for the controller
     *
     * @param anchorView The {@link View} that the scrim anchors too.
     * @param activityTabProvider The {@link ActivityTabProvider} for this Activity.
     * @param backPressManager The {@link BackPressManager} responsible for handling back presses.
     * @param scrimManager The {@link ScrimManager} responsible for displaying the scrim.
     */
    public GestureUserEducationIphController(
            View anchorView,
            ActivityTabProvider activityTabProvider,
            BackPressManager backPressManager,
            ScrimManager scrimManager) {
        mAnchorView = anchorView;
        mBackPressManager = backPressManager;
        mTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        maybeShowIph(tab);
                    }
                };
        mScrimManager = scrimManager;
    }

    public void unregisterTabObserver() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }
    }

    private void maybeShowIph(Tab tab) {
        if (shouldShowIph(tab)) {
            unregisterTabObserver();
            // TODO(crbug.com/493307156): Display IPH with scrim and animation.
            PropertyModel scrimModel =
                    new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                            .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                            .with(ScrimProperties.AFFECTS_NAVIGATION_BAR, false)
                            .with(ScrimProperties.ANCHOR_VIEW, mAnchorView)
                            .build();
            mScrimManager.showScrim(scrimModel);
        }
    }

    private boolean shouldShowIph(Tab tab) {
        // If gesture nav status has changed return false and destroy tab observer.
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        Window window = windowAndroid == null ? null : windowAndroid.getWindow();
        if (!mIsGestureNavModeForTesting
                && (window == null || !UiUtils.isGestureNavigationMode(window))) {
            unregisterTabObserver();
            return false;
        }

        // Ensure that the tab history has at least two web pages to navigate back to.
        boolean validPageHistory =
                tab.getWebContents() != null
                        && tab.getWebContents()
                                .getNavigationController()
                                .canGoToOffset(PAGE_HISTORY_MIN_OFFSET);

        // Ensures that a back swipe would be handled by the tab history back press handler.
        boolean backPressHandlerConsumingBackEvent =
                mBackPressManager.isBackPressHandlerConsumingBackEvent(
                        BackPressHandler.Type.TAB_HISTORY);
        return validPageHistory
                && backPressHandlerConsumingBackEvent
                && TrackerFactory.getTrackerForProfile(tab.getProfile())
                        .shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION);
    }

    @Nullable ActivityTabTabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    void setIsGestureNavModeForTesting(boolean isGestureNavModeForTesting) {
        mIsGestureNavModeForTesting = isGestureNavModeForTesting;
    }
}
