// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.test.util.OmniboxTestUtils.buildSuggestionMap;

import android.annotation.SuppressLint;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v4.view.ViewCompat;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.params.SkipCommandLineParameterization;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.status.StatusViewCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinatorTestUtils;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionView;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsResult;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsResultBuilder;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestAutocompleteController;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestSuggestionResultsBuilder;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests of the Omnibox.
 *
 * TODO(yolandyan): Replace the ParameterizedCommandLineFlags with new JUnit4
 * parameterized framework once it supports Test Rule Parameterization.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// clang-format off
@ParameterizedCommandLineFlags({
  @Switches(),
  @Switches("disable-features=" + ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE),
})
// clang-format on
@SuppressLint("SetTextI18n")
public class OmniboxTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private void clearUrlBar() {
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.setText(""); });
    }

    private static final OnSuggestionsReceivedListener sEmptySuggestionListener =
            new OnSuggestionsReceivedListener() {
                @Override
                public void onSuggestionsReceived(
                        List<OmniboxSuggestion> suggestions, String inlineAutocompleteText) {}
            };

    /**
     * Sanity check of Omnibox.  The problem in http://b/5021723 would
     * cause this to fail (hang or crash).
     */
    @Test
    @EnormousTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testSimpleUse() throws InterruptedException {
        mActivityTestRule.typeInOmnibox("aaaaaaa", false);

        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        ChromeTabUtils.waitForTabPageLoadStart(
                mActivityTestRule.getActivity().getActivityTab(), new Runnable() {
                    @Override
                    public void run() {
                        final UrlBar urlBar =
                                (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
                        KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(),
                                urlBar, KeyEvent.KEYCODE_ENTER);
                    }
                }, 20L);
    }

    /**
     * Test for checking whether soft input model switches with focus.
     */
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testFocusChangingSoftInputMode() {
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return mActivityTestRule.getActivity()
                                .getWindow()
                                .getAttributes()
                                .softInputMode;
                    }
                }));

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return mActivityTestRule.getActivity()
                                .getWindow()
                                .getAttributes()
                                .softInputMode;
                    }
                }));
    }

    /**
     * Tests that focusing a url bar starts a zero suggest request.
     */
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testRequestZeroSuggestOnFocus() {
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.setText("http://www.example.com/"); });

        final TestAutocompleteController controller = new TestAutocompleteController(locationBar,
                sEmptySuggestionListener, new HashMap<String, List<SuggestionsResult>>());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutocompleteCoordinatorTestUtils.setAutocompleteController(
                    locationBar.getAutocompleteCoordinator(), controller);
        });
        Assert.assertEquals("Should not have any zero suggest requests yet", 0,
                controller.numZeroSuggestRequests());

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return controller.numZeroSuggestRequests();
            }
        }));

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertFalse(controller.isStartAutocompleteCalled());
    }

    /**
     * Tests that focusing a url bar starts a zero suggest request.
     */
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testRequestZeroSuggestAfterDelete() throws InterruptedException {
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final ImageButton deleteButton =
                (ImageButton) mActivityTestRule.getActivity().findViewById(R.id.delete_button);

        final TestAutocompleteController controller = new TestAutocompleteController(locationBar,
                sEmptySuggestionListener, new HashMap<String, List<SuggestionsResult>>());

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutocompleteCoordinatorTestUtils.setAutocompleteController(
                    locationBar.getAutocompleteCoordinator(), controller);
            urlBar.setText("g");
        });

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Should have drawn the delete button") {
                    @Override
                    public boolean isSatisfied() {
                        return deleteButton.getWidth() > 0;
                    }
                });

        // The click view below ends up clicking on the menu button underneath the delete button
        // for some time after the delete button appears. Wait for UI to settle down before
        // clicking.
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        TouchCommon.singleClickView(deleteButton);

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return controller.numZeroSuggestRequests();
            }
        }));
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testRequestZeroSuggestTypeAndBackspace() {
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        final TestAutocompleteController controller = new TestAutocompleteController(locationBar,
                sEmptySuggestionListener, new HashMap<String, List<SuggestionsResult>>());

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutocompleteCoordinatorTestUtils.setAutocompleteController(
                    locationBar.getAutocompleteCoordinator(), controller);
            urlBar.setText("g");
            urlBar.setSelection(1);
        });

        Assert.assertEquals("No calls to zero suggest yet", 0, controller.numZeroSuggestRequests());
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), urlBar, KeyEvent.KEYCODE_DEL);
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return controller.numZeroSuggestRequests();
            }
        }));
    }

    // Sanity check that no text is displayed in the omnibox when on the NTP page and that the hint
    // text is correct.
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testDefaultText() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        // Omnibox on NTP shows the hint text.
        Assert.assertNotNull(urlBar);
        Assert.assertEquals("Location bar has text.", "", urlBar.getText().toString());
        Assert.assertEquals("Location bar has incorrect hint.",
                mActivityTestRule.getActivity().getResources().getString(
                        R.string.search_or_type_web_address),
                urlBar.getHint().toString());

        // Type something in the omnibox.
        // Note that the TextView does not provide a way to test if the hint is showing, the API
        // documentation simply says it shows when the text is empty.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            urlBar.requestFocus();
            urlBar.setText("G");
        });
        Assert.assertEquals("Location bar should have text.", "G", urlBar.getText().toString());
    }

    @Test
    @MediumTest
    @Feature({"Omnibox", "Main"})
    @RetryOnFailure
    public void testAutoCompleteAndCorrectionLandscape()
            throws ExecutionException, InterruptedException {
        // Default orientation for tablets is landscape. Default for phones is portrait.
        int requestedOrientation = 1;
        if (mActivityTestRule.getActivity().isTablet()) {
            requestedOrientation = 0;
        }
        doTestAutoCompleteAndCorrectionForOrientation(requestedOrientation);
    }

    @Test
    @MediumTest
    @Feature({"Omnibox", "Main"})
    @RetryOnFailure
    public void testAutoCompleteAndCorrectionPortrait()
            throws ExecutionException, InterruptedException {
        // Default orientation for tablets is landscape. Default for phones is portrait.
        int requestedOrientation = 0;
        if (mActivityTestRule.getActivity().isTablet()) {
            requestedOrientation = 1;
        }
        doTestAutoCompleteAndCorrectionForOrientation(requestedOrientation);
    }

    private void doTestAutoCompleteAndCorrectionForOrientation(
            int orientation) throws ExecutionException, InterruptedException {
        mActivityTestRule.getActivity().setRequestedOrientation(orientation);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Map<String, List<SuggestionsResult>> suggestionsMap = buildSuggestionMap(
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("wiki")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "wikipedia", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "wiki", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "wikileaks", null)
                                .setAutocompleteText("pedia")),
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("onomatop")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "onomatopoeia", null)
                                .addGeneratedSuggestion(
                                        OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "onomatopoeia foo", null)
                                .setAutocompleteText("oeia")),
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("mispellled")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "misspelled", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                                        "misspelled words", null)
                                .setAutocompleteText(""))
        );
        checkAutocompleteText(suggestionsMap, "wiki", "wikipedia", 4, 9);
        checkAutocompleteText(suggestionsMap, "onomatop", "onomatopoeia", 8, 12);
        checkAutocompleteText(suggestionsMap, "mispellled", "mispellled", 10, 10);
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testDuplicateAutocompleteTextResults()
            throws InterruptedException, ExecutionException {
        Map<String, List<SuggestionsResult>> suggestionsMap = buildSuggestionMap(
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("test")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testing", null)
                                .setAutocompleteText("ing"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testz", null)
                                .setAutocompleteText("ing"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testblarg", null)
                                .setAutocompleteText("ing")));
        checkAutocompleteText(suggestionsMap, "test", "testing", 4, 7);
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testGrowingAutocompleteTextResults()
            throws InterruptedException, ExecutionException {
        Map<String, List<SuggestionsResult>> suggestionsMap = buildSuggestionMap(
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("test")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testing", null)
                                .setAutocompleteText("i"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testz", null)
                                .setAutocompleteText("in"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testblarg", null)
                                .setAutocompleteText("ing for the win")));
        checkAutocompleteText(suggestionsMap, "test", "testing for the win", 4, 19);
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    @DisabledTest
    public void testShrinkingAutocompleteTextResults()
            throws InterruptedException, ExecutionException {
        Map<String, List<SuggestionsResult>> suggestionsMap = buildSuggestionMap(
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("test")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testing", null)
                                .setAutocompleteText("ing is awesome"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testz", null)
                                .setAutocompleteText("ing is hard"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "test", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "testblarg", null)
                                .setAutocompleteText("ingz")));
        checkAutocompleteText(suggestionsMap, "test", "testingz", 4, 8);
    }

    private void checkAutocompleteText(
            Map<String, List<SuggestionsResult>> suggestionsMap,
            final String textToType, final String expectedAutocompleteText,
            final int expectedAutocompleteStart, final int expectedAutocompleteEnd)
            throws InterruptedException, ExecutionException {
        final TextView urlBarView =
                (TextView) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            urlBarView.requestFocus();
            urlBarView.setText("");
        });

        final LocationBarLayout locationBar =
                ((LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                        R.id.location_bar));

        final Object suggestionsProcessedSignal = new Object();
        final AtomicInteger suggestionsLeft = new AtomicInteger(
                suggestionsMap.get(textToType).size());
        OnSuggestionsReceivedListener suggestionsListener = new OnSuggestionsReceivedListener() {
            @Override
            public void onSuggestionsReceived(
                    List<OmniboxSuggestion> suggestions,
                    String inlineAutocompleteText) {
                AutocompleteCoordinatorTestUtils
                        .getSuggestionsReceivedListenerForTest(
                                locationBar.getAutocompleteCoordinator())
                        .onSuggestionsReceived(suggestions, inlineAutocompleteText);
                synchronized (suggestionsProcessedSignal) {
                    int remaining = suggestionsLeft.decrementAndGet();
                    if (remaining == 0) {
                        suggestionsProcessedSignal.notifyAll();
                    } else if (remaining < 0) {
                        Assert.fail("Unexpected suggestions received");
                    }
                }
            }
        };
        final TestAutocompleteController controller = new TestAutocompleteController(
                locationBar, suggestionsListener, suggestionsMap);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutocompleteCoordinatorTestUtils.setAutocompleteController(
                    locationBar.getAutocompleteCoordinator(), controller);
        });

        KeyUtils.typeTextIntoView(
                InstrumentationRegistry.getInstrumentation(), urlBarView, textToType);

        synchronized (suggestionsProcessedSignal) {
            long endTime = SystemClock.uptimeMillis() + 3000;
            while (suggestionsLeft.get() != 0) {
                long waitTime = endTime - SystemClock.uptimeMillis();
                if (waitTime <= 0) break;
                suggestionsProcessedSignal.wait(waitTime);
            }
        }

        CharSequence urlText = TestThreadUtils.runOnUiThreadBlocking(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return urlBarView.getText();
            }
        });
        Assert.assertEquals("URL Bar text not autocompleted as expected.", expectedAutocompleteText,
                urlText.toString());
        Assert.assertEquals(expectedAutocompleteStart, Selection.getSelectionStart(urlText));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            Assert.assertEquals(expectedAutocompleteEnd, urlText.length());
        } else {
            Assert.assertEquals(expectedAutocompleteEnd, Selection.getSelectionEnd(urlText));
        }
    }

    /**
     * The following test is a basic way to assess how much instant slows down typing in the
     * omnibox. It is meant to be run manually for investigation purposes.
     * When instant was enabled for all suggestions (including searched), I would get a 40% increase
     * in the average time on this test. With instant off, it was almost identical.
     * Marking the test disabled so it is not picked up by our test runner, as it is supposed to be
     * run manually.
     */
    public void manualTestTypingPerformance() throws InterruptedException {
        final String text = "searching for pizza";
        // Type 10 times something on the omnibox and get the average time with and without instant.
        long instantAverage = 0;
        long noInstantAverage = 0;

        for (int i = 0; i < 2; ++i) {
            boolean instantOn = (i == 1);
            mActivityTestRule.setNetworkPredictionEnabled(instantOn);

            for (int j = 0; j < 10; ++j) {
                long before = System.currentTimeMillis();
                mActivityTestRule.typeInOmnibox(text, true);
                if (instantOn) {
                    instantAverage += System.currentTimeMillis() - before;
                } else {
                    noInstantAverage += System.currentTimeMillis() - before;
                }
                clearUrlBar();
                InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            }
        }
        instantAverage /= 10;
        noInstantAverage /= 10;
        System.err.println("******************************************************************");
        System.err.println("**** Instant average=" + instantAverage);
        System.err.println("**** No instant average=" + noInstantAverage);
        System.err.println("******************************************************************");
    }

    /**
     * Test to verify that the security icon is present when visiting http:// URLs.
     */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    public void testSecurityIconOnHTTP() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            final String testUrl = testServer.getURL("/chrome/test/data/android/omnibox/one.html");

            mActivityTestRule.loadUrl(testUrl);
            final LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            StatusViewCoordinator statusViewCoordinator =
                    locationBar.getStatusViewCoordinatorForTesting();
            boolean securityIcon = statusViewCoordinator.isSecurityButtonShown();
            if (mActivityTestRule.getActivity().isTablet()) {
                Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
                Assert.assertEquals(
                        R.drawable.omnibox_info, statusViewCoordinator.getSecurityIconResourceId());
            } else {
                Assert.assertFalse("Omnibox should not have a Security icon", securityIcon);
            }
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Test to verify that the security icon is present when visiting https:// URLs.
     */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    public void testSecurityIconOnHTTPS() throws Exception {
        EmbeddedTestServer httpsTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(),
                ServerCertificate.CERT_OK);
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                onSSLStateUpdatedCallbackHelper.notifyCalled();
            }
        };
        mActivityTestRule.getActivity().getActivityTab().addObserver(observer);

        try {
            final String testHttpsUrl =
                    httpsTestServer.getURL("/chrome/test/data/android/omnibox/one.html");

            ImageButton securityButton = (ImageButton) mActivityTestRule.getActivity().findViewById(
                    R.id.location_bar_status_icon);

            mActivityTestRule.loadUrl(testHttpsUrl);
            onSSLStateUpdatedCallbackHelper.waitForCallback(0);

            final LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            StatusViewCoordinator statusViewCoordinator =
                    locationBar.getStatusViewCoordinatorForTesting();
            boolean securityIcon = statusViewCoordinator.isSecurityButtonShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals("location_bar_status_icon with wrong resource-id",
                    R.id.location_bar_status_icon, securityButton.getId());
            Assert.assertTrue(securityButton.isShown());
            Assert.assertEquals(R.drawable.omnibox_https_valid,
                    statusViewCoordinator.getSecurityIconResourceId());
        } finally {
            httpsTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Test whether the color of the Location bar is correct for HTTPS scheme.
     */
    @Test
    @SmallTest
    @SkipCommandLineParameterization
    public void testHttpsLocationBarColor() throws Exception {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        CallbackHelper didThemeColorChangedCallbackHelper = new CallbackHelper();
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        new TabModelSelectorTabObserver(mActivityTestRule.getActivity().getTabModelSelector()) {
            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                didThemeColorChangedCallbackHelper.notifyCalled();
            }
            @Override
            public void onSSLStateUpdated(Tab tab) {
                onSSLStateUpdatedCallbackHelper.notifyCalled();
            }
        };

        try {
            final String testHttpsUrl =
                    testServer.getURL("/chrome/test/data/android/theme_color_test.html");

            mActivityTestRule.loadUrl(testHttpsUrl);

            // Tablets don't have website theme colors.
            if (!mActivityTestRule.getActivity().isTablet()) {
                didThemeColorChangedCallbackHelper.waitForCallback(0);
            }

            onSSLStateUpdatedCallbackHelper.waitForCallback(0);

            LocationBarLayout locationBarLayout =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            ImageButton securityButton = (ImageButton) mActivityTestRule.getActivity().findViewById(
                    R.id.location_bar_status_icon);

            boolean securityIcon =
                    locationBarLayout.getStatusViewCoordinatorForTesting().isSecurityButtonShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals("location_bar_status_icon with wrong resource-id",
                    R.id.location_bar_status_icon, securityButton.getId());

            if (mActivityTestRule.getActivity().isTablet()) {
                Assert.assertTrue(mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getLocationBarModelForTesting()
                                          .shouldEmphasizeHttpsScheme());
            } else {
                Assert.assertFalse(mActivityTestRule.getActivity()
                                           .getToolbarManager()
                                           .getLocationBarModelForTesting()
                                           .shouldEmphasizeHttpsScheme());
            }
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    @DisabledTest // https://crbug.com/950556
    public void testSuggestionDirectionSwitching() {
        final TextView urlBarView =
                (TextView) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            urlBarView.requestFocus();
            urlBarView.setText("");
        });

        final LocationBarLayout locationBar =
                ((LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                        R.id.location_bar));

        Map<String, List<SuggestionsResult>> suggestionsMap = buildSuggestionMap(
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("ل")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "للك", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "www.test.com", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "للكتا", null)),
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("للك")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "للكتاب", null)),
                new TestSuggestionResultsBuilder()
                        .setTextShownFor("f")
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "f", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "fa", null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        "fac", null)));
        final TestAutocompleteController controller = new TestAutocompleteController(locationBar,
                AutocompleteCoordinatorTestUtils.getSuggestionsReceivedListenerForTest(
                        locationBar.getAutocompleteCoordinator()),
                suggestionsMap);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutocompleteCoordinatorTestUtils.setAutocompleteController(
                    locationBar.getAutocompleteCoordinator(), controller);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBarView.setText("ل"); });
        verifyOmniboxSuggestionAlignment(locationBar, 3, View.LAYOUT_DIRECTION_RTL);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBarView.setText("للك"); });
        verifyOmniboxSuggestionAlignment(locationBar, 1, View.LAYOUT_DIRECTION_RTL);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBarView.setText("f"); });
        verifyOmniboxSuggestionAlignment(locationBar, 3, View.LAYOUT_DIRECTION_LTR);
    }

    private void verifyOmniboxSuggestionAlignment(final LocationBarLayout locationBar,
            final int expectedSuggestionCount, final int expectedLayoutDirection) {
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, expectedSuggestionCount);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ListView suggestionsList = AutocompleteCoordinatorTestUtils.getSuggestionList(
                    locationBar.getAutocompleteCoordinator());
            Assert.assertEquals(expectedSuggestionCount, suggestionsList.getChildCount());
            for (int i = 0; i < suggestionsList.getChildCount(); i++) {
                SuggestionView suggestionView = (SuggestionView) suggestionsList.getChildAt(i);
                Assert.assertEquals(
                        String.format(Locale.getDefault(),
                                "Incorrect layout direction of suggestion at index %d", i),
                        expectedLayoutDirection, ViewCompat.getLayoutDirection(suggestionView));
            }
        });
    }

    @Before
    public void setUp() throws InterruptedException {
        if (mActivityTestRule.getName().equals("testsplitPathFromUrlDisplayText")
                || mActivityTestRule.getName().equals("testDefaultText")) {
            return;
        }
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
