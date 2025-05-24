// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** A class for showing UI whenever the Android-OS-supplied advanced-protection state changes. */
@NullMarked
public class AdvancedProtectionCoordinator {
    private final AdvancedProtectionMediator mMediator;

    public AdvancedProtectionCoordinator(
            WindowAndroid windowAndroid, Class<? extends Fragment> privacySettingsFragmentClass) {
        mMediator = new AdvancedProtectionMediator(windowAndroid, privacySettingsFragmentClass);
    }

    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Shows message-UI informing the user about the Android-OS requested advanced-protection state
     * if the advanced-protection state has changed since Chrome was last open.
     *
     * @return whether message-UI was shown.
     */
    public boolean showMessageOnStartupIfNeeded() {
        return mMediator.showMessageOnStartupIfNeeded();
    }
}
