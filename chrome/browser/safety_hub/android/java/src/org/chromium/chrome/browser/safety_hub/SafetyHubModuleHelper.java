// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

/** Interface for a helper of the {@link SafetyHubModuleMediator}. */
@NullMarked
interface SafetyHubModuleHelper {
    public String getTitle();

    public @Nullable String getSummary();

    public @Nullable String getPrimaryButtonText();

    public View.@Nullable OnClickListener getPrimaryButtonListener();

    public @Nullable String getSecondaryButtonText();

    public View.@Nullable OnClickListener getSecondaryButtonListener();

    public @ModuleState int getModuleState();
}
