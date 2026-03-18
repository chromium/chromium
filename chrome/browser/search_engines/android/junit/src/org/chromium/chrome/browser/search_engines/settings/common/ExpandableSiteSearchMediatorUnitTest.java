// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.withSettings;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ExpandableSiteSearchMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExpandableSiteSearchMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;

    private Context mContext;
    private ModelList mModelList;
    private ExpandableSiteSearchMediator mMediator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        mModelList = new ModelList();

        mMediator =
                mock(
                        ExpandableSiteSearchMediator.class,
                        withSettings()
                                .useConstructor(mContext, mModelList, mProfile)
                                .defaultAnswer(Mockito.CALLS_REAL_METHODS));
    }

    @Test
    public void testOnTemplateURLServiceChanged() {
        PropertyModel buttonModel = new PropertyModel(SiteSearchProperties.ALL_KEYS);
        mMediator.onMoreButtonClicked(buttonModel);
        assertTrue(mMediator.isExpandedForTesting());

        mMediator.onTemplateURLServiceChanged();

        assertFalse(mMediator.isExpandedForTesting());
        verify(mMediator).refreshList();
    }

    @Test
    public void testHiddenItemsManagement() {
        assertTrue(mMediator.areExpandableItemsEmptyForTesting());

        ListItem item1 = new ListItem(0, null);
        ListItem item2 = new ListItem(0, null);

        mMediator.addExpandableItemForTesting(item1);
        mMediator.addExpandableItemForTesting(item2);

        assertFalse(mMediator.areExpandableItemsEmptyForTesting());

        mMediator.clearAllItems();

        assertTrue(mMediator.areExpandableItemsEmptyForTesting());
    }

    @Test
    public void testSetUpMoreButtonIfNeeded_AtThreshold() {
        mMediator.setUpMoreButtonIfNeeded(ExpandableSiteSearchMediator.DEFAULT_MAX_ROWS);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testSetUpMoreButtonIfNeeded_AboveThreshold() {
        mMediator.setUpMoreButtonIfNeeded(ExpandableSiteSearchMediator.DEFAULT_MAX_ROWS + 1);

        assertEquals(1, mModelList.size());
        ListItem moreItem = mModelList.get(0);
        assertEquals(SiteSearchProperties.ViewType.MORE, moreItem.type);
        assertFalse(moreItem.model.get(SiteSearchProperties.IS_EXPANDED));
        assertNotNull(moreItem.model.get(SiteSearchProperties.ON_CLICK));
    }

    @Test
    public void testOnMoreButtonClicked_ExpandAndCollapse() {
        ListItem baseItem = new ListItem(0, null);
        mModelList.add(baseItem);

        ListItem hiddenItem1 = new ListItem(0, null);
        ListItem hiddenItem2 = new ListItem(0, null);
        mMediator.addExpandableItemForTesting(hiddenItem1);
        mMediator.addExpandableItemForTesting(hiddenItem2);

        PropertyModel moreButtonModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.IS_EXPANDED, false)
                        .build();
        mModelList.add(new ListItem(SiteSearchProperties.ViewType.MORE, moreButtonModel));

        assertFalse(mMediator.isExpandedForTesting());
        assertEquals(2, mModelList.size()); // baseItem + moreButton

        // Click to expand
        mMediator.onMoreButtonClicked(moreButtonModel);

        assertTrue(mMediator.isExpandedForTesting());
        assertTrue(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));

        // ModelList should now have 4 items: baseItem, moreButton, hiddenItem1, hiddenItem2
        assertEquals(4, mModelList.size());
        assertEquals(hiddenItem1, mModelList.get(2));
        assertEquals(hiddenItem2, mModelList.get(3));

        // Click to collapse
        mMediator.onMoreButtonClicked(moreButtonModel);

        assertFalse(mMediator.isExpandedForTesting());
        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));

        // Hidden items should be removed, leaving only the baseItem and moreButton
        assertEquals(2, mModelList.size());
        assertEquals(baseItem, mModelList.get(0));
        assertEquals(SiteSearchProperties.ViewType.MORE, mModelList.get(1).type);
    }
}
