// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

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

        for (int color : mColorIds) {
            PropertyModel model =
                    ColorPickerItemProperties.create(
                            /* color= */ color,
                            /* colorPickerType= */ ColorPickerType.TAB_GROUP,
                            /* isIncognito= */ false,
                            /* onClickListener= */ () -> {
                                mMediator.setSelectedColorItem(color);
                            },
                            /* isSelected= */ false);
            mColorItems.add(model);
        }

        mMediator = new ColorPickerMediator(mColorItems);
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
