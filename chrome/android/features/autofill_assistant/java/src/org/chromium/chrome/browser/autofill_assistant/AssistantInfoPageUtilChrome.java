// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;

/**
 * Implementation of {@link AssistantInfoPageUtil} for Chrome.
 */
public class AssistantInfoPageUtilChrome implements AssistantInfoPageUtil {
    @Override
    public void showInfoPage(Context context, String url) {
        CustomTabActivity.showInfoPage(context, url);
    }
}
