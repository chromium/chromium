// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import android.content.Intent;
import android.content.res.Configuration;
import android.view.View;
import android.widget.LinearLayout;

import androidx.lifecycle.Lifecycle;
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
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.KeyboardUtils;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

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
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mGoogleSearchEngine;
    @Mock private TemplateUrl mNonGoogleSearchEngine;
    @Mock private LensController mLensController;
    @Mock private LocaleManagerDelegate mLocaleManagerDelegate;
    @Mock private VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock private SearchEngineUtils mSearchEngineUtils;

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
                    doReturn(new StatusIconResource(null))
                            .when(mSearchEngineUtils)
                            .getSearchEngineLogo(anyInt());
                });
    }

    private void startActivityNormally() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        doPostActivitySetup(mActivity);
    }

    private void startActivityWithDeferredNativeInitialization() {
        CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        Intent intent = new Intent("about:blank");
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, "about:blank");
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
        if (!mActivity.isInitialLayoutInflationComplete()) {
            AtomicBoolean isInflated = new AtomicBoolean();
            mActivity
                    .getLifecycleDispatcher()
                    .register(
                            new InflationObserver() {
                                @Override
                                public void onPreInflationStartup() {}

                                @Override
                                public void onPostInflationStartup() {
                                    isInflated.set(true);
                                }
                            });
            CriteriaHelper.pollUiThread(isInflated::get);
        }
        doPostActivitySetup(mActivity);
    }

    private void triggerAndWaitForDeferredNativeInitialization() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().startDelayedNativeInitializationForTests();
                });
        mActivityTestRule.waitForActivityNativeInitializationComplete();
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

                    StatusIconResource logo =
                            new StatusIconResource(
                                    isGoogle
                                            ? R.drawable.ic_logo_googleg_20dp
                                            : R.drawable.ic_search,
                                    0);

                    doReturn(logo).when(mSearchEngineUtils).getSearchEngineLogo(anyInt());
                });
    }

    private void assertTheLastVisibleButtonInSearchBoxById(int id) {
        LinearLayout urlActionContainer =
                mActivityTestRule.getActivity().findViewById(R.id.url_action_container);

        for (int i = urlActionContainer.getChildCount() - 1; i >= 0; i--) {
            if (urlActionContainer.getChildAt(i).getVisibility() != View.VISIBLE) {
                continue;
            }
            Assert.assertEquals(urlActionContainer.getChildAt(i).getId(), id);
            break;
        }
    }

    private void updateLocationBar() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarMediator mediator = mLocationBarCoordinator.getMediatorForTesting();
                    mediator.onIncognitoStateChanged();
                    mediator.onPrimaryColorChanged();
                    mediator.onSecurityStateChanged();
                    mediator.onTemplateURLServiceChanged();
                    mediator.onUrlChanged();
                });
    }

    @Test
    @MediumTest
    public void testSetSearchQueryFocusesUrlBar() {
        startActivityNormally();
        final String query = "testing query";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarMediator.setSearchQuery(query);
                    Assert.assertEquals(query, mUrlBar.getTextWithoutAutocomplete());
                    Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
                });

        CriteriaHelper.pollUiThread(
                () -> mKeyboardDelegate.isKeyboardShowing(mUrlBar.getContext(), mUrlBar));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1470145")
    public void testSetSearchQueryFocusesUrlBar_preNative() {
        startActivityWithDeferredNativeInitialization();
        final String query = "testing query";

        ThreadUtils.runOnUiThreadBlocking(() -> mLocationBarMediator.setSearchQuery(query));

        triggerAndWaitForDeferredNativeInitialization();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mUrlBar.getTextWithoutAutocomplete(), Matchers.is(query));
                    Criteria.checkThat(mLocationBarMediator.isUrlBarFocused(), Matchers.is(true));
                });
    }

    @Test
    @MediumTest
    public void testPerformSearchQuery() {
        startActivityNormally();
        doReturn(mSearchUrl)
                .when(mTemplateUrlService)
                .getUrlForSearchQuery(TEST_QUERY, TEST_PARAMS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.performSearchQuery(TEST_QUERY, TEST_PARAMS));

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), mSearchUrl);
    }

    @Test
    @MediumTest
    public void testPerformSearchQuery_emptyUrl() {
        startActivityNormally();
        doReturn("").when(mTemplateUrlService).getUrlForSearchQuery(TEST_QUERY, TEST_PARAMS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarMediator.performSearchQuery(TEST_QUERY, TEST_PARAMS);
                    Assert.assertEquals(TEST_QUERY, mUrlBar.getTextWithoutAutocomplete());
                });
    }

    @Test
    @MediumTest
    public void testOnConfigurationChanged() {
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
        configuration.keyboard = Configuration.KEYBOARD_12KEY;

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
        testEditingText(
                /* expectRetainOmniboxOnFocus= */ OmniboxFeatures.shouldRetainOmniboxOnFocus());
    }

    @Test
    @MediumTest
    public void testEditingText_withRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        testEditingText(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    @MediumTest
    public void testEditingText_withRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        testEditingText(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void testEditingText(boolean expectRetainOmniboxOnFocus) {
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
                    if (expectRetainOmniboxOnFocus) {
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

        onView(withId(R.id.url_action_container))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        ViewUtils.waitForVisibleView(allOf(withId(R.id.url_action_container)));
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

        ViewUtils.waitForViewCheckingState(withId(R.id.url_action_container), ViewUtils.VIEW_GONE);
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

        onView(withId(R.id.url_action_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        ViewUtils.waitForVisibleView(withId(R.id.url_action_container));
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));
        assertTheLastVisibleButtonInSearchBoxById(R.id.mic_button);

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

        ViewUtils.waitForViewCheckingState(withId(R.id.url_action_container), ViewUtils.VIEW_GONE);
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

        onView(withId(R.id.url_action_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        ViewUtils.waitForVisibleView(withId(R.id.url_action_container));
        onView(withId(R.id.lens_camera_button)).check(matches(isDisplayed()));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));
        assertTheLastVisibleButtonInSearchBoxById(R.id.lens_camera_button);

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

        ViewUtils.waitForViewCheckingState(withId(R.id.url_action_container), ViewUtils.VIEW_GONE);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_lenButtonVisibilityOnNtpPhone_updatedOnceWhenNtpScrolled() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        ViewUtils.waitForVisibleView(withId(R.id.voice_search_button));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Mockito.reset(mVoiceRecognitionHandler);
                    doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

                    // Updating the fraction once should query voice search visibility.
                    mLocationBarMediator.setUrlFocusChangeFraction(.5f, .5f);
                    Mockito.verify(mVoiceRecognitionHandler).isVoiceSearchEnabled();

                    // Further updates to the fraction shouldn't trigger a button visibility update.
                    mLocationBarMediator.setUrlFocusChangeFraction(.6f, .6f);
                    Mockito.verify(mVoiceRecognitionHandler, Mockito.times(1))
                            .isVoiceSearchEnabled();
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
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
        ViewUtils.waitForViewCheckingState(withId(R.id.lens_camera_button), ViewUtils.VIEW_GONE);
        ViewUtils.waitForVisibleView(withId(R.id.mic_button));
        assertTheLastVisibleButtonInSearchBoxById(R.id.mic_button);

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
        assertTheLastVisibleButtonInSearchBoxById(R.id.lens_camera_button);
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
        ViewUtils.waitForViewCheckingState(withId(R.id.lens_camera_button), ViewUtils.VIEW_GONE);
        ViewUtils.waitForVisibleView(withId(R.id.mic_button));
        assertTheLastVisibleButtonInSearchBoxById(R.id.mic_button);

        // Test when search engine is Google.
        doReturn(true).when(mLensController).isLensEnabled(any());
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
        updateLocationBar();
        onView(withId(R.id.lens_camera_button)).check(matches(not(isDisplayed())));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });
        ViewUtils.waitForVisibleView(withId(R.id.lens_camera_button));
        assertTheLastVisibleButtonInSearchBoxById(R.id.lens_camera_button);
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
    @Restriction(DeviceFormFactor.TABLET)
    public void testFocusLogic_buttonVisibilityTablet() {
        testFocusLogic_buttonVisibilityTablet(
                /* expectRetainOmniboxOnFocus= */ OmniboxFeatures.shouldRetainOmniboxOnFocus());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
    })
    @Restriction(DeviceFormFactor.TABLET)
    public void testFocusLogic_buttonVisibilityTabletWithRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        testFocusLogic_buttonVisibilityTablet(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
    })
    @Restriction(DeviceFormFactor.TABLET)
    public void testFocusLogic_buttonVisibilityTabletWithRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        testFocusLogic_buttonVisibilityTablet(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void testFocusLogic_buttonVisibilityTablet(boolean expectRetainOmniboxOnFocus) {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        String url =
                mActivityTestRule
                        .getEmbeddedTestServerRule()
                        .getServer()
                        .getURLWithHostName(HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

        onView(withId(R.id.url_action_container))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.bookmark_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.save_offline_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.requestFocus();
                });

        if (expectRetainOmniboxOnFocus) {
            onView(withId(R.id.mic_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
            onView(withId(R.id.delete_button))
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        } else {
            onView(withId(R.id.mic_button))
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
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                    mLocationBarCoordinator.setShouldShowButtonsWhenUnfocusedForTablet(false);
                });

        onView(withId(R.id.bookmark_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.save_offline_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    @Test
    @MediumTest
    public void testFocusLogic_keyboardVisibility() {
        startActivityNormally();
        assertFalse(mKeyboardDelegate.isKeyboardShowing(mUrlBar.getContext(), mUrlBar));

        mOmnibox.requestFocus();
        mOmnibox.checkFocus(true);
        mOmnibox.clearFocus();
        mOmnibox.checkFocus(false);
    }

    /** Test that back press should make the omnibox unfocused. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_backPress() {
        startActivityNormally();

        mOmnibox.requestFocus();
        mOmnibox.checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mLocationBarMediator.backKeyPressed());
        mOmnibox.checkFocus(false);
    }

    /** Same with {@link #testFocusLogic_backPress()}, but with predictive back gesture enabled. */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    @Restriction(DeviceFormFactor.PHONE)
    public void testFocusLogic_backPress_Refactored() {
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

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP_nonGoogleSearchEngine() {
        setupSearchEngineLogo(NON_GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP_incognito() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ true);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testOmniboxSearchEngineLogo_focusedOnSRP() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mOmnibox.requestFocus();
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOmniboxSearchEngineLogo_ntpToSite() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
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
}
