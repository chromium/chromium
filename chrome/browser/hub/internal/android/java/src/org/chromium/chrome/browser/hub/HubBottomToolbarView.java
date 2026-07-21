// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;

/** Basic view that represents the bottom toolbar in the Hub. */
@NullMarked
public class HubBottomToolbarView extends LinearLayout {
    private final HubColorMixerRegistrationHelper mColorMixerHelper =
            new HubColorMixerRegistrationHelper();

    /** Default {@link LinearLayout} constructor called by inflation. */
    public HubBottomToolbarView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        Context context = getContext();
        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getHubBottomToolbarColor(context, colorScheme),
                        this::setBackgroundColor));
    }

    void setColorMixer(HubColorMixer mixer) {
        mColorMixerHelper.setColorMixer(mixer);
    }
}
