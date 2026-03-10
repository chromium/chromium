// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator.TabCreationData;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.CompletableFuture;

/** Unit tests for {@link RecordingTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
public class RecordingTabCreatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabCreator mDelegate;
    @Mock private Tab mTab;
    @Mock private TabState mTabState;
    @Mock private WebContents mWebContents;
    @Mock private CompletableFuture<Boolean> mAddTabToModel;

    private LoadUrlParams mLoadUrlParams;
    private GURL mGurl;
    private RecordingTabCreator mRecordingTabCreator;

    @Before
    public void setUp() {
        mGurl = new GURL("https://example.com");
        mLoadUrlParams = new LoadUrlParams(mGurl.getSpec());
        mRecordingTabCreator = new RecordingTabCreator();
        mRecordingTabCreator.setDelegate(mDelegate);
    }

    @Test
    public void testCreateNewTab() {
        mRecordingTabCreator.createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab);

        verify(mDelegate).createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertEquals(mGurl.getSpec(), data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    public void testCreateNewTab_WithPosition() {
        mRecordingTabCreator.createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab, 5);

        verify(mDelegate).createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab, 5);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertEquals(mGurl.getSpec(), data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    public void testCreateNewTab_WithTitle() {
        mRecordingTabCreator.createNewTab(
                mLoadUrlParams, "Title", TabLaunchType.FROM_LINK, mTab, 5);

        verify(mDelegate).createNewTab(mLoadUrlParams, "Title", TabLaunchType.FROM_LINK, mTab, 5);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertEquals(mGurl.getSpec(), data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    public void testCreateFrozenTab() {
        mTabState.timestampMillis = 12345L;
        mRecordingTabCreator.createFrozenTab(mTabState, 5, TabModel.INVALID_TAB_INDEX);

        verify(mDelegate).createFrozenTab(mTabState, 5, TabModel.INVALID_TAB_INDEX);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(1, frozenData.size());
        assertEquals(5, frozenData.get(0).id);
        assertEquals(12345L, frozenData.get(0).timestampMillis);

        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(0, data.size());
    }

    @Test
    public void testLaunchUrl() {
        mRecordingTabCreator.launchUrl(mGurl.getSpec(), TabLaunchType.FROM_LINK);

        verify(mDelegate).launchUrl(mGurl.getSpec(), TabLaunchType.FROM_LINK);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertEquals(mGurl.getSpec(), data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    public void testCreateTabWithWebContents() {
        mRecordingTabCreator.createTabWithWebContents(
                mTab, true, mWebContents, TabLaunchType.FROM_LINK, mGurl, 5, mAddTabToModel);

        verify(mDelegate)
                .createTabWithWebContents(
                        mTab,
                        true,
                        mWebContents,
                        TabLaunchType.FROM_LINK,
                        mGurl,
                        5,
                        mAddTabToModel);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertEquals(mGurl.getSpec(), data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    public void testCreateTabWithHistory() {
        when(mTab.getUrl()).thenReturn(mGurl);

        mRecordingTabCreator.createTabWithHistory(mTab, TabLaunchType.FROM_LINK);

        verify(mDelegate).createTabWithHistory(mTab, TabLaunchType.FROM_LINK);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertEquals(mGurl.getSpec(), data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    public void testLaunchNtp() {
        mRecordingTabCreator.launchNtp(TabLaunchType.FROM_LINK);

        verify(mDelegate).launchNtp(TabLaunchType.FROM_LINK);

        assertEquals(1, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(1, data.size());
        assertEquals(Tab.INVALID_TAB_ID, data.get(0).id);
        assertEquals(0, data.get(0).timestampMillis);
        assertNull(data.get(0).url);

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testCreateNewTab_FeatureDisabled() {
        mRecordingTabCreator.createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab);

        verify(mDelegate).createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab);

        assertEquals(0, mRecordingTabCreator.getTabCount());
        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(0, data.size());

        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testCreateFrozenTab_FeatureDisabled() {
        mTabState.timestampMillis = 12345L;
        mRecordingTabCreator.createFrozenTab(mTabState, 5, TabModel.INVALID_TAB_INDEX);

        verify(mDelegate).createFrozenTab(mTabState, 5, TabModel.INVALID_TAB_INDEX);

        assertEquals(0, mRecordingTabCreator.getTabCount());
        List<TabCreationData> frozenData = mRecordingTabCreator.getFrozenTabCreationData();
        assertEquals(0, frozenData.size());

        List<TabCreationData> data = mRecordingTabCreator.getNewTabCreationData();
        assertEquals(0, data.size());
    }
}
