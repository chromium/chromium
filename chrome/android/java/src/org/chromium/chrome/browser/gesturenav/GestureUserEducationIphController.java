// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.Window;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
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
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Controls showing IPH for Gesture User Education. */
@NullMarked
public class GestureUserEducationIphController {
    public static final int PAGE_HISTORY_MIN_OFFSET = -3;
    // TODO(crbug.com/493307156): Confirm animation durations.
    private static final int SLIDE_ANIMATION_DURATION_MS = 750;
    private static final int ANIMATION_DELAY_MS = 750;
    private static final float ANIMATION_X_TRANSLATION = 24;

    private final ViewGroup mAnchorView;
    private final BackPressManager mBackPressManager;
    private final GestureDetector.SimpleOnGestureListener mGestureDetectorListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onDown(MotionEvent e) {
                    if (mIsIphShowing) {
                        hideIph();
                    }
                    return super.onDown(e);
                }
            };
    private final ScrimManager mScrimManager;
    private @Nullable PropertyModel mScrimPropertyModel;
    private @Nullable ActivityTabTabObserver mTabObserver;
    private @Nullable GestureDetector mDetector;
    private @Nullable View mGestureUserEducationIphLayout;
    private @Nullable LottieAnimationView mBackArrowAnimation;
    private @Nullable ViewPropertyAnimator mTextBubbleAnimation;
    private boolean mIsIphShowing;
    private boolean mIsGestureNavModeForTesting;

    /**
     * Constructor for the controller
     *
     * @param anchorView The {@link ViewGroup} that inflates the iph layout.
     * @param activityTabProvider The {@link ActivityTabProvider} for this Activity.
     * @param backPressManager The {@link BackPressManager} responsible for handling back presses.
     * @param scrimManager The {@link ScrimManager} responsible for displaying the scrim.
     */
    public GestureUserEducationIphController(
            ViewGroup anchorView,
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

                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        // Hide IPH if tab is switched.
                        if (mIsIphShowing) {
                            hideIph();
                        }
                        super.onObservingDifferentTab(tab);
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
            mIsIphShowing = true;

            // Inflate layout
            mGestureUserEducationIphLayout =
                    LayoutInflater.from(tab.getContext())
                            .inflate(
                                    R.layout.gesture_user_education_iph_layout, mAnchorView, false);
            mAnchorView.addView(mGestureUserEducationIphLayout);

            // Create Gesture Detector for touch events on the scrim
            mDetector = new GestureDetector(tab.getContext(), mGestureDetectorListener);

            // Display scrim
            mScrimPropertyModel =
                    new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                            .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                            .with(ScrimProperties.AFFECTS_NAVIGATION_BAR, false)
                            .with(ScrimProperties.ANCHOR_VIEW, mGestureUserEducationIphLayout)
                            .with(ScrimProperties.CUSTOM_PARENT, mAnchorView)
                            .with(ScrimProperties.GESTURE_DETECTOR, mDetector)
                            .build();

            mScrimManager.showScrim(mScrimPropertyModel);

            // Adjust animation direction based on RTL.
            float rtlSign = LocalizationUtils.isLayoutRtl() ? -1.0f : 1.0f;

            // Set and display animations
            mBackArrowAnimation =
                    mGestureUserEducationIphLayout.findViewById(R.id.back_gesture_arrow_animation);
            mBackArrowAnimation.setAnimation(R.raw.back_gesture_arrow_animation);
            mBackArrowAnimation.setScaleX(rtlSign);

            View iphBubble = mGestureUserEducationIphLayout.findViewById(R.id.iph_bubble);
            float density = iphBubble.getResources().getDisplayMetrics().density;
            mTextBubbleAnimation =
                    iphBubble
                            .animate()
                            .translationX(rtlSign * ANIMATION_X_TRANSLATION * density)
                            .setInterpolator(Interpolators.STANDARD_INTERPOLATOR)
                            .setDuration(SLIDE_ANIMATION_DURATION_MS)
                            .withEndAction(
                                    () -> {
                                        iphBubble
                                                .animate()
                                                .setStartDelay(ANIMATION_DELAY_MS)
                                                .translationX(0)
                                                .setDuration(SLIDE_ANIMATION_DURATION_MS)
                                                .setInterpolator(
                                                        Interpolators.STANDARD_INTERPOLATOR)
                                                .start();
                                    });
            mTextBubbleAnimation.start();
            mBackArrowAnimation.playAnimation();
        }
    }

    private void hideIph() {
        assert mIsIphShowing;
        if (mScrimPropertyModel != null) {
            mScrimManager.hideScrim(mScrimPropertyModel, false);
        }

        if (mTextBubbleAnimation != null) {
            mTextBubbleAnimation.setListener(null);
            mTextBubbleAnimation.cancel();
        }

        if (mBackArrowAnimation != null) {
            mBackArrowAnimation.cancelAnimation();
        }

        mAnchorView.removeView(mGestureUserEducationIphLayout);
        unregisterTabObserver();
        mIsIphShowing = false;
        mDetector = null;
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
