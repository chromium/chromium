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
    String getTitle();

    @Nullable CharSequence getSummary();

    @Nullable String getPrimaryButtonText();

    View.@Nullable OnClickListener getPrimaryButtonListener();

    @Nullable String getSecondaryButtonText();

    View.@Nullable OnClickListener getSecondaryButtonListener();

    @ModuleState
    int getModuleState();
}
