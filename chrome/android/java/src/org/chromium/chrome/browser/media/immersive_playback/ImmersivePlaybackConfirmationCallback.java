// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;

/** Callback for immersive playback confirmation flow. */
@NullMarked
public interface ImmersivePlaybackConfirmationCallback {
    void onResult(
            @ImmersivePlaybackConfirmationStatus int status,
            @ImmersiveStereoMode int stereoMode,
            @ImmersiveProjectionType int projectionType);
}
