// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shortcuts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.ChromeCommandIds;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/** Unit tests for {@link BrowserCommandDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserCommandDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MenuOrKeyboardActionController mController;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;
    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;

    private boolean mIsUiInitialized = true;
    private boolean mIsCurrentTabVisible = true;
    private BrowserCommandDelegate mTabbedDelegate;
    private BrowserCommandDelegate mCustomTabDelegate;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mController.onMenuOrKeyboardAction(anyInt(), anyBoolean())).thenReturn(true);

        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);

        mTabbedDelegate =
                new BrowserCommandDelegate(
                        () -> mIsUiInitialized,
                        () -> mIsCurrentTabVisible,
                        /* tabbedBrowserCommandsSupported= */ true,
                        () -> mTabModelSelector,
                        mController);
        mCustomTabDelegate =
                new BrowserCommandDelegate(
                        () -> mIsUiInitialized,
                        () -> mIsCurrentTabVisible,
                        /* tabbedBrowserCommandsSupported= */ false,
                        () -> mTabModelSelector,
                        mController);
    }

    @Test
    public void testUiNotInitialized_DisablesAllCommands() {
        mIsUiInitialized = false;

        // Arbitrarily chosen command to verify that no command is enabled or executed before UI
        // initialization completes.
        assertFalse(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.SHOW_APP_MENU));
        assertFalse(mTabbedDelegate.executeCommand(ChromeCommandIds.SHOW_APP_MENU));
        verify(mController, never()).onMenuOrKeyboardAction(anyInt(), anyBoolean());
    }

    @Test
    public void testHubMode_EnablesWindowCommands_DisablesActiveTabCommands() {
        mIsCurrentTabVisible = false;

        // One representative command per category is tested below:
        // 1. Always-enabled window command.
        assertTrue(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.SHOW_APP_MENU));
        // 2. Tabbed-window command that does not require a visible tab.
        assertTrue(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.NEW_TAB));
        // 3. Tabbed-window command that requires a visible tab.
        assertFalse(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.FOCUS_LOCATION));
        // 4. Single-tab command that requires a visible tab.
        assertFalse(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.FIND));
        assertFalse(mTabbedDelegate.executeCommand(ChromeCommandIds.FIND));
    }

    @Test
    public void testCustomTab_DisablesTabbedCommands_EnablesSingleTabCommands() {
        // One representative command per category is tested below:
        // 1. Tabbed-window command (disabled in Custom Tabs / PWAs).
        assertFalse(mCustomTabDelegate.isCommandEnabled(ChromeCommandIds.NEW_TAB));
        // 2. Tabbed-window + visible-tab command (disabled in Custom Tabs / PWAs).
        assertFalse(mCustomTabDelegate.isCommandEnabled(ChromeCommandIds.FOCUS_LOCATION));
        // 3. Single-tab command (enabled in Custom Tabs / PWAs).
        assertTrue(mCustomTabDelegate.isCommandEnabled(ChromeCommandIds.FIND));
        // 4. Always-enabled window command (enabled in Custom Tabs / PWAs).
        assertTrue(mCustomTabDelegate.isCommandEnabled(ChromeCommandIds.SHOW_APP_MENU));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testNewTab_RoutesBasedOnIncognitoStateAndPolicy() {
        when(mTabModel.isIncognito()).thenReturn(false);
        doReturn(false).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.NEW_TAB));
        verify(mController).onMenuOrKeyboardAction(eq(R.id.new_tab_menu_id), eq(false));

        when(mTabModel.isIncognito()).thenReturn(true);
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.NEW_TAB));
        verify(mController).onMenuOrKeyboardAction(eq(R.id.new_incognito_tab_menu_id), eq(false));

        when(mTabModel.isIncognito()).thenReturn(false);
        doReturn(true).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.NEW_TAB));
        verify(mController, times(2))
                .onMenuOrKeyboardAction(eq(R.id.new_incognito_tab_menu_id), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testNewWindow_RespectsMultiInstanceAndIncognitoForced() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        assertFalse(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.NEW_WINDOW));

        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        doReturn(false).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.NEW_WINDOW));
        verify(mController).onMenuOrKeyboardAction(eq(R.id.new_window_menu_id), eq(false));

        doReturn(true).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.NEW_WINDOW));
        verify(mController)
                .onMenuOrKeyboardAction(eq(R.id.new_incognito_window_menu_id), eq(false));
    }

    @Test
    public void testFeedback_RespectsPolicy() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
        assertTrue(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.FEEDBACK));
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.FEEDBACK));
        verify(mController).onMenuOrKeyboardAction(eq(R.id.feedback_form), eq(false));

        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(false);
        assertFalse(mTabbedDelegate.isCommandEnabled(ChromeCommandIds.FEEDBACK));
        assertFalse(mTabbedDelegate.executeCommand(ChromeCommandIds.FEEDBACK));
    }

    @Test
    public void testCloseFindOrStop() {
        when(mController.onMenuOrKeyboardAction(eq(R.id.esc_key), eq(false))).thenReturn(true);
        assertTrue(mTabbedDelegate.executeCommand(ChromeCommandIds.CLOSE_FIND_OR_STOP));

        when(mController.onMenuOrKeyboardAction(eq(R.id.esc_key), eq(false))).thenReturn(false);
        assertFalse(mTabbedDelegate.executeCommand(ChromeCommandIds.CLOSE_FIND_OR_STOP));
    }
}
