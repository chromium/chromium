// Copyright 2020 The Chromium Authors. All rights reserved.
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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Instrumentation tests for the LocationBar component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1", "ignore-certificate-errors"})
public class LocationBarTest {
    private static final String TEST_QUERY = "testing query";
    private static final List<String> TEST_PARAMS = Arrays.asList("foo=bar");
    private static final String HOSTNAME = "suchwowveryyes.edu";
    private static final String GOOGLE_URL = "https://www.google.com";
    private static final String NON_GOOGLE_URL = "https://www.notgoogle.com";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    private TemplateUrl mGoogleSearchEngine;
    @Mock
    private TemplateUrl mNonGoogleSearchEngine;
    @Mock
    private LocaleManager mLocaleManager;
    @Mock
    private VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock
    private SearchEngineLogoUtils mSearchEngineLogoUtils;

    ChromeTabbedActivity mActivity;
    UrlBar mUrlBar;
    LocationBarCoordinator mLocationBarCoordinator;
    LocationBarMediator mLocationBarMediator;
    String mSearchUrl;

    @Before
    public void setUp() throws InterruptedException {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LocaleManager.setInstanceForTest(mLocaleManager);
        SearchEngineLogoUtils.setInstanceForTesting(mSearchEngineLogoUtils);
    }

    @After
    public void tearDown() {
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        LocaleManager.setInstanceForTest(null);
        SearchEngineLogoUtils.setInstanceForTesting(null);
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
            mActivity.getLifecycleDispatcher().register(new InflationObserver() {
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().startDelayedNativeInitializationForTests();
        });
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    private void doPostActivitySetup(ChromeActivity activity) {
        mUrlBar = activity.findViewById(org.chromium.chrome.R.id.url_bar);
        mLocationBarCoordinator = ((LocationBarCoordinator) activity.getToolbarManager()
                                           .getToolbarLayoutForTesting()
                                           .getLocationBar());
        mLocationBarMediator = mLocationBarCoordinator.getMediatorForTesting();
        mSearchUrl = mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/search");
        mLocationBarCoordinator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
    }

    private void setupSearchEngineLogo(String url) {
        boolean isGoogle = url.equals(GOOGLE_URL);
        doReturn(isGoogle).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(url).when(mSearchEngineLogoUtils).getSearchLogoUrl(mTemplateUrlService);
        doReturn(true)
                .when(mSearchEngineLogoUtils)
                .shouldShowSearchEngineLogo(/* incognito= */ false);
        doReturn(isGoogle ? mGoogleSearchEngine : mNonGoogleSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();

        // Return null to fallback to the search loupe behavior.
        Answer bitmapAnswer = (invocation) -> {
            ((Callback<Bitmap>) invocation.getArgument(2)).onResult(null);
            return null;
        };
        doAnswer(bitmapAnswer)
                .when(mSearchEngineLogoUtils)
                .getSearchEngineLogoFavicon(any(), any(), any(), any());
    }

    @Test
    @MediumTest
    public void testSetSearchQueryFocusesUrlBar() {
        startActivityNormally();
        final String query = "testing query";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.setSearchQuery(query);
            Assert.assertEquals(query, mUrlBar.getTextWithoutAutocomplete());
            Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
        });

        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
    }

