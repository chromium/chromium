// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the all passwords bottom sheet and compare them to a gold
 * standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AllPasswordsBottomSheetRenderTest {
    private static final String ORIGIN = "google.com";
    private static final Credential ANA =
            new Credential(
                    /* username= */ "ana@gmail.com",
                    /* password= */ "S3cr3t",
                    /* formattedUsername= */ "ana@gmail.com",
                    /* originUrl= */ "https://example.com",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ true);
    private static final Credential NO_ONE =
            new Credential(
                    /* username= */ "",
                    /* password= */ "***",
                    /* formattedUsername= */ "No Username",
                    /* originUrl= */ "https://m.example.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    private static final Credential BOB =
            new Credential(
                    /* username= */ "Bob",
                    /* password= */ "***",
                    /* formattedUsername= */ "Bob",
                    /* originUrl= */ "android://com.facebook.org",
                    /* isAndroidCredential= */ true,
                    /* appDisplayName= */ "facebook",
                    /* isPlusAddressUsername= */ false);

    private static final List<Credential> CREDENTIALS = List.of(ANA, NO_ONE, BOB);

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
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Mock private Profile mProfile;
    @Mock private AllPasswordsBottomSheetCoordinator.Delegate mDelegate;

    public AllPasswordsBottomSheetRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowBottomSheet() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    AllPasswordsBottomSheetCoordinator coordinator =
                            new AllPasswordsBottomSheetCoordinator();
                    coordinator.initialize(
                            activity,
                            mProfile,
                            activity.getRootUiCoordinatorForTesting().getBottomSheetController(),
                            mDelegate,
                            ORIGIN);

                    coordinator.showCredentials(
                            new ArrayList<>(CREDENTIALS), /* isPasswordField= */ false);
                });
        BottomSheetTestSupport.waitForOpen(
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController());

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.all_passwords_bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "all_passwords_bottom_sheet");
    }
}
