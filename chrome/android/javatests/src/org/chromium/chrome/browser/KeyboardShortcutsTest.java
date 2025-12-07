// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
import android.util.Pair;
import android.view.KeyEvent;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;

import java.util.List;
import java.util.Set;

/** Unit tests for {@link KeyboardShortcuts}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures({
    ChromeFeatureList.TASK_MANAGER_CLANK,
    ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND,
    ContentFeatureList.ANDROID_CARET_BROWSING
})
public class KeyboardShortcutsTest {

    private static final int TAB_ID = 0;
    private static final int TAB_ID_2 = 0;
    // Want this to be less than 8 so we can test that "go to tab" keyboard shortcut is not called.
    private static final int SMALL_NUMBER_OF_TABS = 7;
    // Want this to be greater than 10 so we can test "go to tab" keyboard shortcut.
    private static final int LARGE_NUMBER_OF_TABS = 11;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabRemover mTabRemover;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        setUpTabModelSelector(List.of(mTab));
        when(mMenuOrKeyboardActionController.onMenuOrKeyboardAction(anyInt(), anyBoolean()))
                .thenReturn(true);
    }

    /**
     * Sets up the mock {@link #mTabModelSelector}, which should be passed to {@code
     * KeyboardShortcuts.onKeyDown()} for testing.
     */
    private void setUpTabModelSelector(List<Tab> tabs) {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mTabModel.getCount()).thenReturn(tabs.size());
        when(mTabModel.index()).thenReturn(0);
        for (int i = 0; i < tabs.size(); i++) {
            when(mTabModel.getTabAt(i)).thenReturn(tabs.get(i));
            when(tabs.get(i).getId()).thenReturn(i);
            when(tabs.get(i).getWebContents()).thenReturn(mWebContents);
        }
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.isTabMultiSelected(TAB_ID)).thenReturn(true);

        doNothing().when(mTabRemover).closeTabs(any(TabClosureParams.class), anyBoolean());
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
        for (List<Tab> tabsToClose : List.of(List.of(mTab), List.of(mTab, mTab2))) {
            setUpTabModelSelector(tabsToClose);
            for (Tab tab : tabsToClose) {
                when(mTabModel.isTabMultiSelected(tab.getId())).thenReturn(true);
            }
            for (int i = 0; i < keyCodeAndModifier.size(); i++) {
                clearInvocations(mTabRemover);
                int keyCode = keyCodeAndModifier.get(i).first;
                int modifier = keyCodeAndModifier.get(i).second;
                boolean isKeyEventHandled =
                        keyDown(keyCode, modifier, /* isCurrentTabVisible= */ true);

                String debugString =
                        "at test index "
                                + i
                                + " with key code "
                                + keyCode
                                + " with modifier "
                                + modifier;
                assertTrue(
                        "Expected key event to be handled for " + debugString, isKeyEventHandled);
                verify(
                                mTabRemover,
                                description(
                                        "Expected closeTabs to be called with correct"
                                                + " TabClosureParams"))
                        .closeTabs(
                                eq(
                                        TabClosureParams.closeTabs(tabsToClose)
                                                .allowUndo(false)
                                                .tabClosingSource(
                                                        TabClosingSource.KEYBOARD_SHORTCUT)
                                                .build()),
                                /* allowDialog= */ eq(true));
            }
        }
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
        testOpenBookmarks(
                /* expectHandled= */ false,
                /* isCurrentTabVisible= */ true,
                /* metaState= */ KeyEvent.META_CTRL_ON);
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
                        mMenuOrKeyboardActionController,
                        mToolbarManager));

        verify(mMenuOrKeyboardActionController, expectHandled ? times(1) : never())
                .onMenuOrKeyboardAction(
                        eq(R.id.toggle_bookmark_bar),
                        /* fromMenu= */ expectHandled ? eq(false) : anyBoolean());
    }

    // Go To Tab shortcuts

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToTab_withNumberKeys_smallNumberOfTabs() {
        when(mTabModel.getCount()).thenReturn(SMALL_NUMBER_OF_TABS);
        for (int keyCode = KeyEvent.KEYCODE_1; keyCode <= KeyEvent.KEYCODE_8; keyCode++) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                assertGoToTab(
                        (keyCode - KeyEvent.KEYCODE_0 - 1) < SMALL_NUMBER_OF_TABS,
                        keyCode,
                        metaState);
                clearInvocations(mTabModel);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToTab_withNumberKeys_largeNumberOfTabs() {
        when(mTabModel.getCount()).thenReturn(LARGE_NUMBER_OF_TABS);
        for (int keyCode = KeyEvent.KEYCODE_1; keyCode <= KeyEvent.KEYCODE_8; keyCode++) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                assertGoToTab(true, keyCode, metaState);
                clearInvocations(mTabModel);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToTab_withNumPad_smallNumberOfTabs() {
        when(mTabModel.getCount()).thenReturn(SMALL_NUMBER_OF_TABS);
        for (int keyCode = KeyEvent.KEYCODE_NUMPAD_1;
                keyCode <= KeyEvent.KEYCODE_NUMPAD_8;
                keyCode++) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                assertGoToTab(
                        (keyCode - KeyEvent.KEYCODE_NUMPAD_0 - 1) < SMALL_NUMBER_OF_TABS,
                        keyCode,
                        metaState);
                clearInvocations(mTabModel);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToTab_withNumPad_largeNumberOfTabs() {
        when(mTabModel.getCount()).thenReturn(LARGE_NUMBER_OF_TABS);
        for (int keyCode = KeyEvent.KEYCODE_NUMPAD_1;
                keyCode <= KeyEvent.KEYCODE_NUMPAD_8;
                keyCode++) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                assertGoToTab(true, keyCode, metaState);
                clearInvocations(mTabModel);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToLastTab_smallNumberOfTabs() {
        when(mTabModel.getCount()).thenReturn(SMALL_NUMBER_OF_TABS);
        for (int keyCode : Set.of(KeyEvent.KEYCODE_9, KeyEvent.KEYCODE_NUMPAD_9)) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                boolean expectHandled = (keyCode - KeyEvent.KEYCODE_0) < SMALL_NUMBER_OF_TABS;
                String message =
                        String.format(
                                "expected handling of key event with keycode %s and metaState %s to"
                                        + " be %s",
                                keyCode, metaState, expectHandled);
                assertTrue(message, keyDown(keyCode, metaState, true));
                verify(mTabModel, times(1))
                        .setIndex(SMALL_NUMBER_OF_TABS - 1, TabSelectionType.FROM_USER);
                clearInvocations(mTabModel);
            }
        }
    }

    @Test
    @SmallTest
    @Feature("Keyboard Shortcuts")
    public void testGoToLastTab_largeNumberOfTabs() {
        when(mTabModel.getCount()).thenReturn(LARGE_NUMBER_OF_TABS);
        for (int keyCode : Set.of(KeyEvent.KEYCODE_9, KeyEvent.KEYCODE_NUMPAD_9)) {
            for (int metaState : Set.of(KeyEvent.META_CTRL_ON, KeyEvent.META_ALT_ON)) {
                String message =
                        String.format(
                                "expected key event with keycode %s and metaState %s to be handled",
                                keyCode, metaState);
                assertTrue(message, keyDown(keyCode, metaState, true));
                verify(mTabModel, times(1))
                        .setIndex(LARGE_NUMBER_OF_TABS - 1, TabSelectionType.FROM_USER);
                clearInvocations(mTabModel);
            }
        }
    }

    // Tests for focus placement on top Clank.

    @Test
    @SmallTest
    public void testGoToToolbar() {
        assertTrue(
                keyDown(KeyEvent.KEYCODE_T, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mToolbarManager, times(1)).requestFocus();
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
        keyDown(KeyEvent.KEYCODE_F6, 0, true);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.switch_keyboard_focus_row), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    public void testOpenStripContextMenu() {
        keyDown(KeyEvent.KEYCODE_F10, KeyEvent.META_SHIFT_ON, true);
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.open_tab_strip_context_menu), /* fromMenu= */ eq(false));
    }

    /** Test that pressing F7 triggers the caret browsing dialog. */
    @Test
    @SmallTest
    public void testToggleCaretBrowsing() {
        // Ensure we handle F7 key (this was previously ignored)
        assertTrue(keyDown(KeyEvent.KEYCODE_F7, 0, true));

        // Ensure we trigger the caret browsing dialog
        verify(mMenuOrKeyboardActionController)
                .onMenuOrKeyboardAction(eq(R.id.toggle_caret_browsing), eq(false));
    }

    private void testOpenBookmarks(
            boolean expectHandled, boolean isCurrentTabVisible, int metaState) {
        assertEquals(expectHandled, keyDown(KeyEvent.KEYCODE_O, metaState, isCurrentTabVisible));

        verify(mMenuOrKeyboardActionController, expectHandled ? times(1) : never())
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.all_bookmarks_menu_id),
                        /* fromMenu= */ expectHandled ? eq(false) : anyBoolean());
    }

    private void assertGoToTab(boolean expectHandled, int keyCode, int metaState) {
        String message =
                String.format(
                        "expected handling of key event with keycode %s and metaState %s to be %s",
                        keyCode, metaState, expectHandled);
        // Note: we always expect (CTRL or ALT) + [1-9] to be a "go to tab" shortcut; we expect
        // onKeyDown to always be true. However, setting the index of the tab model won't happen if
        // the number is out of range.
        assertTrue(message, keyDown(keyCode, metaState, true));
        int numCode =
                (KeyEvent.KEYCODE_1 <= keyCode && keyCode <= KeyEvent.KEYCODE_8)
                        ? keyCode - KeyEvent.KEYCODE_0
                        : keyCode - KeyEvent.KEYCODE_NUMPAD_0;
        verify(mTabModel, expectHandled ? times(1) : never())
                .setIndex(numCode - 1, TabSelectionType.FROM_USER);
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
                mMenuOrKeyboardActionController,
                mToolbarManager);
    }
}
