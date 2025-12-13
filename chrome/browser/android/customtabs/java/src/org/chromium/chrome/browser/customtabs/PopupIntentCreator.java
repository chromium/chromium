// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.util.WindowFeatures;

/** Interface for creating Intents to launch CCT popups. */
@NullMarked
public interface PopupIntentCreator {
    /**
     * Creates an Intent for a Custom Tab popup.
     *
     * @param windowFeatures The requested window features.
     * @param isIncognito Whether the popup is for an incognito tab.
     * @return The intent to launch the popup.
     */
    Intent createPopupIntent(@Nullable WindowFeatures windowFeatures, boolean isIncognito);
}
