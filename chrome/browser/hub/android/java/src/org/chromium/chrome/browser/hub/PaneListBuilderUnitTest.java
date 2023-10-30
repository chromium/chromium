// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link PaneListBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaneListBuilderUnitTest {
    @Mock private Supplier<Pane> mMockSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

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

        var panes =
                new PaneListBuilder(orderController)
                        .registerPane(PaneId.BOOKMARKS, mMockSupplier)
                        .registerPane(PaneId.INCOGNITO_TAB_SWITCHER, mMockSupplier)
                        .registerPane(PaneId.TAB_SWITCHER, mMockSupplier)
                        .build();

        assertEquals(3, panes.size());
        assertEquals(orderController.getPaneOrder(), panes.keySet());
    }

    @Test
    @SmallTest
    public void testRegisterAllWithReverseDefaultOrder() {
        PaneOrderController orderController = createReverseDefaultOrderController();

        var panes =
                new PaneListBuilder(orderController)
                        .registerPane(PaneId.TAB_SWITCHER, mMockSupplier)
                        .registerPane(PaneId.INCOGNITO_TAB_SWITCHER, mMockSupplier)
                        .registerPane(PaneId.BOOKMARKS, mMockSupplier)
                        .build();

        assertEquals(3, panes.size());
        assertEquals(orderController.getPaneOrder().asList(), panes.keySet().asList());
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
    }

    private PaneOrderController createReverseDefaultOrderController() {
        return new PaneOrderController() {
            @Override
            public ImmutableSet<Integer> getPaneOrder() {
                return ImmutableSet.copyOf(
                        new DefaultPaneOrderController().getPaneOrder().asList().reverse());
            }
        };
    }
}
