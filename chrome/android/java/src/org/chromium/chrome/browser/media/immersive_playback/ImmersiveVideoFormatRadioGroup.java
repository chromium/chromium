// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;

/** A custom RadioGroup that presents options for immersive playback formats. */
@NullMarked
public class ImmersiveVideoFormatRadioGroup extends RadioGroup {

    public static class FormatOption {
        public final int stringResId;
        public final int stereoMode;
        public final int projectionType;

        public FormatOption(int stringResId, int stereoMode, int projectionType) {
            this.stringResId = stringResId;
            this.stereoMode = stereoMode;
            this.projectionType = projectionType;
        }
    }

    public static final FormatOption[] SUPPORTED_FORMATS = {
        new FormatOption(
                R.string.immersive_playback_confirmation_option_standard,
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD),
        new FormatOption(
                R.string.immersive_playback_confirmation_option_stereo_3d,
                ImmersiveStereoMode.SIDE_BY_SIDE,
                ImmersiveProjectionType.QUAD),
        new FormatOption(
                R.string.immersive_playback_confirmation_option_vr180,
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.HEMISPHERE),
        new FormatOption(
                R.string.immersive_playback_confirmation_option_vr360,
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.SPHERE)
    };

    public ImmersiveVideoFormatRadioGroup(Context context) {
        super(context);
        init();
    }

    public ImmersiveVideoFormatRadioGroup(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setOrientation(VERTICAL);

        for (FormatOption option : SUPPORTED_FORMATS) {
            addOption(option);
        }

        // Check the first one by default after all are added to ensure correct RadioGroup behavior.
        if (getChildCount() > 0) {
            check(getChildAt(0).getId());
        }
    }

    private void addOption(FormatOption option) {
        Context context = getContext();
        RadioButton radioButton = new RadioButton(context);
        radioButton.setId(View.generateViewId());
        radioButton.setText(context.getString(option.stringResId));
        radioButton.setTag(option);
        radioButton.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
        RadioGroup.LayoutParams layoutParams =
                new RadioGroup.LayoutParams(
                        RadioGroup.LayoutParams.MATCH_PARENT, RadioGroup.LayoutParams.WRAP_CONTENT);
        addView(radioButton, layoutParams);
    }

    public FormatOption getSelectedFormat() {
        for (int i = 0; i < getChildCount(); i++) {
            RadioButton rb = (RadioButton) getChildAt(i);
            if (rb.isChecked()) {
                return (FormatOption) rb.getTag();
            }
        }
        return SUPPORTED_FORMATS[0];
    }
}
