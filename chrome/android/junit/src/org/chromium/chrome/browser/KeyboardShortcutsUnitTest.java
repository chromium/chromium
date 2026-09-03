// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
import android.view.KeyEvent;

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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

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

        KeyEvent event =
                new KeyEvent(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_N,
                        0,
                        KeyEvent.META_CTRL_ON);

        assertTrue(
                KeyboardShortcuts.onKeyDown(
                        event, true, true, mTabModelSelector, mController, mToolbarManager));

        verify(mController)
                .onMenuOrKeyboardAction(eq(R.id.new_incognito_window_menu_id), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testOpenNewTab_RedirectsToIncognito_WhenForced() {
        doReturn(true).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());
        when(mTabModel.isIncognito()).thenReturn(false);

        KeyEvent event =
                new KeyEvent(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_T,
                        0,
                        KeyEvent.META_CTRL_ON);

        assertTrue(
                KeyboardShortcuts.onKeyDown(
                        event, true, true, mTabModelSelector, mController, mToolbarManager));

        verify(mController).onMenuOrKeyboardAction(eq(R.id.new_incognito_tab_menu_id), eq(false));
    }

    @Test
    public void testQuitChrome_TriggersMenuOrKeyboardAction() {
        doReturn(true).when(mController).onMenuOrKeyboardAction(eq(R.id.quit_chrome), eq(false));

        KeyEvent event =
                new KeyEvent(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_Q,
                        0,
                        KeyEvent.META_CTRL_ON);

        assertTrue(
                KeyboardShortcuts.onKeyDown(
                        event, true, true, mTabModelSelector, mController, mToolbarManager));

        verify(mController).onMenuOrKeyboardAction(eq(R.id.quit_chrome), eq(false));
    }
}
