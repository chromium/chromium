// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Instrumentation Render tests for default {@link CustomTabActivity} UI. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class IncognitoCustomTabActivityRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParameter =
            Arrays.asList(
                    new ParameterSet().name("HTTPS").value(true),
                    new ParameterSet().name("HTTP").value(false));

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final int PORT_NO = 31415;

    private final boolean mRunWithHttps;
    private Intent mIntent;

    @Rule
    public final IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Rule
    public final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(3)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_CUSTOM_TABS)
                    .build();

    @Before
    public void setUp() throws TimeoutException {
        mEmbeddedTestServerRule.setServerUsesHttps(mRunWithHttps);
        mEmbeddedTestServerRule.setServerPort(PORT_NO);
        prepareCCTIntent();

        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
    }

    public IncognitoCustomTabActivityRenderTest(boolean runWithHttps) {
        mRunWithHttps = runWithHttps;
    }

    private void prepareCCTIntent() {
        String url = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        mIntent =
                CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), url);
    }

    private void startActivity(String renderTestId, int mScreenOrientation) throws IOException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        mCustomTabActivityTestRule.getActivity().setRequestedOrientation(mScreenOrientation);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, renderTestId);
    }

    private void startActivity(String renderTestId) throws IOException {
        startActivity(renderTestId, ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    private String testIdSuffix() {
        return "_https_" + mRunWithHttps;
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbar() throws IOException {
        startActivity("default_incognito_cct_toolbar_with_https" + testIdSuffix());
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarInLandscapeMode() throws IOException {
        startActivity(
                "default_incognito_cct_toolbar_in_landscape_with_https" + testIdSuffix(),
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }
}
