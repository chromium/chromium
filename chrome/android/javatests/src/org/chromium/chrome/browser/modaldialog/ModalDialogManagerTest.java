// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.List;

/**
 * Tests for displaying and functioning of modal dialogs on tabs.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ModalDialogManagerTest {
    private static class TestObserver implements UrlFocusChangeListener {
        public final CallbackHelper onUrlFocusChangedCallback = new CallbackHelper();
        public final CallbackHelper onDialogDismissedCallback = new CallbackHelper();

        @Override
        public void onUrlFocusChange(boolean hasFocus) {
            onUrlFocusChangedCallback.notifyCalled();
        }

        public void onDialogDismissed() {
            onDialogDismissedCallback.notifyCalled();
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int MAX_DIALOGS = 4;
    private ChromeTabbedActivity mActivity;
    private ModalDialogManager mManager;
    private ModalDialogView[] mModalDialogViews;
    private TestObserver mTestObserver;
    private Integer mExpectedDismissalCause;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mManager = mActivity.getModalDialogManager();
        mModalDialogViews = new ModalDialogView[MAX_DIALOGS];
        for (int i = 0; i < MAX_DIALOGS; i++) mModalDialogViews[i] = createDialog(i);
        mTestObserver = new TestObserver();
        mActivity.getToolbarManager().getToolbarLayout().getLocationBar().addUrlFocusChangeListener(
                mTestObserver);
        TabModalPresenter presenter =
                (TabModalPresenter) mManager.getPresenterForTest(ModalDialogType.TAB);
        presenter.disableAnimationForTest();
    }

    @Test
    @SmallTest
    public void testOneDialog() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show a dialog. The pending list should be empty, and the dialog should be showing.
        // Browser controls should be restricted.
        showDialog(0, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dismiss the dialog by clicking positive button.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(not(hasDescendant(withText("0")))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testTwoDialogs() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show the first dialog.
        // The pending list should be empty, and the dialog should be showing.
        // The tab modal container shouldn't be in the window hierarchy when an app modal dialog is
        // showing.
        showDialog(0, ModalDialogType.APP);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("0")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Show the second dialog. It should be added to the pending list, and the first dialog
        // should still be shown.
        showDialog(1, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withText("0")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Dismiss the first dialog by clicking cancel. The second dialog should be removed from
        // pending list and shown immediately after.
        onView(withText(R.string.cancel)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("0")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(not(hasDescendant(withText("0"))), hasDescendant(withText("1")))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dismiss the second dialog by clicking ok. Browser controls should no longer be
        // restricted.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("0")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(
                        not(hasDescendant(withText("0"))), not(hasDescendant(withText("1"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testThreeDialogs() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show the first dialog.
        // The pending list should be empty, and the dialog should be showing.
        // Browser controls should be restricted.
        showDialog(0, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Show the second dialog. It should be added to the pending list, and the first dialog
        // should still be shown.
        showDialog(1, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("0")), not(hasDescendant(withText("1"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Show the third dialog (app modal). The first tab modal dialog should be back to the
        // pending list and this app modal dialog should be shown right after.
        showDialog(2, ModalDialogType.APP);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Simulate dismissing the dialog by non-user action. The second dialog should be removed
        // from pending list without showing.
        dismissDialog(1);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Dismiss the second dialog twice and verify nothing breaks.
        dismissDialog(1);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Dismiss the third dialog. The first dialog should be removed from pending list and
        // shown immediately after. The tab modal container shouldn't be in the window hierarchy
        // when an app modal dialog is showing.
        dismissDialog(2);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dismiss the first dialog by clicking OK. Browser controls should no longer be restricted.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(not(hasDescendant(withText("0"))),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testShow_ShowNext() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Insert two tab modal dialogs and two app modal dialogs for showing.
        showDialog(0, ModalDialogType.TAB);
        showDialog(1, ModalDialogType.TAB);
        showDialog(2, ModalDialogType.APP);
        showDialog(3, ModalDialogType.APP);

        // The first inserted app modal dialog is shown.
        checkPendingSize(ModalDialogType.APP, 1);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Dismiss the dialog by clicking OK. The second inserted app modal dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withText("3")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Dismiss the dialog by clicking OK. The first tab modal dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))),
                        not(hasDescendant(withText("3"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dismiss the dialog by clicking OK. The second tab modal dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("1")),
                        not(hasDescendant(withText("0"))), not(hasDescendant(withText("2"))),
                        not(hasDescendant(withText("3"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    public void testShow_ShowDialogAsNext() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Insert a tab modal dialog and a app modal dialog for showing.
        showDialog(0, ModalDialogType.TAB);
        showDialog(1, ModalDialogType.TAB);
        showDialog(2, ModalDialogType.APP);

        // The first inserted app modal dialog is shown.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Show a tab modal dialog as the next dialog to be shown. Verify that the app modal dialog
        // is still showing.
        showDialogAsNext(3, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 3);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Dismiss the dialog by clicking OK. The third tab modal dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("3")),
                        not(hasDescendant(withText("0"))), not(hasDescendant(withText("1"))),
                        not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dismiss the dialog by clicking OK. The first tab modal dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))),
                        not(hasDescendant(withText("3"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/812066")
    public void testShow_UrlBarFocused() throws Exception {
        // Show a dialog. The dialog should be shown on top of the toolbar.
        showDialog(0, ModalDialogType.TAB);

        TabModalPresenter presenter =
                (TabModalPresenter) mManager.getPresenterForTest(ModalDialogType.TAB);
        final View dialogContainer = presenter.getDialogContainerForTest();
        final View controlContainer = mActivity.findViewById(R.id.control_container);
        final ViewGroup containerParent = presenter.getContainerParentForTest();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(containerParent.indexOfChild(dialogContainer)
                    > containerParent.indexOfChild(controlContainer));
        });

        // When editing URL, it should be shown on top of the dialog.
        int callCount = mTestObserver.onUrlFocusChangedCallback.getCallCount();
        onView(withId(R.id.url_bar)).perform(click());
        mTestObserver.onUrlFocusChangedCallback.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(containerParent.indexOfChild(dialogContainer)
                    < containerParent.indexOfChild(controlContainer));
        });

        // When URL bar is not focused, the dialog should be shown on top of the toolbar again.
        callCount = mTestObserver.onUrlFocusChangedCallback.getCallCount();
        Espresso.pressBack();
        mTestObserver.onUrlFocusChangedCallback.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(containerParent.indexOfChild(dialogContainer)
                    > containerParent.indexOfChild(controlContainer));
        });

        // Dismiss the dialog by clicking OK.
        onView(withText(R.string.ok)).perform(click());
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "crbug.com/804858")
    public void testSuspend_ToggleOverview() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add two dialogs available for showing.
        showDialog(0, ModalDialogType.TAB);
        showDialog(1, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("0")), not(hasDescendant(withText("1"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        //  Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(
                        not(hasDescendant(withText("0"))), not(hasDescendant(withText("1"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Exit overview mode. The first dialog should be showing again.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("0")), not(hasDescendant(withText("1"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dismiss the first dialog. The second dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(not(hasDescendant(withText("0"))), hasDescendant(withText("1")))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "crbug.com/804858")
    public void testSuspend_ShowNext() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add a tab modal dialog available for showing.
        showDialog(0, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher. App modal dialog should
        // be showing.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);

        // An app modal dialog can be shown in tab switcher.
        showDialog(1, ModalDialogType.APP);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        onView(withText("1")).check(matches(isDisplayed()));
        checkCurrentPresenter(ModalDialogType.APP);

        // Close the app modal dialog and exit overview mode. The first dialog should be showing
        // again.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testSuspend_LastTabClosed() throws Exception {
        // Make sure there is only one opened tab.
        while (mActivity.getCurrentTabModel().getCount() > 1) {
            ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        }

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add a tab modal dialog available for showing.
        showDialog(0, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Close the only tab in the tab switcher.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testSuspend_TabClosed() throws Exception {
        mActivityTestRule.loadUrlInNewTab("about:blank");

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add a tab modal dialog available for showing.
        showDialog(0, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Close current tab in the tab switcher.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show a new tab modal dialog, and it should be suspended in tab switcher.
        showDialog(1, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show an app modal dialog. The app modal dialog should be shown.
        showDialog(2, ModalDialogType.APP);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withText("2")).check(matches(isDisplayed()));
        checkCurrentPresenter(ModalDialogType.APP);
    }

    @Test
    @SmallTest
    public void testDismiss_SwitchTab() throws Exception {
        // Open a new tab and make sure that the current tab is at index 0.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 0);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add a tab modal dialog available for showing.
        showDialog(0, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Dialog should be dismissed after switching to a different tab.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 1);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Open a tab modal dialog in the current tab. The dialog should be shown.
        showDialog(1, ModalDialogType.TAB);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    public void testDismiss_BackPressed() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add three dialogs available for showing. The app modal dialog should be shown first.
        showDialog(0, ModalDialogType.TAB);
        showDialog(1, ModalDialogType.TAB);
        showDialog(2, ModalDialogType.APP);
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 2);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogType.APP);

        // Perform back press. The app modal dialog should be dismissed.
        // The first tab modal dialog should be shown. The tab modal container shouldn't be in the
        // window hierarchy when an app modal dialog is showing.
        Espresso.pressBack();
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 1);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Perform a second back press. The first tab modal dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(not(hasDescendant(withText("0"))),
                        hasDescendant(withText("1")), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogType.TAB);

        // Perform a third back press. The second tab modal dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(ModalDialogType.APP, 0);
        checkPendingSize(ModalDialogType.TAB, 0);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(not(hasDescendant(withText("0"))),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testDismiss_DismissalCause_BackPressed() throws Exception {
        mExpectedDismissalCause = DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        showDialog(0, ModalDialogType.APP);
        showDialog(1, ModalDialogType.TAB);

        // Dismiss the app modal dialog and veirify dimissal cause.
        Espresso.pressBack();
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        // Dismiss the tab modal dialog and veirify dimissal cause.
        callCount = mTestObserver.onDialogDismissedCallback.getCallCount();
        Espresso.pressBack();
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    public void testDismiss_DismissalCause_TabSwitched() throws Exception {
        mExpectedDismissalCause = DialogDismissalCause.TAB_SWITCHED;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Open a new tab and make sure that the current tab is at index 0.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 0);

        // Show a tab modal dialog and then switch tab.
        showDialog(0, ModalDialogType.TAB);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 1);
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    public void testDismiss_DismissalCause_TabDestroyed() throws Exception {
        mExpectedDismissalCause = DialogDismissalCause.TAB_DESTROYED;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Show a tab modal dialog and then close tab.
        showDialog(0, ModalDialogType.TAB);
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    private ModalDialogView createDialog(final int index) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> {
            ModalDialogView.Controller controller = new ModalDialogView.Controller() {
                @Override
                public void onDismiss(@DialogDismissalCause int dismissalCause) {
                    mTestObserver.onDialogDismissed();
                    checkDialogDismissalCause(dismissalCause);
                }

                @Override
                public void onClick(int buttonType) {
                    switch (buttonType) {
                        case ModalDialogView.ButtonType.POSITIVE:
                        case ModalDialogView.ButtonType.NEGATIVE:
                            dismissDialog(index);
                            break;
                        default:
                            Assert.fail("Unknown button type: " + buttonType);
                    }
                }
            };
            final ModalDialogView.Params p = new ModalDialogView.Params();
            p.title = Integer.toString(index);
            p.positiveButtonTextId = R.string.ok;
            p.negativeButtonTextId = R.string.cancel;
            return new ModalDialogView(controller, p);
        });
    }

    private void showDialog(
            final int index, final @ModalDialogManager.ModalDialogType int dialogType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mManager.showDialog(mModalDialogViews[index], dialogType));
    }

    private void showDialogAsNext(
            final int index, final @ModalDialogManager.ModalDialogType int dialogType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mManager.showDialog(mModalDialogViews[index], dialogType, true));
    }

    private void dismissDialog(final int index) {
        ThreadUtils.runOnUiThreadBlocking(() -> mManager.dismissDialog(mModalDialogViews[index]));
    }

    private void checkPendingSize(
            final @ModalDialogManager.ModalDialogType int dialogType, final int expected) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            List list = mManager.getPendingDialogsForTest(dialogType);
            Assert.assertEquals(expected, list != null ? list.size() : 0);
        });
    }

    private void checkCurrentPresenter(final Integer dialogType) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (dialogType == null) {
                Assert.assertFalse(mManager.isShowing());
                Assert.assertNull(mManager.getCurrentPresenterForTest());
            } else {
                Assert.assertTrue(mManager.isShowing());
                Assert.assertEquals(mManager.getPresenterForTest(dialogType),
                        mManager.getCurrentPresenterForTest());
            }
        });
    }

    private void checkBrowserControls(boolean restricted) {
        if (restricted) {
            Assert.assertTrue("All tabs should be obscured", mActivity.isViewObscuringAllTabs());
            onView(withId(R.id.menu_button)).check(matches(not(isEnabled())));
        } else {
            Assert.assertFalse("Tabs shouldn't be obscured", mActivity.isViewObscuringAllTabs());
            onView(withId(R.id.menu_button)).check(matches(isEnabled()));
        }
    }

    private void checkDialogDismissalCause(int dismissalCause) {
        if (mExpectedDismissalCause == null) return;
        Assert.assertEquals(mExpectedDismissalCause.intValue(), dismissalCause);
    }
}
