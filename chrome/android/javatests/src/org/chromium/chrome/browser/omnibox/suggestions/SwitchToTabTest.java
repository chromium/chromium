// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.moveActivityToFront;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.PendingIntent;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.List;

/**
 * Tests of the Switch To Tab feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SwitchToTabTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    private static final int INVALID_INDEX = -1;
    private static final long SEARCH_ACTIVITY_MAX_TIME_TO_POLL = 10000L;

    private EmbeddedTestServer mTestServer;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Type the |text| into |activity|'s url_bar.
     *
     * @param activity The Activity which url_bar is in.
     * @param text The text will be typed into url_bar.
     */
    private void typeInOmnibox(Activity activity, String text) throws InterruptedException {
        final UrlBar urlBar = activity.findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        mOmnibox.requestFocus();

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.setText(text); });
    }

    /**
     * Type the |text| into |activity|'s URL bar, and wait for switch to tab suggestion shows up.
     *
     * @param activity The Activity which URL bar is in.
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param tab The tab will be switched to.
     */
    private void typeAndClickMatchingTabMatchSuggestion(Activity activity,
            LocationBarLayout locationBarLayout, Tab tab) throws InterruptedException {
        typeInOmnibox(activity, ChromeTabUtils.getTitleOnUiThread(tab));

        mOmnibox.checkSuggestionsShown();
        // waitForOmniboxSuggestions only wait until one suggestion shows up, we need to wait util
        // autocomplete return more suggestions.
        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion =
                    findTabMatchOmniboxSuggestion(locationBarLayout, tab);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());

            OmniboxSuggestionsDropdown suggestionsDropdown =
                    locationBarLayout.getAutocompleteCoordinator().getSuggestionsDropdownForTest();

            // Make sure data populated to UI
            int index = findIndexOfTabMatchSuggestionView(suggestionsDropdown, matchSuggestion);
            Criteria.checkThat(index, Matchers.not(INVALID_INDEX));

            try {
                clickSuggestionActionAt(suggestionsDropdown, index);
            } catch (InterruptedException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
        }, SEARCH_ACTIVITY_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Find the switch to tab suggestion which suggests the |tab|, and return the
     * suggestion. This method needs to run on the UI thread.
     *
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param tab The tab which the AutocompleteMatch should suggest.
     * @return The suggestion which suggests the |tab|.
     */
    private AutocompleteMatch findTabMatchOmniboxSuggestion(
            LocationBarLayout locationBarLayout, Tab tab) {
        ThreadUtils.assertOnUiThread();

        AutocompleteCoordinator coordinator = locationBarLayout.getAutocompleteCoordinator();
        // Find the first matching suggestion.
        for (int i = 0; i < coordinator.getSuggestionCount(); ++i) {
            AutocompleteMatch suggestion = coordinator.getSuggestionAt(i);
            if (suggestion != null && suggestion.hasTabMatch()
                    && TextUtils.equals(
                            suggestion.getDescription(), ChromeTabUtils.getTitleOnUiThread(tab))
                    && TextUtils.equals(suggestion.getUrl().getSpec(), tab.getUrl().getSpec())) {
                return suggestion;
            }
        }
        return null;
    }

    /**
     * Find the index of the tab match suggestion in OmniboxSuggestionsDropdown. This method needs
     * to run on the UI thread.
     *
     * @param suggestionsDropdown The OmniboxSuggestionsDropdown contains all the suggestions.
     * @param suggestion The AutocompleteMatch we are looking for in the view.
     * @return The matching suggestion's index.
     */
    private int findIndexOfTabMatchSuggestionView(
            OmniboxSuggestionsDropdown suggestionsDropdown, AutocompleteMatch suggestion) {
        ThreadUtils.assertOnUiThread();

        ViewGroup viewGroup = suggestionsDropdown.getViewGroup();
        if (viewGroup == null) {
            return INVALID_INDEX;
        }

        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            BaseSuggestionView baseSuggestionView = null;
            try {
                baseSuggestionView = (BaseSuggestionView) viewGroup.getChildAt(i);
            } catch (ClassCastException e) {
                continue;
            }

            if (baseSuggestionView == null) {
                continue;
            }

            TextView line1 = baseSuggestionView.findViewById(R.id.line_1);
            TextView line2 = baseSuggestionView.findViewById(R.id.line_2);
            if (line1 == null || line2 == null
                    || !TextUtils.equals(suggestion.getDescription(), line1.getText())
                    || !TextUtils.equals(suggestion.getDisplayText(), line2.getText())) {
                continue;
            }

            List<ImageView> buttonsList = baseSuggestionView.getActionButtons();
            if (buttonsList != null && buttonsList.size() == 1
                    && TextUtils.equals(baseSuggestionView.getResources().getString(
                                                R.string.accessibility_omnibox_switch_to_tab),
                            buttonsList.get(0).getContentDescription())) {
                return i;
            }
        }

        return INVALID_INDEX;
    }

    /**
     * Find the |index|th suggestion in |suggestionsDropdown| and click it.
     *
     * @param suggestionsDropdown The omnibox suggestion's dropdown list.
     * @param index The index of the suggestion tied to click.
     */
    private void clickSuggestionActionAt(OmniboxSuggestionsDropdown suggestionsDropdown, int index)
            throws InterruptedException {
        ViewGroup viewGroup = suggestionsDropdown.getViewGroup();
        BaseSuggestionView baseSuggestionView = (BaseSuggestionView) viewGroup.getChildAt(index);
        Assert.assertNotNull("Null suggestion for index: " + index, baseSuggestionView);

        List<ImageView> buttonsList = baseSuggestionView.getActionButtons();
        Assert.assertNotNull(buttonsList);
        Assert.assertEquals(buttonsList.size(), 1);
        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), buttonsList.get(0));
    }

    /**
     * Launch the SearchActivity.
     */
    private SearchActivity startSearchActivity() {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Context context = instrumentation.getContext();

        ActivityMonitor searchMonitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(searchMonitor);

        // Fire the Intent to start up the SearchActivity.
        try {
            SearchWidgetProvider.createIntent(context, false).send();
        } catch (PendingIntent.CanceledException e) {
            Assert.assertTrue("Intent canceled", false);
        }

        Activity searchActivity = instrumentation.waitForMonitorWithTimeout(
                searchMonitor, SEARCH_ACTIVITY_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
        instrumentation.removeMonitor(searchMonitor);
        mOmnibox = new OmniboxTestUtils(searchActivity);
        return (SearchActivity) searchActivity;
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"disable-features=OmniboxUpdateResultDebounce"})
    public void testSwitchToTabSuggestion() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        final Tab aboutTab = mActivityTestRule.loadUrlInNewTab(testHttpsUrl1);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeAndClickMatchingTabMatchSuggestion(
                mActivityTestRule.getActivity(), locationBarLayout, aboutTab);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab, Matchers.is(aboutTab));
            Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(testHttpsUrl1));
        });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    @DisabledTest(message = "https://crbug.com/1291136")
    public void testSwitchToTabSuggestionWhenIncognitoTabOnTop() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);
        final Tab aboutTab = mActivityTestRule.loadUrlInNewTab(testHttpsUrl1);

        // Move "about.html" page to cta2 and create an incognito tab on top of "about.html".
        final ChromeTabbedActivity cta1 = mActivityTestRule.getActivity();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta1,
                R.id.move_to_other_window_menu_id);
        final ChromeTabbedActivity2 cta2 = waitForSecondChromeTabbedActivity();
        ChromeActivityTestRule.waitForActivityNativeInitializationComplete(cta2);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta2,
                true /*incognito*/, false /*waitForNtpLoad*/);
        moveActivityToFront(cta1);

        // Switch back to cta1, and try to switch to "about.html" in cta2.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) cta1.findViewById(R.id.location_bar);
        typeAndClickMatchingTabMatchSuggestion(cta1, locationBarLayout, aboutTab);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = cta2.getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab, Matchers.is(aboutTab));
            Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(testHttpsUrl1));
        });
    }

    @Test
    @MediumTest
    public void testNoSwitchToIncognitoTabFromNormalModel() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        // Open the url trying to match in incognito mode.
        final Tab aboutTab = mActivityTestRule.loadUrlInNewTab(testHttpsUrl1, true);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        // trying to match incognito tab.
        mActivityTestRule.typeInOmnibox("about", false);
        mOmnibox.checkSuggestionsShown();

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion =
                    findTabMatchOmniboxSuggestion(locationBarLayout, aboutTab);
            Criteria.checkThat(matchSuggestion, Matchers.nullValue());
        });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"disable-features=OmniboxUpdateResultDebounce"})
    public void testSwitchToTabInSearchActivity() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String testHttpsUrl1 = mTestServer.getURL("/chrome/test/data/android/about.html");
        final String testHttpsUrl2 = mTestServer.getURL("/chrome/test/data/android/ok.txt");
        final String testHttpsUrl3 = mTestServer.getURL("/chrome/test/data/android/test.html");
        final Tab aboutTab = mActivityTestRule.loadUrlInNewTab(testHttpsUrl1);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mActivityTestRule.loadUrlInNewTab(testHttpsUrl3);
        Assert.assertNotEquals(mActivityTestRule.getActivity().getActivityTab(), aboutTab);

        // Send Chrome to the background so Launch Cause Metrics are gathered (and this is more
        // realistic).
        ChromeApplicationTestUtils.fireHomeScreenIntent(mActivityTestRule.getActivity());

        final SearchActivity searchActivity = startSearchActivity();
        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());

            // Make sure chrome fully in background.
            Criteria.checkThat(tab.getWindowAndroid().getActivityState(),
                    Matchers.isOneOf(ActivityState.STOPPED, ActivityState.DESTROYED));
        }, SEARCH_ACTIVITY_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);

        final LocationBarLayout locationBarLayout =
                (LocationBarLayout) searchActivity.findViewById(R.id.search_location_bar);
        typeAndClickMatchingTabMatchSuggestion(searchActivity, locationBarLayout, aboutTab);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab, Matchers.is(aboutTab));
            Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(testHttpsUrl1));
            // Make sure tab is loaded and in foreground.
            Criteria.checkThat(
                    tab.getWindowAndroid().getActivityState(), Matchers.is(ActivityState.RESUMED));
            Assert.assertEquals(tab, aboutTab);
            Criteria.checkThat(RecordHistogram.getHistogramValueCountForTesting(
                                       LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                                       LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET),
                    Matchers.is(1));
        }, SEARCH_ACTIVITY_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }
}
