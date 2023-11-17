// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link PaneTransitionHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaneTransitionHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private Pane mBookmarksPane;
    @Mock private PaneLookup mPaneLookup;

    private PaneTransitionHelper mPaneTransitionHelper;

    @Before
    public void setUp() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mPaneLookup.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPane);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mPaneLookup.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER))
                .thenReturn(mIncognitoTabSwitcherPane);
        when(mBookmarksPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        when(mPaneLookup.getPaneForId(PaneId.BOOKMARKS)).thenReturn(mBookmarksPane);

        mPaneTransitionHelper = new PaneTransitionHelper(mPaneLookup);
    }

    @After
    public void tearDown() {
        if (mPaneTransitionHelper != null) {
            mPaneTransitionHelper.destroy();
        }
    }

    @Test
    @SmallTest
    public void testProcessTransition() {
        mPaneTransitionHelper.processTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));
    }

    @Test
    @SmallTest
    public void testProcessAlreadyQueuedTransition() {
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.COLD);
        mPaneTransitionHelper.processTransition(PaneId.TAB_SWITCHER, LoadHint.WARM);
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));
        ShadowLooper.idleMainLooper();
        // Verify no additional calls.
        verify(mTabSwitcherPane).notifyLoadHint(anyInt());
    }

    @Test
    @SmallTest
    public void testRemoveQueuedTransition() {
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        mPaneTransitionHelper.removeTransition(PaneId.TAB_SWITCHER);
        ShadowLooper.idleMainLooper();
        verify(mTabSwitcherPane, never()).notifyLoadHint(anyInt());
    }

    @Test
    @SmallTest
    public void testQueueRepeatedly() {
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        ShadowLooper.idleMainLooper();
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));
    }

    @Test
    @SmallTest
    public void testQueueTransitions() {
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        mPaneTransitionHelper.queueTransition(PaneId.INCOGNITO_TAB_SWITCHER, LoadHint.WARM);
        mPaneTransitionHelper.processTransition(PaneId.BOOKMARKS, LoadHint.COLD);
        verify(mBookmarksPane).notifyLoadHint(eq(LoadHint.COLD));

        mPaneTransitionHelper.queueTransition(PaneId.BOOKMARKS, LoadHint.WARM);

        ShadowLooper.runMainLooperOneTask();
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.WARM);

        ShadowLooper.runMainLooperOneTask();
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));

        ShadowLooper.runMainLooperOneTask();
        verify(mBookmarksPane).notifyLoadHint(eq(LoadHint.WARM));

        mPaneTransitionHelper.removeTransition(PaneId.TAB_SWITCHER);
        ShadowLooper.idleMainLooper();
        // Verify no additional calls.
        verify(mTabSwitcherPane).notifyLoadHint(anyInt());
    }

    @Test
    @SmallTest
    public void testQueueTransitionsAndDestroy() {
        mPaneTransitionHelper.queueTransition(PaneId.TAB_SWITCHER, LoadHint.HOT);
        mPaneTransitionHelper.queueTransition(PaneId.INCOGNITO_TAB_SWITCHER, LoadHint.WARM);
        mPaneTransitionHelper.processTransition(PaneId.BOOKMARKS, LoadHint.COLD);
        verify(mBookmarksPane).notifyLoadHint(eq(LoadHint.COLD));

        mPaneTransitionHelper.destroy();
        ShadowLooper.idleMainLooper();
        verify(mTabSwitcherPane, never()).notifyLoadHint(anyInt());
        verify(mIncognitoTabSwitcherPane, never()).notifyLoadHint(anyInt());

        mPaneTransitionHelper = null;
    }
}
