// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.content.Intent;
import android.view.ViewGroup;

import androidx.test.core.app.ActivityScenario;

import com.google.android.material.tabs.TabLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.url.GURL;

/** Unit tests for HistoryClustersCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HistoryClustersCoordinatorTest.ShadowHistoryClustersBridge.class},
        manifest = Config.NONE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
public class HistoryClustersCoordinatorTest {
    @Implements(HistoryClustersBridge.class)
    static class ShadowHistoryClustersBridge {
        static HistoryClustersBridge sBridge;
        @Implementation
        public static HistoryClustersBridge getForProfile(Profile profile) {
            return sBridge;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();
    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private Tab mTab;
    @Mock
    private Profile mProfile;
    @Mock
    private HistoryClustersBridge mHistoryClustersBridge;
    @Mock
    LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private TabLayout mToggleView;

    private ActivityScenario<ChromeTabbedActivity> mActivityScenario;
    private HistoryClustersCoordinator mHistoryClustersCoordinator;
    private Intent mIntent = new Intent();
    private Activity mActivity;
    private Promise mPromise = new Promise();

    @Before
    public void setUp() {
        resetStaticState();
        jniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);
        doReturn(1L).when(mMockLargeIconBridgeJni).init();
        ShadowHistoryClustersBridge.sBridge = mHistoryClustersBridge;
        doReturn(mPromise).when(mHistoryClustersBridge).queryClusters(anyString());

        mActivityScenario = ActivityScenario.launch(ChromeTabbedActivity.class);
        HistoryClustersDelegate historyClustersDelegate = new HistoryClustersDelegate() {
            @Override
            public boolean isSeparateActivity() {
                return true;
            }

            @Override
            public Tab getTab() {
                return mTab;
            }

            @Override
            public Intent getHistoryActivityIntent() {
                return mIntent;
            }

            @Override
            public Intent getOpenUrlIntent(GURL gurl) {
                return mIntent;
            }

            @Override
            public ViewGroup getToggleView(ViewGroup parent) {
                return mToggleView;
            }
        };

        mActivityScenario.onActivity(activity -> {
            mActivity = activity;
            mHistoryClustersCoordinator = new HistoryClustersCoordinator(
                    mProfile, activity, mTemplateUrlService, historyClustersDelegate);
        });
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
        resetStaticState();
    }

    @Test
    public void testGetActivityContentView() {
        ViewGroup viewGroup = mHistoryClustersCoordinator.getActivityContentView();
        assertNotNull(viewGroup);

        SelectableListLayout listLayout = viewGroup.findViewById(R.id.selectable_list);
        assertNotNull(listLayout);

        HistoryClustersToolbar toolbar = listLayout.findViewById(R.id.action_bar);
        assertNotNull(toolbar);
    }

    @Test
    public void testSearchMenuItem() {
        HistoryClustersToolbar toolbar = mHistoryClustersCoordinator.getActivityContentView()
                                                 .findViewById(R.id.selectable_list)
                                                 .findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        mHistoryClustersCoordinator.onMenuItemClick(
                toolbar.getMenu().findItem(R.id.search_menu_id));
        assertTrue(toolbar.isSearching());
    }

    @Test
    public void testCloseMenuItem() {
        HistoryClustersToolbar toolbar = mHistoryClustersCoordinator.getActivityContentView()
                                                 .findViewById(R.id.selectable_list)
                                                 .findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        assertFalse(mActivity.isFinishing());
        mHistoryClustersCoordinator.onMenuItemClick(toolbar.getMenu().findItem(R.id.close_menu_id));
        assertTrue(mActivity.isFinishing());
    }

    private static void resetStaticState() {
        DisplayAndroidManager.resetInstanceForTesting();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }
}
