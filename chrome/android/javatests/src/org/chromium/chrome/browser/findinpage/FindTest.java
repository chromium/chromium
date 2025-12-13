// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.Spannable;
import android.text.style.StyleSpan;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CloseableOnMainThread;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.KeyUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;

/** Find in page tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class FindTest {
    private static final String FILEPATH = "/chrome/test/data/android/find/test.html";

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private WebPageStation mPage;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        mPage = mActivityTestRule.startOnBlankPage();

        waitForFindInPageVisibility(false);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getModel(true)
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs().build(),
                                    /* allowDialog= */ false);
                });
    }

    /** Returns the FindResults text. */
    private String waitForFindResults(String expectedResult) {
        final TextView findResults =
                (TextView) mActivityTestRule.getActivity().findViewById(R.id.find_status);
        Assert.assertNotNull(expectedResult);
        Assert.assertNotNull(findResults);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(findResults.getText(), Matchers.is(expectedResult)));
        return findResults.getText().toString();
    }

    /** Find in page by invoking the 'find in page' menu item. */
    private void findInPageFromMenu() {
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().findViewById(R.id.menu_button_wrapper)::isShown);

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    FindToolbar findToolbar =
                            (FindToolbar)
                                    mActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
                    if (visible) {
                        Criteria.checkThat(findToolbar, Matchers.notNullValue());
                        Criteria.checkThat(findToolbar.isShown(), Matchers.is(true));
                    } else {
                        if (findToolbar == null) return;
                        Criteria.checkThat(findToolbar.isShown(), Matchers.is(false));
                    }
                    Criteria.checkThat(findToolbar.isAnimating(), Matchers.is(false));
                });
    }

    private String findStringInPage(final String query, String expectedResult) {
        findInPageFromMenu();
        // FindToolbar should automatically get focus.
        final TextView findQueryText = getFindQueryText();
        Assert.assertTrue("FindToolbar should have focus", findQueryText.hasFocus());

        // We have to send each key 1-by-1 to trigger the right listeners in the toolbar.
        KeyCharacterMap keyCharacterMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        final KeyEvent[] events = keyCharacterMap.getEvents(query.toCharArray());
        Assert.assertNotNull(events);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < events.length; i++) {
                        if (!findQueryText.dispatchKeyEventPreIme(events[i])) {
                            findQueryText.dispatchKeyEvent(events[i]);
                        }
                    }
                });
        return waitForFindResults(expectedResult);
    }

    private void loadTestAndVerifyFindInPage(String query, String expectedResult) {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        String findResults = findStringInPage(query, expectedResult);
        Assert.assertTrue(
                "Expected: "
                        + expectedResult
                        + " Got: "
                        + findResults
                        + " for: "
                        + mActivityTestRule.getTestServer().getURL(FILEPATH),
                findResults.contains(expectedResult));
    }

    private FindToolbar getFindToolbar() {
        final FindToolbar findToolbar =
                (FindToolbar) mActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
        Assert.assertNotNull("FindToolbar not found", findToolbar);
        return findToolbar;
    }

    private EditText getFindQueryText() {
        final EditText findQueryText =
                (EditText) mActivityTestRule.getActivity().findViewById(R.id.find_query);
        Assert.assertNotNull("FindQueryText not found", findQueryText);
        return findQueryText;
    }

    /** Verify Find In Page is not case sensitive. */
    @Test
    @MediumTest
    @Feature({"FindInPage", "Main"})
    public void testFind() {
        loadTestAndVerifyFindInPage("pitts", "1/7");
    }

    /** Verify Find In Page with just one result. */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFind101() {
        loadTestAndVerifyFindInPage("it", "1/101");
    }

    /** Verify Find In Page with a multi-line string. */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindMultiLine() {
        String multiLineSearchTerm =
                "This is the text of this document.\n"
                        + " I am going to write the word \'Pitts\' 7 times. (That was one.)";
        loadTestAndVerifyFindInPage(multiLineSearchTerm, "1/1");
    }

    /**
     * Test for Find In Page with a multi-line string. Search string has an extra character added to
     * the end so it should not be found.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindMultiLineFalse() {
        String multiLineSearchTerm =
                "aThis is the text of this document.\n"
                        + " I am going to write the word \'Pitts\' 7 times. (That was one.)";
        loadTestAndVerifyFindInPage(multiLineSearchTerm, "0/0");
    }

    /** Verify Find In Page Next button. */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindNext() {
        String query = "pitts";
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        for (int i = 2; i <= 7; i++) {
            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        }
        waitForFindResults("1/7");
    }

    /** Verify Find In Page Next/Previous button. */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindNextPrevious() {
        String query = "pitts";
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_prev_button));
        waitForFindResults("1/7");
    }

    /** Verify that Find in page toolbar is dismissed on entering fullscreen. */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFullscreen() {
        loadTestAndVerifyFindInPage("pitts", "1/7");

        Tab tab = mActivityTestRule.getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity());
        waitForFindInPageVisibility(false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, false, mActivityTestRule.getActivity());
        waitForFindInPageVisibility(false);
    }

    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testResultsBarInitiallyVisible() {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();
        final FindToolbar findToolbar = getFindToolbar();
        final View resultBar = findToolbar.getFindResultBar();
        Assert.assertNotNull(resultBar);
        Assert.assertEquals(View.VISIBLE, resultBar.getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testResultsBarVisibleAfterTypingText() {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();
        final FindToolbar findToolbar = getFindToolbar();
        final View resultBar = findToolbar.getFindResultBar();
        Assert.assertNotNull(resultBar);
        final TextView findQueryText = getFindQueryText();

        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_T);
        Assert.assertEquals(View.VISIBLE, resultBar.getVisibility());
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_DEL);
        Assert.assertEquals(View.VISIBLE, resultBar.getVisibility());
    }

    /**
     * Verify Find In Page isn't dismissed and matches no results if invoked with an empty string.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindDismissOnEmptyString() {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final TextView findQueryText = getFindQueryText();
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_T);
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_DEL);
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(),
                findQueryText,
                KeyEvent.KEYCODE_ENTER);

        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());

        String findResults = waitForFindResults("");
        Assert.assertEquals(0, findResults.length());
    }

    /** Verify "Find in page" is dismissed when ESCAPE is pressed w/o modifiers. */
    @Test
    @SmallTest
    @Feature({"FindInPage"})
    public void testFindDismissOnEscape() {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final TextView findQueryText = getFindQueryText();
        Assert.assertTrue(findQueryText.hasFocus());

        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(),
                findQueryText,
                KeyEvent.KEYCODE_ESCAPE);

        Assert.assertEquals(View.GONE, findToolbar.getVisibility());
        Assert.assertFalse(findQueryText.hasFocus());
    }

    /** Verify "Find in page" isn't dismissed when ESCAPE is pressed w/ modifiers. */
    @Test
    @SmallTest
    @Feature({"FindInPage"})
    public void testFindDismissOnEscapeWithModifiers() {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final TextView findQueryText = getFindQueryText();
        Assert.assertTrue(findQueryText.hasFocus());

        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(),
                findQueryText,
                KeyEvent.KEYCODE_ESCAPE,
                KeyEvent.META_CTRL_ON);

        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        Assert.assertTrue(findQueryText.hasFocus());
    }

    /** Verify FIP in IncognitoTabs. */
    @Test
    @SmallTest
    @Feature({"FindInPage"})
    public void testFindNextPreviousIncognitoTab() {
        String query = "pitts";
        var incognitoPage = mPage.openNewIncognitoTabOrWindowFast();
        var incognitoActivity = incognitoPage.getActivity();
        var prevActivity = mActivityTestRule.getActivity();
        // TODO(crbug.com/439491767): Remove this workaround in favor of accessing the activity
        // through the page.
        mActivityTestRule.getActivityTestRule().setActivity(incognitoPage.getActivity());
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_prev_button));
        waitForFindResults("1/7");
        if (incognitoActivity != prevActivity) {
            ApplicationTestUtils.finishActivity(incognitoActivity);
            mActivityTestRule.getActivityTestRule().setActivity(prevActivity);
        }
    }

    /** Verify Find in Page text isnt restored on Incognito Tabs. */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFipTextNotRestoredIncognitoTab() throws InterruptedException {
        var incognitoPage = mPage.openNewIncognitoTabOrWindowFast();
        var incognitoActivity = incognitoPage.getActivity();
        var prevActivity = mActivityTestRule.getActivity();
        // TODO(crbug.com/439491767): Remove this workaround in favor of accessing the activity
        // through the page.
        mActivityTestRule.getActivityTestRule().setActivity(incognitoActivity);
        loadTestAndVerifyFindInPage("pitts", "1/7");
        // close the fip
        final View v = mActivityTestRule.getActivity().findViewById(R.id.close_find_button);
        TouchCommon.singleClickView(v);
        waitForFindInPageVisibility(false);

        // Reopen and check the text.
        findInPageFromMenu();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Verify the text content.
        final EditText e = getFindQueryText();
        String myText = e.getText().toString();
        Assert.assertTrue("expected empty string : " + myText, myText.isEmpty());

        if (incognitoActivity != prevActivity) {
            ApplicationTestUtils.finishActivity(incognitoActivity);
            mActivityTestRule.getActivityTestRule().setActivity(prevActivity);
        }
    }

    /** Verify pasted text in the FindQuery text box doesn't retain formatting */
    @Test
    @SmallTest
    @Feature({"FindInPage"})
    public void testPastedTextStylingRemoved() throws Throwable {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final EditText findQueryText = getFindQueryText();

        // Allow all thread policies temporarily in main thread to avoid
        // DiskWrite and UnBufferedIo violations during copying under
        // emulator environment.
        try (CloseableOnMainThread ignored =
                CloseableOnMainThread.StrictMode.allowAllThreadPolicies()) {
            // Emulate pasting the text into the find query text box
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // Setup the clipboard with a selection of stylized text
                        ClipboardManager clipboard =
                                (ClipboardManager)
                                        InstrumentationRegistry.getInstrumentation()
                                                .getTargetContext()
                                                .getSystemService(Context.CLIPBOARD_SERVICE);
                        clipboard.setPrimaryClip(
                                ClipData.newHtmlText("label", "text", "<b>text</b>"));

                        findQueryText.onTextContextMenuItem(android.R.id.paste);
                    });
        }

        // Resulting text in the find query box should be unstyled
        final Spannable text = findQueryText.getText();
        final StyleSpan[] spans = text.getSpans(0, text.length(), StyleSpan.class);
        Assert.assertEquals(0, spans.length);
    }

    /**
     * Verify Find in page toolbar is dismissed when device back key is pressed when IME is not
     * present. First back key press itself will dismiss Find in page toolbar.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testBackKeyDoesNotDismissFindWhenImeIsPresent() {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FILEPATH));
        findInPageFromMenu();
        final TextView findQueryText = getFindQueryText();
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_A);
        waitForIME(true);
        // IME is present at this moment, so IME will consume BACK key.
        Espresso.pressBack();
        waitForIME(false);
        waitForFindInPageVisibility(true);
        Espresso.pressBack();
        waitForFindInPageVisibility(false);
    }

    /**
     * Verify Find in page toolbar is dismissed when device back key is pressed when IME is not
     * present. First back key press itself will dismiss Find in page toolbar.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @DisabledTest(message = "https://crbug.com/1458344")
    public void testBackKeyDismissesFind() {
        loadTestAndVerifyFindInPage("pitts", "1/7");
        waitForIME(true);
        // Hide IME by clicking next button from find tool bar.
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        waitForIME(false);
        Espresso.pressBack();
        waitForFindInPageVisibility(false);
    }

    private void waitForIME(final boolean imePresent) {
        // Wait for IME to appear.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(getFindQueryText()),
                            Matchers.is(imePresent));
                });
    }
}
