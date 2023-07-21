// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.feed.FeedServiceUtil;
import org.chromium.chrome.browser.feed.TabGroupEnabledState;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/**
 * Implements some utilities used for the feed service.
 */
public class FeedServiceUtilImpl implements FeedServiceUtil {
    @Override
    public @TabGroupEnabledState int getTabGroupEnabledState() {
        Context context = ContextUtils.getApplicationContext();
        if (ReturnToChromeUtil.isStartSurfaceEnabled(context)) {
            return TabGroupEnabledState.NONE;
        }
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(context)) {
            return TabGroupEnabledState.BOTH;
        }
        return TabGroupEnabledState.NONE;
    }
}
