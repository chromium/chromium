// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.chrome.browser.modules.xr.R;

/** Custom parent ViewGroup for the format selection panel that inflates its layout. */
@NullMarked
public class ImmersiveVideoFormatView extends ImmersiveVideoHoverLayout {

    public ImmersiveVideoFormatView(Context context) {
        this(context, null);
    }

    public ImmersiveVideoFormatView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ImmersiveVideoFormatView(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        LayoutInflater.from(context).inflate(R.layout.immersive_video_format_view, this, true);
        setAccessibilityPaneTitle(
                context.getString(R.string.immersive_playback_confirmation_format_title));
    }

    /** Exposes the internal radio button group for format selections. */
    public ImmersiveVideoFormatRadioGroup getRadioGroup() {
        return findViewById(R.id.format_radio_group);
    }

    /** Requests accessibility focus on the format selection options. */
    public void requestFocusForAccessibility() {
        ImmersiveVideoFormatRadioGroup radioGroup = getRadioGroup();
        if (radioGroup != null) {
            radioGroup.requestFocusForAccessibility();
        }
    }
}
