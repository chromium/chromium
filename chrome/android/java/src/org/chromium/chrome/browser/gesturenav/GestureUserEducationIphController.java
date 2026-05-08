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
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.base.CancelableRunnable;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.UiUtils;
import org.chromium.ui.accessibility.AccessibilityState;
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
    private @Nullable CancelableRunnable mMaybeShowIphCancelableRunnable;
    private @Nullable PropertyModel mScrimPropertyModel;
    private @Nullable ActivityTabTabObserver mTabObserver;
    private @Nullable GestureDetector mDetector;
    private @Nullable View mGestureUserEducationIphLayout;
    private @Nullable LottieAnimationView mBackArrowAnimation;
    private @Nullable ViewPropertyAnimator mTextBubbleAnimation;
    private @Nullable Profile mProfile;
    private boolean mIsIphShowing;
    private boolean mIsGestureNavModeForTesting;
    private @Nullable WebContentsAccessibility mWebContentsAccessibility;

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
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        if (mMaybeShowIphCancelableRunnable != null) {
                            mMaybeShowIphCancelableRunnable.cancel();
                        }

                        if (mIsIphShowing) {
                            hideIph();
                        }
                        super.onPageLoadStarted(tab, url);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        mMaybeShowIphCancelableRunnable =
                                new CancelableRunnable(
                                        () -> {
                                            maybeShowIph(tab);
                                        });
                        PostTask.postDelayedTask(
                                TaskTraits.UI_DEFAULT,
                                mMaybeShowIphCancelableRunnable,
                                ChromeFeatureList.sGestureUserEducationPageDelay.getValue());
                    }

                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mMaybeShowIphCancelableRunnable != null) {
                            mMaybeShowIphCancelableRunnable.cancel();
                        }

                        // Hide IPH if tab is switched.
                        if (mIsIphShowing) {
                            hideIph();
                        }
                        super.onObservingDifferentTab(tab);
                    }
                };
        mScrimManager = scrimManager;
    }

    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }

        if (mMaybeShowIphCancelableRunnable != null) {
            mMaybeShowIphCancelableRunnable.cancel();
            mMaybeShowIphCancelableRunnable = null;
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

            // Handle accessibility
            AccessibilityEvent accessibilityEvent =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
            accessibilityEvent.setContentChangeTypes(
                    AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED);
            AccessibilityState.sendAccessibilityEvent(accessibilityEvent);

            if (mWebContentsAccessibility == null && tab.getWebContents() != null) {
                mWebContentsAccessibility =
                        WebContentsAccessibility.fromWebContents(tab.getWebContents());
            }
            if (mWebContentsAccessibility != null) {
                mWebContentsAccessibility.setObscuredByAnotherView(true);
            }

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

            TextView iphBubbleText =
                    mGestureUserEducationIphLayout.findViewById(R.id.iph_bubble_text);
            iphBubbleText.setText(
                    LocalizationUtils.isLayoutRtl()
                            ? R.string.iph_gesture_user_education_text_bubble_right
                            : R.string.iph_gesture_user_education_text_bubble_left);

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

        AccessibilityEvent accessibilityEvent =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        accessibilityEvent.setContentChangeTypes(
                AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED);
        AccessibilityState.sendAccessibilityEvent(accessibilityEvent);

        if (mWebContentsAccessibility != null) {
            mWebContentsAccessibility.setObscuredByAnotherView(false);
        }

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
        mIsIphShowing = false;
        mDetector = null;
        if (mProfile != null) {
            TrackerFactory.getTrackerForProfile(mProfile)
                    .dismissed(FeatureConstants.GESTURE_USER_EDUCATION);
        }
        destroy();
    }

    private boolean shouldShowIph(Tab tab) {
        // If gesture nav status has changed return false and destroy tab observer.
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        Window window = windowAndroid == null ? null : windowAndroid.getWindow();
        if (!mIsGestureNavModeForTesting
                && (window == null || !UiUtils.isGestureNavigationMode(window))) {
            destroy();
            return false;
        }

        mProfile = tab.getProfile();

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
                && TrackerFactory.getTrackerForProfile(mProfile)
                        .shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION);
    }

    @Nullable ActivityTabTabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    void setIsGestureNavModeForTesting(boolean isGestureNavModeForTesting) {
        mIsGestureNavModeForTesting = isGestureNavModeForTesting;
    }

    void setWebContentsAccessibilityForTesting(WebContentsAccessibility webContentsAccessibility) {
        mWebContentsAccessibility = webContentsAccessibility;
    }
}
