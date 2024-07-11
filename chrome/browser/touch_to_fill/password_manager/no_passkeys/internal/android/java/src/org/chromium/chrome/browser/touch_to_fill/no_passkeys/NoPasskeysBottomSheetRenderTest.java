// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.graphics.Color;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of touch to fill for credit cards sheet and compare them to a gold
 * standard.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NoPasskeysBottomSheetRenderTest {
    private static final String TEST_ORIGIN = "origin.com";

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(Component.UI_BROWSER_PASSWORDS)
                    .build();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock NoPasskeysBottomSheetCoordinator.NativeDelegate mNativeDelegate;

    private BottomSheetController mBottomSheetController;
    private NoPasskeysBottomSheetCoordinator mCoordinator;

    public NoPasskeysBottomSheetRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.openMocks(this);
        mActivityRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityRule.getActivity(), Stage.RESUMED);
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController = createBottomSheetController();
                    mCoordinator =
                            new NoPasskeysBottomSheetCoordinator(
                                    new WeakReference<>(getActivity()),
                                    new WeakReference<>(mBottomSheetController),
                                    mNativeDelegate);
                });
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try { // Because activity may already be closed (e.g. during to last test's tear down).
            finishActivity(getActivity());
        } catch (Exception e) {
        }
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRendersAllButtons() throws IOException {
        runOnUiThreadBlocking(() -> mCoordinator.show(TEST_ORIGIN));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        mRenderTestRule.render(
                getActivity().findViewById(R.id.bottom_sheet), "touch_to_fill_no_passkeys_sheet");
    }

    private BlankUiTestActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private BottomSheetController createBottomSheetController() {
        ViewGroup activityContentView = getActivity().findViewById(android.R.id.content);
        ScrimCoordinator scrimCoordinator =
                new ScrimCoordinator(
                        getActivity(),
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {}

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        activityContentView,
                        Color.WHITE);
        return BottomSheetControllerFactory.createFullWidthBottomSheetController(
                () -> scrimCoordinator,
                (unused) -> {},
                getActivity().getWindow(),
                KeyboardVisibilityDelegate.getInstance(),
                () -> activityContentView);
    }
}
