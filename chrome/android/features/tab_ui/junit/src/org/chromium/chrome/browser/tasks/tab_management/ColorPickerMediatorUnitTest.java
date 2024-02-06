// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Tests for the ColorPickerMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ColorPickerMediatorUnitTest {
    @Mock private ColorPickerContainer mContainerView;

    private Activity mActivity;
    private ColorPickerMediator mMediator;
    private List<Integer> mColorIds;
    private List<PropertyModel> mColorItems = new ArrayList<>();

    @Captor ArgumentCaptor<List<FrameLayout>> mColorViewsCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mColorIds = ColorPickerUtils.getTabGroupColorIdList();
        mMediator = new ColorPickerMediator(mColorIds, mColorItems);
    }

    @Test
    public void testColorPicker_setColorListItems() {
        when(mContainerView.getContext()).thenReturn(mActivity);

        mMediator.setColorListItems(mContainerView);
        verify(mContainerView, times(1)).setColorViews(mColorViewsCaptor.capture());

        List<FrameLayout> colorViewsValue = mColorViewsCaptor.getValue();

        assertNotNull(colorViewsValue);
        assertEquals(mColorIds.size(), colorViewsValue.size());
    }

    @Test
    public void testColorPicker_setSelectedColor() {
        int selectedColor = mColorIds.get(1);
        mMediator.setSelectedColorItem(selectedColor);

        for (PropertyModel model : mColorItems) {
            if (selectedColor == model.get(ColorPickerItemProperties.COLOR_ID)) {
                assertTrue(model.get(ColorPickerItemProperties.IS_SELECTED));
            } else {
                assertFalse(model.get(ColorPickerItemProperties.IS_SELECTED));
            }
        }

        assertEquals(selectedColor, (int) mMediator.getSelectedColorSupplier().get());
    }
}
