// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.dragdrop.toolbar.ToolbarDragDropCoordinator.isValidMimeType;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipData.Item;
import android.view.DragEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Basic test for creating, using drag and drop to omnibox with {@link ToolbarDragDropCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarDragDropCoordinatorUnitTest {
    @Mock
    private AutocompleteDelegate mAutocompleteDelegate;
    private Activity mActivity;

    private ToolbarDragDropCoordinator mToolbarDragDropCoordinator;
    private TargetViewDragListener mTargetViewDragListener;
    private FrameLayout mTargetView;
    private ClipData mValidData =
            new ClipData(null, new String[] {"text/plain"}, new Item("Mock Text"));
    private ClipData mInvalidData =
            new ClipData(null, new String[] {"image/jpeg", "text/html"}, new Item("Mock Text"));

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mTargetView = (FrameLayout) LayoutInflater.from(mActivity).inflate(
                R.layout.drag_drop_target_view, null);
        mAutocompleteDelegate = Mockito.mock(AutocompleteDelegate.class);
        mToolbarDragDropCoordinator =
                new ToolbarDragDropCoordinator(mTargetView, mAutocompleteDelegate);
        PropertyModel model = new PropertyModel.Builder(TargetViewProperties.ALL_KEYS)
                                      .with(TargetViewProperties.TARGET_VIEW_VISIBLE, View.GONE)
                                      .build();
        // mTargetViewDragListener is stateless in order to test TargetViewDragListener
        mTargetViewDragListener = new TargetViewDragListener(model, mAutocompleteDelegate);
    }

    @After
    public void tearDown() {
        mToolbarDragDropCoordinator.destroy();
        mTargetViewDragListener = null;
        assertEquals(
                "target view should be gone on destroy", mTargetView.getVisibility(), View.GONE);
        mActivity.finish();
    }

    @Test
    public void testOnDragStart_MimeTypeNotSupported() {
        DragEvent event = mockDragEvent(DragEvent.ACTION_DRAG_STARTED, mInvalidData);
        boolean result = mToolbarDragDropCoordinator.onDrag(mTargetView, event);
        Assert.assertFalse("Drag event should not be consumed.", result);
        assertEquals("MimeType should not be valid", isValidMimeType(event), false);
        assertEquals(
                "The target view should not be visible.", mTargetView.getVisibility(), View.GONE);
    }

    @Test
    public void testOnDragStart_MimeTypeSupported() {
        DragEvent event = mockDragEvent(DragEvent.ACTION_DRAG_STARTED, mValidData);
        boolean result = mToolbarDragDropCoordinator.onDrag(mTargetView, event);
        assertTrue("Drag event should be consumed.", result);
        assertEquals("MimeType should be valid", isValidMimeType(event), true);
        assertEquals(
                "The target view should be visible.", mTargetView.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testOnDragEnd_ViewGone() {
        DragEvent event = mockDragEvent(DragEvent.ACTION_DRAG_ENDED, mValidData);
        boolean result = mToolbarDragDropCoordinator.onDrag(mTargetView, event);
        assertFalse("Drag event should not be consumed.", result);
        assertEquals(
                "The target view should not be visible.", mTargetView.getVisibility(), View.GONE);
    }

    @Test
    public void testToolbarDragEvents_EventsNotConsumed() {
        DragEvent eventDragEntered = mockDragEvent(DragEvent.ACTION_DRAG_ENTERED, mValidData);
        boolean resultDragEntered =
                mToolbarDragDropCoordinator.onDrag(mTargetView, eventDragEntered);
        assertFalse("Drag event should not be consumed by toolbar.", resultDragEntered);

        DragEvent eventDragExited = mockDragEvent(DragEvent.ACTION_DRAG_EXITED, mValidData);
        boolean resultDragExited = mToolbarDragDropCoordinator.onDrag(mTargetView, eventDragExited);
        assertFalse("Drag event should not be consumed by toolbar.", resultDragExited);

        DragEvent eventDragLocation = mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, mValidData);
        boolean resultDragLocation =
                mToolbarDragDropCoordinator.onDrag(mTargetView, eventDragLocation);
        assertFalse("Drag event should not be consumed by toolbar.", resultDragLocation);

        DragEvent eventDrop = mockDragEvent(DragEvent.ACTION_DROP, mValidData);
        boolean resultDrop = mToolbarDragDropCoordinator.onDrag(mTargetView, eventDrop);
        assertFalse("Drag event should not be consumed by toolbar.", resultDrop);
    }

    @Test
    public void testTargetViewDragListener_DragEntered() {
        DragEvent event = mockDragEvent(DragEvent.ACTION_DRAG_ENTERED, mValidData);
        boolean result = mTargetViewDragListener.onDrag(mTargetView, event);
        assertFalse("Drag event should not be consumed by target view.", result);
    }

    @Test
    public void testTargetViewDragListener_DragExited() {
        DragEvent event = mockDragEvent(DragEvent.ACTION_DRAG_EXITED, mValidData);
        boolean result = mTargetViewDragListener.onDrag(mTargetView, event);
        assertFalse("Drag event should not be consumed by target view.", result);
    }

    @Test
    public void testTargetViewDragListener_ActionDrop() {
        DragEvent event = mockDragEvent(DragEvent.ACTION_DROP, mValidData);
        boolean result = mTargetViewDragListener.onDrag(mTargetView, event);
        assertTrue("Drag event should be consumed by target view", result);
        // TODO(https://crbug.com/1465940): Fix parsing and add tests for parsing
    }

    @Test
    public void testTargetViewDragEvents_EventsNotConsumed() {
        DragEvent eventDragEnded = mockDragEvent(DragEvent.ACTION_DRAG_ENDED, mValidData);
        boolean resultDragEnded = mTargetViewDragListener.onDrag(mTargetView, eventDragEnded);
        assertFalse("Drag event should not be consumed by target view.", resultDragEnded);

        DragEvent eventDragLocation = mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, mValidData);
        boolean resultDragLocation = mTargetViewDragListener.onDrag(mTargetView, eventDragLocation);
        assertFalse("Drag event should not be consumed by target view.", resultDragLocation);
    }

    private DragEvent mockDragEvent(int action, ClipData data) {
        DragEvent event = Mockito.mock(DragEvent.class);
        doReturn(action).when(event).getAction();
        doReturn(data).when(event).getClipData();
        doReturn(data.getDescription()).when(event).getClipDescription();
        return event;
    }
}
