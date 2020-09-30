// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import android.view.View;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Unit tests for {@link LocationBarLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarLayoutTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String SEARCH_TERMS = "machine learning";
    private static final String SEARCH_TERMS_URL = "testing.com";
    private static final String GOOGLE_SRP_URL = "https://www.google.com/search?q=machine+learning";

    private static final String VERBOSE_URL = "https://www.suchwowveryyes.edu";
    private static final String TRIMMED_URL = "suchwowveryyes.edu";

    private static final String GOOGLE_URL = "https://www.google.com";
    private static final String YAHOO_URL = "https://www.yahoo.com";

    private TestLocationBarModel mTestLocationBarModel;

    private class TestLocationBarModel extends LocationBarModel {
        private String mCurrentUrl;
        private String mEditingText;
        private String mDisplayText;
        private Integer mSecurityLevel;

        public TestLocationBarModel() {
            super(ContextUtils.getApplicationContext());
            initializeWithNative();
        }

        void setCurrentUrl(String url) {
            mCurrentUrl = url;
        }

        void setSecurityLevel(@ConnectionSecurityLevel int securityLevel) {
            mSecurityLevel = securityLevel;
        }

        @Override
        public String getCurrentUrl() {
            if (mCurrentUrl == null) return super.getCurrentUrl();
            return mCurrentUrl;
        }

        @Override
        @ConnectionSecurityLevel
        public int getSecurityLevel() {
            if (mSecurityLevel == null) return super.getSecurityLevel();
            return mSecurityLevel;
        }

        @Override
        public UrlBarData getUrlBarData() {
            UrlBarData urlBarData = super.getUrlBarData();
            CharSequence displayText = mDisplayText == null ? urlBarData.displayText : mDisplayText;
            String editingText = mEditingText == null ? urlBarData.editingText : mEditingText;
            return UrlBarData.forUrlAndText(getCurrentUrl(), displayText.toString(), editingText);
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        setupModelsForCurrentTab();
    }

    private void setupModelsForCurrentTab() {
        mTestLocationBarModel = new TestLocationBarModel();
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mTestLocationBarModel.setTab(tab, tab.isIncognito());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> getLocationBar().setToolbarDataProvider(mTestLocationBarModel));
    }

    private void setUrlToPageUrl(LocationBarLayout locationBar) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { getLocationBar().updateLoadingState(true); });
    }

    private String getUrlText(UrlBar urlBar) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.getText().toString());
        } catch (ExecutionException ex) {
            throw new RuntimeException(
                    "Failed to get the UrlBar's text! Exception below:\n" + ex.toString());
        }
    }

    private UrlBar getUrlBar() {
        return (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
    }

    private LocationBarLayout getLocationBar() {
        return (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    private ImageButton getDeleteButton() {
        return mActivityTestRule.getActivity().findViewById(R.id.delete_button);
    }

    private ImageButton getMicButton() {
        return mActivityTestRule.getActivity().findViewById(R.id.mic_button);
    }

    private View getStatusIconView() {
        return mActivityTestRule.getActivity().findViewById(R.id.location_bar_status_icon);
    }

    private void setUrlBarTextAndFocus(String text) {
        final UrlBar urlBar = getUrlBar();
        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.requestFocus(); });
        CriteriaHelper.pollUiThread(() -> urlBar.hasFocus());

        try {
            TestThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
                @Override
                public Void call() throws InterruptedException {
                    mActivityTestRule.typeInOmnibox(text, false);
                    return null;
                }
            });
        } catch (ExecutionException e) {
            throw new RuntimeException("Failed to type \"" + text + "\" into the omnibox!");
        }
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotShowingVoiceSearchButtonIfUrlBarContainsText() throws ExecutionException {
        // When there is text, the delete button should be visible.
        setUrlBarTextAndFocus("testing");

        onView(withId(R.id.delete_button)).check(matches(isDisplayed()));
        onView(withId(R.id.mic_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "Flaky. See https://crbug.com/1091646")
    public void testShowingVoiceSearchButtonIfUrlBarIsEmpty() throws ExecutionException {
        // When there's no text, the mic button should be visible.
        setUrlBarTextAndFocus("");

        onView(withId(R.id.mic_button)).check(matches(isDisplayed()));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testDeleteButton() throws ExecutionException {
        setUrlBarTextAndFocus("testing");
        Assert.assertEquals(getDeleteButton().getVisibility(), VISIBLE);
        ClickUtils.clickButton(getDeleteButton());
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(getDeleteButton().getVisibility(), Matchers.not(VISIBLE));
        });
        Assert.assertEquals("", getUrlText(getUrlBar()));
    }

    @Test
    @SmallTest
    public void testEditingTextShownOnFocus() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestLocationBarModel.setCurrentUrl(VERBOSE_URL);
        mTestLocationBarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        mTestLocationBarModel.mDisplayText = TRIMMED_URL;
        mTestLocationBarModel.mEditingText = VERBOSE_URL;
        setUrlToPageUrl(locationBar);

        Assert.assertEquals(TRIMMED_URL, getUrlText(urlBar));

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.requestFocus(); });

        Assert.assertEquals(VERBOSE_URL, getUrlText(urlBar));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, urlBar.getSelectionStart());
            Assert.assertEquals(VERBOSE_URL.length(), urlBar.getSelectionEnd());
        });
    }

    /*
     *  Search engine logo tests.
     */

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    @Feature({"OmniboxSearchEngineLogo"})
    public void testOmniboxSearchEngineLogo_unfocusedOnSRP() {
        final LocationBarLayout locationBar = getLocationBar();
        final ImageView iconView = (ImageView) locationBar.getSecurityIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        mTestLocationBarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestLocationBarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        onView(withId(R.id.location_bar_status)).check((view, e) -> {
            Assert.assertEquals(iconView.getVisibility(), VISIBLE);
            Assert.assertEquals(R.drawable.omnibox_https_valid,
                    locationBar.getStatusCoordinatorForTesting()
                            .getSecurityIconResourceIdForTesting());
        });
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    @Feature({"OmniboxSearchEngineLogo"})
    public void testOmniboxSearchEngineLogo_focusedOnSRP() throws ExecutionException {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        mTestLocationBarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestLocationBarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);
        setUrlBarTextAndFocus("");

        onView(withId(R.id.location_bar_status)).check((view, e) -> {
            Assert.assertEquals(statusIconView.getVisibility(), VISIBLE);
            Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                    locationBar.getStatusCoordinatorForTesting()
                            .getSecurityIconResourceIdForTesting());
        });
    }

    /*
     * End-to-end scenarios for search engine logo testing.
     */

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntp() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.NTP_URL, /* incognito= */ false, locationBar);

        onView(withId(R.id.location_bar_status))
                .check((view, e) -> Assert.assertEquals(GONE, statusIconView.getVisibility()));

        // Focus the UrlBar and check that the status view is VISIBLE.
        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntpIncognito() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.NTP_URL, /* incognito= */ true, locationBar);

        // The status view should be hidden in both focused/unfocused while incognito.
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals("Should be gone when unfocused", GONE,
                                        statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals("Should be gone when focused", GONE,
                                        statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntpToSite() throws ExecutionException {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        Tab tab = loadUrlInNewTabAndUpdateModels(
                UrlConstants.NTP_URL, /* incognito= */ false, locationBar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadUrl(new LoadUrlParams(UrlConstants.ABOUT_URL)));

        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_site() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.ABOUT_URL, /* incognito= */ false, locationBar);

        // The status view should be hidden in both focused/unfocused while incognito.
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals(
                                        "Status should be visible when unfocused on a site.",
                                        VISIBLE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals(
                                        "Status should be visible when focused on a site.", VISIBLE,
                                        statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_siteIncognito() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.ABOUT_URL, /* incognito= */ true, locationBar);

        // The status view should be hidden in both focused/unfocused while incognito.
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(GONE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(GONE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_siteToSite() throws ExecutionException {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithGoogle(locationBar);
        Tab tab = loadUrlInNewTabAndUpdateModels(
                UrlConstants.CHROME_BLANK_URL, /* incognito= */ false, locationBar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadUrl(new LoadUrlParams(UrlConstants.ABOUT_URL)));

        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntp_nonGoogle() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithYahoo(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.NTP_URL, /* incognito= */ false, locationBar);

        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(GONE, statusIconView.getVisibility()));

        // Focus the UrlBar and check that the status view is VISIBLE.
        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntpIncognito_nonGoogle() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithYahoo(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.NTP_URL, /* incognito= */ true, locationBar);

        // The status view should be hidden in both focused/unfocused while incognito.
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals("Should be gone when unfocused", GONE,
                                        statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals("Should be gone when focused", GONE,
                                        statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_ntpToSite_nonGoogle() throws ExecutionException {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithYahoo(locationBar);
        Tab tab = loadUrlInNewTabAndUpdateModels(
                UrlConstants.NTP_URL, /* incognito= */ false, locationBar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadUrl(new LoadUrlParams(UrlConstants.ABOUT_URL)));

        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_site_nonGoogle() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithYahoo(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.ABOUT_URL, /* incognito= */ false, locationBar);

        // The status view should be hidden in both focused/unfocused while incognito.
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals(
                                        "Status should be visible when unfocused on a site.",
                                        VISIBLE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e)
                                -> Assert.assertEquals(
                                        "Status should be visible when focused on a site.", VISIBLE,
                                        statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_siteIncognito_nonGoogle() {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithYahoo(locationBar);
        loadUrlInNewTabAndUpdateModels(UrlConstants.ABOUT_URL, /* incognito= */ true, locationBar);

        // The status view should be hidden in both focused/unfocused while incognito.
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(GONE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(GONE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void testOmniboxSearchEngineLogo_siteToSite_nonGoogle() throws ExecutionException {
        final LocationBarLayout locationBar = getLocationBar();
        final View statusIconView = getStatusIconView();
        updateSearchEngineLogoWithYahoo(locationBar);
        Tab tab = loadUrlInNewTabAndUpdateModels(
                UrlConstants.CHROME_BLANK_URL, /* incognito= */ false, locationBar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadUrl(new LoadUrlParams(UrlConstants.ABOUT_URL)));

        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));

        setUrlBarTextAndFocus("");
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertEquals(VISIBLE, statusIconView.getVisibility()));
    }

    @Test
    @SmallTest
    public void testSetUrlBarFocus() {
        final LocationBarLayout locationBar = getLocationBar();

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBar.setUrlBarFocus(
                    true, SEARCH_TERMS_URL, LocationBar.OmniboxFocusReason.FAKE_BOX_LONG_PRESS);
        });
        Assert.assertTrue(locationBar.isUrlBarFocused());
        Assert.assertTrue(locationBar.didFocusUrlFromFakebox());
        Assert.assertEquals(SEARCH_TERMS_URL, getUrlText(getUrlBar()));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBar.setUrlBarFocus(
                    true, SEARCH_TERMS, LocationBar.OmniboxFocusReason.SEARCH_QUERY);
        });
        Assert.assertTrue(locationBar.isUrlBarFocused());
        Assert.assertTrue(locationBar.didFocusUrlFromFakebox());
        Assert.assertEquals(SEARCH_TERMS, getUrlText(getUrlBar()));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBar.setUrlBarFocus(false, null, LocationBar.OmniboxFocusReason.UNFOCUS);
        });
        Assert.assertFalse(locationBar.isUrlBarFocused());
        Assert.assertFalse(locationBar.didFocusUrlFromFakebox());
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBar.setUrlBarFocus(true, null, LocationBar.OmniboxFocusReason.OMNIBOX_TAP);
        });
        Assert.assertTrue(locationBar.isUrlBarFocused());
        Assert.assertFalse(locationBar.didFocusUrlFromFakebox());
        Assert.assertEquals(
                2, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));
    }

    /**
     * Test for checking whether soft input model switches with focus.
     */
    @Test
    @MediumTest
    @Feature("Omnibox")
    public void testFocusChangingSoftInputMode() {
        final UrlBar urlBar = getUrlBar();

        Callable<Integer> softInputModeCallable = () -> {
            return mActivityTestRule.getActivity().getWindow().getAttributes().softInputMode;
        };
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        CriteriaHelper.pollUiThread(urlBar::hasFocus);
        CriteriaHelper.pollUiThread(() -> {
            int inputMode =
                    mActivityTestRule.getActivity().getWindow().getAttributes().softInputMode;
            Criteria.checkThat(inputMode, is(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN));
        });

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        CriteriaHelper.pollUiThread(() -> !urlBar.hasFocus());
        CriteriaHelper.pollUiThread(() -> {
            int inputMode =
                    mActivityTestRule.getActivity().getWindow().getAttributes().softInputMode;
            Criteria.checkThat(inputMode, is(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE));
        });
    }

    /** Test NPE when focus callback triggers after LocationBarLayout is destroyed. */
    @Test
    @MediumTest
    @Feature("Omnibox")
    public void testAutocompleteCoordinatorNpeWhenFocusedAfterDestroy() {
        LocationBarLayout locationBar = getLocationBar();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBar.destroy();
            locationBar.onUrlFocusChange(false);
        });
    }

    /** Load a new URL and also update the locaiton bar models. */
    private Tab loadUrlInNewTabAndUpdateModels(
            String url, boolean incognito, LocationBarLayout locationBar) {
        Tab tab = mActivityTestRule.loadUrlInNewTab(url, incognito);
        setupModelsForCurrentTab();
        setUrlToPageUrl(locationBar);
        TestThreadUtils.runOnUiThreadBlocking(() -> { locationBar.updateVisualsForState(); });
        return tab;
    }

    /** Performs an update on {@link LocationBar} to show the Google logo. */
    private void updateSearchEngineLogoWithGoogle(LocationBarLayout locationBar) {
        updateSearchEngineLogo(locationBar, GOOGLE_URL);
    }

    /** Performs an update on {@link LocationBar} to show the Yahoo logo. */
    private void updateSearchEngineLogoWithYahoo(LocationBarLayout locationBar) {
        updateSearchEngineLogo(locationBar, YAHOO_URL);
    }

    private void updateSearchEngineLogo(LocationBarLayout locationBar, String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBar.updateSearchEngineStatusIcon(
                    /* shouldShow= */ true, /* isGoogle= */ url.equals(GOOGLE_URL), url);
        });
    }
}
