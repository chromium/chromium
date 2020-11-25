// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting;
import org.chromium.chrome.browser.autofill_assistant.drawable.AssistantDrawableIcon;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantDrawable;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.ui.widget.ChromeImageView;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Handles construction and state changes on a progress bar with steps.
 */
public class AssistantStepProgressBar {
    private static final int ANIMATION_DELAY_MS = 1_000;
    private static final int ICON_ENABLED_ANIMATION_DURATION_MS = 300;
    private static final int LINE_ANIMATION_DURATION_MS = 1_000;
    private static final int PULSING_DURATION_MS = 1_000;
    // This controls the delay between the scaling of the pulsating and the alpha change. I.e. the
    // animation starts scaling and after a while the alpha changes.
    private static final int PULSING_ALPHA_CHANGE_DELAY_MS = 300;
    private static final int PULSING_RESTART_DELAY_MS = 2_000;
    private static final int COLOR_LIST = R.color.blue_when_enabled;
    private static final int ERROR_COLOR_LIST = R.color.default_red;

    private static class IconViewHolder {

        private final RelativeLayout mView;
        private final Context mContext;
        private final View mPulsor;
        private final ChromeImageView mIcon;
        private final ValueAnimator mPulseAnimation;

        private boolean mShouldRunAnimation;
        private boolean mDisableAnimations;
        private boolean mFirstAnimation;

        IconViewHolder(ViewGroup view, Context context) {
            mContext = context;
            mView = addContainer(view, mContext);
            mPulsor = addPulsor(mView, mContext);
            mIcon = addIcon(mView, mContext);
            mPulseAnimation = createPulseAnimation();
        }

        void setTag(String tag) {
            mView.setTag(tag);
        }

        void disableAnimations(boolean disable) {
            mDisableAnimations = disable;
        }

        private RelativeLayout addContainer(ViewGroup view, Context context) {
            RelativeLayout container = new RelativeLayout(context);
            view.addView(container);

            int size = context.getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_progress_icon_background_size);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(size, size);
            container.setLayoutParams(params);

            return container;
        }

        private View addPulsor(RelativeLayout view, Context context) {
            View pulsor = new View(context);
            view.addView(pulsor);

            RelativeLayout.LayoutParams params =
                    new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT,
                            RelativeLayout.LayoutParams.MATCH_PARENT);
            params.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
            pulsor.setLayoutParams(params);

            pulsor.setBackground(ApiCompatibilityUtils.getDrawable(
                    context.getResources(), R.drawable.autofill_assistant_circle_background));

            pulsor.setVisibility(View.GONE);

