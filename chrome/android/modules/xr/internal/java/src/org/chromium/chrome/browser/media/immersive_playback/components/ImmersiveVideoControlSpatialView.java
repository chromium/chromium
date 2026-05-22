// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;

/**
 * Wrapper holding the {@link ImmersiveVideoControlView} and its corresponding XR panel entity
 * holder.
 */
@NullMarked
public class ImmersiveVideoControlSpatialView {
    public final ImmersiveVideoControlView androidView;
    public final XrPanelEntityHolder<?> spatialEntityHolder;

    /**
     * Creates a new {@link ImmersiveVideoControlSpatialView}.
     *
     * @param androidView The {@link ImmersiveVideoControlView}.
     * @param spatialEntityHolder The {@link XrPanelEntityHolder}.
     */
    public ImmersiveVideoControlSpatialView(
            ImmersiveVideoControlView androidView, XrPanelEntityHolder<?> spatialEntityHolder) {
        this.androidView = androidView;
        this.spatialEntityHolder = spatialEntityHolder;
    }
}
