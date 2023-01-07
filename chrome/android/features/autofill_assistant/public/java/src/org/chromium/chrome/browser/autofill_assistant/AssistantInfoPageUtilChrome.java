// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;

/**
 * Implementation of {@link AssistantInfoPageUtil} for Chrome.
 */
public class AssistantInfoPageUtilChrome implements AssistantInfoPageUtil {
    @Override
    public void showInfoPage(Context context, String url) {
        CustomTabActivity.showInfoPage(context, url);
    }
}
