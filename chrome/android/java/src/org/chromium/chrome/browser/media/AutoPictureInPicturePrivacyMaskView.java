// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/**
 * A view that acts as a privacy mask for the auto picture-in-picture permission prompt. It obscures
 * the underlying WebContents and blocks interactions.
 */
@NullMarked
public class AutoPictureInPicturePrivacyMaskView extends FrameLayout {
    // Matches fade-in duration used on desktop Chrome.
    private static final int FADE_IN_DURATION_MS = 500;

    public AutoPictureInPicturePrivacyMaskView(
            @NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setBackgroundColor(ContextCompat.getColor(context, R.color.modal_dialog_scrim_color));
        // Consume touch events to block interaction with the underlying content.
        setClickable(true);

        // Trap focus to prevent users from navigating to the underlying content via keyboard or
        // accessibility tools (e.g. TalkBack).
        // TODO(crbug.com/459582604): The controller(pending) should also set
        // IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS on the underlying WebContents container
        // to prevent linear navigation (swiping) from reaching the obscured content.
        setFocusable(true);
        setFocusableInTouchMode(true);

        setAlpha(0.0f);
    }

    /** Fades the view in. */
    public void show() {
        animate().alpha(1.0f).setDuration(FADE_IN_DURATION_MS).start();
    }

    /** Removes the view from its parent immediately. */
    public void hide() {
        if (getParent() != null) {
            ((ViewGroup) getParent()).removeView(this);
        }
    }
}
