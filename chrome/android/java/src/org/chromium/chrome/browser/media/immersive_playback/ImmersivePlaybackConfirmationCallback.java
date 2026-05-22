// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import org.chromium.blink.mojom.ImmersivePlaybackConfirmationStatus;
import org.chromium.blink.mojom.ImmersiveProjectionType;
import org.chromium.blink.mojom.ImmersiveStereoMode;
import org.chromium.build.annotations.NullMarked;

/** Callback for immersive playback confirmation flow. */
@NullMarked
public interface ImmersivePlaybackConfirmationCallback {
    void onResult(
            @ImmersivePlaybackConfirmationStatus.EnumType int status,
            @ImmersiveStereoMode.EnumType int stereoMode,
            @ImmersiveProjectionType.EnumType int projectionType);
}
