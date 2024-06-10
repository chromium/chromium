// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.webapps.WebappActivity;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Custom test rule for simulating a {@link WebappActivity} with a Display Cutout. */
@RequiresApi(Build.VERSION_CODES.P)
public class WebappDisplayCutoutTestRule extends DisplayCutoutTestRule<WebappActivity> {
    /** Test data for the test webapp. */
    private static final String WEBAPP_ID = "webapp_id";

    private static final String WEBAPP_NAME = "webapp name";
    private static final String WEBAPP_SHORT_NAME = "webapp short name";

    /** The maximum waiting time to start {@link WebActivity} in ms. */
    private static final long STARTUP_TIMEOUT = 10000L;

    /** Contains test specific configuration for launching {@link WebappActivity}. */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface TestConfiguration {
        @DisplayMode.EnumType
        int displayMode();
    }

    private TestConfiguration mTestConfiguration;

    public WebappDisplayCutoutTestRule() {
        super(WebappActivity.class);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        mTestConfiguration = description.getAnnotation(TestConfiguration.class);
        return super.apply(base, description);
    }

    @Override
    protected void startActivity() {
        startWebappActivity(mTestConfiguration.displayMode());
    }

    private void startWebappActivity(@DisplayMode.EnumType int displayMode) {
        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), WebappActivity.class);
        intent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + WEBAPP_ID));
        intent.putExtra(WebappConstants.EXTRA_ID, WEBAPP_ID);
        intent.putExtra(WebappConstants.EXTRA_URL, getTestURL());
        intent.putExtra(WebappConstants.EXTRA_NAME, WEBAPP_NAME);
        intent.putExtra(WebappConstants.EXTRA_SHORT_NAME, WEBAPP_SHORT_NAME);
        intent.putExtra(WebappConstants.EXTRA_DISPLAY_MODE, displayMode);

        launchActivity(intent);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(getActivity().getActivityTab(), Matchers.notNullValue());
                    Criteria.checkThat(
                            getActivity().getActivityTab().isLoading(), Matchers.is(false));
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        waitForActivityNativeInitializationComplete();
    }
}
