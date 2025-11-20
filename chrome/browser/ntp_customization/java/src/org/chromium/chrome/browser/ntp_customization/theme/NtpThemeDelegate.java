// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import org.chromium.build.annotations.NullMarked;

/** Delegate for {@link NtpThemeMediator} to communicate with {@link NtpThemeCoordinator}. */
@NullMarked
public interface NtpThemeDelegate {
    /** Called when the chrome colors section is clicked. */
    void onChromeColorsClicked();

    /** Called when the theme collections section is clicked. */
    void onThemeCollectionsClicked();
}
