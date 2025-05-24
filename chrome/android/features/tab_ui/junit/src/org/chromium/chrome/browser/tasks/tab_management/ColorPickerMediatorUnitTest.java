// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Tests for the ColorPickerMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ColorPickerMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private ColorPickerMediator mMediator;
    private List<Integer> mColorIds;
    private final List<PropertyModel> mColorItems = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mColorIds = TabGroupColorUtils.getTabGroupColorIdList();

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
