// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.base.test.util.Criteria.checkThat;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.view.View;
import android.view.ViewGroup;

import androidx.collection.ArraySet;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Map;
import java.util.Set;

/** Instrumentation tests for the LocationBar component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@Batch(Batch.PER_CLASS)
public class LocationBarTest {
    private static final String HOSTNAME = "suchwowveryyes.edu";
    private static final String GOOGLE_URL = "https://www.google.com";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mGoogleSearchEngine;
    @Mock private TemplateUrl mNonGoogleSearchEngine;
    @Mock private LensController mLensController;
    @Mock private LocaleManagerDelegate mLocaleManagerDelegate;
    @Mock private VoiceRecognitionHandler mVoiceRecognitionHandler;

    private ChromeTabbedActivity mActivity;
    private UrlBar mUrlBar;
    private LocationBarCoordinator mLocationBarCoordinator;
    private LocationBarMediator mLocationBarMediator;
    private String mHostUrl;
    private ActivityKeyboardVisibilityDelegate mKeyboardDelegate;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
                    LocaleManager.getInstance().setDelegateForTest(mLocaleManagerDelegate);
                });
        mHostUrl =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
    }

    private WebPageStation startActivityNormally() {
        WebPageStation webPageStation = mActivityTestRule.startOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        doPostActivitySetup(mActivity);
        return webPageStation;
    }

    private void doPostActivitySetup(ChromeActivity activity) {
        mOmnibox = new OmniboxTestUtils(activity);
        mUrlBar = activity.findViewById(R.id.url_bar);
        mLocationBarCoordinator =
                ((LocationBarCoordinator)
                        activity.getToolbarManager().getToolbarLayoutForTesting().getLocationBar());
        mLocationBarMediator = mLocationBarCoordinator.getMediatorForTesting();
        mLocationBarCoordinator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mLocationBarCoordinator.setLensControllerForTesting(mLensController);
        mKeyboardDelegate = mActivity.getWindowAndroid().getKeyboardDelegate();
    }

    private void setupSearchEngineLogo(String url) {
        boolean isGoogle = url.equals(GOOGLE_URL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Do not show a logo image on NTP, unless the default engine is Google, to
                    // avoid occasional timeout in loading it.
                    doReturn(isGoogle).when(mTemplateUrlService).doesDefaultSearchEngineHaveLogo();
                    doReturn(isGoogle).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
                    doReturn(isGoogle ? mGoogleSearchEngine : mNonGoogleSearchEngine)
                            .when(mTemplateUrlService)
                            .getDefaultSearchEngineTemplateUrl();
                });
    }

    private void assertLocationBarButtonsAre(Integer... expectedIdsArray) {
        Set<Integer> expectedIds = Set.of(expectedIdsArray);
        Set<Integer> actualIds = new ArraySet<>();

        Map<Integer, String> knownIds =
                Map.ofEntries(
                        Map.entry(R.id.mic_button, "R.id.mic_button"),
                        Map.entry(R.id.lens_camera_button, "R.id.lens_camera_button"),
                        Map.entry(R.id.zoom_button, "R.id.zoom_button"),
                        Map.entry(R.id.install_button, "R.id.install_button"),
                        Map.entry(R.id.bookmark_button, "R.id.bookmark_button"),
                        Map.entry(R.id.delete_button, "R.id.delete_button"));

        ViewGroup locationBar = mActivityTestRule.getActivity().findViewById(R.id.location_bar);

        for (int id : knownIds.keySet()) {
            var button = locationBar.findViewById(id);
            if (button.getVisibility() == View.VISIBLE) {
                actualIds.add(id);
            }
        }

        if (expectedIds.equals(actualIds)) return;

        Set<Integer> excessIds = new ArraySet<>(actualIds);
        excessIds.removeAll(expectedIds);

        Set<Integer> missingIds = new ArraySet<>(expectedIds);
        missingIds.removeAll(actualIds);

        var errorMsg = new StringBuilder();
        errorMsg.append("Unexpected IDs: [ ");
        for (int id : excessIds) {
            errorMsg.append(knownIds.get(id));
            errorMsg.append(' ');
        }
        errorMsg.append("], Missing IDs: [ ");
        for (int id : missingIds) {
            errorMsg.append(knownIds.get(id));
            errorMsg.append(' ');
        }
        errorMsg.append(']');
        Assert.assertEquals(errorMsg.toString(), expectedIds, actualIds);
    }

    @Test
    @MediumTest
    public void testEditingText_withDesktopModeDisabled() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);
        testEditingText(/* expectDesktopMode= */ false);
    }

    @Test
    @MediumTest
    public void testEditingText_withDesktopModeEnabled() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        testEditingText(/* expectDesktopMode= */ true);
    }

    private void testEditingText(boolean expectDesktopMode) {
        startActivityNormally();
        mActivityTestRule.loadUrl(mHostUrl);

        // Select the omnibox and confirm expected ready state:
        // - Mobile devices show (by default) no text
        // - Desktop devices show the current page URL
        // - in both cases any text in the Omnibox is selected.
        ThreadUtils.runOnUiThread(mUrlBar::requestFocus);
        CriteriaHelper.pollUiThread(
                () -> {
                    var text = mUrlBar.getText().toString();
                    if (expectDesktopMode) {
                        checkThat("Omnibox holds URL", text, Matchers.startsWith(HOSTNAME));
                    } else {
                        checkThat("Omnibox holds URL", text, Matchers.isEmptyString());
                    }

                    checkThat(
                            "Selection starts at text end",
                            mUrlBar.getSelectionStart(),
                            equalTo(text.length()));
                    checkThat("Selection ends at 0", mUrlBar.getSelectionEnd(), equalTo(0));
                });

        // Now, type some text and confirm cursor placement again.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarCoordinator.setOmniboxEditingText(mHostUrl);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    var text = mUrlBar.getText().toString();
                    checkThat(
                            "No characters are dropped during typing",
                            text,
                            Matchers.startsWith(mHostUrl));
                    checkThat(
                            "No text selection",
                            mUrlBar.getSelectionStart(),
                            equalTo(mUrlBar.getSelectionEnd()));
                    checkThat("Cursor at end", mUrlBar.getSelectionStart(), equalTo(text.length()));
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_buttonVisibilityPhone() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        mActivityTestRule.loadUrl(mHostUrl);

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        ViewUtils.waitForVisibleView(withId(R.id.mic_button));

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarCoordinator.setOmniboxEditingText(mHostUrl);
                });

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                });

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_cameraAssistedSearchLenButtonVisibilityPhone_lensDisabled() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(false).when(mLensController).isLensEnabled(any());
        mActivityTestRule.loadUrl(mHostUrl);

        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        ViewUtils.waitForVisibleView(withId(R.id.mic_button));
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));
        assertLocationBarButtonsAre(R.id.mic_button);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarCoordinator.setOmniboxEditingText(mHostUrl);
                });

        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                });

        waitForNoView(withId(R.id.delete_button));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_cameraAssistedSearchLenButtonVisibilityPhone_lensEnabled() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(true).when(mLensController).isLensEnabled(any());
        mActivityTestRule.loadUrl(mHostUrl);

        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        ViewUtils.waitForVisibleView(withId(R.id.lens_camera_button));
        onView(withId(R.id.lens_camera_button)).check(matches(isDisplayed()));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));
        assertLocationBarButtonsAre(R.id.lens_camera_button, R.id.mic_button);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarCoordinator.setOmniboxEditingText(mHostUrl);
                });

        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                });

        waitForNoView(withId(R.id.delete_button));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.PROFILE_DISC_ON_ALL_PAGES)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFocusLogic_buttonVisibilityTablet_ProfileDiscDisabled_DesktopDisabled() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(false);
        DeviceInput.setSupportsPrecisionPointerForTesting(false);
        testFocusLogic_buttonVisibilityTablet(
                /* expectDesktopMode= */ false, /* profileDiscEnabled= */ false);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        SigninFeatures.PROFILE_DISC_ON_ALL_PAGES,
        SigninFeatures.SIGNIN_LEVEL_UP_BUTTON
    })
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFocusLogic_buttonVisibilityTablet_ProfileDiscEnabled_DesktopDisabled() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(false);
        DeviceInput.setSupportsPrecisionPointerForTesting(false);
        testFocusLogic_buttonVisibilityTablet(
                /* expectDesktopMode= */ false, /* profileDiscEnabled= */ true);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.PROFILE_DISC_ON_ALL_PAGES)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFocusLogic_buttonVisibilityTablet_ProfileDiscDisabled_DesktopEnabled() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        testFocusLogic_buttonVisibilityTablet(
                /* expectDesktopMode= */ true, /* profileDiscEnabled= */ false);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        SigninFeatures.PROFILE_DISC_ON_ALL_PAGES,
        SigninFeatures.SIGNIN_LEVEL_UP_BUTTON
    })
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFocusLogic_buttonVisibilityTablet_ProfileDiscEnabled_DesktopEnabled() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        testFocusLogic_buttonVisibilityTablet(
                /* expectDesktopMode= */ true, /* profileDiscEnabled= */ true);
    }

    private void testFocusLogic_buttonVisibilityTablet(
            boolean expectDesktopMode, boolean profileDiscEnabled) {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(expectDesktopMode);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(expectDesktopMode);
        startActivityNormally();

        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(true).when(mLensController).isLensEnabled(any());

        mActivityTestRule.loadUrl(mHostUrl);

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.bookmark_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        if (expectDesktopMode) {
            onView(withId(R.id.mic_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
            onView(withId(R.id.lens_camera_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
            onView(withId(R.id.delete_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        } else {
            onView(withId(R.id.mic_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
            // With profileDiscEnabled a Signin button appears with priority over the lens button.
            onView(withId(profileDiscEnabled ? R.id.signin_button : R.id.lens_camera_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
            onView(withId(R.id.delete_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarCoordinator.setOmniboxEditingText(mHostUrl);
                });

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.lens_camera_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        if (expectDesktopMode) {
            onView(withId(R.id.delete_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        } else {
            onView(withId(R.id.delete_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                    mLocationBarCoordinator.setShouldShowButtonsWhenUnfocusedForTablet(false);
                });

        onView(withId(R.id.bookmark_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    @Test
    @MediumTest
    public void testFocusLogic_keyboardVisibility() {
        startActivityNormally();
        assertFalse(mKeyboardDelegate.isKeyboardShowing(mUrlBar));

        mOmnibox.requestFocus();
        mOmnibox.checkFocus(true);
        mOmnibox.clearFocus();
        mOmnibox.checkFocus(false);
    }

    /** Test that back press should make the omnibox unfocused. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_backPress() {
        startActivityNormally();

        mOmnibox.requestFocus();
        mOmnibox.checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getOnBackPressedDispatcher().onBackPressed());
        mOmnibox.checkFocus(false);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP_incognito() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), /* incognito= */ true);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testOmniboxSearchEngineLogo_focusedOnSRP() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        mOmnibox.requestFocus();
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOmniboxSearchEngineLogo_ntpToSite() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));

        mActivityTestRule.loadUrl(UrlConstants.ABOUT_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testOmniboxSearchEngineLogo_siteToSite() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(UrlConstants.GPU_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));

        mActivityTestRule.loadUrl(UrlConstants.VERSION_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testFocusCarriesSelection() {
        // Force desktop experience on desktop form factor.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        startActivityNormally();
        mActivityTestRule.loadUrl(mHostUrl);

        Assert.assertFalse(mLocationBarMediator.isUrlBarFocused());

        // Set unfocused selection
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setSelection(2, 5));

        // Focus the omnibox.
        ThreadUtils.runOnUiThread(mUrlBar::requestFocus);

        // Verify focus and that selection is preserved.
        CriteriaHelper.pollUiThread(
                () -> {
                    Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
                    Assert.assertEquals(2, mUrlBar.getSelectionStart());
                    Assert.assertEquals(5, mUrlBar.getSelectionEnd());
                });
    }
}
