// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.chrome.browser.profiles.Profile;

/** Specifies that this settings entry is dependent on the current profile. */
public interface ProfileDependentSetting {
    /**
     * @param profile The currently selected profile.
     */
    void setProfile(Profile profile);
}
