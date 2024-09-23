// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils.INVALID_COLOR_ID;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.collection.ArraySet;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Set;

/** Tests for {@link TabGroupColorUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupColorUtilsUnitTest {

    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final String MIGRATION_CHECK = "migration_check";
    private static final int MIGRATION_DONE = 1;

    private static final int ROOT_ID_1 = 123;
    private static final int ROOT_ID_2 = 456;
    private static final int ROOT_ID_3 = 789;
    private static final int ROOT_ID_4 = 147;
    private static final int ROOT_ID_5 = 258;
    private static final int ROOT_ID_6 = 369;
    private static final int ROOT_ID_7 = 159;
    private static final int ROOT_ID_8 = 160;
    private static final int ROOT_ID_9 = 161;
    private static final int ROOT_ID_10 = 162;
    private static final int COLOR_1 = 0;
    private static final int COLOR_2 = 1;
    private static final int COLOR_3 = 2;
    private static final int COLOR_4 = 3;
    private static final int COLOR_5 = 4;
    private static final int COLOR_6 = 5;
    private static final int COLOR_7 = 6;
    private static final int COLOR_8 = 7;
    private static final int COLOR_9 = 8;

    @Mock Context mContext;
    @Mock TabGroupModelFilter mFilter;
    @Mock SharedPreferences mSharedPreferences;
    @Mock SharedPreferences.Editor mEditor;
    @Mock SharedPreferences.Editor mPutIntEditor;
    @Mock SharedPreferences.Editor mRemoveEditor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mRemoveEditor).when(mEditor).remove(any(String.class));
        doReturn(mPutIntEditor).when(mEditor).putInt(any(String.class), any(Integer.class));
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    public void testDeleteTabGroupColor() {
        TabGroupColorUtils.deleteTabGroupColor(ROOT_ID_1);

        verify(mEditor).remove(eq(String.valueOf(ROOT_ID_1)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testGetTabGroupColor() {
        // Mock that we have a stored tab group color with reference to ROOT_ID.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);

        assertThat(TabGroupColorUtils.getTabGroupColor(ROOT_ID_1), equalTo(COLOR_1));
    }

    @Test
    public void testStoreTabGroupColor() {
        TabGroupColorUtils.storeTabGroupColor(ROOT_ID_1, COLOR_1);

        verify(mEditor).putInt(eq(String.valueOf(ROOT_ID_1)), eq(COLOR_1));
        verify(mPutIntEditor).apply();
    }

    @Test
    public void testAssignDefaultTabGroupColors() {
        Set<Integer> rootIdsSet = new ArraySet<>();
        rootIdsSet.add(ROOT_ID_1);
        rootIdsSet.add(ROOT_ID_2);
        rootIdsSet.add(ROOT_ID_3);

        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        // Mock that there is no stored tab group color for these root ids.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(INVALID_COLOR_ID);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_2), INVALID_COLOR_ID))
                .thenReturn(INVALID_COLOR_ID);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_3), INVALID_COLOR_ID))
                .thenReturn(INVALID_COLOR_ID);

        TabGroupColorUtils.assignTabGroupColorsIfApplicable(mFilter);

        // Test the scenario where no tab groups have colors so the first colors in order are
        // assigned.
        verify(mEditor).putInt(eq(String.valueOf(ROOT_ID_1)), eq(COLOR_1));
        verify(mEditor).putInt(eq(String.valueOf(ROOT_ID_2)), eq(COLOR_2));
        verify(mEditor).putInt(eq(String.valueOf(ROOT_ID_3)), eq(COLOR_3));
        verify(mEditor).putInt(eq(MIGRATION_CHECK), eq(MIGRATION_DONE));
        verify(mPutIntEditor, times(4)).apply();
    }

    @Test
    public void testNextSuggestedColorFirstAndThird() {
        Set<Integer> rootIdsSet = new ArraySet<>();
        rootIdsSet.add(ROOT_ID_1);
        rootIdsSet.add(ROOT_ID_2);

        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        // Mock that the first and third colors already exist.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_2), INVALID_COLOR_ID))
                .thenReturn(COLOR_3);

        assertEquals(COLOR_2, TabGroupColorUtils.getNextSuggestedColorId(mFilter));
    }

    @Test
    public void testNextSuggestedColorDoubleFirstAndSecond() {
        Set<Integer> rootIdsSet = new ArraySet<>();
        rootIdsSet.add(ROOT_ID_1);
        rootIdsSet.add(ROOT_ID_2);
        rootIdsSet.add(ROOT_ID_3);

        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        // Mock that the first and second colors already exist.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_2), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_3), INVALID_COLOR_ID))
                .thenReturn(COLOR_2);

        assertEquals(COLOR_3, TabGroupColorUtils.getNextSuggestedColorId(mFilter));
    }

    @Test
    public void testNextSuggestedColorSecondColor() {
        Set<Integer> rootIdsSet = new ArraySet<>();
        rootIdsSet.add(ROOT_ID_1);

        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        // Mock that only the second color already exists.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(COLOR_2);

        assertEquals(COLOR_1, TabGroupColorUtils.getNextSuggestedColorId(mFilter));
    }

    @Test
    public void testNextSuggestedColorAllColorsUsed() {
        Set<Integer> rootIdsSet = new ArraySet<>();
        rootIdsSet.add(ROOT_ID_1);
        rootIdsSet.add(ROOT_ID_2);
        rootIdsSet.add(ROOT_ID_3);
        rootIdsSet.add(ROOT_ID_4);
        rootIdsSet.add(ROOT_ID_5);
        rootIdsSet.add(ROOT_ID_6);
        rootIdsSet.add(ROOT_ID_7);
        rootIdsSet.add(ROOT_ID_8);
        rootIdsSet.add(ROOT_ID_9);

        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        // Mock that all colors are used.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_2), INVALID_COLOR_ID))
                .thenReturn(COLOR_2);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_3), INVALID_COLOR_ID))
                .thenReturn(COLOR_3);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_4), INVALID_COLOR_ID))
                .thenReturn(COLOR_4);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_5), INVALID_COLOR_ID))
                .thenReturn(COLOR_5);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_6), INVALID_COLOR_ID))
                .thenReturn(COLOR_6);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_7), INVALID_COLOR_ID))
                .thenReturn(COLOR_7);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_8), INVALID_COLOR_ID))
                .thenReturn(COLOR_8);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_9), INVALID_COLOR_ID))
                .thenReturn(COLOR_9);

        assertEquals(COLOR_1, TabGroupColorUtils.getNextSuggestedColorId(mFilter));
    }

    @Test
    public void testNextSuggestedColorContinuousSuggestion() {
        Set<Integer> rootIdsSet = new ArraySet<>();
        rootIdsSet.add(ROOT_ID_1);
        rootIdsSet.add(ROOT_ID_2);
        rootIdsSet.add(ROOT_ID_3);
        rootIdsSet.add(ROOT_ID_4);
        rootIdsSet.add(ROOT_ID_5);
        rootIdsSet.add(ROOT_ID_6);
        rootIdsSet.add(ROOT_ID_7);
        rootIdsSet.add(ROOT_ID_8);

        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        // Mock that all colors are used except for COLOR_8.
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_1), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_2), INVALID_COLOR_ID))
                .thenReturn(COLOR_2);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_3), INVALID_COLOR_ID))
                .thenReturn(COLOR_3);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_4), INVALID_COLOR_ID))
                .thenReturn(COLOR_4);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_5), INVALID_COLOR_ID))
                .thenReturn(COLOR_5);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_6), INVALID_COLOR_ID))
                .thenReturn(COLOR_6);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_7), INVALID_COLOR_ID))
                .thenReturn(COLOR_7);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_8), INVALID_COLOR_ID))
                .thenReturn(COLOR_9);

        assertEquals(COLOR_8, TabGroupColorUtils.getNextSuggestedColorId(mFilter));

        // Mock that subsequent addition of the missing color directs the suggestion to COLOR_1.
        rootIdsSet.add(ROOT_ID_9);
        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_9), INVALID_COLOR_ID))
                .thenReturn(COLOR_8);
        assertEquals(COLOR_1, TabGroupColorUtils.getNextSuggestedColorId(mFilter));

        // Mock that subsequent addition of the first color directs the suggestion to COLOR_2.
        rootIdsSet.add(ROOT_ID_10);
        when(mFilter.getAllTabGroupRootIds()).thenReturn(rootIdsSet);
        when(mSharedPreferences.getInt(String.valueOf(ROOT_ID_10), INVALID_COLOR_ID))
                .thenReturn(COLOR_1);
        assertEquals(COLOR_2, TabGroupColorUtils.getNextSuggestedColorId(mFilter));
    }
}
