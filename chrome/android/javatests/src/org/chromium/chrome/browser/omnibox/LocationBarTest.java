// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.collection.ArraySet;
import androidx.lifecycle.Lifecycle;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Instrumentation tests for the LocationBar component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DoNotBatch(reason = "Test start up behaviors.")
public class LocationBarTest {
    private static final String TEST_QUERY = "testing query";
    private static final List<String> TEST_PARAMS = Arrays.asList("foo=bar");
    private static final String HOSTNAME = "suchwowveryyes.edu";
    private static final String GOOGLE_URL = "https://www.google.com";
    private static final String NON_GOOGLE_URL = "https://www.notgoogle.com";

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
    private String mSearchUrl;
    private ActivityKeyboardVisibilityDelegate mKeyboardDelegate;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
                    LocaleManager.getInstance().setDelegateForTest(mLocaleManagerDelegate);
                });
    }

    @After
    public void tearDown() {
        mActivityTestRule.skipWindowAndTabStateCleanup();
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
        mSearchUrl = mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/search");
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

    private void updateLocationBar() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarMediator mediator = mLocationBarCoordinator.getMediatorForTesting();
                    mediator.onIncognitoStateChanged();
                    mediator.onPrimaryColorChanged();
                    mediator.onSecurityStateChanged();
                    mediator.onTemplateURLServiceChanged();
                    mediator.onUrlChanged(false);
                });
    }

    @Test
    @MediumTest
    public void testSetSearchQueryFocusesUrlBar() {
        startActivityNormally();
        final String query = "testing query";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutocompleteInput input =
                            new AutocompleteInput()
                                    .setUserText(query)
                                    .setFocusReason(OmniboxFocusReason.SEARCH_QUERY);
                    mLocationBarMediator.beginInput(input);
                });
        // Query cannot be applied right away because the UrlBar needs to acquire focus first.
        CriteriaHelper.pollUiThread(
                () -> {
                    Assert.assertEquals(query, mUrlBar.getTextWithoutAutocomplete());
                    Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
                    mKeyboardDelegate.isKeyboardShowing(mUrlBar);
                });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/507245181")
    public void testOnConfigurationChanged() {
        // Start activity in Desktop mode. Expect UrlBar to focus.
        // The DesktopMode check verifies connected peripherals, not just the Configuration change.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        startActivityNormally();
        // We expect the UrlBar to be focused iff a Hardware keyboard handler does not automatically
        // call up Software keyboard (IME).
        boolean wantUrlBarFocus = !KeyboardUtils.shouldShowImeWithHardwareKeyboard(mActivity);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarMediator.showUrlBarCursorWithoutFocusAnimations();
                    // If IME is configured to show up with hardware keys, url bar should not
                    // receive focus.
                    Assert.assertEquals(wantUrlBarFocus, mLocationBarMediator.isUrlBarFocused());
                });

        Configuration configuration = mActivity.getSavedConfigurationForTesting();
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarMediator.onConfigurationChanged(configuration);
                    Assert.assertFalse(mLocationBarMediator.isUrlBarFocused());
                });
    }

    @Test
    @MediumTest
    public void testPostDestroyFocusLogic() {
        startActivityNormally();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.finish();
                });

        CriteriaHelper.pollUiThread(
                () -> mActivity.getLifecycle().getCurrentState().equals(Lifecycle.State.DESTROYED));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarMediator.setUrlFocusChangeInProgress(false);
                    mLocationBarMediator.finishUrlFocusChange(true, true);
                });
    }

    @Test
    @MediumTest
    public void testEditingText() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        testEditingText(/* expectDesktopMode= */ true);
    }

    @Test
    @MediumTest
    public void testEditingText_withDesktopModeDisabled() {
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
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mUrlBar.getText().toString().startsWith(HOSTNAME));
                    mUrlBar.requestFocus();
                    if (expectDesktopMode) {
                        Assert.assertTrue(mUrlBar.getText().toString().startsWith(HOSTNAME));
                    } else {
                        Assert.assertEquals("", mUrlBar.getText().toString());
                    }
                    mLocationBarCoordinator.setOmniboxEditingText(url);
                    Assert.assertEquals(url, mUrlBar.getText().toString());
                    Assert.assertEquals(url.length(), mUrlBar.getSelectionStart());
                    Assert.assertEquals(url.length(), mUrlBar.getSelectionEnd());
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_buttonVisibilityPhone() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

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
                    mLocationBarCoordinator.setOmniboxEditingText(url);
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
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

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
                    mLocationBarCoordinator.setOmniboxEditingText(url);
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
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

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
                    mLocationBarCoordinator.setOmniboxEditingText(url);
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
    public void testFocusLogic_lenButtonVisibilityOnNtpPhone_updatedOnceWhenNtpScrolled() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());

        Mockito.reset(mVoiceRecognitionHandler);

        // Proabably never worked. crbug.com/446200399
        // onView(
        //                 allOf(
        //                         withId(R.id.voice_search_button),
        //                         withParent(withId(R.layout.new_tab_page_layout))))
        //         .check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

                    // Updating the fraction once should query voice search visibility.
                    mLocationBarMediator.setUrlFocusChangeFraction(.5f, .5f);

                    // Further updates to the fraction shouldn't trigger a button visibility update.
                    mLocationBarMediator.setUrlFocusChangeFraction(.6f, .6f);
                    Mockito.verify(mVoiceRecognitionHandler, Mockito.atMost(1))
                            .isVoiceSearchEnabled();
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    // TODO(crbug.com/423465927): Explore a better approach to make the
    // existing tests run with the prewarm feature enabled.
    @DisableFeatures({"Prewarm"})
    public void testFocusLogic_lenButtonVisibilityOnLocationBarOnIncognitoStateChange() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(false).when(mLensController).isLensEnabled(any());
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        // Test when incognito is true.
        mActivityTestRule.loadUrlInNewTab(url, /* incognito= */ true);
        updateLocationBar();
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });
        waitForNoView(withId(R.id.lens_camera_button));
        ViewUtils.waitForVisibleView(withId(R.id.mic_button));
        assertLocationBarButtonsAre(R.id.mic_button);

        // Test when incognito is false.
        doReturn(true).when(mLensController).isLensEnabled(any());
        mActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
        updateLocationBar();
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });
        ViewUtils.waitForVisibleView(withId(R.id.lens_camera_button));
        assertLocationBarButtonsAre(R.id.lens_camera_button, R.id.mic_button);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_lenButtonVisibilityOnLocationBarOnDefaultSearchEngineChange() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(false).when(mLensController).isLensEnabled(any());
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        // Test when search engine is not Google.
        mActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });
        waitForNoView(withId(R.id.lens_camera_button));
        ViewUtils.waitForVisibleView(withId(R.id.mic_button));
        assertLocationBarButtonsAre(R.id.mic_button);

        // Test when search engine is Google.
        doReturn(true).when(mLensController).isLensEnabled(any());
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        updateLocationBar();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });
        ViewUtils.waitForVisibleView(withId(R.id.lens_camera_button));
        assertLocationBarButtonsAre(R.id.lens_camera_button, R.id.mic_button);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
    })
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
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
    })
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
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
    })
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
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
    })
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

        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

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
                    mLocationBarCoordinator.setOmniboxEditingText(url);
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
        Assert.assertTrue(mLocationBarMediator.getHandleBackPressChangedSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getOnBackPressedDispatcher().onBackPressed());
        Assert.assertFalse(mLocationBarMediator.getHandleBackPressChangedSupplier().get());
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

    private void showOptionalButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ButtonData.ButtonSpec spec =
                            new ButtonData.ButtonSpec.Builder(
                                            new ColorDrawable(Color.RED), "test", true)
                                    .setButtonVariant(AdaptiveToolbarButtonVariant.SHARE)
                                    .build();
                    ButtonData buttonData =
                            new ButtonData() {
                                @Override
                                public boolean canShow() {
                                    return true;
                                }

                                @Override
                                public boolean isEnabled() {
                                    return true;
                                }

                                @Override
                                public boolean shouldShowTextBubble() {
                                    return false;
                                }

                                @Override
                                public ButtonSpec getButtonSpec() {
                                    return spec;
                                }
                            };
                    mLocationBarCoordinator.updateOptionalButton(buttonData);
                });

        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.optional_button), isDescendantOfA(withId(R.id.location_bar))));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOptionalButton() {
        startActivityNormally();

        showOptionalButton();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarCoordinator.hideOptionalButton();
                });

        waitForNoView(
                allOf(withId(R.id.optional_button), isDescendantOfA(withId(R.id.location_bar))));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOptionalButton_HiddenWhenUrlFocused() {
        startActivityNormally();

        showOptionalButton();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        waitForNoView(
                allOf(withId(R.id.optional_button), isDescendantOfA(withId(R.id.location_bar))));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOptionalButton_HiddenOnNtp() {
        startActivityNormally();

        showOptionalButton();

        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());

        waitForNoView(
                allOf(withId(R.id.optional_button), isDescendantOfA(withId(R.id.location_bar))));
    }
}
