// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.KeyboardShortcutInfo;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.PinnedTabClosureManager;
import org.chromium.chrome.browser.tabmodel.PinnedTabClosureManagerFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

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

    private static final int TAB_ID = 0;
    private static final int TAB_ID_2 = 0;
    // Want this to be less than 8 so we can test that "go to tab" keyboard shortcut is not called.
    private static final int SMALL_NUMBER_OF_TABS = 7;
    // Want this to be greater than 10 so we can test "go to tab" keyboard shortcut.
    private static final int LARGE_NUMBER_OF_TABS = 11;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private PinnedTabClosureManager mPinnedTabCloseManager;

    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabRemover mTabRemover;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;

    @Mock private HomepageManager mHomepageManager;
    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;

    @Before
    public void setUp() {
        setUpTabModelSelector(List.of(mTab));
        when(mMenuOrKeyboardActionController.onMenuOrKeyboardAction(anyInt(), anyBoolean()))
                .thenReturn(true);
        when(mTab.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        mPinnedTabCloseManager = spy(PinnedTabClosureManagerFactory.getInstance());
        PinnedTabClosureManagerFactory.setInstanceForTesting(mPinnedTabCloseManager);

        HomepageManager.setInstanceForTesting(mHomepageManager);
        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
    }

    @Test
    @SmallTest
    public void testOpenHomePage() {
        GURL mockGurl = JUnitTestGURLs.EXAMPLE_URL;
        when(mHomepageManager.getHomepageGurl(anyBoolean())).thenReturn(mockGurl);

        boolean isKeyEventHandled =
                keyDown(
                        KeyEvent.KEYCODE_HOME,
                        KeyEvent.META_ALT_ON,
                        /* isCurrentTabVisible= */ true);

        assertTrue("Expected key event to be handled for Alt+Home", isKeyEventHandled);
        verify(mTab).loadUrl(any(LoadUrlParams.class));
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
        when(mTabModel.getOrderedMultiSelectedTabs()).thenReturn(List.of(tabs.get(0)));

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
            when(mTabModel.getOrderedMultiSelectedTabs()).thenReturn(tabsToClose);
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

    @Test
    @SmallTest
    public void testCloseTab_noMultiSelect() {
        setUpTabModelSelector(List.of(mTab));
        when(mTab.getIsPinned()).thenReturn(false);
        when(mTabModel.isTabMultiSelected(TAB_ID)).thenReturn(false);

        boolean isKeyEventHandled =
                keyDown(KeyEvent.KEYCODE_W, KeyEvent.META_CTRL_ON, /* isCurrentTabVisible= */ true);

        assertTrue("Expected key event to be handled", isKeyEventHandled);
        verify(mTabRemover, description("Expected closeTabs to be called with the current tab"))
                .closeTabs(
                        eq(
                                TabClosureParams.closeTabs(List.of(mTab))
                                        .allowUndo(false)
                                        .tabClosingSource(TabClosingSource.KEYBOARD_SHORTCUT)
                                        .build()),
                        /* allowDialog= */ eq(true));
    }

    @Test
    @SmallTest
    public void testCloseTab_singlePinnedTab_firstAttempt_tabShouldNotClose() {
        // Setup the first closure attempt of a pinned tab.
        setUpTabModelSelector(List.of(mTab));
        when(mTab.getIsPinned()).thenReturn(true);
        when(mTabModel.isTabMultiSelected(TAB_ID)).thenReturn(false);

        // trigger Ctrl+W keyboard shortcut once.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean isKeyEventHandled =
                            keyDown(
                                    KeyEvent.KEYCODE_W,
                                    KeyEvent.META_CTRL_ON,
                                    /* isCurrentTabVisible= */ true);
                    assertTrue("Expected key event to be handled", isKeyEventHandled);
                });

        // Verify pinned tab is not closed and toast is shown.
        verify(mTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mPinnedTabCloseManager).showToast(any());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky - crbug.com/490369117")
    public void testCloseTab_singlePinnedTab_firstAttempt_timeout() {
        // Setup the first closure attempt of a pinned tab.
        setUpTabModelSelector(List.of(mTab));
        when(mTab.getIsPinned()).thenReturn(true);
        when(mTabModel.isTabMultiSelected(TAB_ID)).thenReturn(false);

        // trigger Ctrl+W keyboard shortcut once.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean isKeyEventHandled =
                            keyDown(
                                    KeyEvent.KEYCODE_W,
                                    KeyEvent.META_CTRL_ON,
                                    /* isCurrentTabVisible= */ true);
                    assertTrue("Expected key event to be handled", isKeyEventHandled);
                });

        // Verify pinned tab is not closed and toast is shown.
        verify(mTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mPinnedTabCloseManager).showToast(any());

        // Verify pending state is cleared after ~4 seconds.
        SystemClock.sleep(4000);
        verify(mPinnedTabCloseManager).clearPendingState(mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testCloseTab_singlePinnedTab_secondAttempt_tabShouldClose() {
        // Setup the second closure attempt of a pinned tab.
        setUpTabModelSelector(List.of(mTab));
        when(mTab.getIsPinned()).thenReturn(true);
        when(mTabModel.isTabMultiSelected(TAB_ID)).thenReturn(false);

        // trigger Ctrl+W keyboard shortcut twice.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean isKeyEventHandled =
                            keyDown(
                                    KeyEvent.KEYCODE_W,
                                    KeyEvent.META_CTRL_ON,
                                    /* isCurrentTabVisible= */ true);
                    assertTrue("Expected key event to be handled", isKeyEventHandled);

                    isKeyEventHandled =
                            keyDown(
                                    KeyEvent.KEYCODE_W,
                                    KeyEvent.META_CTRL_ON,
                                    /* isCurrentTabVisible= */ true);
                    assertTrue("Expected key event to be handled", isKeyEventHandled);
                });

        // Verify pinned tab is closed and pending state is cleared.
        verify(
                        mTabRemover,
                        description(
                                "Expected closeTabs to be called on the pinned tab on second"
                                        + " attempt."))
                .closeTabs(
                        eq(
                                TabClosureParams.closeTabs(List.of(mTab))
                                        .allowUndo(false)
                                        .tabClosingSource(TabClosingSource.KEYBOARD_SHORTCUT)
                                        .build()),
                        /* allowDialog= */ eq(true));
        verify(mPinnedTabCloseManager).clearPendingState(mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testCloseTab_pinnedTab_multiselect_tabShouldClose() {
        // Setup multi-select closure attempt.
        setUpTabModelSelector(List.of(mTab, mTab2));
        when(mTab.getIsPinned()).thenReturn(true);
        when(mTabModel.isTabMultiSelected(0)).thenReturn(true);
        when(mTabModel.isTabMultiSelected(1)).thenReturn(true);
        when(mTabModel.getOrderedMultiSelectedTabs()).thenReturn(List.of(mTab, mTab2));

        // trigger Ctrl+W keyboard shortcut once.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean isKeyEventHandled =
                            keyDown(
                                    KeyEvent.KEYCODE_W,
                                    KeyEvent.META_CTRL_ON,
                                    /* isCurrentTabVisible= */ true);
                    assertTrue("Expected key event to be handled", isKeyEventHandled);
                });

        // Verify tab is closed and pending state is cleared.
        verify(
                        mTabRemover,
                        description(
                                "Expected closeTabs to be called on the pinned tab on second"
                                        + " attempt."))
                .closeTabs(
                        eq(
                                TabClosureParams.closeTabs(List.of(mTab, mTab2))
                                        .allowUndo(false)
                                        .tabClosingSource(TabClosingSource.KEYBOARD_SHORTCUT)
                                        .build()),
                        /* allowDialog= */ eq(true));
        verify(mPinnedTabCloseManager).clearPendingState(mTabModelSelector);
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
        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_F6, 0));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.switch_keyboard_focus_row), /* fromMenu= */ eq(false));
    }

    @Test
    @SmallTest
    public void testTabSearch() {
        assertTrue(
                keyDown(KeyEvent.KEYCODE_A, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON, true));
        verify(mMenuOrKeyboardActionController, times(1))
                .onMenuOrKeyboardAction(/* id= */ eq(R.id.tab_search), /* fromMenu= */ eq(false));
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
        View mockMenuButton = mock(View.class);
        when(mToolbarManager.getMenuButtonView()).thenReturn(mockMenuButton);

        assertTrue(keyDown(KeyEvent.KEYCODE_F10, 0, true));
        verify(mToolbarManager, times(1)).getMenuButtonView();
        verify(mockMenuButton, times(1)).requestFocus();
    }

    /** Test that pressing F7 triggers the caret browsing dialog. */
    @Test
    @SmallTest
    public void testToggleCaretBrowsing() {
        // Ensure we handle F7 key (this was previously ignored)
        assertTrue(dispatchKeyEvent(KeyEvent.KEYCODE_F7, 0));

        // Ensure we trigger the caret browsing dialog
        verify(mMenuOrKeyboardActionController)
                .onMenuOrKeyboardAction(eq(R.id.toggle_caret_browsing), eq(false));
    }

    /** Test that pressing F1 triggers the help action. */
    @Test
    @SmallTest
    public void testOpenHelp() {
        // Ensure we handle F1 key
        assertTrue(keyDown(KeyEvent.KEYCODE_F1, 0, true));

        // Ensure we trigger the help action
        verify(mMenuOrKeyboardActionController).onMenuOrKeyboardAction(eq(R.id.help_id), eq(false));
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
