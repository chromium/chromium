// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.feed.FeedServiceUtil;
import org.chromium.chrome.browser.feed.TabGroupEnabledState;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/**
 * Implements some utilities used for the feed service.
 */
public class FeedServiceUtilImpl implements FeedServiceUtil {
    @Override
    public @TabGroupEnabledState int getTabGroupEnabledState() {
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(ContextUtils.getApplicationContext())) {
            if (TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue()) {
                return TabGroupEnabledState.REPLACED;
            } else {
                return TabGroupEnabledState.BOTH;
            }
        }
        return TabGroupEnabledState.NONE;
    }
}
