// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Tests for the TabGroupColorPickerMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TabGroupColorPickerMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private TabGroupColorPickerMediator mMediator;
    private List<Integer> mColorIds;
    private final List<PropertyModel> mColorItems = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mColorIds = TabGroupColorPickerUtils.getTabGroupColorIdList();

        for (int i = 0; i < mColorIds.size(); i++) {
            int color = mColorIds.get(i);
            PropertyModel model =
                    TabGroupColorPickerItemProperties.create(
                            /* color= */ color,
                            /* colorPickerType= */ TabGroupColorPickerType.TAB_GROUP,
                            /* isIncognito= */ false,
                            /* onClickListener= */ () -> {
                                mMediator.setSelectedColorItem(color);
                            },
                            /* isSelected= */ false,
                            /* itemIndex= */ i);
            mColorItems.add(model);
        }

        mMediator = new TabGroupColorPickerMediator(mColorItems);
    }

    @Test
    public void testTabGroupColorPicker_setSelectedColor() {
        int selectedColor = mColorIds.get(1);
        mMediator.setSelectedColorItem(selectedColor);

        for (PropertyModel model : mColorItems) {
            if (selectedColor == model.get(TabGroupColorPickerItemProperties.COLOR_ID)) {
                assertTrue(model.get(TabGroupColorPickerItemProperties.IS_SELECTED));
            } else {
                assertFalse(model.get(TabGroupColorPickerItemProperties.IS_SELECTED));
            }
        }

        assertEquals(selectedColor, (int) mMediator.getSelectedColorSupplier().get());
    }
}
