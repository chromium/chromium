// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;

/**
 * Tests for {@link SnackbarCollection}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class SnackbarCollectionUnitTest {
    private static final String ACTION_TITLE = "stack";
    private static final String NOTIFICATION_TITLE = "queue";

    @Mock private SnackbarController mMockController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @Feature({"Browser", "Snackbar"})
    public void testActionCoversNotification() {
        SnackbarCollection collection = new SnackbarCollection();
        assertTrue(collection.isEmpty());

        Snackbar notiBar = makeNotificationSnackbar();
        collection.add(notiBar);
        assertFalse(collection.isEmpty());
        assertEquals(notiBar, collection.getCurrent());

        Snackbar actionBar = makeActionSnackbar();
        collection.add(actionBar);
        verify(mMockController, times(1)).onDismissNoAction(null);
        assertFalse(collection.isEmpty());
        assertEquals("Notification snackbar should not cover action snackbar!", actionBar,
                collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(mMockController, times(1)).onAction(null);
        assertTrue(collection.isEmpty());
    }

    @Test
    @Feature({"Browser", "Snackbar"})
    public void testNotificationGoesUnderAction() {
        SnackbarCollection collection = new SnackbarCollection();
        assertTrue(collection.isEmpty());

        Snackbar actionBar = makeActionSnackbar();
        collection.add(actionBar);
        assertFalse(collection.isEmpty());
        assertEquals(actionBar, collection.getCurrent());

        Snackbar notiBar = makeNotificationSnackbar();
        collection.add(notiBar);
        verify(mMockController, times(0)).onDismissNoAction(null);
        assertFalse(collection.isEmpty());
        assertEquals("Action snackbar should not be covered by notification snackbars!", actionBar,
                collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(mMockController, times(1)).onAction(null);
        assertFalse(collection.isEmpty());
        assertEquals(notiBar, collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(mMockController, times(2)).onAction(null);
        assertTrue(collection.isEmpty());
    }

    @Test
    @Feature({"Browser", "Snackbar"})
    public void testClear() {
        SnackbarCollection collection = new SnackbarCollection();
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar());
            collection.add(makeNotificationSnackbar());
        }
        assertFalse(collection.isEmpty());

        collection.clear();
        assertTrue(collection.isEmpty());
    }

    @Test
    @Feature({"Browser", "Snackbar"})
    public void testRemoveMatchingSnackbars() {
        SnackbarCollection collection = new SnackbarCollection();
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar());
            collection.add(makeNotificationSnackbar());
        }
        SnackbarController anotherController = mock(SnackbarController.class);
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar(anotherController));
            collection.add(makeNotificationSnackbar(anotherController));
        }

        collection.removeMatchingSnackbars(mMockController);
        while (!collection.isEmpty()) {
            Snackbar removed = collection.removeCurrentDueToAction();
            assertEquals(anotherController, removed.getController());
        }
    }

    @Test
    @Feature({"Browser", "Snackbar"})
    public void testRemoveMatchingSnackbarsWithData() {
        SnackbarCollection collection = new SnackbarCollection();
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar().setAction(ACTION_TITLE, i));
            collection.add(makeNotificationSnackbar().setAction(NOTIFICATION_TITLE, i));
        }
        SnackbarController anotherController = mock(SnackbarController.class);
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar(anotherController).setAction(ACTION_TITLE, i));
            collection.add(
                    makeNotificationSnackbar(anotherController).setAction(NOTIFICATION_TITLE, i));
        }

        final Integer dataToRemove = 0;
        collection.removeMatchingSnackbars(mMockController, dataToRemove);
        while (!collection.isEmpty()) {
            Snackbar removed = collection.removeCurrentDueToAction();
            assertFalse(mMockController == removed.getController()
                    && dataToRemove.equals(removed.getActionData()));
        }
    }

    private Snackbar makeActionSnackbar(SnackbarController controller) {
        return Snackbar.make(ACTION_TITLE, controller, Snackbar.TYPE_ACTION,
                Snackbar.UMA_TEST_SNACKBAR);
    }

    private Snackbar makeNotificationSnackbar(SnackbarController controller) {
        return Snackbar.make(NOTIFICATION_TITLE, controller, Snackbar.TYPE_NOTIFICATION,
                Snackbar.UMA_TEST_SNACKBAR);
    }

    private Snackbar makeActionSnackbar() {
        return makeActionSnackbar(mMockController);
    }

    private Snackbar makeNotificationSnackbar() {
        return makeNotificationSnackbar(mMockController);
    }
}
