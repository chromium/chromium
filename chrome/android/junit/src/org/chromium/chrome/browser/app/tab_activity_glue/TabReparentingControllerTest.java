// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController.Delegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManagerFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link TabReparentingControllerTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabReparentingControllerTest {
    class FakeNightModeReparentingDelegate implements Delegate {
        TabModelSelector mTabModelSelector;

        @Override
        public TabModelSelector getTabModelSelector() {
            if (mTabModelSelector == null) {
                // setup
                mTabModelSelector = Mockito.mock(TabModelSelector.class);

                doReturn(mTabModel).when(mTabModelSelector).getModel(false);
                doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
            }

            return mTabModelSelector;
        }

        @Override
        public boolean isNtpUrl(GURL url) {
            return UrlConstants.NTP_NON_NATIVE_URL.equals(url.getSpec())
                    || UrlConstants.NTP_URL.equals(url.getSpec());
        }
    }

    @Mock ReparentingTask mTask;
    @Mock Profile mProfile;
    @Mock Profile mIncognitoProfile;

    MockTabModel mTabModel;
    MockTabModel mIncognitoTabModel;
    Map<Tab, Integer> mTabIndexMapping = new HashMap<>();
    Tab mForegroundTab;

    TabReparentingController mController;
    FakeNightModeReparentingDelegate mFakeDelegate;
    AsyncTabParamsManager mRealAsyncTabParamsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        mTabModel = new MockTabModel(mProfile, null);
        mIncognitoTabModel = new MockTabModel(mIncognitoProfile, null);

        mFakeDelegate = new FakeNightModeReparentingDelegate();
        mRealAsyncTabParamsManager = AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        mController = new TabReparentingController(mFakeDelegate, mRealAsyncTabParamsManager);
    }

    @After
    public void tearDown() {
        mForegroundTab = null;
        mRealAsyncTabParamsManager.getAsyncTabParams().clear();
        mTabIndexMapping.clear();
    }

    @Test
    public void testReparenting_singleTab() {
        mForegroundTab = createAndAddMockTab(1, false);
        mController.prepareTabsForReparenting();

        AsyncTabParams params = mRealAsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        verify(mTask, times(1)).detach();
    }

    @Test
    public void testReparenting_singleTab_NTP() {
        // New tab pages aren't reparented intentionally.
        mForegroundTab = createAndAddMockTab(1, false, JUnitTestGURLs.NTP_URL);
        mController.prepareTabsForReparenting();

        Assert.assertFalse(mRealAsyncTabParamsManager.hasParamsWithTabToReparent());
    }

    @Test
    public void testReparenting_singleTab_reparentingAttemptedTwice() {
        mForegroundTab = createAndAddMockTab(1, false);
        mController.prepareTabsForReparenting();
        // Simulate the theme being changed twice before the application is recreated.
        mController.prepareTabsForReparenting();

        AsyncTabParams params = mRealAsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        verify(mTask, times(1)).detach();
    }

    @Test
    public void testReparenting_multipleTabs() {
        mForegroundTab = createAndAddMockTab(1, false);
        createAndAddMockTab(2, false);
        mController.prepareTabsForReparenting();

        TabReparentingParams trp =
                (TabReparentingParams) mRealAsyncTabParamsManager.getAsyncTabParams().get(1);
        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        trp = (TabReparentingParams) mRealAsyncTabParamsManager.getAsyncTabParams().get(2);

        tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);

        verify(mTask, times(2)).detach();
    }

    @Test
    public void testReparenting_twoTabsOutOfOrder() {
        createAndAddMockTab(1, false);
        mForegroundTab = createAndAddMockTab(2, false);
        mController.prepareTabsForReparenting();

        AsyncTabParams params = mRealAsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);

        verify(mTask, times(2)).detach();
    }

    @Test
    public void testReparenting_twoTabsOneIncognito() {
        createAndAddMockTab(1, false);
        mForegroundTab = createAndAddMockTab(2, true);
        mController.prepareTabsForReparenting();

        AsyncTabParams params = mRealAsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);

        verify(mTask, times(2)).detach();
    }

    @Test
    public void testReparenting_threeTabsOutOfOrder() {
        createAndAddMockTab(3, false);
        mForegroundTab = createAndAddMockTab(2, false);
        createAndAddMockTab(1, false);
        mController.prepareTabsForReparenting();

        // Check the foreground tab.
        TabReparentingParams trp =
                (TabReparentingParams) mRealAsyncTabParamsManager.getAsyncTabParams().get(2);

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);

        // Check the background tabs.
        trp = (TabReparentingParams) mRealAsyncTabParamsManager.getAsyncTabParams().get(1);
        trp = (TabReparentingParams) mRealAsyncTabParamsManager.getAsyncTabParams().get(3);

        verify(mTask, times(3)).detach();
    }

    @Test
    public void testReparenting_stopLoadingIfNeeded() {
        // New tab pages aren't reparented intentionally.
        mForegroundTab = createAndAddMockTab(1, false);
        doReturn(true).when(mForegroundTab).isLoading();

        mController.prepareTabsForReparenting();

        verify(mForegroundTab).stopLoading();
        verify(mForegroundTab.getWebContents().getNavigationController()).setNeedsReload();
    }

    /**
     * Adds a tab to the correct model and sets the index in the mapping.
     *
     * @param id The id to give to the tab.
     * @param incognito Whether to add the tab to the incognito model or the regular model.
     * @param url The url to set for the tab.
     * @return The tab that was added. Use the return value to set the foreground tab for tests.
     */
    private Tab createAndAddMockTab(int id, boolean incognito, GURL url) {
        Tab tab = Mockito.mock(Tab.class);
        WebContents wc = Mockito.mock(WebContents.class);
        NavigationController nc = Mockito.mock(NavigationController.class);
        doReturn(url).when(tab).getUrl();
        doReturn(wc).when(tab).getWebContents();
        doReturn(nc).when(wc).getNavigationController();
        UserDataHost udh = new UserDataHost();
        udh.setUserData(ReparentingTask.class, mTask);
        doReturn(udh).when(tab).getUserDataHost();
        doReturn(id).when(tab).getId();

        int index;
        if (incognito) {
            mIncognitoTabModel.addTab(
                    tab,
                    -1,
                    TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabCreationState.LIVE_IN_FOREGROUND);
            index = mIncognitoTabModel.indexOf(tab);
        } else {
            mTabModel.addTab(
                    tab,
                    -1,
                    TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabCreationState.LIVE_IN_FOREGROUND);
            index = mTabModel.indexOf(tab);
        }
        mTabIndexMapping.put(tab, index);

        return tab;
    }

    private Tab createAndAddMockTab(int id, boolean incognito) {
        return createAndAddMockTab(id, incognito, JUnitTestGURLs.EXAMPLE_URL);
    }
}
