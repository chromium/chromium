// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link PaneManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaneManagerImplUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DisplayButtonData mReferenceButtonData;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private Supplier<Pane> mPaneSupplier;
    @Mock private Runnable mRunnable;

    private final ObservableSupplierImpl<Boolean> mHubVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<DisplayButtonData>
            mTabSwitcherPaneReferenceButtonDataSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<DisplayButtonData>
            mIncognitoTabSwitcherPaneReferenceButtonDataSupplier = new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mTabSwitcherPaneReferenceButtonDataSupplier);
        mTabSwitcherPaneReferenceButtonDataSupplier.set(mReferenceButtonData);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mIncognitoTabSwitcherPaneReferenceButtonDataSupplier);
        mIncognitoTabSwitcherPaneReferenceButtonDataSupplier.set(mReferenceButtonData);
    }

    @Test
    @SmallTest
    public void testFocusChangesPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane));
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);

        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(mIncognitoTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        mIncognitoTabSwitcherPaneReferenceButtonDataSupplier.set(null);
        assertFalse(paneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        verify(mTabSwitcherPane, never()).destroy();
        verify(mIncognitoTabSwitcherPane, never()).destroy();

        paneManager.destroy();

        verify(mTabSwitcherPane).destroy();
        verify(mIncognitoTabSwitcherPane).destroy();
    }

    @Test
    @SmallTest
    public void testFocusUnregisteredPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane));
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);

        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        paneManager.destroy();
    }

    @Test
    @SmallTest
    public void testFocusUnsuppliedPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(PaneId.BOOKMARKS, LazyOneshotSupplier.fromValue(null));
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);

        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        paneManager.destroy();
    }

    @Test
    @SmallTest
    public void testPaneSuppliedLazily() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromSupplier(mPaneSupplier));
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);
        verifyNoInteractions(mPaneSupplier);

        paneManager.focusPane(PaneId.TAB_SWITCHER);
        verify(mPaneSupplier).get();

        paneManager.destroy();
    }

    @Test
    @SmallTest
    public void testPaneNotDestroyedIfNotSupplied() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane));
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);

        paneManager.destroy();
        verifyNoInteractions(mTabSwitcherPane);
    }

    @Test
    @SmallTest
    public void testRepeatFocusIgnored() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane));
        mHubVisibilitySupplier.set(true);
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);
        ShadowLooper.runUiThreadTasks();

        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));

        paneManager.focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));

        paneManager.focusPane(PaneId.TAB_SWITCHER);
        ShadowLooper.runUiThreadTasks();
        // Not notified a second time.
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));

        paneManager.destroy();
    }

    @Test
    @SmallTest
    public void testChangeHubVisibilityNoFocusedPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane));
        mHubVisibilitySupplier.set(false);
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);
        ShadowLooper.runUiThreadTasks();

        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.COLD));
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.COLD));

        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));

        paneManager.destroy();
    }

    @Test
    @SmallTest
    public void testChangeHubVisibilityWithFocusedPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane));
        mHubVisibilitySupplier.set(true);
        PaneManagerImpl paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);
        ShadowLooper.runUiThreadTasks();

        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.WARM));

        paneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER);
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));

        mHubVisibilitySupplier.set(false);
        ShadowLooper.runUiThreadTasks();
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.COLD));
        verify(mIncognitoTabSwitcherPane, times(2)).notifyLoadHint(eq(LoadHint.WARM));
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.COLD));

        paneManager.focusPane(PaneId.TAB_SWITCHER);
        ShadowLooper.runUiThreadTasks();
        // Not counted again.
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.COLD));
        verify(mIncognitoTabSwitcherPane).notifyLoadHint(eq(LoadHint.COLD));

        mHubVisibilitySupplier.set(true);
        verify(mTabSwitcherPane).notifyLoadHint(eq(LoadHint.HOT));
        ShadowLooper.runUiThreadTasks();
        verify(mIncognitoTabSwitcherPane, times(3)).notifyLoadHint(eq(LoadHint.WARM));

        paneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER);
        verify(mIncognitoTabSwitcherPane, times(2)).notifyLoadHint(eq(LoadHint.HOT));
        ShadowLooper.runUiThreadTasks();
        verify(mTabSwitcherPane, times(2)).notifyLoadHint(eq(LoadHint.WARM));

        paneManager.destroy();
    }

    @Test
    @SmallTest
    public void testGetPaneById() {
        LazyOneshotSupplierImpl<Pane> supplier =
                new LazyOneshotSupplierImpl<>() {
                    @Override
                    public void doSet() {
                        mRunnable.run();
                        // Don't call set. We'll do that manually. Call mRunnable so we can verify
                        // called.
                    }
                };

        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(PaneId.TAB_SWITCHER, supplier);
        PaneManager paneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);
        assertNull(paneManager.getPaneForId(PaneId.TAB_SWITCHER));
        verify(mRunnable).run();

        supplier.set(mTabSwitcherPane);
        assertEquals(mTabSwitcherPane, paneManager.getPaneForId(PaneId.TAB_SWITCHER));
    }
}
