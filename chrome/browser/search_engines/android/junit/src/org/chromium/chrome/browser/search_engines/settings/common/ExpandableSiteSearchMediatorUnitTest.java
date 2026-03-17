// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ExpandableSiteSearchMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExpandableSiteSearchMediatorUnitTest {
    // TODO(crbug.com/492059926): This abstract class should be tested with mock and verify the
    // methods called instead.
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private View mMockView;

    private Context mContext;
    private ModelList mModelList;
    private TestBaseMediator mMediator;

    private class TestBaseMediator extends ExpandableSiteSearchMediator {
        private int mUrlsToSimulate;

        public TestBaseMediator(Context context, ModelList modelList, Profile profile) {
            super(context, modelList, profile);
            initializeTemplateUrlService();
        }

        public void setUrlsToSimulate(int count) {
            mUrlsToSimulate = count;
            refreshList();
        }

        @Override
        protected void refreshList() {
            mModelList.clear();
            clearHiddenItems();

            for (int i = 0; i < mUrlsToSimulate; i++) {
                ListItem item = createListItem(mTemplateUrl);
                if (i < DEFAULT_MAX_ROWS) {
                    mModelList.add(item);
                } else {
                    addHiddenItem(item);
                }
            }

            // Simulating an 'Add' button to ensure dynamic index math works properly
            mModelList.add(new ListItem(SiteSearchProperties.ViewType.ADD, new PropertyModel()));

            setUpMoreButtonIfNeeded(mUrlsToSimulate);
        }

        @Override
        protected ListMenuDelegate createMenuDelegate(TemplateUrl url) {
            return null;
        }
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mModelList = new ModelList();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        when(mTemplateUrl.getShortName()).thenReturn("Test");

        mMediator = new TestBaseMediator(mContext, mModelList, mProfile);
    }

    @Test
    public void testSetUpMoreButton_LessThanDefaultMaxRows() {
        mMediator.setUrlsToSimulate(3); // Less than DEFAULT_MAX_ROWS (5)

        // ModelList should contain: 3 engines + 1 Add button = 4 items
        assertEquals(4, mModelList.size());

        // Verify no MORE button exists
        for (ListItem item : mModelList) {
            assertFalse(item.type == SiteSearchProperties.ViewType.MORE);
        }
    }

    @Test
    public void testSetUpMoreButton_GreaterThanDefaultMaxRows() {
        mMediator.setUrlsToSimulate(8); // Greater than DEFAULT_MAX_ROWS (5)

        // ModelList should contain: 5 engines + 1 Add button + 1 More button = 7 items
        assertEquals(7, mModelList.size());

        ListItem lastItem = mModelList.get(mModelList.size() - 1);
        assertEquals(SiteSearchProperties.ViewType.MORE, lastItem.type);

        PropertyModel moreButtonModel = lastItem.model;
        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
    }

    @Test
    public void testExpandAndCollapse_DynamicRowsCount() {
        mMediator.setUrlsToSimulate(8); // 3 hidden items

        ListItem moreButtonItem = mModelList.get(mModelList.size() - 1);
        PropertyModel moreButtonModel = moreButtonItem.model;

        int initialListSize = mModelList.size(); // Should be 7

        // --- EXPAND ---
        // Trigger the click listener on the More button
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(mMockView);

        assertTrue(mMediator.isExpandedForTesting());
        assertTrue(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));

        // Size should grow by the 3 hidden items (7 + 3 = 10)
        assertEquals(initialListSize + 3, mModelList.size());

        // --- COLLAPSE ---
        // Trigger the click listener again
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(mMockView);

        assertFalse(mMediator.isExpandedForTesting());
        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));

        assertEquals(initialListSize, mModelList.size());
    }

    @Test
    public void testOnTemplateURLServiceChanged_ResetsExpandedState() {
        mMediator.setUrlsToSimulate(8);

        // Expand it
        ListItem moreButtonItem = mModelList.get(mModelList.size() - 1);
        moreButtonItem.model.get(SiteSearchProperties.ON_CLICK).onClick(mMockView);
        assertTrue(mMediator.isExpandedForTesting());

        // Trigger a data change
        mMediator.onTemplateURLServiceChanged();

        // State should be reset to false
        assertFalse(mMediator.isExpandedForTesting());
    }
}
