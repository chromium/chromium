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

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
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
@DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
public class KeyboardFocusRowManagerTest {

    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mActivityTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextRule =
            new OverrideContextWrapperTestRule();

    @Rule public MockitoRule mockito = MockitoJUnit.rule(); // todo delete if not needed

    private WebPageStation mPage;
    private ChromeTabbedActivity mActivity;
    private KeyboardFocusRowManager mKeyboardFocusRowManager;
    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @BeforeClass
    public static void setUpClass() {
        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);

        // Explicitly override FeatureParam for consistency.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", true);
        overrides.apply();
    }

    @Before
    public void setUp() {
        mPage = mActivityTestRule.start();
        mActivity = mPage.getActivity();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mActivity.getRootUiCoordinatorForTesting();
        mKeyboardFocusRowManager = mTabbedRootUiCoordinator.getKeyboardFocusRowManagerForTesting();
        mOverrideContextRule.setIsDesktop(true);
    }

    @After
    public void tearDown() {
        setUserPrefsShowBookmarksBar(false);
        setBookmarkBarFeatureParam(false);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @Feature("KeyboardShortcuts")
    public void testSwitchKeyboardFocusRow_withTabletTabStrip() {
        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Switch the first time.
        switchRow();
        assertOnToolbar();

        // Switch a 2nd time.
        switchRow();
        assertOnTabStrip();

        // Switch a 3rd time.
        switchRow();
        assertOnNone();
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
        switchRow();
        assertOnToolbar();

        // Switch a 2nd time.
        switchRow();
        assertOnNone();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @Feature("KeyboardShortcuts")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testSwitchKeyboardFocusRow_withBookmarksBar() {
        setBookmarkBarFeatureParam(true);
        setUserPrefsShowBookmarksBar(true);

        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Switch the first time.
        switchRow();
        assertOnToolbar();

        // Switch a 2nd time.
        switchRow();
        assertOnTabStrip();

        // Switch a 3rd time.
        switchRow();
        assertOnBookmarksBar();

        // Switch a 4th time.
        switchRow();
        assertOnNone();
    }

    @Test
    @SmallTest
    @Feature("KeyboardShortcuts")
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testSwitchKeyboardFocusRow_withBookmarkBarFocus() {
        setBookmarkBarFeatureParam(true);
        setUserPrefsShowBookmarksBar(true);

        ThreadUtils.runOnUiThreadBlocking(
                mTabbedRootUiCoordinator::initializeBookmarkBarCoordinatorForTesting);

        // Put something in the content view so we can focus on it.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);

        // Start out by using the keyboard shortcut to switch focus rows.
        switchRow();

        // Focus directly on bookmarks bar with shortcut even though it's not next in cycle order.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.focus_bookmarks, false));
        assertOnBookmarksBar();

        // Now switch and make sure we appropriately switch given our new cycle position.
        switchRow();
        assertOnNone();
    }

    @Test
    @SmallTest
    @Feature("KeyboardShortcuts")
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
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
        switchRow();
        assertOnToolbar();

        // Switch a 2nd time.
        switchRow();
        assertOnNone();
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
        switchRow();
        // Assert we haven't moved focus.
        assertOnNone();

        // Clean up.
        ThreadUtils.runOnUiThreadBlocking(() -> modalDialogManager.dismissAllDialogs(UNKNOWN));
    }

    // Helper methods for readability

    private void switchRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.onMenuOrKeyboardAction(R.id.switch_keyboard_focus_row, false));
    }

    private void assertOnToolbar() {
        assertEquals(
                "Expected focus to be on toolbar after invocation of keyboard focus row switch",
                KeyboardFocusRow.TOOLBAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    private void assertOnTabStrip() {
        assertEquals(
                "Expected focus to be on tab strip after invocation of keyboard focus row switch",
                KeyboardFocusRow.TAB_STRIP,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    private void assertOnBookmarksBar() {
        assertEquals(
                "Expected focus to be on bookmarks bar after invocation of keyboard focus row"
                    + " switch",
                KeyboardFocusRow.BOOKMARKS_BAR,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    private void assertOnNone() {
        assertEquals(
                "Expected focus to be on none after invocation of keyboard focus row switch",
                KeyboardFocusRow.NONE,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    private void setUserPrefsShowBookmarksBar(boolean showBookmarksBar) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                                mActivity.getProfileProviderSupplier().get().getOriginalProfile(),
                                showBookmarksBar,
                                /* fromKeyboardShortcut= */ false));
    }

    private void setBookmarkBarFeatureParam(boolean param) {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", param);
        overrides.apply();
    }
}
