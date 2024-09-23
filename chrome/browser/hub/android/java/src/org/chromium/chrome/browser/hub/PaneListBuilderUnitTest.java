// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.verifyNoInteractions;

import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link PaneListBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaneListBuilderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LazyOneshotSupplier<Pane> mMockSupplier;

    @Test
    @SmallTest
    public void testRegisterNoPanes() {
        var panes = new PaneListBuilder(new DefaultPaneOrderController()).build();

        assertTrue(panes.isEmpty());
    }

    @Test
    @SmallTest
    public void testRegisterAllWithDefaultOrder() {
        PaneOrderController orderController = new DefaultPaneOrderController();
        PaneListBuilder builder = new PaneListBuilder(orderController);
        registerAllPanes(builder);
        ImmutableMap<Integer, LazyOneshotSupplier<Pane>> panes = builder.build();

        assertEquals(PaneId.COUNT, panes.size());
        assertEquals(orderController.getPaneOrder(), panes.keySet());
        verifyNoInteractions(mMockSupplier);
    }

    @Test
    @SmallTest
    public void testRegisterAllWithReverseDefaultOrder() {
        PaneOrderController orderController = createReverseDefaultOrderController();
        PaneListBuilder builder = new PaneListBuilder(orderController);
        registerAllPanes(builder);
        ImmutableMap<Integer, LazyOneshotSupplier<Pane>> panes = builder.build();

        assertEquals(PaneId.COUNT, panes.size());
        assertEquals(orderController.getPaneOrder().asList(), panes.keySet().asList());
        verifyNoInteractions(mMockSupplier);
    }

    @Test
    @SmallTest
    public void testRegisterSubsetOfPanesInPaneOrderController() {
        PaneOrderController orderController = createReverseDefaultOrderController();

        var panes =
                new PaneListBuilder(orderController)
                        .registerPane(PaneId.TAB_SWITCHER, mMockSupplier)
                        .build();

        assertEquals(1, panes.size());
        assertTrue(panes.containsKey(PaneId.TAB_SWITCHER));
        assertFalse(panes.containsKey(PaneId.BOOKMARKS));
        verifyNoInteractions(mMockSupplier);
    }

    @Test
    @SmallTest
    public void testAlreadyBuiltThrowsException() {
        PaneOrderController orderController = new DefaultPaneOrderController();
        PaneListBuilder builder = new PaneListBuilder(orderController);
        assertFalse(builder.isBuilt());

        builder.registerPane(PaneId.TAB_SWITCHER, mMockSupplier);
        assertFalse(builder.isBuilt());

        builder.build();
        assertTrue(builder.isBuilt());

        try {
            builder.registerPane(PaneId.INCOGNITO_TAB_SWITCHER, mMockSupplier);
            fail("IllegalStateException should have been thrown for registerPane().");
        } catch (IllegalStateException e) {
            // This should catch the exception silently.
        }

        try {
            builder.build();
            fail("IllegalStateException should have been thrown for build().");
        } catch (IllegalStateException e) {
            // This should catch the exception silently.
        }
        verifyNoInteractions(mMockSupplier);
    }

    private PaneOrderController createReverseDefaultOrderController() {
        return () ->
                ImmutableSet.copyOf(
                        new DefaultPaneOrderController().getPaneOrder().asList().reverse());
    }

    private void registerAllPanes(PaneListBuilder builder) {
        // Assumes there are no missing numbers. Relatively fragile.
        for (@PaneId int i = 0; i < PaneId.COUNT; i++) {
            builder.registerPane(i, mMockSupplier);
        }
    }
}
