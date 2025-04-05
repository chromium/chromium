// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tab.TabObscuringHandler.Target.ALL_TABS_AND_TOOLBAR;
import static org.chromium.ui.modaldialog.DialogDismissalCause.UNKNOWN;
import static org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType.APP;
import static org.chromium.ui.modaldialog.ModalDialogProperties.ALL_KEYS;
import static org.chromium.ui.modaldialog.ModalDialogProperties.CONTROLLER;

import android.view.View;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.accessibility.KeyboardFocusRow;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Tests for {@link KeyboardFocusRowManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class KeyboardFocusRowManagerTest {

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mockito = MockitoJUnit.rule(); // todo delete if not needed

    private ChromeTabbedActivity mActivity;
    private KeyboardFocusRowManager mKeyboardFocusRowManager;
    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @BeforeClass
    public static void setUpClass() {
        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        sActivityTestRule.startMainActivityOnBlankPage();
    }

    @Before
    public void setUp() {
        mActivity = sActivityTestRule.getActivity();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator)
                        sActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        mKeyboardFocusRowManager = mTabbedRootUiCoordinator.getKeyboardFocusRowManagerForTesting();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature("KeyboardShortcuts")
    public void testSwitchKeyboardFocusRow_withTabletTabStrip() {
        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Switch the first time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on toolbar after 1st invocation of keyboard focus row switch",
                KeyboardFocusRow.TOOLBAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 2nd time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on tab strip after 2nd keyboard focus row switch",
                KeyboardFocusRow.TAB_STRIP,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 3rd time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to not be in top rows after 3rd keyboard focus row switch",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature("KeyboardShortcuts")
    public void testSwitchKeyboardFocusRow_withoutTabletTabStrip() {
        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Switch the first time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on toolbar after 1st invocation of keyboard focus row switch",
                KeyboardFocusRow.TOOLBAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 2nd time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to not be in top rows after 2nd keyboard focus row switch",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature("KeyboardShortcuts")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testSwitchKeyboardFocusRow_withBookmarksBar() {
        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Switch the first time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on toolbar after 1st invocation of keyboard focus row switch",
                KeyboardFocusRow.TOOLBAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 2nd time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on tab strip after 2nd keyboard focus row switch",
                KeyboardFocusRow.TAB_STRIP,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 3rd time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on bookmarks bar after 3rd keyboard focus row switch",
                KeyboardFocusRow.BOOKMARKS_BAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 4th time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to not be in top rows after 4th keyboard focus row switch",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    @Feature("KeyboardShortcuts")
    @Restriction(DeviceFormFactor.TABLET)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testSwitchKeyboardFocusRow_withBookmarkBarFocus() {
        ThreadUtils.runOnUiThreadBlocking(
                mTabbedRootUiCoordinator::initializeBookmarkBarCoordinatorForTesting);

        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Start out by using the keyboard shortcut to switch focus rows.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));

        // Focus directly on bookmarks bar with shortcut even though it's not next in cycle order.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.focus_bookmarks, false));
        assertEquals(
                "Expected focus to be on bookmarks bar after focus_bookmarks",
                KeyboardFocusRow.BOOKMARKS_BAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Now switch and make sure we appropriately switch given our new cycle position.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to not be on top controls after focus row switch",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    @Feature("KeyboardShortcuts")
    @Restriction(DeviceFormFactor.TABLET)
    public void testSkipStripIfHidden() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivity
                                .getLayoutManager()
                                .getStripLayoutHelperManager()
                                .setIsTabStripHiddenByHeightTransition(true));

        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Switch the first time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to be on toolbar after 1st invocation of keyboard focus row switch",
                KeyboardFocusRow.TOOLBAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Switch a 2nd time.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        assertEquals(
                "Expected focus to not be in top rows after 2nd keyboard focus row switch",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    @Feature("KeyboardShortcuts")
    public void testCannotSwitchKeyboardFocusRow_whenObscured() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabObscuringHandler.Token obscuringToken =
                            mActivity.getTabObscuringHandler().obscure(ALL_TABS_AND_TOOLBAR);

                    // Try to switch focus rows.
                    mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false);
                    // Assert we haven't moved focus.
                    assertEquals(
                            "Expected no keyboard focus row switch if top controls are hidden",
                            KeyboardFocusRow.NONE,
                            mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

                    // Clean up.
                    mActivity.getTabObscuringHandler().unobscure(obscuringToken);
                });
    }

    @Test
    @SmallTest
    @Feature("KeyboardShortcuts")
    public void testCannotSwitchKeyboardFocusRow_whenAppModalOpen() throws TimeoutException {
        Controller controller =
                new Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {}

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {}
                };

        ModalDialogManager modalDialogManager = mActivity.getModalDialogManager();

        // Set a callback helper so we know when the dialog-shown callbacks are called.
        CallbackHelper callbackHelper = new CallbackHelper();
        ModalDialogManagerObserver observer =
                new ModalDialogManagerObserver() {
                    @Override
                    public void onDialogShown(View dialogView) {
                        callbackHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Property model needs to be created on the same thread where it's used.
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(ALL_KEYS)
                                    .with(CONTROLLER, controller)
                                    .build();

                    modalDialogManager.addObserver(observer);
                    modalDialogManager.showDialog(propertyModel, APP);
                });

        // Wait for the callback outside of the UI thread (otherwise we deadlock).
        callbackHelper.waitForOnly();

        // Try to switch focus rows.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
        // Assert we haven't moved focus.
        assertEquals(
                "Expected no keyboard focus row switch if app modal is open",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());

        // Clean up.
        ThreadUtils.runOnUiThreadBlocking(() -> modalDialogManager.dismissAllDialogs(UNKNOWN));
    }
}
