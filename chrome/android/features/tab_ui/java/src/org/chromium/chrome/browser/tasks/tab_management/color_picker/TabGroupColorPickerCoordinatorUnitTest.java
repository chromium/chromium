// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RadioGroup;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;

import java.util.Arrays;
import java.util.List;

/** Tests for TabGroupColorPickerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TabGroupColorPickerCoordinatorUnitTest {
    private Activity mActivity;
    private TabGroupColorPickerCoordinator mCoordinator;
    private TabGroupColorPickerContainer mContainerView;
    private final List<Integer> mColors =
            Arrays.asList(TabGroupColorId.GREY, TabGroupColorId.BLUE, TabGroupColorId.RED);

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        View root =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.tab_group_color_picker_container, null);
        TabGroupColorPickerContainer container = root.findViewById(R.id.color_picker_container);
        mCoordinator =
                new TabGroupColorPickerCoordinator(
                        mActivity,
                        mColors,
                        container,
                        TabGroupColorPickerType.TAB_GROUP,
                        /* isIncognito= */ false,
                        TabGroupColorPickerCoordinator.TabGroupColorPickerLayoutType.DYNAMIC,
                        /* onColorItemClicked= */ null);
        mContainerView = (TabGroupColorPickerContainer) mCoordinator.getContainerView();
    }

    @Test
    @SmallTest
    public void testAccessibilityDelegate() {
        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        mContainerView
                .getAccessibilityDelegate()
                .onInitializeAccessibilityNodeInfo(mContainerView, info.unwrap());

        assertEquals(RadioGroup.class.getName(), info.getClassName());
        AccessibilityNodeInfoCompat.CollectionInfoCompat collectionInfo = info.getCollectionInfo();
        assertNotNull(collectionInfo);
        assertEquals(1, collectionInfo.getRowCount());
        assertEquals(mColors.size(), collectionInfo.getColumnCount());
        assertEquals(
                AccessibilityNodeInfoCompat.CollectionInfoCompat.SELECTION_MODE_SINGLE,
                collectionInfo.getSelectionMode());
    }
}
