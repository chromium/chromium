// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.view.View;
import android.view.ViewStub;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render test of incognito description in the incognito ntp. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
    ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
})
public class IncognitoDescriptionViewRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_INCOGNITO)
                    .build();

    public IncognitoDescriptionViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.incognito_description_layout);
                });
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures({
        ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
        ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
    })
    public void testRender_IncognitoDescriptionView() throws IOException {
        View view = sActivity.findViewById(android.R.id.content);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.setBackgroundResource(R.color.ntp_bg_incognito);
                    ViewStub cardStub = sActivity.findViewById(R.id.cookie_card_stub);
                    cardStub.setLayoutResource(R.layout.incognito_cookie_controls_card);
                    cardStub.inflate();
                });
        mRenderTestRule.render(view, "incognito_description_view");
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures({
        ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
        ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
    })
    public void testRender_IncognitoDescriptionViewTrackingProtection() throws IOException {
        View view = sActivity.findViewById(android.R.id.content);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.setBackgroundResource(R.color.ntp_bg_incognito);
                    ViewStub cardStub = sActivity.findViewById(R.id.cookie_card_stub);
                    cardStub.setLayoutResource(R.layout.incognito_tracking_protection_card);
                    cardStub.inflate();
                });
        mRenderTestRule.render(view, "incognito_description_view_tracking_protection");
    }

    // TODO(crbug.com/408036586): Remove once FingerprintingProtectionUx launched.
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.FINGERPRINTING_PROTECTION_UX})
    public void render_IncognitoDescriptionView_alwaysBlock3pcsIncognito() throws IOException {
        View view = sActivity.findViewById(android.R.id.content);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.setBackgroundResource(R.color.ntp_bg_incognito);
                    ViewStub cardStub = sActivity.findViewById(R.id.cookie_card_stub);
                    cardStub.setLayoutResource(R.layout.incognito_tracking_protection_card);
                    cardStub.inflate();
                });
        mRenderTestRule.render(view, "incognito_description_view_always_block_3pcs_incognito_card");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void render_IncognitoDescriptionView_incognitoTrackingProtectionsEnabled()
            throws IOException {
        View view = sActivity.findViewById(android.R.id.content);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.setBackgroundResource(R.color.ntp_bg_incognito);
                    ViewStub cardStub = sActivity.findViewById(R.id.cookie_card_stub);
                    cardStub.setLayoutResource(R.layout.incognito_tracking_protection_card);
                    cardStub.inflate();
                });
        mRenderTestRule.render(
                view, "incognito_description_view_incognito_tracking_protections_card");
    }
}
