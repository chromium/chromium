// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkCurrentPresenter;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkDialogDismissalCause;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkPendingSize;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.createDialog;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.showDialog;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ChromeTabModalPresenter}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeTabModalPresenterTest {
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
        public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
            onTabInteractabilityChangedCallback.notifyCalled();
        }

        @Override
        public void onDialogDismissed(int dismissalCause) {
            onDialogDismissedCallback.notifyCalled();
            checkDialogDismissalCause(mExpectedDismissalCause, dismissalCause);
        }
    }

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private ChromeTabbedActivity mActivity;
    private ModalDialogManager mManager;
    private ChromeTabModalPresenter mTabModalPresenter;
    private TestObserver mTestObserver;
    private Integer mExpectedDismissalCause;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() {
        mActivity = sActivityTestRule.getActivity();
        mOmnibox = new OmniboxTestUtils(mActivity);
        mManager = ThreadUtils.runOnUiThreadBlocking(mActivity::getModalDialogManager);
        mTestObserver = new TestObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getToolbarManager()
                            .getToolbarLayoutForTesting()
                            .getLocationBar()
                            .getOmniboxStub()
                            .addUrlFocusChangeListener(mTestObserver);
                });
        mTabModalPresenter =
                (ChromeTabModalPresenter) mManager.getPresenterForTest(ModalDialogType.TAB);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testShow_UrlBarFocused() throws Exception {
        // Show a tab modal dialog. The dialog should be shown on top of the toolbar.
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        showDialog(mManager, dialog1, ModalDialogType.TAB);

        final View dialogContainer = mTabModalPresenter.getDialogContainerForTest();
        final View controlContainer = mActivity.findViewById(R.id.control_container);
        final ViewGroup containerParent = mTabModalPresenter.getContainerParentForTest();

        ensureDialogContainerVisible();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(
                            containerParent.indexOfChild(dialogContainer),
                            Matchers.greaterThan(containerParent.indexOfChild(controlContainer)));
                });

        // When editing URL, it should be shown on top of the dialog.
        UrlBar urlBar = mActivity.findViewById(R.id.url_bar);
        int callCount = mTestObserver.onUrlFocusChangedCallback.getCallCount();
        mOmnibox.requestFocus();
        mTestObserver.onUrlFocusChangedCallback.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(
                            containerParent.indexOfChild(dialogContainer),
                            Matchers.lessThan(containerParent.indexOfChild(controlContainer)));
                });

        // When URL bar is not focused, the dialog should be shown on top of the toolbar again.
        callCount = mTestObserver.onUrlFocusChangedCallback.getCallCount();
        mOmnibox.clearFocus();
        mTestObserver.onUrlFocusChangedCallback.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(
                            containerParent.indexOfChild(dialogContainer),
                            Matchers.greaterThan(containerParent.indexOfChild(controlContainer)));
                });

        // Dismiss the dialog by clicking OK.
        onView(withText(R.string.ok)).perform(click());
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "https://crbug.com/1420186")
    public void testSuspend_ToggleOverview() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getActivityTab().addObserver(mTestObserver));
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, mManager, "2", null);
        PropertyModel dialog3 = createDialog(mActivity, mManager, "3", null);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Add two tab modal dialogs available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        onViewWaiting(withId(R.id.tab_modal_dialog_container))
                .check(
                        matches(
                                allOf(
                                        hasDescendant(withText("1")),
                                        not(hasDescendant(withText("2"))))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        //  Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(mManager, ModalDialogType.TAB, 2);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(
                        matches(
                                allOf(
                                        not(hasDescendant(withText("1"))),
                                        not(hasDescendant(withText("2"))))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
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
        pressBack();
        mTestObserver.onTabInteractabilityChangedCallback.waitForCallback(callCount);

        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        ensureDialogContainerVisible();
        onView(withId(R.id.tab_modal_dialog_container))
                .check(
                        matches(
                                allOf(
                                        hasDescendant(withText("1")),
                                        not(hasDescendant(withText("2"))))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Dismiss the first dialog. The second dialog should be shown.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(
                        matches(
                                allOf(
                                        not(hasDescendant(withText("1"))),
                                        hasDescendant(withText("2")))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Reset states.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getActivityTab().removeObserver(mTestObserver));
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testSuspend_LastTabClosed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);

        // Make sure there is only one opened tab.
        while (mActivity.getCurrentTabModel().getCount() > 1) {
            ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        }

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Add a tab modal dialog available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ensureDialogContainerVisible();
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Close the only tab in the tab switcher. Verify that the queued tab modal dialogs are
        // cleared.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @DisabledTest(message = "https://crbug.com/1382221")
    @Restriction(DeviceFormFactor.PHONE)
    public void testSuspend_TabClosed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, mManager, "2", null);
        PropertyModel dialog3 = createDialog(mActivity, mManager, "3", null);
        sActivityTestRule.loadUrlInNewTab("about:blank");

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Add a tab modal dialog available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ensureDialogContainerVisible();
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Tab modal dialogs should be suspended on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Close current tab in the tab switcher.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Show a new tab modal dialog, and it should be suspended in tab switcher.
        showDialog(mManager, dialog2, ModalDialogType.TAB, false);
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
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
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, mManager, "2", null);

        // Open a new tab and make sure that the current tab is at index 0.
        sActivityTestRule.loadUrlInNewTab("about:blank");
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 0);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Add a tab modal dialog available for showing.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ensureDialogContainerVisible();
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Dialog should be dismissed after switching to a different tab.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivity, 1);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Open a tab modal dialog in the current tab. The dialog should be shown.
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ensureDialogContainerVisible();
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("2"))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Features.DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testDismiss_BackPressed() {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, mManager, "2", null);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);

        // Add two tab modal dialogs available for showing. The first dialog should be shown first.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        showDialog(mManager, dialog2, ModalDialogType.TAB);
        checkPendingSize(mManager, ModalDialogType.TAB, 1);
        onViewWaiting(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Perform a back press. The first tab modal dialog should be dismissed.
        pressBack();
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(
                        matches(
                                allOf(
                                        not(hasDescendant(withText("1"))),
                                        hasDescendant(withText("2")))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, true);
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Perform a second back press. The second tab modal dialog should be dismissed.
        pressBack();
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkPendingSize(mManager, ModalDialogType.TAB, 0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(
                        matches(
                                allOf(
                                        not(hasDescendant(withText("1"))),
                                        not(hasDescendant(withText("2"))))));
        ChromeModalDialogTestUtils.checkBrowserControls(mActivity, false);
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Features.EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testDismiss_BackPressed_BackPressRefactor() {
        testDismiss_BackPressed();
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_CancelOnTouchOutside() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);

        // Show a tab modal dialog and verify it shows.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        onViewWaiting(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Click dialog container and verify the dialog is not dismissed.
        final View dialogContainer = mTabModalPresenter.getDialogContainerForTest();
        assertTrue(dialogContainer.isClickable());
        ThreadUtils.runOnUiThreadBlocking(dialogContainer::performClick);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("1"))));
        checkCurrentPresenter(mManager, ModalDialogType.TAB);

        // Enable cancel on touch outside and verify the dialog is dismissed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> dialog1.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true));
        assertTrue(dialogContainer.isClickable());
        ThreadUtils.runOnUiThreadBlocking(dialogContainer::performClick);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(not(hasDescendant(withText("1")))));
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Features.DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testDismiss_DismissalCause_BackPressed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.NAVIGATE_BACK;

        showDialog(mManager, dialog1, ModalDialogType.TAB);

        // Dismiss the tab modal dialog and verify dismissal cause.
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();
        pressBack();
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    @Features.EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testDismiss_DismissalCause_BackPressed_BackPressRefactor() throws Exception {
        testDismiss_DismissalCause_BackPressed();
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_TabSwitched() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.TAB_SWITCHED;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Open a new tab and make sure that the current tab is at index 0.
        sActivityTestRule.loadUrlInNewTab("about:blank");
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
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.TAB_DESTROYED;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Show a tab modal dialog and then close tab.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_TabNavigated() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.NAVIGATE;
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();

        // Show a tab modal dialog and then navigate to a different page.
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        EmbeddedTestServer server =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        sActivityTestRule.loadUrl(server.getURL("/chrome/test/data/android/simple.html"));
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testBrowserControlContraints_ShowHide() {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        Assert.assertEquals(BrowserControlsState.BOTH, getBrowserControlsConstraints());
        showDialog(mManager, dialog1, ModalDialogType.TAB);
        Assert.assertEquals(BrowserControlsState.SHOWN, getBrowserControlsConstraints());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mManager.dismissDialog(
                                dialog1, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));
        Assert.assertEquals(BrowserControlsState.BOTH, getBrowserControlsConstraints());
    }

    @Test
    @SmallTest
    @RequiresRestart("Removing views is global and cannot be reversed.")
    @Feature({"ModalDialog"})
    // Ensures an exception isn't thrown when a dialog is dismissed and the View is no longer
    // attached to a Window. See https://crbug.com/1127254 for the specifics.
    public void testDismissAfterRemovingView() throws Throwable {
        PropertyModel dialog1 = createDialog(mActivity, mManager, "1", null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.showDialog(dialog1, ModalDialogType.TAB);
                    ViewGroup containerParent =
                            (ViewGroup) mTabModalPresenter.getContainerParentForTest();
                    // This is a bit hacky and intended to correspond to a case where the hosting
                    // ViewGroup is no longer attached to a Window.
                    containerParent.removeAllViews();
                    mManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
    }

    private @BrowserControlsState int getBrowserControlsConstraints() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mTabModalPresenter.getBrowserControlsVisibilityDelegate().get());
    }

    private void ensureDialogContainerVisible() {
        final View dialogContainer = mTabModalPresenter.getDialogContainerForTest();
        onViewWaiting(allOf(is(dialogContainer), isDisplayed()));
    }

    private void pressBack() {
        ThreadUtils.runOnUiThreadBlocking(mActivity.getOnBackPressedDispatcher()::onBackPressed);
    }
}
