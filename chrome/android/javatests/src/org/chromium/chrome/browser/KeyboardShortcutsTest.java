// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/** Unit tests for KeyboardShortcuts. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class KeyboardShortcutsTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ToolbarManager mToolbarManager;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModel()).thenAnswer(invocation -> mTabModel);
    }

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

    private void testOpenBookmarks(
            boolean expectHandled, boolean isCurrentTabVisible, int metaState) {
        assertEquals(
                expectHandled,
                KeyboardShortcuts.onKeyDown(
                        new KeyEvent(
                                /* downTime= */ SystemClock.uptimeMillis(),
                                /* eventTime= */ SystemClock.uptimeMillis(),
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_O,
                                /* repeat= */ 0,
                                metaState),
                        isCurrentTabVisible,
                        /* tabSwitchingEnabled= */ true,
                        mTabModelSelector,
                        mMenuOrKeyboardActionController,
                        mToolbarManager));

        verify(mMenuOrKeyboardActionController, expectHandled ? times(1) : never())
                .onMenuOrKeyboardAction(
                        /* id= */ eq(R.id.all_bookmarks_menu_id),
                        /* fromMenu= */ expectHandled ? eq(false) : anyBoolean());
    }
}
