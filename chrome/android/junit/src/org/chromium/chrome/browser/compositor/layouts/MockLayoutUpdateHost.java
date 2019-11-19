// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;

/** A mock implementation of {@link LayoutUpdateHost}. */
public class MockLayoutUpdateHost implements LayoutUpdateHost {
    @Override
    public void requestUpdate() {}

    @Override
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {}

    @Override
    public void doneHiding() {}

    @Override
    public void doneShowing() {}

    @Override
    public boolean isActiveLayout(Layout layout) {
        return true;
    }

    @Override
    public void initLayoutTabFromHost(final int tabId) {}

    @Override
    public LayoutTab createLayoutTab(int id, boolean incognito, boolean showCloseButton,
            boolean isTitleNeeded, float maxContentWidth, float maxContentHeight) {
        return null;
    }

    @Override
    public void releaseTabLayout(int id) {}

    @Override
    public void releaseResourcesForTab(int tabId) {}

    @Override
    public CompositorAnimationHandler getAnimationHandler() {
        return null;
    }
}
