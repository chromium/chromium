// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import androidx.annotation.Nullable;

/**
 * Provides helper methods that characterize a Homepage so they can be accessed from modules such as
 * {@link PartnerBrowserCustomizations}. TODO(crbug.com/40273149) move this to the Homepage Manager
 * when Homepage is modularized.
 */
public interface HomepageCharacterizationHelper {
    /**
     * @return whether the given URL is the NTP. An input of (@code null} returns {@code false}.
     */
    boolean isUrlNtp(@Nullable String url);

    /** @return whether the current Homepage is the Partner customized Homepage or NTP. */
    boolean isPartner();

    /** @return whether the current Homepage is any kind of NTP. */
    boolean isNtp();
}
