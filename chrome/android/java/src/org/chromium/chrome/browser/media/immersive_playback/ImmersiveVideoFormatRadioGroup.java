// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import androidx.annotation.StringRes;

import com.google.common.collect.ImmutableMap;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** A custom view container that presents options for immersive playback formats. */
@NullMarked
public class ImmersiveVideoFormatRadioGroup extends FrameLayout {

    /** Represents a selectable format option. */
    public static class FormatOption {
        /** The stereo mode of this format. */
        public final @ImmersiveStereoMode int stereoMode;

        /** The projection type of this format. */
        public final @ImmersiveProjectionType int projectionType;

        /**
         * Creates a new {@link FormatOption}.
         *
         * @param stereoMode The stereo mode.
         * @param projectionType The projection type.
         */
        public FormatOption(
                @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
            this.stereoMode = stereoMode;
            this.projectionType = projectionType;
        }
    }

    private static final ImmutableMap<Integer, FormatOption> FORMAT_OPTIONS =
            ImmutableMap.of(
                    R.id.standard_option,
                    new FormatOption(ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD),
                    R.id.stereoscopic_option,
                    new FormatOption(
                            ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.QUAD),
                    R.id.hemisphere_option,
                    new FormatOption(
                            ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.HEMISPHERE),
                    R.id.sphere_option,
                    new FormatOption(ImmersiveStereoMode.MONO, ImmersiveProjectionType.SPHERE));

    private final List<RadioButtonWithDescription> mRadioButtons = new ArrayList<>();
    private final RadioButtonWithDescriptionLayout mLayout;
    private final RadioButtonWithDescription mRecommendedButton;
    private @Nullable RadioButtonWithDescription mFocusTarget;
    private @Nullable Runnable mFocusRunnable;

    public ImmersiveVideoFormatRadioGroup(Context context) {
        this(context, null);
    }

    public ImmersiveVideoFormatRadioGroup(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context)
                .inflate(R.layout.immersive_video_format_radio_group_layout, this, true);
        mLayout = findViewById(R.id.format_radio_group_layout);

        mRecommendedButton = mLayout.findViewById(R.id.recommended_option);
        mRadioButtons.add(mRecommendedButton);

        for (Map.Entry<Integer, FormatOption> entry : FORMAT_OPTIONS.entrySet()) {
            RadioButtonWithDescription button = mLayout.findViewById(entry.getKey());
            button.setTag(entry.getValue());
            mRadioButtons.add(button);
        }
    }

    public void setSelectionCallback(Callback<@Nullable FormatOption> callback) {
        mLayout.setOnCheckedChangeListener(
                (group, checkedId) -> callback.onResult(getSelectedOption()));
    }

    /**
     * Sets the recommended projection option and makes it available in the list.
     *
     * @param stereoMode The video's recommended stereo mode.
     * @param projectionType The video's recommended projection type.
     */
    public void setRecommendedOption(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        mRecommendedButton.setTag(new FormatOption(stereoMode, projectionType));
        mRecommendedButton.setDescriptionText(getDescriptionText(stereoMode, projectionType));
        mRecommendedButton.setVisibility(View.VISIBLE);
    }

    /**
     * Programmatically checks the radio button that matches the specified format options.
     *
     * @param stereoMode The {@link ImmersiveStereoMode} to match.
     * @param projectionType The {@link ImmersiveProjectionType} to match.
     */
    public void checkOption(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        for (RadioButtonWithDescription rb : mRadioButtons) {
            FormatOption option = (FormatOption) rb.getTag();
            if (option != null
                    && option.stereoMode == stereoMode
                    && option.projectionType == projectionType) {
                rb.setChecked(true);
                break;
            }
        }
    }

    /** Returns the selected format option. */
    public @Nullable FormatOption getSelectedOption() {
        RadioButtonWithDescription checked = getSelectedButton();
        return checked != null ? (FormatOption) checked.getTag() : null;
    }

    private @Nullable RadioButtonWithDescription getSelectedButton() {
        for (RadioButtonWithDescription rb : mRadioButtons) {
            if (rb.isChecked()) {
                return rb;
            }
        }
        return null;
    }

    /** Requests accessibility focus on the selected radio button option. */
    public void requestFocusForAccessibility() {
        cancelPendingFocusRequests();
        final RadioButtonWithDescription target = findTargetForAccessibility();
        if (target != null) {
            mFocusTarget = target;
            mFocusRunnable = this::requestFocusForAccessibilityInternal;
            target.postDelayed(mFocusRunnable, 150);
        }
    }

    @SuppressLint("AccessibilityFocus")
    private void requestFocusForAccessibilityInternal() {
        if (mFocusTarget != null && mFocusTarget.isAttachedToWindow() && mFocusTarget.isShown()) {
            mFocusTarget.requestFocus();
            mFocusTarget.performAccessibilityAction(
                    AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
        }
        mFocusRunnable = null;
        mFocusTarget = null;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (getVisibility() == View.VISIBLE) {
            requestFocusForAccessibility();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        cancelPendingFocusRequests();
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (visibility != VISIBLE) {
            cancelPendingFocusRequests();
        }
    }

    private void cancelPendingFocusRequests() {
        if (mFocusTarget != null && mFocusRunnable != null) {
            mFocusTarget.removeCallbacks(mFocusRunnable);
        }
        mFocusRunnable = null;
        mFocusTarget = null;
    }

    private @Nullable RadioButtonWithDescription findTargetForAccessibility() {
        RadioButtonWithDescription checked = getSelectedButton();
        if (checked != null && checked.getVisibility() == View.VISIBLE) {
            return checked;
        }
        for (RadioButtonWithDescription rb : mRadioButtons) {
            if (rb.getVisibility() == View.VISIBLE) {
                return rb;
            }
        }
        return null;
    }

    private String getDescriptionText(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        @StringRes int resId = 0;
        switch (projectionType) {
            case ImmersiveProjectionType.QUAD:
                resId =
                        stereoMode == ImmersiveStereoMode.MONO
                                ? R.string.immersive_playback_format_standard
                                : R.string.immersive_playback_format_stereoscopic;
                break;
            case ImmersiveProjectionType.SPHERE:
                resId =
                        stereoMode == ImmersiveStereoMode.MONO
                                ? R.string.immersive_playback_format_360
                                : R.string.immersive_playback_format_360_stereoscopic;
                break;
            case ImmersiveProjectionType.HEMISPHERE:
                resId =
                        stereoMode == ImmersiveStereoMode.MONO
                                ? R.string.immersive_playback_format_180
                                : R.string.immersive_playback_format_180_stereoscopic;
                break;
            default:
                break;
        }
        return resId != 0 ? getContext().getString(resId) : "";
    }
}