    @Test
    @MediumTest
    public void testSetSearchQueryFocusesUrlBar_preNative() {
        startActivityWithDeferredNativeInitialization();
        final String query = "testing query";

        TestThreadUtils.runOnUiThreadBlocking(() -> mLocationBarMediator.setSearchQuery(query));

        triggerAndWaitForDeferredNativeInitialization();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(query, mUrlBar.getTextWithoutAutocomplete());
            Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
        });

        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
    }

    @Test
    @MediumTest
    public void testPerformSearchQuery() {
        startActivityNormally();
        final List<String> params = Arrays.asList("foo=bar");
        doReturn(mSearchUrl)
                .when(mTemplateUrlService)
                .getUrlForSearchQuery(TEST_QUERY, TEST_PARAMS);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.performSearchQuery(TEST_QUERY, TEST_PARAMS));

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), mSearchUrl);
    }

    @Test
    @MediumTest
    public void testPerformSearchQuery_emptyUrl() {
        startActivityNormally();
        doReturn("").when(mTemplateUrlService).getUrlForSearchQuery(TEST_QUERY, TEST_PARAMS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.performSearchQuery(TEST_QUERY, TEST_PARAMS);
            Assert.assertEquals(TEST_QUERY, mUrlBar.getTextWithoutAutocomplete());
        });
    }

    @Test
    @MediumTest
    public void testOnConfigurationChanged() {
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.showUrlBarCursorWithoutFocusAnimations();
            Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
        });

        Configuration configuration = mActivity.getSavedConfigurationForTesting();
        configuration.keyboard = Configuration.KEYBOARD_12KEY;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.onConfigurationChanged(configuration);
            Assert.assertFalse(mLocationBarMediator.isUrlBarFocused());
        });
    }

    @Test
    @MediumTest
    public void testTemplateUrlServiceChange() throws InterruptedException {
        doReturn(false).when(mLocaleManager).needToCheckForSearchEnginePromo();
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.onTemplateURLServiceChanged());

        mActivityTestRule.typeInOmnibox("", true);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mLocationBarCoordinator.getStatusCoordinatorForTesting()
                        .getSecurityIconResourceIdForTesting());

        setupSearchEngineLogo(NON_GOOGLE_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.onTemplateURLServiceChanged());
        Assert.assertEquals(R.drawable.ic_search,
                mLocationBarCoordinator.getStatusCoordinatorForTesting()
                        .getSecurityIconResourceIdForTesting());
    }

    @Test
    @MediumTest
    public void testPostDestroyFocusLogic() {
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivity.finish(); });

        CriteriaHelper.pollUiThread(
                () -> mActivity.getLifecycle().getCurrentState().equals(Lifecycle.State.DESTROYED));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.setUrlFocusChangeInProgress(false);
            mLocationBarMediator.finishUrlFocusChange(true, true);
        });
    }

    @Test
    @MediumTest
    public void testEditingText() {
        startActivityNormally();
        String url = mActivityTestRule.getEmbeddedTestServerRule().getServer().getURLWithHostName(
                HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mUrlBar.getText().toString().startsWith(HOSTNAME));
            mUrlBar.requestFocus();
            Assert.assertEquals("", mUrlBar.getText().toString());
            mLocationBarCoordinator.setOmniboxEditingText(url);
            Assert.assertEquals(url, mUrlBar.getText().toString());
            Assert.assertEquals(url.length(), mUrlBar.getSelectionStart());
            Assert.assertEquals(url.length(), mUrlBar.getSelectionEnd());
        });
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testFocusLogic_buttonVisibilityPhone() {
        startActivityNormally();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        String url = mActivityTestRule.getEmbeddedTestServerRule().getServer().getURLWithHostName(
                HOSTNAME, "/");
        mActivityTestRule.loadUrl(url);

        onView(withId(R.id.url_action_container))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mUrlBar.requestFocus(); });

        ViewUtils.waitForView(allOf(withId(R.id.url_action_container),
                withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarCoordinator.setOmniboxEditingText(url); });

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mUrlBar.clearFocus(); });

        ViewUtils.waitForView(allOf(withId(R.id.url_action_container),
                withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    public void testFocusLogic_buttonVisibilityTablet() {
        startActivityNormally();
        String url = mActivityTestRule.getEmbeddedTestServerRule().getServer().getURLWithHostName(
                HOSTNAME, "/");
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

        TestThreadUtils.runOnUiThreadBlocking(() -> { mUrlBar.requestFocus(); });

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarCoordinator.setOmniboxEditingText(url); });

        onView(withId(R.id.mic_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        onView(withId(R.id.delete_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
    @DisabledTest(message = "Flaky test: https://crbug.com/1169250")
    public void testFocusLogic_keyboardVisibility() {
        startActivityNormally();
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, false);

        TestThreadUtils.runOnUiThreadBlocking(() -> { mUrlBar.requestFocus(); });
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> { mUrlBar.clearFocus(); });
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, false);
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarMediator.updateSearchEngineStatusIcon(); });

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP_nonGoogleSearchEngine() {
        setupSearchEngineLogo(NON_GOOGLE_URL);
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarMediator.updateSearchEngineStatusIcon(); });

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP_incognito() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarMediator.updateSearchEngineStatusIcon(); });

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ true);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    public void testOmniboxSearchEngineLogo_focusedOnSRP() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarMediator.updateSearchEngineStatusIcon(); });

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mUrlBar.requestFocus(); });
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntpToSite() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarMediator.updateSearchEngineStatusIcon(); });

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(not(isDisplayed())));

        mActivityTestRule.loadUrl(UrlConstants.ABOUT_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_siteToSite() {
        setupSearchEngineLogo(GOOGLE_URL);
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mLocationBarMediator.updateSearchEngineStatusIcon(); });

        mActivityTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));

        mActivityTestRule.loadUrl(UrlConstants.ABOUT_URL);
        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
    }
}
