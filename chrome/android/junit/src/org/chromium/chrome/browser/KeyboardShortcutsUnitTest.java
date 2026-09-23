// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.device.gamepad.GamepadList;

/** Unit tests for {@link KeyboardShortcuts} redirection. */
@RunWith(BaseRobolectricTestRunner.class)
public class KeyboardShortcutsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MenuOrKeyboardActionController mController;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private Profile mProfile;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testOpenNewWindow_RedirectsToIncognito_WhenForced() {
        doReturn(true).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());

        assertTrue(onKeyDown(KeyEvent.KEYCODE_N, KeyEvent.META_CTRL_ON));

        verify(mController)
                .onMenuOrKeyboardAction(eq(R.id.new_incognito_window_menu_id), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testOpenNewTab_RedirectsToIncognito_WhenForced() {
        doReturn(true).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());
        when(mTabModel.isIncognito()).thenReturn(false);

        assertTrue(onKeyDown(KeyEvent.KEYCODE_T, KeyEvent.META_CTRL_ON));

        verify(mController).onMenuOrKeyboardAction(eq(R.id.new_incognito_tab_menu_id), eq(false));
    }

    @Test
    public void testQuitChrome_TriggersMenuOrKeyboardAction() {
        doReturn(true).when(mController).onMenuOrKeyboardAction(eq(R.id.quit_chrome), eq(false));

        assertTrue(onKeyDown(KeyEvent.KEYCODE_Q, KeyEvent.META_CTRL_ON));

        verify(mController).onMenuOrKeyboardAction(eq(R.id.quit_chrome), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_KEYBOARD_SHORTCUT_OPEN_FILE)
    public void testOpenFile_CtrlO_TriggersOpenFileDialog() {
        KeyEvent event =
                new KeyEvent(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_O,
                        0,
                        KeyEvent.META_CTRL_ON);

        assertTrue(
                KeyboardShortcuts.onKeyDown(
                        event, true, true, mTabModelSelector, mController, mToolbarManager));

        verify(mController).onMenuOrKeyboardAction(eq(R.id.open_file_id), eq(false));
    }

    @Test
    public void testGamepadR1_WhenGamepadApiActive_DoesNotMoveTab() {
        GamepadList.setGamepadApiActiveForTesting(true);

        assertNull(dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_R1));
        verify(mController, never()).onMenuOrKeyboardAction(anyInt(), anyBoolean());
    }

    @Test
    public void testGamepadL1_WhenGamepadApiActive_DoesNotMoveTab() {
        GamepadList.setGamepadApiActiveForTesting(true);

        assertNull(dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_L1));
        verify(mController, never()).onMenuOrKeyboardAction(anyInt(), anyBoolean());
    }

    @Test
    public void testGamepadR1_WhenGamepadApiInactive_MovesToNextTab() {
        GamepadList.setGamepadApiActiveForTesting(false);

        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_R1));
        verify(mController).onMenuOrKeyboardAction(eq(R.id.select_next_tab), eq(false));
    }

    @Test
    public void testGamepadL1_WhenGamepadApiInactive_MovesToPreviousTab() {
        GamepadList.setGamepadApiActiveForTesting(false);

        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_BUTTON_L1));
        verify(mController).onMenuOrKeyboardAction(eq(R.id.select_previous_tab), eq(false));
    }

    @Test
    public void testGamepadButtonB_OnKeyDown_WhenGamepadApiActive_ReturnsFalse() {
        GamepadList.setGamepadApiActiveForTesting(true);

        assertFalse(onKeyDown(KeyEvent.KEYCODE_BUTTON_B));
        verify(mController, never()).onMenuOrKeyboardAction(anyInt(), anyBoolean());
    }

    @Test
    public void testGamepadButtonB_OnKeyDown_WhenGamepadApiInactive_ClosesTab() {
        GamepadList.setGamepadApiActiveForTesting(false);

        Tab tab = mock(Tab.class);
        TabRemover tabRemover = mock(TabRemover.class);
        when(tab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentTab()).thenReturn(tab);
        when(mTabModel.index()).thenReturn(0);
        when(mTabModel.getTabAt(0)).thenReturn(tab);
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);

        assertTrue(onKeyDown(KeyEvent.KEYCODE_BUTTON_B));
        verify(tabRemover).closeTabs(any(), eq(true));
    }

    private static KeyEvent createKeyEvent(int keyCode, int metaState) {
        return new KeyEvent(
                SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(),
                KeyEvent.ACTION_DOWN,
                keyCode,
                /* repeat= */ 0,
                metaState);
    }

    private Boolean dispatchKeyEvent(int keyCode) {
        return KeyboardShortcuts.dispatchKeyEvent(
                createKeyEvent(keyCode, /* metaState= */ 0),
                /* uiInitialized= */ true,
                /* fullscreenManager= */ null,
                mController,
                ApplicationProvider.getApplicationContext());
    }

    private boolean onKeyDown(int keyCode) {
        return onKeyDown(keyCode, /* metaState= */ 0);
    }

    private boolean onKeyDown(int keyCode, int metaState) {
        return KeyboardShortcuts.onKeyDown(
                createKeyEvent(keyCode, metaState),
                /* isCurrentTabVisible= */ true,
                /* tabSwitchingEnabled= */ true,
                mTabModelSelector,
                mController,
                mToolbarManager);
    }
}
