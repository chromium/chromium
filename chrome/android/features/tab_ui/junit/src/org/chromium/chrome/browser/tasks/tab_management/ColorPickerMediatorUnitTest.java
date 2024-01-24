// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Tests for the ColorPickerMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ColorPickerMediatorUnitTest {
    private static final @TabGroupColorId int DEFAULT_SELECTION = TabGroupColorId.GREY;

    private ColorPickerMediator mMediator;
    private ModelList mModelList = new ModelList();

    @Before
    public void setUp() {
        mMediator = new ColorPickerMediator(mModelList);
    }

    @Test
    public void testAddColorPicker_selectionState() {
        mMediator.setColorListItems();

        Assert.assertEquals(TabGroupColorId.class.getDeclaredFields().length, mModelList.size());

        for (ListItem item : mModelList) {
            if (DEFAULT_SELECTION == item.model.get(ColorPickerItemProperties.COLOR_ID)) {
                Assert.assertTrue(item.model.get(ColorPickerItemProperties.IS_SELECTED));
            } else {
                Assert.assertFalse(item.model.get(ColorPickerItemProperties.IS_SELECTED));
            }
        }

        Assert.assertEquals(DEFAULT_SELECTION, mMediator.getSelectedColor());
    }
}