            return pulsor;
        }

        private ChromeImageView addIcon(RelativeLayout view, Context context) {
            ChromeImageView icon = new ChromeImageView(context);
            view.addView(icon);

            int size = context.getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_progress_icon_size);
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(size, size);
            params.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
            icon.setLayoutParams(params);

            ApiCompatibilityUtils.setImageTintList(
                    icon, ContextCompat.getColorStateList(context, COLOR_LIST));
            icon.setEnabled(false);

            return icon;
        }

        private ValueAnimator createPulseAnimation() {
            ValueAnimator pulseAnimation = ValueAnimator.ofFloat(0f, PULSING_DURATION_MS);
            pulseAnimation.setDuration(PULSING_DURATION_MS);
            pulseAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            pulseAnimation.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationStart(Animator animator) {
                    if (mShouldRunAnimation) {
                        mPulsor.setScaleX(0f);
                        mPulsor.setScaleY(0f);
                        mPulsor.setAlpha(1f);
                        mPulsor.setVisibility(View.VISIBLE);
                    }
                }

                @Override
                public void onAnimationEnd(Animator animator) {
                    mPulsor.setVisibility(View.GONE);
                    mFirstAnimation = false;
                    if (mShouldRunAnimation) {
                        pulseAnimation.setStartDelay(PULSING_RESTART_DELAY_MS);
                        pulseAnimation.start();
                    }
                }
            });
            pulseAnimation.addUpdateListener(animation -> {
                float time = (float) animation.getAnimatedValue();
                float scale = time / PULSING_DURATION_MS;
                mPulsor.setScaleX(scale);
                mPulsor.setScaleY(scale);
                float alpha = time < PULSING_ALPHA_CHANGE_DELAY_MS ? 1
                                                                   : 1
                                - ((time - PULSING_ALPHA_CHANGE_DELAY_MS)
                                        / (float) (PULSING_DURATION_MS
                                                - PULSING_ALPHA_CHANGE_DELAY_MS));
                mPulsor.setAlpha(alpha);
            });

            return pulseAnimation;
        }

        void setIcon(AssistantDrawable drawable) {
            drawable.getDrawable(mContext, mIcon::setImageDrawable);
        }

        void setEnabled(boolean enabled) {
            mIcon.setEnabled(enabled);
        }

        void startEnabledAnimation() {
            if (mDisableAnimations) {
                setEnabled(true);
                return;
            }

            ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setStartDelay(ANIMATION_DELAY_MS);
            animator.setDuration(ICON_ENABLED_ANIMATION_DURATION_MS);
            animator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationStart(Animator animator) {
                    mIcon.setScaleX(0f);
                    mIcon.setScaleY(0f);
                    mIcon.setEnabled(true);
                }

                @Override
                public void onAnimationEnd(Animator animator) {
                    mIcon.setScaleX(1f);
                    mIcon.setScaleY(1f);
                }
            });
            animator.addUpdateListener(animation -> {
                float animatedValue = (float) animation.getAnimatedValue();
                mIcon.setScaleX(animatedValue);
                mIcon.setScaleY(animatedValue);
            });
            animator.start();
        }

        void setPulsingAnimationEnabled(boolean pulsate) {
            if (pulsate) {
                startPulsingAnimation(/* delayed= */ false);
            } else {
                stopPulsingAnimation();
            }
        }

        void startPulsingAnimation(boolean delayed) {
            if (mDisableAnimations) {
                return;
            }

            mFirstAnimation = true;
            mShouldRunAnimation = true;
            mPulseAnimation.setStartDelay(delayed
                            ? ANIMATION_DELAY_MS + ICON_ENABLED_ANIMATION_DURATION_MS
                                    + LINE_ANIMATION_DURATION_MS
                            : 0);
            mPulseAnimation.start();
        }

        private void stopPulsingAnimation() {
            mShouldRunAnimation = false;
        }

        void setError(boolean error) {
            if (mIcon.isEnabled() || !error) {
                ApiCompatibilityUtils.setImageTintList(mIcon,
                        ContextCompat.getColorStateList(
                                mContext, error ? ERROR_COLOR_LIST : COLOR_LIST));
            } else if ((!mPulseAnimation.isStarted() || mFirstAnimation) && !mDisableAnimations) {
                // This is in case when the blue line is animating towards the icon that we need to
                // set in error state. We want to wait for the line to reach the icon before turning
                // the icon red.
                mPulseAnimation.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        mPulseAnimation.removeListener(this);
                        ApiCompatibilityUtils.setImageTintList(
                                mIcon, ContextCompat.getColorStateList(mContext, ERROR_COLOR_LIST));
                    }
                });
            } else {
                mPulseAnimation.cancel();
                ApiCompatibilityUtils.setImageTintList(
                        mIcon, ContextCompat.getColorStateList(mContext, ERROR_COLOR_LIST));
            }
        }
    }

    private static class LineViewHolder {
        private final Context mContext;
        private final LinearLayout mView;
        private final View mLineForeground;

        private boolean mDisableAnimations;

        LineViewHolder(ViewGroup view, Context context) {
            mContext = context;
            mView = addMainContainer(view, context);
            RelativeLayout relativeContainer = addRelativeContainer(mView, context);
            addBackgroundLine(relativeContainer, context);
            mLineForeground = addForegroundLine(relativeContainer, context);
        }

        void setTag(String tag) {
            mView.setTag(tag);
        }

        void disableAnimations(boolean disable) {
            mDisableAnimations = disable;
        }

        private LinearLayout addMainContainer(ViewGroup view, Context context) {
            LinearLayout container = new LinearLayout(context);
            view.addView(container);

            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                    context.getResources().getDimensionPixelSize(
                            R.dimen.autofill_assistant_progress_line_height));
            params.weight = 1.0f;
            container.setLayoutParams(params);
            int padding = context.getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_progress_padding);
            container.setPadding(padding, 0, padding, 0);

            return container;
        }

        private RelativeLayout addRelativeContainer(ViewGroup view, Context context) {
            RelativeLayout container = new RelativeLayout(context);
            view.addView(container);

            return container;
        }

        private void addBackgroundLine(ViewGroup view, Context context) {
            View line = new View(context);
            view.addView(line);

            line.setBackground(ApiCompatibilityUtils.getDrawable(context.getResources(),
                    R.drawable.autofill_assistant_rounded_corner_background));
            line.setEnabled(false);
        }

        private View addForegroundLine(ViewGroup view, Context context) {
            View line = new View(context);
            view.addView(line);

            line.setBackground(ApiCompatibilityUtils.getDrawable(context.getResources(),
                    R.drawable.autofill_assistant_rounded_corner_background));

            line.setTag(AssistantTagsForTesting.PROGRESSBAR_LINE_FOREGROUND_TAG);
            line.setEnabled(true);

            line.setScaleX(0f);

            return line;
        }

        void setEnabled(boolean enabled) {
            mLineForeground.setScaleX(enabled ? 1f : 0f);
        }

        void startAnimation() {
            if (mDisableAnimations) {
                setEnabled(true);
                return;
            }

            ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setStartDelay(ANIMATION_DELAY_MS + ICON_ENABLED_ANIMATION_DURATION_MS);
            animator.setDuration(LINE_ANIMATION_DURATION_MS);
            animator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationStart(Animator animator) {
                    mLineForeground.setScaleX(0f);
                    mLineForeground.setPivotX(0f);
                }

                @Override
                public void onAnimationEnd(Animator animator) {
                    mLineForeground.setScaleX(1f);
                    mLineForeground.setPivotX(0f);
                }
            });
            animator.addUpdateListener(animation -> {
                float animatedValue = (float) animation.getAnimatedValue();
                mLineForeground.setScaleX(animatedValue);
                mLineForeground.setPivotX(-mLineForeground.getWidth());
            });
            animator.start();
        }

        void setError(boolean error) {
            int colorId = error ? R.color.default_red : R.color.default_icon_color_blue;
            mLineForeground.setBackgroundColor(ContextCompat.getColor(mContext, colorId));
        }
    }

    private final ViewGroup mView;
    private int mNumberOfSteps;
    private IconViewHolder[] mIcons;
    private LineViewHolder[] mLines;

    private int mCurrentStep = -1;

    AssistantStepProgressBar(ViewGroup view) {
        mView = view;
        setSteps(new ArrayList<AssistantDrawable>() {
            {
                add(AssistantDrawable.createFromIcon(
                        AssistantDrawableIcon.PROGRESSBAR_DEFAULT_INITIAL_STEP));
                add(AssistantDrawable.createFromIcon(
                        AssistantDrawableIcon.PROGRESSBAR_DEFAULT_FINAL_STEP));
            }
        });
    }

    public void setSteps(List<AssistantDrawable> icons) {
        assert icons.size() >= 2;
        // NOTE: instead of immediately removing the old icons, we first add the new ones. This
        // prevents a change of the bottom sheet height, which was the cause for animation glitches
        // (see b/174014637).
        int oldNumChilds = mView.getChildCount();
        mNumberOfSteps = icons.size();
        mCurrentStep = -1;

        mIcons = new IconViewHolder[mNumberOfSteps];
        mLines = new LineViewHolder[mNumberOfSteps - 1];
        for (int i = 0; i < mNumberOfSteps; ++i) {
            mIcons[i] = new IconViewHolder(mView, mView.getContext());
            mIcons[i].setIcon(icons.get(i));
            mIcons[i].setTag(String.format(
                    Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i));
            if (i < mNumberOfSteps - 1) {
                mLines[i] = new LineViewHolder(mView, mView.getContext());
                mLines[i].setTag(String.format(
                        Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i));
            }
        }

        mView.removeViews(0, oldNumChilds);
    }

    public void setVisible(boolean visible) {
        mView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    public void disableAnimations(boolean disable) {
        for (IconViewHolder icon : mIcons) {
            icon.disableAnimations(disable);
        }
        for (LineViewHolder line : mLines) {
            line.disableAnimations(disable);
        }
    }

    public void setActiveStep(int step) {
        assert step >= 0 && step <= mNumberOfSteps;
        assert step >= mCurrentStep;
        if (step == mCurrentStep) {
            return;
        }

        for (int i = 0; i < mNumberOfSteps; ++i) {
            if (i == mCurrentStep && step == mCurrentStep + 1) {
                mIcons[i].startEnabledAnimation();
            } else {
                mIcons[i].setEnabled(i < step);
            }

            if (i == step && step == mCurrentStep + 1 && mCurrentStep != -1) {
                // In case we advance to a new step, start the enable animation on the current
                // icon. If not for the first step, start the pulsating animation with a delay such
                // that it only starts after the other animations have run.
                mIcons[i].startPulsingAnimation(/* delayed= */ true);
            } else {
                mIcons[i].setPulsingAnimationEnabled(i == step);
            }

            if (i < mNumberOfSteps - 1) {
                if (i == step - 1 && step == mCurrentStep + 1) {
                    mLines[i].startAnimation();
                } else {
                    mLines[i].setEnabled(i < step);
                }
            }
        }
        mCurrentStep = step;
    }

    public void setError(boolean error) {
        boolean errorAfterCompletion = error && mCurrentStep >= mNumberOfSteps;
        for (int i = 0; i < mNumberOfSteps; ++i) {
            mIcons[i].setError(errorAfterCompletion || (error && i == mCurrentStep));
            mIcons[i].setPulsingAnimationEnabled(!error && i == mCurrentStep);
        }
        for (LineViewHolder line : mLines) {
            line.setError(errorAfterCompletion);
        }
    }
}
