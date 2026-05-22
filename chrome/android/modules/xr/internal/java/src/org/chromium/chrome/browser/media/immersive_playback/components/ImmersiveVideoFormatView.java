// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
        LayoutInflater.from(context).inflate(R.layout.immersive_video_format_view, this, true);
    }
}
