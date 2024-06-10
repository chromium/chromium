// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Custom {@link ChromeActivityTestRule} for tests using a WebAPK {@link WebappActivity}. */
public class WebApkActivityTestRule extends ChromeActivityTestRule<WebappActivity> {
    /** Time in milliseconds to wait for page to be loaded. */
    private static final long STARTUP_TIMEOUT = 10000;

    public WebApkActivityTestRule() {
        super(WebappActivity.class);
    }

    @Override
    protected void before() throws Throwable {
        WebApkUpdateManager.setUpdatesDisabledForTesting(true);
        super.before();
    }

    /**
     * Launches a WebAPK Activity and waits for the page to have finished loading and for the splash
     * screen to be hidden.
     */
    public WebappActivity startWebApkActivity(
            BrowserServicesIntentDataProvider webApkIntentDataProvider) {
        WebappInfo webApkInfo = WebappInfo.create(webApkIntentDataProvider);
        Intent intent = createIntent(webApkInfo);
        WebappActivity.setIntentDataProviderForTesting(webApkIntentDataProvider);

        return startWebApkActivity(intent, webApkInfo.url());
    }

    /**
     * Launches a WebAPK Activity and waits for the page to have finished loading and for the splash
     * screen to be hidden.
     */
    public WebappActivity startWebApkActivity(final String startUrl) {
        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), WebappActivity.class);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, "org.chromium.webapk.test");
        intent.putExtra(WebappConstants.EXTRA_URL, startUrl);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        return startWebApkActivity(intent, startUrl);
    }

    private WebappActivity startWebApkActivity(final Intent intent, final String startUrl) {
        final WebappActivity webApkActivity =
                (WebappActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        setActivity(webApkActivity);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(webApkActivity.getActivityTab(), Matchers.notNullValue());
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(webApkActivity.getActivityTab(), startUrl);
        WebappActivityTestRule.waitUntilSplashHides(webApkActivity);
        return webApkActivity;
    }

    private Intent createIntent(WebappInfo webApkInfo) {
        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), WebappActivity.class);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkInfo.webApkPackageName());
        intent.putExtra(WebappConstants.EXTRA_ID, webApkInfo.id());
        intent.putExtra(WebappConstants.EXTRA_URL, webApkInfo.url());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        return intent;
    }
}
