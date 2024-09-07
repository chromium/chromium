// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.text.SpannableString;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of touch to fill for credit cards sheet and compare them to a gold
 * standard.
 */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SimpleNoticeSheetRenderTest {

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_PASSWORDS)
                    .build();

    private BottomSheetController mBottomSheetController;
    private SimpleNoticeSheetCoordinator mCoordinator;
    private static final String sTitle = "Simple notice sheet title";
    private static final SpannableString sText =
            SpannableString.valueOf("The text about the simple notice sheet");
    private static final String sButtonText = "Button";

    public SimpleNoticeSheetRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.openMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new SimpleNoticeSheetCoordinator(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                });
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        runOnUiThreadBlocking(() -> tearDownNightModeAfterChromeActivityDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsSimpleNoticeSheet() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(SimpleNoticeSheetProperties.ALL_KEYS)
                                    .with(SimpleNoticeSheetProperties.SHEET_TITLE, sTitle)
                                    .with(SimpleNoticeSheetProperties.SHEET_TEXT, sText)
                                    .with(SimpleNoticeSheetProperties.BUTTON_TITLE, sButtonText)
                                    .with(SimpleNoticeSheetProperties.BUTTON_ACTION, Assert::fail)
                                    .build();
                    mCoordinator.showSheet(model);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "simple_notice_bottom_sheet");
    }
}
