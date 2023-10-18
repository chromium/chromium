// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.view.View;
import android.view.ViewStub;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render test of revamped incognito description in the incognito ntp. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class RevampedIncognitoDescriptionViewRenderTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_INCOGNITO)
                    .build();

    public RevampedIncognitoDescriptionViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = getActivity();
                    activity.setContentView(R.layout.revamped_incognito_description_layout);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_RevampedIncognitoDescriptionView() throws IOException {
        View view = getActivity().findViewById(android.R.id.content);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.setBackgroundResource(R.color.ntp_bg_incognito);
                    ViewStub cardStub = getActivity().findViewById(R.id.cookie_card_stub);
                    cardStub.setLayoutResource(R.layout.revamped_incognito_cookie_controls_card);
                    cardStub.inflate();
                });
        mRenderTestRule.render(view, "revamped_incognito_description_view");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_RevampedIncognitoDescriptionViewTrackingProtection() throws IOException {
        View view = getActivity().findViewById(android.R.id.content);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.setBackgroundResource(R.color.ntp_bg_incognito);
                    ViewStub cardStub = getActivity().findViewById(R.id.cookie_card_stub);
                    cardStub.setLayoutResource(
                            R.layout.revamped_incognito_tracking_protection_card);
                    cardStub.inflate();
                });
        mRenderTestRule.render(view, "revamped_incognito_description_view_tracking_protection");
    }
}
