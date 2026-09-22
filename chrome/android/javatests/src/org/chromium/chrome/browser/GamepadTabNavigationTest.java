// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.device.gamepad.GamepadList;

import java.util.concurrent.TimeoutException;

/**
 * End-to-end browser tests for Gamepad L1/R1 tab switching behavior and its interaction with the
 * HTML5 Gamepad API (b/560325665).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests activate process-level Gamepad API state via navigator.getGamepads().")
public class GamepadTabNavigationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private void dispatchKeyEvent(int keyCode) {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        long now = SystemClock.uptimeMillis();
        KeyEvent downEvent = new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0);
        downEvent.setSource(InputDevice.SOURCE_GAMEPAD);
        KeyEvent upEvent = new KeyEvent(now, now, KeyEvent.ACTION_UP, keyCode, 0);
        upEvent.setSource(InputDevice.SOURCE_GAMEPAD);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.dispatchKeyEvent(downEvent);
                    activity.dispatchKeyEvent(upEvent);
                });
    }

    private void dispatchKeyboardShortcut(int keyCode, int metaState) {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        long now = SystemClock.uptimeMillis();
        KeyEvent downEvent = new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0, metaState);
        KeyEvent upEvent = new KeyEvent(now, now, KeyEvent.ACTION_UP, keyCode, 0, metaState);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.dispatchKeyEvent(downEvent);
                    activity.dispatchKeyEvent(upEvent);
                });
    }

    private static int getTabCount(TabModel tabModel) {
        return ThreadUtils.runOnUiThreadBlocking(tabModel::getCount);
    }

    private static int getTabIndex(TabModel tabModel) {
        return ThreadUtils.runOnUiThreadBlocking(tabModel::index);
    }

    @Test
    @MediumTest
    @Feature({"Gamepad", "TabNavigation"})
    public void testL1R1SwitchesTabs_whenGamepadApiInactive() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/google.html"));

        TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
        assertEquals(2, getTabCount(tabModel));
        assertEquals(1, getTabIndex(tabModel));

        assertFalse(
                "Gamepad API should not be active on blank page",
                ThreadUtils.runOnUiThreadBlocking(GamepadList::isGamepadAPIActive));

        // Press L1 (previous tab) -> should switch from tab 1 to tab 0.
        dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_L1);
        CriteriaHelper.pollUiThread(
                () -> tabModel.index() == 0,
                "L1 button should switch to previous tab when Gamepad API is inactive");

        // Press R1 (next tab) -> should switch from tab 0 to tab 1.
        dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_R1);
        CriteriaHelper.pollUiThread(
                () -> tabModel.index() == 1,
                "R1 button should switch to next tab when Gamepad API is inactive");
    }

    @Test
    @MediumTest
    @Feature({"Gamepad", "TabNavigation"})
    public void testL1R1DoesNotSwitchTabs_whenGamepadApiActive() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/google.html"));

        TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
        assertEquals(2, getTabCount(tabModel));
        assertEquals(1, getTabIndex(tabModel));

        // Activate Gamepad API via JavaScript getGamepads() call.
        mActivityTestRule.runJavaScriptCodeInCurrentTab("navigator.getGamepads()");

        // Wait for the asynchronous Mojo/JNI pipeline to set GamepadList.isGamepadAPIActive() to
        // true.
        CriteriaHelper.pollUiThread(
                GamepadList::isGamepadAPIActive,
                "Gamepad API failed to become active after navigator.getGamepads()");

        // Press L1 (previous tab) -> must NOT switch tabs.
        dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_L1);
        assertEquals(
                "L1 button must not switch tabs when Gamepad API is active",
                1,
                getTabIndex(tabModel));

        // Press R1 (next tab) -> must NOT switch tabs.
        dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_R1);
        assertEquals(
                "R1 button must not switch tabs when Gamepad API is active",
                1,
                getTabIndex(tabModel));

        // Positive control: Verify tab index is still 1 before testing standard keyboard shortcut.
        assertEquals(
                "Tab index should remain 1 before keyboard shortcut test",
                1,
                getTabIndex(tabModel));

        // Regular keyboard shortcuts (Ctrl+Shift+Tab) should still switch tabs.
        dispatchKeyboardShortcut(
                KeyEvent.KEYCODE_TAB, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);
        CriteriaHelper.pollUiThread(
                () -> tabModel.index() == 0,
                "Ctrl+Shift+Tab should switch to previous tab even when Gamepad API is active");
    }
}
