// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import android.content.Intent;
import android.webkit.WebView;

import androidx.test.espresso.BaseLayerComponent;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;

/**
 * CapturedSitesUiTestRule provides a way to load a URL from the web.
 *
 * Note that this must be run on test thread.
 *
 */
public class CapturedSitesTestRule extends BaseActivityTestRule<WebViewUiTestActivity> {
    // TODO (crbug/1470289) make this class inherit from WebViewTestRule.
    private CapturedSitesSyncWrapper mSyncWrapper;
    private String mLayout;
    private BaseLayerComponent mBaseLayerComponent;

    public CapturedSitesTestRule(Class<WebViewUiTestActivity> activityClass) {
        super(activityClass);
    }

    @Override
    public Statement apply(Statement base, Description desc) {
        UseLayout a = desc.getAnnotation(UseLayout.class);
        if (a != null) {
            mLayout = a.value();
        }
        return super.apply(base, desc);
    }

    @Override
    public void launchActivity(Intent i) {
        if (mLayout != null && !mLayout.isEmpty()) {
            if (i == null) i = getActivityIntent();
            i.putExtra(WebViewUiTestActivity.EXTRA_TEST_LAYOUT_FILE, mLayout);
        }
        super.launchActivity(i);
        mSyncWrapper =
                new CapturedSitesSyncWrapper((WebView) getActivity().findViewById(R.id.webview));
    }

    public void launchActivity() {
        launchActivity(null);
    }

    public void loadUrlSync(String url) {
        mSyncWrapper.loadUrlSync(url);
    }
}
