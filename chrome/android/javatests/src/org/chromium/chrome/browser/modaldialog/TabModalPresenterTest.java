// Copyright 2018 The Chromium Authors. All rights reserved.
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
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.checkCurrentPresenter;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.checkDialogDismissalCause;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.checkPendingSize;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.createDialog;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.showDialog;

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
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for {@link TabModalPresenter}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabModalPresenterTest {
    private class TestObserver extends EmptyTabObserver
            implements UrlFocusChangeListener, ModalDialogTestUtils.TestDialogDismissedObserver {
        public final CallbackHelper onUrlFocusChangedCallback = new CallbackHelper();
        public final CallbackHelper onDialogDismissedCallback = new CallbackHelper();
        public final CallbackHelper onTabInteractabilityChangedCallback = new CallbackHelper();

        @Override
        public void onUrlFocusChange(boolean hasFocus) {
            onUrlFocusChangedCallback.notifyCalled();
        }

        @Override
        public void onInteractabilityChanged(boolean isInteractable) {
            onTabInteractabilityChangedCallback.notifyCalled();
        }

        @Override
        public void onDialogDismissed(int dismissalCause) {
            onDialogDismissedCallback.notifyCalled();
            checkDialogDismissalCause(mExpectedDismissalCause, dismissalCause);
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mActivity;
    private ModalDialogManager mManager;
    private TabModalPresenter mTabModalPresenter;
    private TestObserver mTestObserver;
    private Integer mExpectedDismissalCause;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mManager = mActivity.getModalDialogManager();
        mTestObserver = new TestObserver();
        mActivity.getToolbarManager()
                .getToolbarLayoutForTesting()
                .getLocationBar()
                .addUrlFocusChangeListener(mTestObserver);
        mTabModalPresenter = (TabModalPresenter) mManager.getPresenterForTest(ModalDialogType.TAB);
        mTabModalPresenter.disableAnimationForTest();
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testShow_UrlBarFocused() throws Exception {
        // Show a tab modal dialog. The dialog should be shown on top of the toolbar.
        PropertyModel dialog1 = createDialog(mActivity, "1", null);
        showDialog(mManager, dialog1, ModalDialogType.TAB);

        TabModalPresenter presenter =
                (TabModalPresenter) mManager.getPresenterForTest(ModalDialogType.TAB);
        final View dialogContainer = presenter.getDialogContainerForTest();
        final View controlContainer = mActivity.findViewById(R.id.control_container);
        final ViewGroup containerParent = presenter.getContainerParentForTest();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(containerParent.indexOfChild(dialogContainer)
                    > containerParent.indexOfChild(controlContainer));
        });

        // When editing URL, it should be shown on top of the dialog.
        UrlBar urlBar = mActivity.findViewById(R.id.url_bar);
        int callCount = mTestObserver.onUrlFocusChangedCallback.getCallCount();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        mTestObserver.onUrlFocusChangedCallback.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(containerParent.indexOfChild(dialogContainer)
                    < containerParent.indexOfChild(controlContainer));
        });

        // When URL bar is not focused, the dialog should be shown on top of the toolbar again.
        callCount = mTestObserver.onUrlFocusChangedCallback.getCallCount();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        mTestObserver.onUrlFocusChangedCallback.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(containerParent.indexOfChild(dialogContainer)
                    > containerParent.indexOfChild(controlContainer));
        });

        // Dismiss the dialog by clicking OK.
        onView(withText(R.string.ok)).perform(click());
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testSuspend_ToggleOverview() throws Exception {
        mActivity.getActivityTab().addObserver(mTestObserver);
        PropertyModel dialog1 = createDialog(mActivity, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, "2", null);
        PropertyModel dialog3 = createDialog(mActivity, "3", null);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Add two tab modal dialogs available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("1")), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        //  Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(mManager, ModalDialogType.TAB, 2);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // An app modal dialog can be shown in tab switcher.
        showDialog(mManager, dialog3, ModalDialogType.APP);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 2);
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        onView(withText("3")).check(matches(isDisplayed()));
        checkCurrentPresenter(mManager, ModalDialogType.APP);

        // Close the app modal dialog and verify that the tab modal dialog is still queued.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 2);

        // Exit overview mode. The first dialog should be showing again.
        int callCount = mTestObserver.onTabInteractabilityChangedCallback.getCallCount();
        onView(withId(R.id.tab_switcher_mode_tab_switcher_button)).perform(click());
        mTestObserver.onTabInteractabilityChangedCallback.waitForCallback(callCount);

        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("1")), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Dismiss the first dialog. The second dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(not(hasDescendant(withText("1"))), hasDescendant(withText("2")))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Reset states.
        mActivity.getActivityTab().removeObserver(mTestObserver);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testSuspend_LastTabClosed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", null);

        // Make sure there is only one opened tab.
        while (mActivity.getCurrentTabModel().getCount() > 1) {
            ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        }

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Add a tab modal dialog available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Close the only tab in the tab switcher. Verify that the queued tab modal dialogs are
        // cleared.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testSuspend_TabClosed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, "2", null);
        PropertyModel dialog3 = createDialog(mActivity, "3", null);
        mActivityTestRule.loadUrlInNewTab("about:blank");

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Add a tab modal dialog available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Close current tab in the tab switcher.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Show a new tab modal dialog, and it should be suspended in tab switcher.
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Show an app modal dialog. The app modal dialog should be shown.
        showDialog(mManager, dialog3, ModalDialogType.APP);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        onView(withText("3")).check(matches(isDisplayed()));
        checkCurrentPresenter(mManager, ModalDialogType.APP);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_SwitchTab() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, "2", null);

        // Open a new tab and make sure that the current tab is at index 0.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 0);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Add a tab modal dialog available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Dialog should be dismissed after switching to a different tab.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 1);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Open a tab modal dialog in the current tab. The dialog should be shown.
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("2"))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_BackPressed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, "2", null);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);

        // Add two tab modal dialogs available for showing. The first dialog should be shown first.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Perform a back press. The first tab modal dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(not(hasDescendant(withText("1"))), hasDescendant(withText("2")))));
        checkBrowserControls(true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Perform a second back press. The second tab modal dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_CancelOnTouchOutside() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", null);

        // Show a tab modal dialog and verify it shows.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Click dialog container and verify the dialog is not dismissed.
        final View dialogContainer = mTabModalPresenter.getDialogContainerForTest();
        assertTrue(dialogContainer.isClickable());
        TestThreadUtils.runOnUiThreadBlocking(dialogContainer::performClick);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Enable cancel on touch outside and verify the dialog is dismissed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> dialog1.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true));
        assertTrue(dialogContainer.isClickable());
        TestThreadUtils.runOnUiThreadBlocking(dialogContainer::performClick);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(not(hasDescendant(withText("1")))));
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_BackPressed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;

        showDialog(mManager, dialog1, ModalDialogType.TAB);

        // Dismiss the tab modal dialog and verify dismissal cause.
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();
        Espresso.pressBack();
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_TabSwitched() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.TAB_SWITCHED;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Open a new tab and make sure that the current tab is at index 0.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 0);

        // Show a tab modal dialog and then switch tab.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 1);
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_TabDestroyed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.TAB_DESTROYED;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Show a tab modal dialog and then close tab.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    private void checkBrowserControls(boolean restricted) {
        if (restricted) {
            assertTrue("All tabs should be obscured", mActivity.isViewObscuringAllTabs());
            onView(allOf(isDisplayed(), withId(R.id.menu_button))).check(matches(not(isEnabled())));
        } else {
            Assert.assertFalse("Tabs shouldn't be obscured", mActivity.isViewObscuringAllTabs());
            onView(allOf(isDisplayed(), withId(R.id.menu_button))).check(matches(isEnabled()));
        }
    }
}
