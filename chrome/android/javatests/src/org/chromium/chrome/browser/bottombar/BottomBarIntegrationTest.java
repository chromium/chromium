// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.Criteria.checkThat;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.bottombar.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Integration tests for the Bottom Bar country variations and extra button candidate resolution.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(DeviceFormFactor.PHONE)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BottomBarIntegrationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;
    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ false);
    }

    @After
    public void tearDown() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ false);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false"})
    public void testBottomBarCountrySupplier_populatedInNativeInit() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var countrySupplier = mTabbedRootUiCoordinator.getCountrySupplierForTesting();
                    assertNotNull(countrySupplier);
                });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"variations-override-country=us"})
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false"})
    public void testBottomBarExtraButton_WithUsCountry_ShowsGlicButton() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ true);
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        CriteriaHelper.pollUiThread(
                () -> {
                    var countrySupplier = mTabbedRootUiCoordinator.getCountrySupplierForTesting();
                    checkThat(countrySupplier, notNullValue());
                    assert countrySupplier != null;
                    checkThat(countrySupplier.get(), is("us"));

                    final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    View extraContainer = activity.findViewById(R.id.extra_button_container);
                    checkThat(extraContainer, notNullValue());
                    assert extraContainer != null;
                    checkThat(extraContainer.getVisibility(), is(View.VISIBLE));

                    View extraButton = activity.findViewById(R.id.extra_button);
                    checkThat(extraButton, notNullValue());
                    assert extraButton != null;
                    checkThat(extraButton.getVisibility(), is(View.VISIBLE));
                    CharSequence contentDescription = extraButton.getContentDescription();
                    checkThat(contentDescription, notNullValue());
                    assert contentDescription != null;
                    checkThat(
                            contentDescription.toString(),
                            anyOf(containsString("Gemini"), containsString("GLIC")));
                });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"variations-override-country=au"})
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR
                + ":show_glic_setting_toggle/false/bypass_aim_geofencing/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR_AIM
    })
    public void testBottomBarExtraButton_WithAuCountry_ShowsAiModeButton() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        CriteriaHelper.pollUiThread(
                () -> {
                    var countrySupplier = mTabbedRootUiCoordinator.getCountrySupplierForTesting();
                    checkThat(countrySupplier, notNullValue());
                    assert countrySupplier != null;
                    checkThat(countrySupplier.get(), is("au"));

                    final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    View extraContainer = activity.findViewById(R.id.extra_button_container);
                    checkThat(extraContainer, notNullValue());
                    assert extraContainer != null;
                    checkThat(extraContainer.getVisibility(), is(View.VISIBLE));

                    View extraButton = activity.findViewById(R.id.extra_button);
                    checkThat(extraButton, notNullValue());
                    assert extraButton != null;
                    checkThat(extraButton.getVisibility(), is(View.VISIBLE));
                    CharSequence contentDescription = extraButton.getContentDescription();
                    checkThat(contentDescription, notNullValue());
                    assert contentDescription != null;
                    checkThat(contentDescription.toString(), containsString("AI Mode"));
                });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"variations-override-country=fr"})
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false",
        ChromeFeatureList.ANDROID_BOTTOM_BAR_AIM
    })
    public void testBottomBarExtraButton_WithFrCountry_HidesExtraButton() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        CriteriaHelper.pollUiThread(
                () -> {
                    var countrySupplier = mTabbedRootUiCoordinator.getCountrySupplierForTesting();
                    checkThat(countrySupplier, notNullValue());
                    assert countrySupplier != null;
                    checkThat(countrySupplier.get(), is("fr"));

                    final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    View extraContainer = activity.findViewById(R.id.extra_button_container);
                    checkThat(
                            extraContainer == null || extraContainer.getVisibility() == View.GONE,
                            is(/* expected= */ true));
                });
    }
}
