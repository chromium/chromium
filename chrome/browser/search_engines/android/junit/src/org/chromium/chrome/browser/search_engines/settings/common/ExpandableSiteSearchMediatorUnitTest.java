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
import org.chromium.components.search_engines.FakeTemplateUrl;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ExpandableSiteSearchMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExpandableSiteSearchMediatorUnitTest {
    private static final String MORE_BUTTON_TEXT = "More";
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;

    private Context mContext;
    private ModelList mModelList;
    private ExpandableSiteSearchMediator mMediator;
    private ListItem mListItem;

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

        mListItem =
                new ListItem(
                        SiteSearchProperties.ViewType.SEARCH_ENGINE,
                        new PropertyModel(SiteSearchProperties.ALL_KEYS));
        Mockito.doReturn(mListItem).when(mMediator).createListItem(Mockito.any());
    }

    @Test
    public void testOnTemplateURLServiceChanged() {
        PropertyModel buttonModel = new PropertyModel(SiteSearchProperties.ALL_KEYS);
        mMediator.onMoreButtonClicked(buttonModel);
        assertTrue(mMediator.isExpandedForTesting());

        mMediator.onTemplateURLServiceChanged();

        assertTrue(mMediator.isExpandedForTesting());
        verify(mMediator).refreshList();
    }

    @Test
    public void testHiddenItemsManagement() {
        assertTrue(mMediator.areExpandableItemsEmptyForTesting());

        mMediator.addExpandableItemForTesting(mListItem);
        mMediator.addExpandableItemForTesting(mListItem);

        assertFalse(mMediator.areExpandableItemsEmptyForTesting());

        mMediator.clearAllItems();

        assertTrue(mMediator.areExpandableItemsEmptyForTesting());
    }

    @Test
    public void testSetUpMoreButtonIfNeeded_NoStagedUrls() {
        setUpStagedUrls(0);
        mMediator.setUpMoreButtonIfNeeded(MORE_BUTTON_TEXT);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testSetUpMoreButtonIfNeeded_WithStagedUrls() {
        setUpStagedUrls(1);
        mMediator.setUpMoreButtonIfNeeded(MORE_BUTTON_TEXT);

        assertEquals(1, mModelList.size());
        ListItem moreItem = mModelList.get(0);
        assertEquals(SiteSearchProperties.ViewType.MORE, moreItem.type);
        assertFalse(moreItem.model.get(SiteSearchProperties.IS_EXPANDED));
        assertNotNull(moreItem.model.get(SiteSearchProperties.ON_CLICK));
    }

    @Test
    public void testSetUpMoreButtonIfNeeded_InitiallyExpandedAndNowAboveThreshold() {
        setUpStagedUrls(1);
        // Expand the list first
        mMediator.onMoreButtonClicked(new PropertyModel(SiteSearchProperties.ALL_KEYS));
        assertTrue(mMediator.isExpandedForTesting());

        // Clear the list and populate again
        mMediator.clearAllItems();
        setUpStagedUrls(1);
        mMediator.setUpMoreButtonIfNeeded(MORE_BUTTON_TEXT);
        mMediator.maybeExpandListFromPreviousState();

        // Should still be expanded
        assertTrue(mMediator.isExpandedForTesting());
        assertEquals(2, mModelList.size());
        ListItem moreItem = mModelList.get(0);
        assertEquals(SiteSearchProperties.ViewType.MORE, moreItem.type);
        assertTrue(moreItem.model.get(SiteSearchProperties.IS_EXPANDED));
        assertNotNull(moreItem.model.get(SiteSearchProperties.ON_CLICK));
    }

    @Test
    public void testSetUpMoreButtonIfNeeded_InitiallyExpandedAndNowAtThreshold() {
        // Expand the list first
        mMediator.onMoreButtonClicked(new PropertyModel(SiteSearchProperties.ALL_KEYS));
        assertTrue(mMediator.isExpandedForTesting());
        mModelList.clear();

        mMediator.setUpMoreButtonIfNeeded(MORE_BUTTON_TEXT);

        // After refresh, there should be no "More" button and the state will become collapsed
        assertFalse(mMediator.isExpandedForTesting());
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testMaybeExpandListFromPreviousState_Expanded() {
        mMediator.onMoreButtonClicked(new PropertyModel(SiteSearchProperties.ALL_KEYS));
        mModelList.clear();

        int totalUrls = ExpandableSiteSearchMediator.DEFAULT_MAX_ROWS + 2;
        populateTemplateUrls(totalUrls);

        mMediator.maybeExpandListFromPreviousState();
        assertEquals(totalUrls, mModelList.size());
        // All elements are added to the list so there are no staged urls left.
        assertTrue(mMediator.areStagedUrlsEmptyForTesting());
    }

    @Test
    public void testMaybeExpandListFromPreviousState_NotExpanded() {
        int totalUrls = ExpandableSiteSearchMediator.DEFAULT_MAX_ROWS + 2;
        populateTemplateUrls(totalUrls);

        mMediator.maybeExpandListFromPreviousState();
        assertEquals(ExpandableSiteSearchMediator.DEFAULT_MAX_ROWS, mModelList.size());
        assertFalse(mMediator.isExpandedForTesting());
        assertFalse(mMediator.areStagedUrlsEmptyForTesting());
    }

    @Test
    public void testOnMoreButtonClicked_ExpandAndCollapse() {
        ListItem baseItem = new ListItem(0, new PropertyModel(SiteSearchProperties.ALL_KEYS));
        mModelList.add(baseItem);

        ListItem hiddenItem1 = new ListItem(0, new PropertyModel(SiteSearchProperties.ALL_KEYS));
        ListItem hiddenItem2 = new ListItem(0, new PropertyModel(SiteSearchProperties.ALL_KEYS));
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

    private void populateTemplateUrls(int totalUrls) {
        List<TemplateUrl> urls = new ArrayList<>();
        for (int i = 0; i < totalUrls; i++) {
            urls.add(new FakeTemplateUrl("url" + i, "keyword" + i));
        }

        mMediator.populateTemplateUrls(urls);
    }

    private void setUpStagedUrls(int totalStagedUrls) {
        for (int i = 0; i < totalStagedUrls; i++) {
            mMediator.addStagedUrlForTesting(
                    new FakeTemplateUrl("stagedUrl" + i, "stagedKeyword" + i));
        }
    }
}
