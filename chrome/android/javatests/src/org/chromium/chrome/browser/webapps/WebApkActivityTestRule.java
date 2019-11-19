// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Custom {@link ChromeActivityTestRule} for tests using {@link WebApkActivity}. */
public class WebApkActivityTestRule extends ChromeActivityTestRule<WebApkActivity> {
    /** Time in milliseconds to wait for page to be loaded. */
    private static final long STARTUP_TIMEOUT = ScalableTimeout.scaleTimeout(10000);

    public WebApkActivityTestRule() {
        super(WebApkActivity.class);
    }

    /**
     * Launches WebApkActivity and waits for the page to have finished loading and for the splash
     * screen to be hidden.
     */
    public WebApkActivity startWebApkActivity(WebApkInfo webApkInfo) {
        Intent intent = createIntent(webApkInfo);

        WebappActivity.addWebappInfo(webApkInfo.id(), webApkInfo);
        final WebApkActivity webApkActivity =
                (WebApkActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);
        setActivity(webApkActivity);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return webApkActivity.getActivityTab() != null;
            }
        }, STARTUP_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(webApkActivity.getActivityTab(), webApkInfo.url());
        WebappActivityTestRule.waitUntilSplashHides(webApkActivity);

        // Launching the WebAPK should have popped the WebApkInfo.
        Assert.assertNull(WebappActivity.popWebappInfo(webApkInfo.id()));

        return webApkActivity;
    }

    private Intent createIntent(WebApkInfo webApkInfo) {
        Intent intent =
                new Intent(InstrumentationRegistry.getTargetContext(), WebApkActivity0.class);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkInfo.webApkPackageName());
        intent.putExtra(ShortcutHelper.EXTRA_ID, webApkInfo.id());
        intent.putExtra(ShortcutHelper.EXTRA_URL, webApkInfo.url());
        intent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        return intent;
    }
}
