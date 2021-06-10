// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Test of {@link ContinuousNavigationUserDataImplTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousNavigationUserDataImplTest {
    private static final String TEST_QUERY = "Bar";
    private static final int TEST_RESULT_TYPE = 1;

    @Mock
    private Tab mTabMock;
    @Mock
    private SearchUrlHelper.Natives mSearchUrlHelperJniMock;

    private GURL mSrpUrl;
    private GURL mResultUrl1;
    private GURL mResultUrl2;
    private GURL mInvalidUrl;
    private UserDataHost mUserDataHost;
    private ContinuousNavigationUserDataImpl mUserData;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Before
    public void setUp() {
        initMocks(this);
        mUserDataHost = new UserDataHost();
        mSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
        mJniMocker.mock(SearchUrlHelperJni.TEST_HOOKS, mSearchUrlHelperJniMock);
        doReturn(TEST_QUERY).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mSrpUrl));
        doReturn(TEST_RESULT_TYPE)
                .when(mSearchUrlHelperJniMock)
                .getSrpPageCategoryFromUrl(eq(mSrpUrl));
        mResultUrl1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        mResultUrl2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1);
        mInvalidUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_2);
        doReturn(mUserDataHost).when(mTabMock).getUserDataHost();
        mUserData = ContinuousNavigationUserDataImpl.getOrCreateForTab(mTabMock);
        mUserData.mAllowNativeUrlChecks = false;
    }

    class FakeContinuousNavigationUserDataObserver implements ContinuousNavigationUserDataObserver {
        private boolean mInvalidated;
        private ContinuousNavigationMetadata mMetadata;
        private GURL mUrl;
        private boolean mOnSrp;

        @Override
        public void onInvalidate() {
            mInvalidated = true;
            mMetadata = null;
            mUrl = null;
            mOnSrp = false;
        }

        @Override
        public void onUpdate(ContinuousNavigationMetadata metadata) {
            mMetadata = metadata;
            mInvalidated = false;
        }

        @Override
        public void onUrlChanged(GURL url, boolean onSrp) {
            mUrl = url;
            mOnSrp = onSrp;
        }

        public boolean wasInvalidated() {
            return mInvalidated;
        }

        public ContinuousNavigationMetadata getMetadata() {
            return mMetadata;
        }

        public GURL getCurrentUrl() {
            return mUrl;
        }

        public boolean getOnSrp() {
            return mOnSrp;
        }
    }

    /**
     * Verifies that if data exists a new observer will immediately observe it.
     */
    @Test
    public void testAddObserverWithData() {
        ContinuousNavigationMetadata metadata = loadData();
        FakeContinuousNavigationUserDataObserver observer = attachObserver();

        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertEquals(mSrpUrl, observer.getCurrentUrl());
        Assert.assertTrue(observer.getOnSrp());
        Assert.assertEquals(metadata, observer.getMetadata());
    }

    /**
     * Verifies that when data is updated an observer will get the notification.
     */
    @Test
    public void testObserveUpdateData() {
        FakeContinuousNavigationUserDataObserver observer = attachObserver();
        ContinuousNavigationMetadata metadata = loadData();

        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertEquals(mSrpUrl, observer.getCurrentUrl());
        Assert.assertTrue(observer.getOnSrp());
        Assert.assertEquals(metadata, observer.getMetadata());
    }

    /**
     * Verifies that when the current URL is updated an observer receives the correct information.
     */
    @Test
    public void testUpdateCurrentUrl() {
        FakeContinuousNavigationUserDataObserver observer = attachObserver();

        // No-op without data.
        mUserData.updateCurrentUrl(mSrpUrl);
        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertNull(observer.getCurrentUrl());
        Assert.assertFalse(observer.getOnSrp());

        loadData();

        // On SRP.
        mUserData.updateCurrentUrl(mSrpUrl);
        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertEquals(mSrpUrl, observer.getCurrentUrl());
        Assert.assertTrue(observer.getOnSrp());

        // On result.
        mUserData.updateCurrentUrl(mResultUrl1);
        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertEquals(mResultUrl1, observer.getCurrentUrl());
        Assert.assertFalse(observer.getOnSrp());

        // On invalid result.
        mUserData.updateCurrentUrl(mInvalidUrl);
        Assert.assertTrue(observer.wasInvalidated());
        Assert.assertNull(observer.getCurrentUrl());
        Assert.assertFalse(observer.getOnSrp());

        Assert.assertFalse(mUserData.isValid());
    }

    /**
     * Verifies that a removed observer will no longer get updates.
     */
    @Test
    public void testAddRemoveObserver() {
        FakeContinuousNavigationUserDataObserver observer = attachObserver();
        loadData();

        mUserData.updateCurrentUrl(mResultUrl2);
        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertEquals(mResultUrl2, observer.getCurrentUrl());
        Assert.assertFalse(observer.getOnSrp());

        mUserData.removeObserver(observer);

        mUserData.updateCurrentUrl(mInvalidUrl);
        // No-change as no longer observing.
        Assert.assertFalse(observer.wasInvalidated());
        Assert.assertEquals(mResultUrl2, observer.getCurrentUrl());
        Assert.assertFalse(observer.getOnSrp());
    }

    private FakeContinuousNavigationUserDataObserver attachObserver() {
        FakeContinuousNavigationUserDataObserver observer =
                new FakeContinuousNavigationUserDataObserver();
        mUserData.addObserver(observer);
        return observer;
    }

    private ContinuousNavigationMetadata loadData() {
        GURL url3 = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_2);
        GURL url4 = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_3);

        List<PageGroup> groups = new ArrayList<PageGroup>();
        List<PageItem> results1 = new ArrayList<PageItem>();
        results1.add(new PageItem(mResultUrl1, "Foo.com 1"));
        groups.add(new PageGroup("Foo", false, results1));
        List<PageItem> results2 = new ArrayList<PageItem>();
        results2.add(new PageItem(mResultUrl2, "Bar.com 1"));
        results2.add(new PageItem(url3, "Bar.com 2"));
        results2.add(new PageItem(url4, "Bar.com 3"));
        groups.add(new PageGroup("Bar", true, results2));

        ContinuousNavigationMetadata metadata = new ContinuousNavigationMetadata(mSrpUrl,
                TEST_QUERY, new ContinuousNavigationMetadata.Provider(TEST_RESULT_TYPE, null, 0),
                groups);
        mUserData.updateData(metadata, mSrpUrl);
        return metadata;
    }
}
