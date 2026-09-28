// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.javascript_dialogs.JavascriptAppModalDialog;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Closes tabs through {@link TabRemover} against a live renderer, so that the whole {@code
 * beforeunload} round trip runs: the prompter's dispatch, the renderer's answer, the native
 * delegate's {@code BeforeUnloadFired}, its JNI call into {@code
 * TabWebContentsDelegateAndroidImpl.onBeforeUnloadFired}, and the {@code TabObserver} fan-out that
 * resumes the closure. Unit tests stub one side or the other of that JNI boundary; these do not.
 *
 * <p>Every case depends on that fan-out. The prompter marks the tab so that Content does not close
 * it on its own, and the closure resumes only when the answer reaches Java: a completion that stops
 * short leaves an agreeing tab open and a refusing tab's closure unresolved.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ANDROID_BEFORE_UNLOAD_SUPPORT})
@Batch(Batch.PER_CLASS)
public class BeforeUnloadTabCloseTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private static final String EMPTY_PAGE =
            UrlUtils.encodeHtmlDataUri("<html><title>Empty</title><p>Empty.</p></html>");
    private static final String BEFORE_UNLOAD_PAGE =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><script>window.onbeforeunload = function() {"
                            + "return 'Are you sure?';"
                            + "};</script></head><body><p>Unsaved work.</p></body></html>");

    @Before
    public void setUp() {
        mActivityTestRule.startOnWebPage(EMPTY_PAGE);
        // The prompter only runs on desktop Android. Reset after each test by
        // ResettersForTesting.
        DeviceInfo.setIsDesktopForTesting(true);
    }

    @Test
    @MediumTest
    public void testCloseTab_UserLeaves_TabCloses() throws TimeoutException {
        Tab tab = openTabWithUserActivation(BEFORE_UNLOAD_PAGE);
        int tabCount = mActivityTestRule.tabsCount(/* incognito= */ false);

        closeTabs(List.of(tab), /* listener= */ null);
        waitForDialog(/* shown= */ true);
        onViewWaiting(withText(R.string.leave)).perform(click());

        waitForTabCount(tabCount - 1);
        waitForTabDestroyed(tab);
    }

    @Test
    @MediumTest
    public void testCloseTab_UserStays_TabSurvives() throws TimeoutException {
        Tab tab = openTabWithUserActivation(BEFORE_UNLOAD_PAGE);
        int tabCount = mActivityTestRule.tabsCount(/* incognito= */ false);
        ResultListener listener = new ResultListener();

        closeTabs(List.of(tab), listener);
        waitForDialog(/* shown= */ true);
        onViewWaiting(withText(R.string.cancel)).perform(click());

        // TabRemover reports a refusal only once the answer has reached Java.
        listener.mResult.waitForOnly();
        assertEquals(ActionConfirmationResult.CONFIRMATION_NEGATIVE, listener.mLastResult);
        waitForDialog(/* shown= */ false);
        assertEquals(tabCount, mActivityTestRule.tabsCount(/* incognito= */ false));
        assertTabOpen(tab);
    }

    @Test
    @MediumTest
    public void testCloseTab_NoUserActivation_ClosesWithoutDialog() {
        // Without sticky activation Blink agrees immediately, so the round trip still runs through
        // the renderer and back, but no dialog is shown.
        Tab tab = mActivityTestRule.loadUrlInNewTab(BEFORE_UNLOAD_PAGE);
        int tabCount = mActivityTestRule.tabsCount(/* incognito= */ false);

        closeTabs(List.of(tab), /* listener= */ null);

        waitForTabCount(tabCount - 1);
        waitForTabDestroyed(tab);
        waitForDialog(/* shown= */ false);
    }

    @Test
    @MediumTest
    public void testCloseTabs_UserStaysOnOne_OnlyThatTabSurvives() throws TimeoutException {
        Tab kept = openTabWithUserActivation(BEFORE_UNLOAD_PAGE);
        Tab closed = openTabWithUserActivation(BEFORE_UNLOAD_PAGE);
        int tabCount = mActivityTestRule.tabsCount(/* incognito= */ false);

        // Tabs are asked in order, one dialog at a time.
        closeTabs(List.of(kept, closed), /* listener= */ null);
        waitForDialog(/* shown= */ true);
        JavascriptAppModalDialog firstDialog = getCurrentDialog();
        onViewWaiting(withText(R.string.cancel)).perform(click());
        // The second tab is asked on a later task, with a dialog of its own.
        CriteriaHelper.pollUiThread(
                () -> {
                    JavascriptAppModalDialog dialog =
                            JavascriptAppModalDialog.getCurrentDialogForTest();
                    Criteria.checkThat(dialog, Matchers.notNullValue());
                    Criteria.checkThat(dialog, Matchers.not(Matchers.sameInstance(firstDialog)));
                });
        onViewWaiting(withText(R.string.leave)).perform(click());

        waitForTabCount(tabCount - 1);
        waitForTabDestroyed(closed);
        assertTabOpen(kept);
    }

    /** Opens {@code url} in a new foreground tab and taps it, so its page can show a dialog. */
    private Tab openTabWithUserActivation(String url) throws TimeoutException {
        Tab tab = mActivityTestRule.loadUrlInNewTab(url);
        CallbackHelper tapped = new CallbackHelper();
        GestureStateListener listener =
                new GestureStateListener() {
                    @Override
                    public void onSingleTap(boolean consumed) {
                        tapped.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebContentsUtils.getGestureListenerManager(tab.getWebContents())
                                .addListener(listener));
        TouchCommon.singleClickView(tab.getView());
        tapped.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebContentsUtils.getGestureListenerManager(tab.getWebContents())
                                .removeListener(listener));
        return tab;
    }

    private void closeTabs(List<Tab> tabs, TabModelActionListener listener) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabClosureParams.Builder params =
                            tabs.size() == 1
                                    ? TabClosureParams.closeTab(tabs.get(0))
                                    : TabClosureParams.closeTabs(tabs);
                    mActivityTestRule
                            .getActivity()
                            .getCurrentTabModel()
                            .getTabRemover()
                            .closeTabs(
                                    params.allowUndo(false).build(),
                                    /* allowDialog= */ true,
                                    listener);
                });
    }

    private static JavascriptAppModalDialog getCurrentDialog() {
        return ThreadUtils.runOnUiThreadBlocking(
                JavascriptAppModalDialog::getCurrentDialogForTest);
    }

    private void waitForDialog(boolean shown) {
        CriteriaHelper.pollUiThread(
                () -> {
                    JavascriptAppModalDialog dialog =
                            JavascriptAppModalDialog.getCurrentDialogForTest();
                    Criteria.checkThat(
                            dialog, shown ? Matchers.notNullValue() : Matchers.nullValue());
                });
    }

    private void waitForTabCount(int count) {
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mActivityTestRule.tabsCount(/* incognito= */ false),
                                Matchers.is(count)));
    }

    private static void waitForTabDestroyed(Tab tab) {
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isDestroyed(), Matchers.is(true)));
    }

    private static void assertTabOpen(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(tab.isClosing());
                    assertFalse(tab.isDestroyed());
                    assertTrue(tab.getWebContents() != null);
                });
    }

    /** Records the outcome {@link TabRemover} reports for a closure. */
    private static class ResultListener implements TabModelActionListener {
        final CallbackHelper mResult = new CallbackHelper();
        @ActionConfirmationResult int mLastResult;

        @Override
        public void onConfirmationDialogResult(
                @DialogType int dialogType, @ActionConfirmationResult int result) {
            mLastResult = result;
            mResult.notifyCalled();
        }
    }
}
