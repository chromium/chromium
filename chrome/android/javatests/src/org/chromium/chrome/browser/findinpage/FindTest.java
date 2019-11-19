// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a simple framework for a test of an Application. See
 * {@link android.test.ApplicationTestCase ApplicationTestCase} for more
 * information on how to write and extend Application tests.
 */

package org.chromium.chrome.browser.findinpage;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.Spannable;
import android.text.style.StyleSpan;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/**
 * Find in page tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class FindTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String FILEPATH = "/chrome/test/data/android/find/test.html";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    /**
     * Returns the FindResults text.
     */
    private String waitForFindResults(String expectedResult) {
        final TextView findResults =
                (TextView) mActivityTestRule.getActivity().findViewById(R.id.find_status);
        Assert.assertNotNull(expectedResult);
        Assert.assertNotNull(findResults);
        CriteriaHelper.pollUiThread(Criteria.equals(expectedResult, new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return findResults.getText();
            }
        }));
        return findResults.getText().toString();
    }

    /**
     * Find in page by invoking the 'find in page' menu item.
     *
     */
    private void findInPageFromMenu() {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                FindToolbar findToolbar =
                        (FindToolbar) mActivityTestRule.getActivity().findViewById(
                                R.id.find_toolbar);

                boolean isVisible = findToolbar != null && findToolbar.isShown();
                return (visible == isVisible) && !findToolbar.isAnimating();
            }
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < events.length; i++) {
                if (!findQueryText.dispatchKeyEventPreIme(events[i])) {
                    findQueryText.dispatchKeyEvent(events[i]);
                }
            }
        });
        return waitForFindResults(expectedResult);
    }

    private void loadTestAndVerifyFindInPage(String query, String expectedResult) {
        mActivityTestRule.loadUrl(mTestServer.getURL(FILEPATH));
        String findResults = findStringInPage(query, expectedResult);
        Assert.assertTrue("Expected: " + expectedResult + " Got: " + findResults
                        + " for: " + mTestServer.getURL(FILEPATH),
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

    /**
     * Verify Find In Page is not case sensitive.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage", "Main"})
    @RetryOnFailure
    public void testFind() {
        loadTestAndVerifyFindInPage("pitts", "1/7");
    }

    /**
     * Verify Find In Page with just one result.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFind101() {
        loadTestAndVerifyFindInPage("it", "1/101");
    }

    /**
     * Verify Find In Page with a multi-line string.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindMultiLine() {
        String multiLineSearchTerm = "This is the text of this document.\n"
                + " I am going to write the word \'Pitts\' 7 times. (That was one.)";
        loadTestAndVerifyFindInPage(multiLineSearchTerm, "1/1");
    }

    /**
     * Test for Find In Page with a multi-line string. Search string has an extra character
     * added to the end so it should not be found.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindMultiLineFalse() {
        String multiLineSearchTerm = "aThis is the text of this document.\n"
                + " I am going to write the word \'Pitts\' 7 times. (That was one.)";
        loadTestAndVerifyFindInPage(multiLineSearchTerm, "0/0");
    }

    /**
     * Verify Find In Page Next button.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
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

    /**
     * Verify Find In Page Next/Previous button.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
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

    /**
     * Verify that Find in page toolbar is dismissed on entering fullscreen.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFullscreen() {
        loadTestAndVerifyFindInPage("pitts", "1/7");

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
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
    @RetryOnFailure
    public void testResultsBarInitiallyVisible() {
        mActivityTestRule.loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();
        final FindToolbar findToolbar = getFindToolbar();
        final View resultBar = findToolbar.getFindResultBar();
        Assert.assertNotNull(resultBar);
        Assert.assertEquals(View.VISIBLE, resultBar.getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testResultsBarVisibleAfterTypingText() {
        mActivityTestRule.loadUrl(mTestServer.getURL(FILEPATH));
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
     * Verify Find In Page isn't dismissed and matches no results
     * if invoked with an empty string.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindDismissOnEmptyString() {
        mActivityTestRule.loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final TextView findQueryText = getFindQueryText();
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_T);
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_DEL);
        KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(), findQueryText,
                KeyEvent.KEYCODE_ENTER);

        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());

        String findResults = waitForFindResults("");
        Assert.assertEquals(0, findResults.length());
    }

    /**
     * Verify FIP in IncognitoTabs.
     */
    @Test
    @SmallTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindNextPreviousIncognitoTab() {
        String query = "pitts";
        mActivityTestRule.newIncognitoTabFromMenu();
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_prev_button));
        waitForFindResults("1/7");
    }

    /**
     * Verify Find in Page text isnt restored on Incognito Tabs.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFipTextNotRestoredIncognitoTab() throws InterruptedException {
        mActivityTestRule.newIncognitoTabFromMenu();
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
    }

    /**
     * Verify pasted text in the FindQuery text box doesn't retain formatting
     */
    @Test
    @SmallTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testPastedTextStylingRemoved() {
        mActivityTestRule.loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        Assert.assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final EditText findQueryText = getFindQueryText();

        // Emulate pasting the text into the find query text box
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Setup the clipboard with a selection of stylized text
            ClipboardManager clipboard =
                    (ClipboardManager) InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getSystemService(Context.CLIPBOARD_SERVICE);
            clipboard.setPrimaryClip(ClipData.newHtmlText("label", "text", "<b>text</b>"));

            findQueryText.onTextContextMenuItem(android.R.id.paste);
        });

        // Resulting text in the find query box should be unstyled
        final Spannable text = findQueryText.getText();
        final StyleSpan[] spans = text.getSpans(0, text.length(), StyleSpan.class);
        Assert.assertEquals(0, spans.length);
    }

    /**
     * Verify Find in page toolbar is not dismissed when device back key is pressed with the
     * presence of IME. First back key should dismiss IME and second back key should dismiss
     * Find in page toolbar.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testBackKeyDoesNotDismissFindWhenImeIsPresent() {
        mActivityTestRule.loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();
        final TextView findQueryText = getFindQueryText();
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), findQueryText, KeyEvent.KEYCODE_A);
        waitForIME(true);
        // IME is present at this moment, so IME will consume BACK key.
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        waitForIME(false);
        waitForFindInPageVisibility(true);
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        waitForFindInPageVisibility(false);
    }

    /**
     * Verify Find in page toolbar is dismissed when device back key is pressed when IME
     * is not present. First back key press itself will dismiss Find in page toolbar.
     */
    @Test
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testBackKeyDismissesFind() {
        loadTestAndVerifyFindInPage("pitts", "1/7");
        waitForIME(true);
        // Hide IME by clicking next button from find tool bar.
        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().findViewById(R.id.find_next_button));
        waitForIME(false);
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        waitForFindInPageVisibility(false);
    }

    private void waitForIME(final boolean imePresent) {
        // Wait for IME to appear.
        CriteriaHelper.pollUiThread(Criteria.equals(imePresent,
                ()
                        -> mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                                mActivityTestRule.getActivity(), getFindQueryText())));
    }
}
