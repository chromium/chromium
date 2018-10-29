// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.test.util.OmniboxTestUtils.buildSuggestionMap;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v4.view.ViewCompat;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.base.test.util.parameter.CommandLineParameter;
import org.chromium.base.test.util.parameter.SkipCommandLineParameterization;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.status.StatusViewCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsList;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionView;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
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
 * TODO(yolandyan): Replace the CommandLineParameter with new JUnit4 parameterized
 * framework once it supports Test Rule Parameterization
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@CommandLineParameter({"", "disable-features=" + ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE})
@SuppressLint("SetTextI18n")
public class OmniboxTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private void clearUrlBar() {
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.setText("");
            }
        });
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
                }, ScalableTimeout.scaleTimeout(20));
    }

    /**
     * Test for checking whether soft input model switches with focus.
     */
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testFocusChangingSoftInputMode() throws InterruptedException {
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
    public void testRequestZeroSuggestOnFocus() throws Exception {
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        ThreadUtils.runOnUiThreadBlocking(new Runnable(){
            @Override
            public void run() {
                urlBar.setText("http://www.example.com/");
            }
        });

        final TestAutocompleteController controller = new TestAutocompleteController(locationBar,
                sEmptySuggestionListener, new HashMap<String, List<SuggestionsResult>>());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                locationBar.getAutocompleteCoordinator().setAutocompleteController(controller);
            }
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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                locationBar.getAutocompleteCoordinator().setAutocompleteController(controller);
                urlBar.setText("g");
            }
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
    public void testRequestZeroSuggestTypeAndBackspace() throws InterruptedException {
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        final TestAutocompleteController controller = new TestAutocompleteController(locationBar,
                sEmptySuggestionListener, new HashMap<String, List<SuggestionsResult>>());

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                locationBar.getAutocompleteCoordinator().setAutocompleteController(controller);
                urlBar.setText("g");
                urlBar.setSelection(1);
            }
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
    public void testDefaultText() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();

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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.requestFocus();
                urlBar.setText("G");
            }
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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBarView.requestFocus();
                urlBarView.setText("");
            }
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
                locationBar.getAutocompleteCoordinator().onSuggestionsReceived(
                        suggestions, inlineAutocompleteText);
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

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                locationBar.getAutocompleteCoordinator().setAutocompleteController(controller);
            }
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

        CharSequence urlText = ThreadUtils.runOnUiThreadBlocking(new Callable<CharSequence>() {
            @Override
            public CharSequence call() throws Exception {
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
    public void testSecurityIconOnHTTP() throws InterruptedException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            final String testUrl = testServer.getURL("/chrome/test/data/android/omnibox/one.html");

            ImageButton securityButton = (ImageButton) mActivityTestRule.getActivity().findViewById(
                    R.id.security_button);

            mActivityTestRule.loadUrl(testUrl);
            final LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            StatusViewCoordinator statusViewCoordinator =
                    locationBar.getStatusViewCoordinatorForTesting();
            boolean securityIcon = statusViewCoordinator.isSecurityButtonShown();
            if (mActivityTestRule.getActivity().isTablet()) {
                Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
                Assert.assertTrue(securityButton.isShown());
                Assert.assertEquals(
                        R.drawable.omnibox_info, statusViewCoordinator.getSecurityIconResourceId());
            } else {
                Assert.assertFalse("Omnibox should not have a Security icon", securityIcon);
                Assert.assertFalse(securityButton.isShown());
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
                    R.id.security_button);

            mActivityTestRule.loadUrl(testHttpsUrl);
            onSSLStateUpdatedCallbackHelper.waitForCallback(0);

            final LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            StatusViewCoordinator statusViewCoordinator =
                    locationBar.getStatusViewCoordinatorForTesting();
            boolean securityIcon = statusViewCoordinator.isSecurityButtonShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals("security_button with wrong resource-id", R.id.security_button,
                    securityButton.getId());
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
            didThemeColorChangedCallbackHelper.waitForCallback(0);
            onSSLStateUpdatedCallbackHelper.waitForCallback(0);

            LocationBarLayout locationBarLayout =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            ImageButton securityButton = (ImageButton) mActivityTestRule.getActivity().findViewById(
                    R.id.security_button);

            boolean securityIcon =
                    locationBarLayout.getStatusViewCoordinatorForTesting().isSecurityButtonShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals("security_button with wrong resource-id", R.id.security_button,
                    securityButton.getId());

            if (mActivityTestRule.getActivity().isTablet()) {
                Assert.assertTrue(mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarModelForTesting()
                                          .shouldEmphasizeHttpsScheme());
            } else {
                Assert.assertFalse(mActivityTestRule.getActivity()
                                           .getToolbarManager()
                                           .getToolbarModelForTesting()
                                           .shouldEmphasizeHttpsScheme());
            }
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    // TODO(bauerb): Move this to a Robolectric test.
    @Test
    @SmallTest
    @SkipCommandLineParameterization
    public void testOriginSpan() {
        verifyOriginSpan("", null, "");
        verifyOriginSpan("https:", null, "https:");
        verifyOriginSpan("about:blank", null, "about:blank");

        verifyOriginSpan("chrome://flags", null, "chrome://flags");
        verifyOriginSpan("chrome://flags", "/?egads", "chrome://flags/?egads");

        verifyOriginSpan("www.google.com", null, "www.google.com");
        verifyOriginSpan("www.google.com", null, "www.google.com/");
        verifyOriginSpan("www.google.com", "/?q=blah", "www.google.com/?q=blah");

        verifyOriginSpan("https://www.google.com", null, "https://www.google.com");
        verifyOriginSpan("https://www.google.com", null, "https://www.google.com/");
        verifyOriginSpan("https://www.google.com", "/?q=blah", "https://www.google.com/?q=blah");

        // crbug.com/414990
        String testUrl = "https://disneyworld.disney.go.com/special-offers/"
                + "?CMP=KNC-WDW_FY15_DOM_Q1RO_BR_Gold_SpOffer|G|4141300.RR.AM.01.47"
                + "&keyword_id=s6JyxRifG_dm|walt%20disney%20world|37174067873|e|1540wwa14043";
        verifyOriginSpan("https://disneyworld.disney.go.com",
                "/special-offers/?CMP=KNC-WDW_FY15_DOM_Q1RO_BR_Gold_SpOffer|G|4141300.RR.AM.01.47"
                        + "&keyword_id=s6JyxRifG_dm|walt%20disney%20world|37174067873|e|"
                        + "1540wwa14043",
                testUrl);

        // crbug.com/415387
        verifyOriginSpan("ftp://example.com", "/ftp.html", "ftp://example.com/ftp.html");

        // crbug.com/447416
        verifyOriginSpan("file:///dev/blah", null, "file:///dev/blah");
        verifyOriginSpan(
                "javascript:window.alert('hello');", null, "javascript:window.alert('hello');");
        verifyOriginSpan("data:text/html;charset=utf-8,Page%201", null,
                "data:text/html;charset=utf-8,Page%201");
    }

    private void verifyOriginSpan(
            String expectedOrigin, @Nullable String expectedOriginSuffix, String url) {
        UrlBarData urlBarData = UrlBarData.forUrl(url);
        String displayText = urlBarData.displayText.toString();
        Assert.assertEquals(expectedOriginSuffix == null ? expectedOrigin
                                                         : expectedOrigin + expectedOriginSuffix,
                displayText);
        Assert.assertEquals(expectedOrigin,
                displayText.substring(urlBarData.originStartIndex, urlBarData.originEndIndex));
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    @MinAndroidSdkLevel(Build.VERSION_CODES.JELLY_BEAN_MR1)
    public void testSuggestionDirectionSwitching() throws InterruptedException {
        final TextView urlBarView =
                (TextView) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBarView.requestFocus();
                urlBarView.setText("");
            }
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
        final TestAutocompleteController controller = new TestAutocompleteController(
                locationBar, locationBar.getAutocompleteCoordinator(), suggestionsMap);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                locationBar.getAutocompleteCoordinator().setAutocompleteController(controller);
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBarView.setText("ل");
            }
        });
        verifyOmniboxSuggestionAlignment(locationBar, 3, View.LAYOUT_DIRECTION_RTL);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBarView.setText("للك");
            }
        });
        verifyOmniboxSuggestionAlignment(locationBar, 1, View.LAYOUT_DIRECTION_RTL);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBarView.setText("f");
            }
        });
        verifyOmniboxSuggestionAlignment(locationBar, 3, View.LAYOUT_DIRECTION_LTR);
    }

    private void verifyOmniboxSuggestionAlignment(final LocationBarLayout locationBar,
            final int expectedSuggestionCount, final int expectedLayoutDirection) {
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, expectedSuggestionCount);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                OmniboxSuggestionsList suggestionsList =
                        locationBar.getAutocompleteCoordinator().getSuggestionList();
                Assert.assertEquals(expectedSuggestionCount, suggestionsList.getChildCount());
                for (int i = 0; i < suggestionsList.getChildCount(); i++) {
                    SuggestionView suggestionView = (SuggestionView) suggestionsList.getChildAt(i);
                    Assert.assertEquals(
                            String.format(Locale.getDefault(),
                                    "Incorrect layout direction of suggestion at index %d", i),
                            expectedLayoutDirection, ViewCompat.getLayoutDirection(suggestionView));
                }
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
