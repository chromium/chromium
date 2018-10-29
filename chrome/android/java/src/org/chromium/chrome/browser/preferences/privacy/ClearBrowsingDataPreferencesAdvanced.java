// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTab;

import java.util.Arrays;
import java.util.List;

/**
 * A more advanced version of {@link ClearBrowsingDataPreferences} with more dialog options and less
 * explanatory text.
 */
public class ClearBrowsingDataPreferencesAdvanced extends ClearBrowsingDataPreferences {
    @Override
    protected int getPreferenceType() {
        return ClearBrowsingDataTab.ADVANCED;
    }

    @Override
    protected List<Integer> getDialogOptions() {
        return Arrays.asList(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_MEDIA_LICENSES, DialogOption.CLEAR_CACHE,
                DialogOption.CLEAR_PASSWORDS, DialogOption.CLEAR_FORM_DATA,
                DialogOption.CLEAR_SITE_SETTINGS);
    }

    @Override
    protected void onClearBrowsingData() {
        super.onClearBrowsingData();
        RecordHistogram.recordEnumeratedHistogram("History.ClearBrowsingData.UserDeletedFromTab",
                ClearBrowsingDataTab.ADVANCED, ClearBrowsingDataTab.NUM_TYPES);
        RecordUserAction.record("ClearBrowsingData_AdvancedTab");
    }
}
