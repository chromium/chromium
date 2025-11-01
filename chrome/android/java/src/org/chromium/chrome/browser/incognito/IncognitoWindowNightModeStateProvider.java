// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import androidx.appcompat.app.AppCompatDelegate;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;

/** Provides night mode state for incognito windows. */
@NullMarked
public class IncognitoWindowNightModeStateProvider implements NightModeStateProvider {

    /**
     * Initializes the initial night mode state.
     *
     * @param delegate The {@link AppCompatDelegate} that controls night mode state in support
     *     library.
     */
    public void initialize(AppCompatDelegate delegate) {
        delegate.setLocalNightMode(AppCompatDelegate.MODE_NIGHT_YES);
    }

    // Incognito windows are default in night mode.
    @Override
    public boolean isInNightMode() {
        return true;
    }

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}
}
