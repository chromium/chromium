// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.KeyboardShortcutInfo;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.ui.util.KeyEventUtils;

import java.util.List;
import java.util.Set;

/** Unit tests for {@link KeyboardShortcuts}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures({
    ChromeFeatureList.TASK_MANAGER_CLANK,
    ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND
})
public class KeyboardShortcutsTest {

    private static final int[] SELECT_TAB_ACTION_IDS = {
        R.id.select_tab_0,
        R.id.select_tab_1,
        R.id.select_tab_2,
        R.id.select_tab_3,
        R.id.select_tab_4,
        R.id.select_tab_5,
        R.id.select_tab_6,
        R.id.select_tab_7,
    };

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mMenuOrKeyboardActionController.onMenuOrKeyboardAction(anyInt(), anyBoolean()))
                .thenReturn(true);

        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
    }

    @Test
    @SmallTest
    public void testOpenHomePage() {
        boolean isKeyEventHandled =
                keyDown(
                        KeyEvent.KEYCODE_HOME,
                        KeyEvent.META_ALT_ON,
                        /* isCurrentTabVisible= */ true);

        assertTrue("Expected key event to be handled for Alt+Home", isKeyEventHandled);
        verify(mMenuOrKeyboardActionController)
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.open_home_page), /* fromMenu= */ eq(false));
    }

    // Close Tab shortcuts

    @Test
    @SmallTest
    public void testCloseTab() {
        List<Pair<Integer, Integer>> keyCodeAndModifier =
                List.of(
                        new Pair<>(KeyEvent.KEYCODE_W, KeyEvent.META_CTRL_ON),
                        new Pair<>(KeyEvent.KEYCODE_F4, KeyEvent.META_CTRL_ON),
                        new Pair<>(KeyEvent.KEYCODE_BUTTON_B, KeyboardUtils.NO_MODIFIER));
        for (int i = 0; i < keyCodeAndModifier.size(); i++) {
            clearInvocations(mMenuOrKeyboardActionController);
            int keyCode = keyCodeAndModifier.get(i).first;
            int modifier = keyCodeAndModifier.get(i).second;
            boolean isKeyEventHandled = keyDown(keyCode, modifier, /* isCurrentTabVisible= */ true);

            String debugString =
                    "at test index "
                            + i
                            + " with key code "
                            + keyCode
                            + " with modifier "
                            + modifier;
            assertTrue("Expected key event to be handled for " + debugString, isKeyEventHandled);
            verify(mMenuOrKeyboardActionController)
                    .onMenuOrKeyboardAction(
                            /* id= */ eq(R.id.close_tab), /* fromMenu= */ eq(false));
        }
    }

    @Test
    @SmallTest
    public void testCloseWindow() {
        boolean isKeyEventHandled =
                keyDown(
                        KeyEvent.KEYCODE_W,
                        (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON),
                        /* isCurrentTabVisible= */ true);

        assertTrue("Expected key event to be handled", isKeyEventHandled);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(/* id= */ eq(R.id.close_window), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_KEYBOARD_SHORTCUT_OPEN_FILE)
    public void testOpenFile() {
        boolean isKeyEventHandled =
                keyDown(KeyEvent.KEYCODE_O, KeyEvent.META_CTRL_ON, /* isCurrentTabVisible= */ true);

        assertTrue("Expected key event to be handled", isKeyEventHandled);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(/* id= */ eq(R.id.open_file_id), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_KEYBOARD_SHORTCUT_OPEN_FILE)
    @Policies.Add({@Policies.Item(key = "AllowFileSelectionDialogs", string = "false")})
    public void testOpenFile_disallowedByPolicy() {
        boolean isKeyEventHandled =
                keyDown(KeyEvent.KEYCODE_O, KeyEvent.META_CTRL_ON, /* isCurrentTabVisible= */ true);

        assertTrue("Expected key event to be handled", isKeyEventHandled);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(/* id= */ eq(R.id.open_file_id), /* fromMenu= */ eq(false));
    }

    // Bookmarks shortcuts

    @Test
    @SmallTest
    public void testOpenBookmarks() {
        testOpenBookmarks(
                /* expectHandled= */ true,
                /* isCurrentTabVisible= */ true,
                /* metaState= */ KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);
    }

    @Test
    @SmallTest
    public void testOpenBookmarks_withoutCurrentTabVisible() {
        testOpenBookmarks(
                /* expectHandled= */ false,
                /* isCurrentTabVisible= */ false,
                /* metaState= */ KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);
    }

    @Test
    @SmallTest
    public void testOpenBookmarks_withoutMetaCtrlOn() {
        testOpenBookmarks(
                /* expectHandled= */ false,
                /* isCurrentTabVisible= */ true,
                /* metaState= */ KeyEvent.META_SHIFT_ON);
    }

    @Test
    @SmallTest
    public void testOpenBookmarks_withoutMetaShiftOn() {
        // Ctrl+O (no Shift) is handled as "open file", not "open bookmarks".
        assertTrue(
                keyDown(
                        KeyEvent.KEYCODE_O,
                        KeyEvent.META_CTRL_ON,
                        /* isCurrentTabVisible= */ true));
        verify(mMenuOrKeyboardActionController, never())
                .onMenuOrKeyboardAction(eq(R.id.all_bookmarks_menu_id), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenBookmarks_withoutMetaState() {
        testOpenBookmarks(
                /* expectHandled= */ false, /* isCurrentTabVisible= */ true, /* metaState= */ 0);
    }

    @Test
    @SmallTest
    public void testToggleBookmarkBar() {
        testToggleBookmarkBar(/* expectHandled= */ true, /* withMetaState= */ true);
    }

    @Test
    @SmallTest
    public void testToggleBookmarkBar_withoutMetaState() {
        testToggleBookmarkBar(/* expectHandled= */ false, /* withMetaState= */ false);
    }

    private void testToggleBookmarkBar(boolean expectHandled, boolean withMetaState) {
        assertEquals(
                expectHandled,
                KeyboardShortcuts.onKeyDown(
                        new KeyEvent(
                                /* downTime= */ SystemClock.uptimeMillis(),
                                /* eventTime= */ SystemClock.uptimeMillis(),
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_B,
                                /* repeat= */ 0,
                                withMetaState ? KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON : 0),
                        /* isCurrentTabVisible= */ true,
                        /* tabSwitchingEnabled= */ true,
                        mTabModelSelector,
                        mMenuOrKeyboardActionController));

        verify(mMenuOrKeyboardActionController, expectHandled ? times(1) : never())
                .onMenuOrKeyboardAction(
                        eq(R.id.toggle_bookmark_bar),
                        /* fromMenu= */ expectHandled ? eq(false) : anyBoolean());
    }

    // Go To Tab shortcuts

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToTab_withNumberKeys() {
        for (int keyCode = KeyEvent.KEYCODE_1; keyCode <= KeyEvent.KEYCODE_8; keyCode++) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                assertGoToTab(keyCode, metaState);
                clearInvocations(mMenuOrKeyboardActionController);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToTab_withNumPad() {
        for (int keyCode = KeyEvent.KEYCODE_NUMPAD_1;
                keyCode <= KeyEvent.KEYCODE_NUMPAD_8;
                keyCode++) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                assertGoToTab(keyCode, metaState);
                clearInvocations(mMenuOrKeyboardActionController);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToLastTab() {
        for (int keyCode : Set.of(KeyEvent.KEYCODE_9, KeyEvent.KEYCODE_NUMPAD_9)) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                String message =
                        String.format(
                                "expected key event with keycode %s and metaState %s to be handled",
                                keyCode, metaState);
                assertTrue(message, keyDown(keyCode, metaState, true));
                verify(mMenuOrKeyboardActionController, times(1))
                        .onMenuOrKeyboardAction(
                                /* id= */ eq(R.id.select_last_tab), /* fromMenu= */ eq(false));
                clearInvocations(mMenuOrKeyboardActionController);
            }
        }
    }

    // Tests for focus placement on top Clank.

    @Test
    @SmallTest
    public void testGoToToolbar() {
        assertTrue(
                keyDown(KeyEvent.KEYCODE_T, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.focus_toolbar), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    public void testGoToBookmarksBar() {
        keyDown(KeyEvent.KEYCODE_B, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON, true);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.focus_bookmarks), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    public void testFocusSwitch() {
        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_F6, 0));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.switch_keyboard_focus_row), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    public void testFocusSwitch_Reverse() {
        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_F6, KeyEvent.META_SHIFT_ON));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.switch_keyboard_focus_row_reverse),
                        /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP})
    public void testTabSearchSideUI_disabled() {
        // Primary shortcut: Ctrl+Shift+A
        assertTrue(
                keyDown(KeyEvent.KEYCODE_A, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(/* id= */ eq(R.id.tab_search), /* fromMenu= */ eq(false));

        // Alternate shortcut: Alt+Shift+A
        clearInvocations(mMenuOrKeyboardActionController);
        assertTrue(
                keyDown(KeyEvent.KEYCODE_A, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(/* id= */ eq(R.id.tab_search), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP})
    public void testTabSearchSideUI_enabled() {
        // Primary shortcut: Ctrl+Shift+A
        assertTrue(
                keyDown(KeyEvent.KEYCODE_A, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.tab_search_side_ui), /* fromMenu= */ eq(false));

        // Alternate shortcut: Alt+Shift+A
        clearInvocations(mMenuOrKeyboardActionController);
        assertTrue(
                keyDown(KeyEvent.KEYCODE_A, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.tab_search_side_ui), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    public void testMoveToNextTab() {
        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_TAB, KeyEvent.META_CTRL_ON));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(eq(R.id.select_next_tab), eq(false));
    }

    @Test
    @SmallTest
    public void testMoveToPreviousTab() {
        assertTrue(
                dispatchKeyEvent(
                        KeyEvent.KEYCODE_TAB, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(eq(R.id.select_previous_tab), eq(false));
    }

    @Test
    @SmallTest
    public void testOpenStripContextMenu() {
        keyDown(KeyEvent.KEYCODE_F10, KeyEvent.META_SHIFT_ON, true);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.open_tab_strip_context_menu), /* fromMenu= */ eq(false));
    }

    /** Test that pressing F10 triggers focus on the app menu button view. */
    @Test
    @SmallTest
    public void testFocusAppMenuButton() {
        when(mMenuOrKeyboardActionController.onMenuOrKeyboardAction(
                        eq(R.id.focus_app_menu_button), eq(false)))
                .thenReturn(true);

        assertTrue(keyDown(KeyEvent.KEYCODE_F10, 0, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.focus_app_menu_button), /* fromMenu= */ eq(false));
    }

    /** Tests that F7 is not consumed before reaching web content. */
    @Test
    @SmallTest
    public void testF7NotHandledBeforeWebContent() {
        assertNull(dispatchKeyEvent(KeyEvent.KEYCODE_F7, 0));

        verify(mMenuOrKeyboardActionController, never())
                .onMenuOrKeyboardAction(eq(R.id.toggle_caret_browsing), eq(false));
    }

    /** Tests that an unhandled F7 requests caret browsing in post-target handling. */
    @Test
    @SmallTest
    public void testToggleCaretBrowsing() {
        // Ensure we handle F7 key (this was previously ignored)
        assertTrue(keyDown(KeyEvent.KEYCODE_F7, 0, true));

        // Ensure we trigger the caret browsing dialog
        verify(mMenuOrKeyboardActionController)
                .onMenuOrKeyboardAction(eq(R.id.toggle_caret_browsing), eq(false));
    }

    /** Tests that KEYCODE_MENU is not consumed before reaching web content when flag is enabled. */
    @Test
    @SmallTest
    @Features.EnableFeatures(BlinkFeatures.ANDROID_KEYBOARD_CONTEXT_MENU)
    public void testKeycodeMenuNotHandledBeforeWebContent() {
        assertNull(dispatchKeyEvent(KeyEvent.KEYCODE_MENU, 0));

        verify(mMenuOrKeyboardActionController, never())
                .onMenuOrKeyboardAction(eq(R.id.show_menu), eq(false));
    }

    /**
     * Tests that an unhandled KEYCODE_MENU opens the tab strip context menu when a tab strip item
     * has focus, and otherwise falls back to opening the app menu.
     */
    @Test
    @SmallTest
    @Features.EnableFeatures(BlinkFeatures.ANDROID_KEYBOARD_CONTEXT_MENU)
    public void testOpenMenuViaKeycodeMenu() {
        // When a tab strip item has focus, open_tab_strip_context_menu handles the action.
        assertTrue(keyDown(KeyEvent.KEYCODE_MENU, 0, true));
        verify(mMenuOrKeyboardActionController)
                .onMenuOrKeyboardAction(eq(R.id.open_tab_strip_context_menu), eq(false));
        verify(mMenuOrKeyboardActionController, never())
                .onMenuOrKeyboardAction(eq(R.id.show_menu), eq(false));

        // When no tab strip item has focus, fall back to opening the 3-dot app menu.
        clearInvocations(mMenuOrKeyboardActionController);
        when(mMenuOrKeyboardActionController.onMenuOrKeyboardAction(
                        eq(R.id.open_tab_strip_context_menu), eq(false)))
                .thenReturn(false);
        assertTrue(keyDown(KeyEvent.KEYCODE_MENU, 0, true));
        verify(mMenuOrKeyboardActionController)
                .onMenuOrKeyboardAction(eq(R.id.show_menu), eq(false));
    }

    /** Tests that pressing F1 from an alphabetic keyboard triggers the help action. */
    @Test
    @SmallTest
    public void testOpenHelp() {
        KeyEventUtils.setIsFromAlphabeticKeyboardForTesting(true);
        assertTrue(keyDown(KeyEvent.KEYCODE_F1, 0, true));
        verify(mMenuOrKeyboardActionController).onMenuOrKeyboardAction(eq(R.id.help_id), eq(false));
    }

    /** Tests that pressing F1 is ignored when it does not come from an alphabetic keyboard. */
    @Test
    @SmallTest
    public void testOpenHelp_nonAlphabeticKeyboard() {
        KeyEventUtils.setIsFromAlphabeticKeyboardForTesting(false);
        assertFalse(dispatchKeyEvent(KeyEvent.KEYCODE_F1, 0));
    }

    @Test
    @SmallTest
    public void testFeedbackShortcut_Allowed() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
        assertTrue(
                keyDown(
                        KeyEvent.KEYCODE_I,
                        KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON,
                        /* isCurrentTabVisible= */ true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(eq(R.id.feedback_form), eq(false));
    }

    @Test
    @SmallTest
    public void testFeedbackShortcut_Disallowed() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(false);
        assertTrue(
                keyDown(
                        KeyEvent.KEYCODE_I,
                        KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON,
                        /* isCurrentTabVisible= */ true));
        verify(mMenuOrKeyboardActionController, never())
                .onMenuOrKeyboardAction(eq(R.id.feedback_form), anyBoolean());
    }

    @Test
    @SmallTest
    public void testFeedbackShortcutInGroup_Allowed() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
        List<KeyboardShortcutGroup> groups =
                KeyboardShortcuts.createShortcutGroup(ApplicationProvider.getApplicationContext());
        boolean found = false;
        for (KeyboardShortcutGroup group : groups) {
            for (KeyboardShortcutInfo item : group.getItems()) {
                if (item.getLabel()
                        .equals(
                                ApplicationProvider.getApplicationContext()
                                        .getString(R.string.keyboard_shortcut_send_feedback))) {
                    found = true;
                    break;
                }
            }
        }
        assertTrue("Feedback shortcut should be in the group when allowed", found);
    }

    @Test
    @SmallTest
    public void testFeedbackShortcutInGroup_Disallowed() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(false);
        List<KeyboardShortcutGroup> groups =
                KeyboardShortcuts.createShortcutGroup(ApplicationProvider.getApplicationContext());
        boolean found = false;
        for (KeyboardShortcutGroup group : groups) {
            for (KeyboardShortcutInfo item : group.getItems()) {
                if (item.getLabel()
                        .equals(
                                ApplicationProvider.getApplicationContext()
                                        .getString(R.string.keyboard_shortcut_send_feedback))) {
                    found = true;
                    break;
                }
            }
        }
        assertFalse("Feedback shortcut should NOT be in the group when disallowed", found);
    }

    @Test
    @SmallTest
    public void testDevTools() {
        assertTrue(
                keyDown(
                        KeyEvent.KEYCODE_I,
                        KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON,
                        /* isCurrentTabVisible= */ true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(eq(R.id.dev_tools), eq(false));

        clearInvocations(mMenuOrKeyboardActionController);
        assertTrue(
                keyDown(
                        KeyEvent.KEYCODE_J,
                        KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON,
                        /* isCurrentTabVisible= */ true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(eq(R.id.dev_tools_console), eq(false));

        clearInvocations(mMenuOrKeyboardActionController);
        assertTrue(
                keyDown(
                        KeyEvent.KEYCODE_C,
                        KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON,
                        /* isCurrentTabVisible= */ true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(eq(R.id.dev_tools_inspect), eq(false));
    }

    private void testOpenBookmarks(
            boolean expectHandled, boolean isCurrentTabVisible, int metaState) {
        assertEquals(expectHandled, keyDown(KeyEvent.KEYCODE_O, metaState, isCurrentTabVisible));

        verify(mMenuOrKeyboardActionController, expectHandled ? times(1) : never())
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.all_bookmarks_menu_id),
                        /* fromMenu= */ expectHandled ? eq(false) : anyBoolean());
    }

    private void assertGoToTab(int keyCode, int metaState) {
        String message =
                String.format(
                        "expected key event with keycode %s and metaState %s to be handled",
                        keyCode, metaState);
        assertTrue(message, keyDown(keyCode, metaState, true));
        int numCode =
                (KeyEvent.KEYCODE_1 <= keyCode && keyCode <= KeyEvent.KEYCODE_8)
                        ? keyCode - KeyEvent.KEYCODE_0
                        : keyCode - KeyEvent.KEYCODE_NUMPAD_0;
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(SELECT_TAB_ACTION_IDS[numCode - 1]),
                        /* fromMenu= */ eq(false));
    }

    private boolean keyDown(int keyCode, int metaState, boolean isCurrentTabVisible) {
        return KeyboardShortcuts.onKeyDown(
                new KeyEvent(
                        /* downTime= */ SystemClock.uptimeMillis(),
                        /* eventTime= */ SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        keyCode,
                        /* repeat= */ 0,
                        metaState),
                isCurrentTabVisible,
                /* tabSwitchingEnabled= */ true,
                mTabModelSelector,
                mMenuOrKeyboardActionController);
    }

    private Boolean dispatchKeyEvent(int keyCode, int metaState) {
        return KeyboardShortcuts.dispatchKeyEvent(
                new KeyEvent(
                        /* downTime= */ SystemClock.uptimeMillis(),
                        /* eventTime= */ SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        keyCode,
                        /* repeat= */ 0,
                        metaState),
                /* uiInitialized= */ true,
                /* fullscreenManager= */ null,
                mMenuOrKeyboardActionController,
                ApplicationProvider.getApplicationContext());
    }
}
