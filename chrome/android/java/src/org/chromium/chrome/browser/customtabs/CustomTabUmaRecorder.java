// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

/**
 * Handles recording User Metrics for Custom Tab Activity.
 */
@ActivityScope
public class CustomTabUmaRecorder {
    private final boolean mRecordDownloadsUiMetrics;

    @Inject
    public CustomTabUmaRecorder(BrowserServicesIntentDataProvider intentDataProvider) {
        mRecordDownloadsUiMetrics = intentDataProvider.shouldEnableEmbeddedMediaExperience();
    }

    /**
     * Records that the close button was clicked.
     */
    public void recordCloseButtonClick() {
        RecordUserAction.record("CustomTabs.CloseButtonClicked");
        if (mRecordDownloadsUiMetrics) {
            RecordUserAction.record("CustomTabs.CloseButtonClicked.DownloadsUI");
        }
    }

    /**
     * Records that a custom button was clicked.
     */
    public void recordCustomButtonClick(Resources resources, CustomButtonParams params) {
        RecordUserAction.record("CustomTabsCustomActionButtonClick");

        if (mRecordDownloadsUiMetrics
                && TextUtils.equals(params.getDescription(), resources.getString(R.string.share))) {
            RecordUserAction.record("CustomTabsCustomActionButtonClick.DownloadsUI.Share");
        }
    }
}
