// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;

/**
 * An empty implementation of {@link OverviewModeObserver}.
 * DEPRECATED, please use {@link
 * org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver} instead.
 */
@Deprecated
public class EmptyOverviewModeObserver implements OverviewModeObserver {
    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {}

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {}

    @Override
    public void onOverviewModeFinishedHiding() {}
}
