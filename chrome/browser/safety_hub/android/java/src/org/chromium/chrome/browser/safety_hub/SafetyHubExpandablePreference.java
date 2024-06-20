// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;

public class SafetyHubExpandablePreference extends ChromeBasePreference {
    public SafetyHubExpandablePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.safety_hub_expandable_preference);
    }
}
