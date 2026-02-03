// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

@NullMarked
public class CustomSearchEngineListPreference extends ChromeBasePreference {

    public CustomSearchEngineListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.custom_search_engine_list_preference);
    }
}
