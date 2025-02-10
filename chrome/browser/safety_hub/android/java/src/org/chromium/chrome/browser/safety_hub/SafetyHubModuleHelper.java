// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.view.View;

import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Interface for a helper of the {@link SafetyHubModuleMediator}. */
interface SafetyHubModuleHelper {
    public String getTitle();

    public String getSummary();

    public String getPrimaryButtonText();

    public View.OnClickListener getPrimaryButtonListener();

    public String getSecondaryButtonText();

    public View.OnClickListener getSecondaryButtonListener();

    public @ModuleState int getModuleState();
}
