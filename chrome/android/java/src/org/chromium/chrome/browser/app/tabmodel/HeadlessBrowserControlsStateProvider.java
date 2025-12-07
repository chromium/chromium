// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;

/** A fake/stub implementation since there are no browser controls for headless mode. */
@NullMarked
public class HeadlessBrowserControlsStateProvider implements BrowserControlsStateProvider {
    @Override
    public void addObserver(Observer obs) {}

    @Override
    public void removeObserver(Observer obs) {}

    @Override
    public int getTopControlsHeight() {
        return 0;
    }

    @Override
    public int getTopControlsHairlineHeight() {
        return 0;
    }

    @Override
    public int getTopControlsMinHeight() {
        return 0;
    }

    @Override
    public int getTopControlOffset() {
        return 0;
    }

    @Override
    public int getTopControlsMinHeightOffset() {
        return 0;
    }

    @Override
    public int getBottomControlsHeight() {
        return 0;
    }

    @Override
    public int getBottomControlsMinHeight() {
        return 0;
    }

    @Override
    public int getBottomControlsMinHeightOffset() {
        return 0;
    }

    @Override
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return false;
    }

    @Override
    public int getBottomControlOffset() {
        return 0;
    }

    @Override
    public float getBrowserControlHiddenRatio() {
        return 0;
    }

    @Override
    public int getContentOffset() {
        return 0;
    }

    @Override
    public float getTopVisibleContentOffset() {
        return 0;
    }

    @Override
    public int getAndroidControlsVisibility() {
        return 0;
    }

    @Override
    public @ControlsPosition int getControlsPosition() {
        return ControlsPosition.NONE;
    }

    @Override
    public boolean isVisibilityForced() {
        return false;
    }
}
