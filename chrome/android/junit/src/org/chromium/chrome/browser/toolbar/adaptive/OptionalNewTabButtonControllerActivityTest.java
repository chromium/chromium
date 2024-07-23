// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.Pair;

import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.top.OptionalBrowsingModeButtonController;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.NoSuchElementException;

/**
 * Robolectric tests running {@link OptionalNewTabButtonController} in a {@link
 * ChromeTabbedActivity}.
 */
@Config(shadows = {OptionalNewTabButtonControllerActivityTest.ShadowDelegate.class})
@RunWith(ChromeRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
    "enable-features="
            + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
            + "<FakeStudyName",
    "force-fieldtrials=FakeStudyName/Enabled",
    "force-fieldtrial-params=FakeStudyName.Enabled:min_version_adaptive/0"
})
public class OptionalNewTabButtonControllerActivityTest {

    /**
     * Shadow of {@link OptionalNewTabButtonController.Delegate}. Injects testing values into every
     * instance of {@link OptionalNewTabButtonController}.
     */
    @Implements(OptionalNewTabButtonController.Delegate.class)
    public static class ShadowDelegate {
        private static MockTabCreatorManager sTabCreatorManager;
        private static MockTabModelSelector sTabModelSelector;

        protected static void reset() {
            sTabModelSelector = null;
            sTabCreatorManager = null;
        }

        @Implementation
        protected TabCreatorManager getTabCreatorManager() {
            return sTabCreatorManager;
        }

        @Implementation
        protected TabModelSelector getTabModelSelector() {
            return sTabModelSelector;
        }
    }

    private ActivityScenario<ChromeTabbedActivity> mActivityScenario;
    private AdaptiveToolbarButtonController mAdaptiveButtonController;
    private MockTab mTab;

    @Before
    public void setUp() {
        Profile originalProfile = Mockito.mock(Profile.class);
        Profile incognitoProfile = Mockito.mock(Profile.class);
        when(originalProfile.getOriginalProfile()).thenReturn(originalProfile);
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        // Avoid leaking state from the previous test.
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(
                AdaptiveToolbarButtonVariant.NEW_TAB);
        // To bypass a direct call to AdaptiveToolbarStatePredictor#readFromSegmentationPlatform for
        // UMA.
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.NEW_TAB)));
        MockTabModelSelector tabModelSelector =
                new MockTabModelSelector(
                        originalProfile,
                        incognitoProfile,
                        /* tabCount= */ 1,
                        /* incognitoTabCount= */ 0,
                        (id, incognito) -> {
                            Profile profile = incognito ? incognitoProfile : originalProfile;
                            MockTab tab = spy(MockTab.createAndInitialize(id, profile));
                            doReturn(Mockito.mock(WebContents.class)).when(tab).getWebContents();
                            return tab;
                        });
        assertNull(ShadowDelegate.sTabModelSelector);
        assertNull(ShadowDelegate.sTabCreatorManager);
        ShadowDelegate.sTabModelSelector = tabModelSelector;
        ShadowDelegate.sTabCreatorManager = new MockTabCreatorManager(tabModelSelector);
        mTab = tabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(JUnitTestGURLs.EXAMPLE_URL);

        mActivityScenario = ActivityScenario.launch(ChromeTabbedActivity.class);
        mActivityScenario.onActivity(
                activity -> {
                    mAdaptiveButtonController =
                            getAdaptiveButton(getOptionalButtonController(activity));
                    mAdaptiveButtonController.setProfile(originalProfile);
                });
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
        ShadowDelegate.reset();
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w390dp-h820dp-land")
    public void testAlwaysShownOnPhone() {
        mActivityScenario.onActivity(
                activity -> {
                    assertTrue(mAdaptiveButtonController.get(mTab).canShow());

                    applyQualifiers(activity, "+port");

                    assertTrue(mAdaptiveButtonController.get(mTab).canShow());
                });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w600dp-h820dp")
    public void testNeverShownOnTablet() {
        mActivityScenario.onActivity(
                activity -> {
                    assertFalse(mAdaptiveButtonController.get(mTab).canShow());

                    // Rotating a tablet should not change canShow.
                    applyQualifiers(activity, "+land");

                    assertFalse(mAdaptiveButtonController.get(mTab).canShow());
                });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w400dp-h600dp")
    public void testNightMode() {
        mActivityScenario.onActivity(
                activity -> {
                    assertTrue(mAdaptiveButtonController.get(mTab).canShow());

                    // Unrelated qualifiers should not change canShow. This covers an early return
                    // from onConfigurationChanged.
                    applyQualifiers(activity, "+night");

                    assertTrue(mAdaptiveButtonController.get(mTab).canShow());
                });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w400dp-h600dp")
    public void testNtp() {
        mActivityScenario.onActivity(
                activity -> {
                    assertTrue(mAdaptiveButtonController.get(mTab).canShow());

                    mTab.setGurlOverrideForTesting(JUnitTestGURLs.NTP_URL);
                    assertFalse(mAdaptiveButtonController.get(mTab).canShow());

                    mTab.setGurlOverrideForTesting(JUnitTestGURLs.EXAMPLE_URL);
                    assertTrue(mAdaptiveButtonController.get(mTab).canShow());
                });
    }

    private static OptionalBrowsingModeButtonController getOptionalButtonController(
            ChromeTabbedActivity activity) {
        TopToolbarCoordinator toolbar =
                (TopToolbarCoordinator) activity.getToolbarManager().getToolbar();
        return toolbar.getOptionalButtonControllerForTesting();
    }

    private static AdaptiveToolbarButtonController getAdaptiveButton(
            OptionalBrowsingModeButtonController optionalButtonController) {
        List<ButtonDataProvider> buttonDataProviders =
                optionalButtonController.getButtonDataProvidersForTesting();
        for (ButtonDataProvider buttonDataProvider : buttonDataProviders) {
            if (!(buttonDataProvider instanceof AdaptiveToolbarButtonController)) {
                continue;
            }
            return (AdaptiveToolbarButtonController) buttonDataProvider;
        }
        throw new NoSuchElementException();
    }

    /** Sets device qualifiers and notifies the activity about configuration change. */
    private static void applyQualifiers(ChromeTabbedActivity activity, String qualifiers) {
        RuntimeEnvironment.setQualifiers(qualifiers);
        Configuration configuration = Resources.getSystem().getConfiguration();
        activity.onConfigurationChanged(configuration);
    }
}
