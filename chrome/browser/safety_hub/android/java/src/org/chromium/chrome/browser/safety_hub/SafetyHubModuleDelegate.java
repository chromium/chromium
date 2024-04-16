// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.chrome.browser.profiles.Profile;

/** A delegate for Safety Hub to handle UI related behaviour. */
public interface SafetyHubModuleDelegate {

    boolean shouldShowPasswordCheckModule(Profile profile);
}
