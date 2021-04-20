// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.feed.NtpListContentManager.FeedContent;
import org.chromium.chrome.browser.feed.NtpListContentManager.FeedContentMetadata;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * A test for ContinuousFeedNavigationHelper
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH, ChromeFeatureList.CONTINUOUS_FEEDS})
public class ContinuousFeedNavigationHelperTest {
    static class TestFeedContent extends FeedContent {
        TestFeedContent(FeedContentMetadata metadata) {
            super("test", metadata);
        }

        @Override
        public boolean isNativeView() {
            return false;
        }
    }

    @Rule
    public final TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private ContinuousNavigationUserData mUserData;
    @Captor
    private ArgumentCaptor<ContinuousNavigationMetadata> mMetadataCaptor;
    @Captor
    private ArgumentCaptor<GURL> mUrlCaptor;

    ContinuousFeedNavigationHelper mFeedNavigationHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        mFeedNavigationHelper = new ContinuousFeedNavigationHelper(tabModelSelector);

        Tab tab = mock(Tab.class);
        doReturn(tab).when(tabModelSelector).getCurrentTab();
        doReturn(new UserDataHost()).when(tab).getUserDataHost();
        ContinuousNavigationUserData.setForTab(tab, mUserData);
    }

    @Test
    @SmallTest
    public void testNoContentChanged() {
        mFeedNavigationHelper.onNavigate("https://www.test.com");
        verify(mUserData, never()).updateData(any(), any());
    }

    @Test
    @SmallTest
    public void testSomeInvalidContents() {
        List<FeedContent> contents = new ArrayList<>();
        contents.add(new TestFeedContent(null));
        contents.add(new TestFeedContent(new FeedContentMetadata("invalid", "Invalid URL")));
        contents.add(new TestFeedContent(new FeedContentMetadata("https://invalid-title.com", "")));
        contents.add(new TestFeedContent(new FeedContentMetadata("https://www.test.com", "Valid")));
        mFeedNavigationHelper.onContentChanged(contents);
        mFeedNavigationHelper.onNavigate("https://www.test.com");

        verify(mUserData).updateData(mMetadataCaptor.capture(), mUrlCaptor.capture());
        ContinuousNavigationMetadata metadata = mMetadataCaptor.getValue();
        PageGroup pageGroup = metadata.getGroups().get(0);
        Assert.assertEquals(1, pageGroup.getPageItems().size());
    }

    @Test
    @SmallTest
    public void testValidContents() {
        List<FeedContent> contents = new ArrayList<>();
        contents.add(new TestFeedContent(new FeedContentMetadata("https://www.test.com", "Test")));
        contents.add(new TestFeedContent(new FeedContentMetadata("https://www.blue.com", "Blue")));
        contents.add(new TestFeedContent(new FeedContentMetadata("https://www.red.com", "Red")));

        mFeedNavigationHelper.onContentChanged(contents);
        mFeedNavigationHelper.onNavigate("https://www.red.com");

        verify(mUserData).updateData(mMetadataCaptor.capture(), mUrlCaptor.capture());
        Assert.assertEquals(new GURL("https://www.red.com"), mUrlCaptor.getValue());

        ContinuousNavigationMetadata metadata = mMetadataCaptor.getValue();
        Assert.assertEquals(PageCategory.DISCOVER, metadata.getCategory());
        Assert.assertEquals(1, metadata.getGroups().size());
        PageGroup pageGroup = metadata.getGroups().get(0);
        Assert.assertEquals(3, pageGroup.getPageItems().size());
    }
}
