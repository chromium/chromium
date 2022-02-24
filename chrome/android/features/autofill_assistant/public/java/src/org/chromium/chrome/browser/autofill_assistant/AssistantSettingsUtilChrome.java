// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.autofill_assistant.AssistantSettingsUtil;

/**
 * Implementation of {@link AssistantSettingsUtil} for Chrome.
 */
public class AssistantSettingsUtilChrome implements AssistantSettingsUtil {
    @Override
    public void launch(Context context) {
        new SettingsLauncherImpl().launchSettingsActivity(
                context, AutofillAssistantPreferenceFragment.class);
    }
}
