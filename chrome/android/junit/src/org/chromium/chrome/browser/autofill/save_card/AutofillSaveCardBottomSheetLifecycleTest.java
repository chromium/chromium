// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** Unit test for {@link AutofillSaveCardBottomSheetLifecycle}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveCardBottomSheetLifecycleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TabModel mTabModel;
    private AutofillSaveCardBottomSheetLifecycle mLifecycle;
    @Mock private AutofillSaveCardBottomSheetMediator mDelegate;
    @Mock private Tab mTab;

    @Before
    public void setUp() {
        mLifecycle =
                new AutofillSaveCardBottomSheetLifecycle(
                        mBottomSheetController, mLayoutStateProvider, mTabModel);
        mLifecycle.begin(mDelegate);
    }

    @Test
    public void testBegin() {
        // mLifecycle.begin(mDelegate) called in setUp().
        verify(mBottomSheetController).addObserver(eq(mLifecycle));
        verify(mLayoutStateProvider).addObserver(eq(mLifecycle));
        verify(mTabModel).addObserver(eq(mLifecycle));
    }

    @Test
    public void testEnd() {
        mLifecycle.end();

        verify(mTabModel).removeObserver(eq(mLifecycle));
        verify(mLayoutStateProvider).removeObserver(eq(mLifecycle));
        verify(mBottomSheetController).removeObserver(eq(mLifecycle));
    }

    @Test
    public void testSheetClosed_whenBackPress() {
        mLifecycle.onSheetClosed(StateChangeReason.BACK_PRESS);
        verify(mDelegate).onCanceled();

        clearInvocations(mDelegate);
        verifyNoInteractionsWhenSheetClosedForAllReasons();
    }

    @Test
    public void testSheetClosed_whenSwipe() {
        mLifecycle.onSheetClosed(StateChangeReason.SWIPE);
        verify(mDelegate).onCanceled();

        clearInvocations(mDelegate);
        verifyNoInteractionsWhenSheetClosedForAllReasons();
    }

    @Test
    public void testSheetClosed_whenTapScrim() {
        mLifecycle.onSheetClosed(StateChangeReason.TAP_SCRIM);
        verify(mDelegate).onCanceled();

        clearInvocations(mDelegate);
        verifyNoInteractionsWhenSheetClosedForAllReasons();
    }

    @Test
    public void testSheetClosed_whenInteractionComplete() {
        mLifecycle.onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);
        verifyNoInteractions(mDelegate);

        verifyNoInteractionsWhenSheetClosedForAllReasons();
    }

    @Test
    public void testSheetClosed_whenNone() {
        mLifecycle.onSheetClosed(StateChangeReason.NONE);
        verify(mDelegate).onIgnored();

        clearInvocations(mDelegate);
        verifyNoInteractionsWhenSheetClosedForAllReasons();
    }

    @Test
    public void testStartedShowing_whenLayoutTypeIsNotBrowsing() {
        mLifecycle.onStartedShowing(LayoutType.TAB_SWITCHER);
        verify(mDelegate).onIgnored();
    }

    @Test
    public void testStartedShowing_whenLayoutTypeIsBrowsing() {
        mLifecycle.onStartedShowing(LayoutType.BROWSING);
        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testStartedShowing_whenLayoutTypeIsNotBrowsing_whenCalledAgain() {
        mLifecycle.onStartedShowing(LayoutType.TAB_SWITCHER);
        clearInvocations(mDelegate);
        mLifecycle.onStartedShowing(LayoutType.TAB_SWITCHER);

        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testSelectTab_whenTabIdIsNotLastId() {
        when(mTab.getId()).thenReturn(1);
        mLifecycle.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 2);

        verify(mDelegate).onIgnored();
    }

    @Test
    public void testSelectTab_whenTabIdIsLastId() {
        when(mTab.getId()).thenReturn(1);
        mLifecycle.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 1);

        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testSelectTab_whenTabIdIsNotLastId_whenCalledAgain() {
        when(mTab.getId()).thenReturn(1);
        mLifecycle.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 2);
        clearInvocations(mDelegate);
        mLifecycle.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 2);

        verifyNoInteractions(mDelegate);
    }

    private void verifyNoInteractionsWhenSheetClosedForAllReasons() {
        for (@StateChangeReason int stateChangeReason = 0;
                stateChangeReason < StateChangeReason.MAX_VALUE;
                stateChangeReason++) {
            mLifecycle.onSheetClosed(stateChangeReason);
        }
        verifyNoInteractions(mDelegate);
    }
}
