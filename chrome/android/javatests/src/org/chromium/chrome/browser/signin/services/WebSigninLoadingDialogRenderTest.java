// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.WebSigninRedirectCoordinator;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for the web signin loading dialog. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(SigninFeatures.ENABLE_WEB_SIGNIN_LOADING_DIALOG)
@Batch(Batch.PER_CLASS)
public class WebSigninLoadingDialogRenderTest {
    public static class NightModeParameterProvider implements ParameterProvider {
        private static final List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet().value(false).name("NightModeDisabled"),
                        new ParameterSet().value(true).name("NightModeEnabled"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .setRevision(1)
                    .build();

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeParameterProvider.class)
    public void testWebSigninLoadingDialog(boolean nightModeEnabled) throws IOException {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        WebSigninRedirectCoordinator coordinator = new WebSigninRedirectCoordinator();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.setTabForTesting(mActivityTestRule.getActivityTab());
                    coordinator.showDialog();
                });
        onViewWaiting(withId(R.id.web_signin_loading_dialog));

        mRenderTestRule.render(
                assumeNonNull(coordinator.getDialogModelForTesting())
                        .get(ModalDialogProperties.CUSTOM_VIEW),
                "web_signin_loading_dialog");
    }
}
