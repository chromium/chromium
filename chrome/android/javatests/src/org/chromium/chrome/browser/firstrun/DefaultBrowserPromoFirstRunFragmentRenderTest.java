// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.displayingAtLeastOption;
import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.content.res.Configuration;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for {@link DefaultBrowserPromoFirstRunFragment}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.DEFAULT_BROWSER_PROMO_FRE})
@DoNotBatch(reason = "This test relies on native initialization")
public class DefaultBrowserPromoFirstRunFragmentRenderTest {
    private static final int REVISION = 0;

    /**
     * * Custom version of the fragment to allow manual injection of the delegate since we aren't
     * running inside a real FirstRunActivity.
     */
    public static class CustomDefaultBrowserPromoFirstRunFragment
            extends DefaultBrowserPromoFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }
    }

    /** Parameter provider for night mode state and device orientation. */
    public static class NightModeAndOrientationParameterProvider implements ParameterProvider {
        private static final List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(/* firstArg= */ false, Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeDisabled_Portrait"),
                        new ParameterSet()
                                .value(/* firstArg= */ false, Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeDisabled_Landscape"),
                        new ParameterSet()
                                .value(/* firstArg= */ true, Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeEnabled_Portrait"),
                        new ParameterSet()
                                .value(/* firstArg= */ true, Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeEnabled_Landscape"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_FIRST_RUN)
                    .setRevision(REVISION)
                    .build();

    @Mock private FirstRunPageDelegate mFirstRunPageDelegateMock;

    private CustomDefaultBrowserPromoFirstRunFragment mFragment;

    // Inject the params into the test environment.
    @UseMethodParameterBefore(NightModeAndOrientationParameterProvider.class)
    public void setupNightModeAndDeviceOrientation(boolean nightModeEnabled, int orientation) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            nightModeEnabled
                                    ? AppCompatDelegate.MODE_NIGHT_YES
                                    : AppCompatDelegate.MODE_NIGHT_NO);
                });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(
                orientation == Configuration.ORIENTATION_PORTRAIT ? "Portrait" : "Landscape");
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);

        // Explicitly set the background color of the container to match the theme
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .findViewById(android.R.id.content)
                            .setBackgroundColor(
                                    SemanticColorUtils.getDefaultBgColor(
                                            mActivityTestRule.getActivity()));
                });

        mFragment = new CustomDefaultBrowserPromoFirstRunFragment();
        mFragment.setPageDelegate(mFirstRunPageDelegateMock);
    }

    @After
    public void tearDown() {
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
                });
    }

    private void launchFragmentWithArm(String arm, int orientation) {
        ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting(arm);
        // Rotate the Blank activity while it's still empty.
        ActivityTestUtils.rotateActivityToOrientation(mActivityTestRule.getActivity(), orientation);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // This forces the host activity background to match the theme color.
                    mActivityTestRule
                            .getActivity()
                            .findViewById(android.R.id.content)
                            .setBackgroundColor(
                                    SemanticColorUtils.getDefaultBgColor(
                                            mActivityTestRule.getActivity()));

                    mActivityTestRule
                            .getActivity()
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, mFragment)
                            .commit();
                });

        int minDisplayedPercentage = orientation == Configuration.ORIENTATION_LANDSCAPE ? 0 : 51;
        waitForView(withId(R.id.title), displayingAtLeastOption(minDisplayedPercentage));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testRender_Arm2_PrimerNoInstructions(boolean nightModeEnabled, int orientation)
            throws IOException {
        launchFragmentWithArm("primer_no_instructions", orientation);
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "default_browser_fre_primer_no_instructions");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testRender_Arm3_PrimerPromotionalText(boolean nightModeEnabled, int orientation)
            throws IOException {
        launchFragmentWithArm("primer_promotional_text", orientation);
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "default_browser_fre_primer_primer_promotional_text");
    }
}
