// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

@NullMarked
public class CustomSiteSearchListPreference extends ChromeBasePreference {

    public CustomSiteSearchListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.custom_site_search_list_preference);
    }
}
