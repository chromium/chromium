// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;

import android.graphics.drawable.Drawable;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;
import java.util.function.BiConsumer;

/** Tests for {@link TabGroupRowMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID, ChromeFeatureList.DATA_SHARING})
public class TabGroupRowMediatorUnitTest {
    private static final String SYNC_GROUP_ID1 = "remote one";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BiConsumer<GURL, Callback<Drawable>> mFaviconResolver;
    @Mock private Callback<Drawable> mFaviconCallback1;
    @Mock private Callback<Drawable> mFaviconCallback2;
    @Mock private Callback<Drawable> mFaviconCallback3;
    @Mock private Callback<Drawable> mFaviconCallback4;

    private PropertyModel buildTestModel(List<SavedTabGroupTab> savedTabs) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = savedTabs;

        return TabGroupRowMediator.buildModel(
                group, mFaviconResolver, /* openRunnable= */ null, /* deleteRunnable= */ null);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testNoParity() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(new SavedTabGroupTab()));
        // 0 is the default value.
        assertEquals(0, propertyModel.get(COLOR_INDEX));
    }

    @Test
    @SmallTest
    public void testFavicons_one() {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.url = JUnitTestGURLs.URL_1;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab));
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        assertNull(propertyModel.get(ASYNC_FAVICON_TOP_RIGHT));
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT));
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
    }

    @Test
    @SmallTest
    public void testFavicons_two() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2));
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT));
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
    }

    @Test
    @SmallTest
    public void testFavicons_three() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroupTab tab3 = new SavedTabGroupTab();
        tab3.url = JUnitTestGURLs.URL_3;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2, tab3));
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT).accept(mFaviconCallback3);
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_3), eq(mFaviconCallback3));
    }

    @Test
    @SmallTest
    public void testFavicons_four() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroupTab tab3 = new SavedTabGroupTab();
        tab3.url = JUnitTestGURLs.URL_3;
        SavedTabGroupTab tab4 = new SavedTabGroupTab();
        tab4.url = JUnitTestGURLs.BLUE_1;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2, tab3, tab4));
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT).accept(mFaviconCallback3);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT).accept(mFaviconCallback4);
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_3), eq(mFaviconCallback3));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.BLUE_1), eq(mFaviconCallback4));
    }

    @Test
    @SmallTest
    public void testFavicons_five() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroupTab tab3 = new SavedTabGroupTab();
        tab3.url = JUnitTestGURLs.URL_3;
        SavedTabGroupTab tab4 = new SavedTabGroupTab();
        tab4.url = JUnitTestGURLs.BLUE_1;
        SavedTabGroupTab tab5 = new SavedTabGroupTab();
        tab5.url = JUnitTestGURLs.BLUE_2;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2, tab3, tab4, tab5));
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT).accept(mFaviconCallback3);
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertEquals(2, propertyModel.get(PLUS_COUNT).intValue());
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_3), eq(mFaviconCallback3));
    }
}
