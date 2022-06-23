// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.view.MenuItem;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;

/** Unit tests for HistoryClustersCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
public class HistoryClustersCoordinatorTest {
    private static final String INCOGNITO_EXTRA = "IN_INCOGNITO";
    private static final String NEW_TAB_EXTRA = "IN_NEW_TAB";

    class TestHistoryClustersDelegate implements HistoryClustersDelegate {
        private final ObservableSupplierImpl<Boolean> mShouldShowPrivacyDisclaimerSupplier =
                new ObservableSupplierImpl<>();
        private final ObservableSupplierImpl<Boolean> mShouldShowClearBrowsingDataSupplier =
                new ObservableSupplierImpl<>();

        public TestHistoryClustersDelegate() {
            mShouldShowPrivacyDisclaimerSupplier.set(true);
            mShouldShowClearBrowsingDataSupplier.set(true);
        }

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
            return mHistoryActivityIntent;
        }

        @Nullable
        @Override
        public Intent getOpenUrlIntent(GURL gurl, boolean inIncognito, boolean createNewTab) {
            mOpenUrlIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mOpenUrlIntent.putExtra(INCOGNITO_EXTRA, inIncognito);
            mOpenUrlIntent.putExtra(NEW_TAB_EXTRA, createNewTab);
            return mOpenUrlIntent;
        }

        @Override
        public ViewGroup getToggleView(ViewGroup parent) {
            return mToggleView;
        }

        @Nullable
        @Override
        public TabCreator getTabCreator(boolean isIncognito) {
            return mTabCreator;
        }

        @Override
        public void toggleInfoHeaderVisibility() {
            mShouldShowPrivacyDisclaimerSupplier.set(!mShouldShowPrivacyDisclaimerSupplier.get());
        }

        @Nullable
        @Override
        public ObservableSupplier<Boolean> shouldShowPrivacyDisclaimerSupplier() {
            return mShouldShowPrivacyDisclaimerSupplier;
        }

        @Nullable
        @Override
        public ObservableSupplier<Boolean> shouldShowClearBrowsingDataSupplier() {
            return mShouldShowClearBrowsingDataSupplier;
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
    @Mock
    private TabCreator mTabCreator;
    @Mock
    private GURL mGurl1;
    @Mock
    private GURL mGurl2;

    private ActivityScenario<ChromeTabbedActivity> mActivityScenario;
    private HistoryClustersCoordinator mHistoryClustersCoordinator;
    private Intent mHistoryActivityIntent = new Intent();
    private Intent mOpenUrlIntent = new Intent();
    private Activity mActivity;
    private Promise mPromise = new Promise();
    private ClusterVisit mVisit1;
    private ClusterVisit mVisit2;
    private TestHistoryClustersDelegate mHistoryClustersDelegate =
            new TestHistoryClustersDelegate();

    @Before
    public void setUp() {
        resetStaticState();
        jniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);
        doReturn(1L).when(mMockLargeIconBridgeJni).init();
        HistoryClustersBridge.setInstanceForTesting(mHistoryClustersBridge);
        doReturn(mPromise).when(mHistoryClustersBridge).queryClusters(anyString());

        mVisit1 = new ClusterVisit(
                1.0F, mGurl1, "Title 1", "foo.com", new ArrayList<>(), new ArrayList<>());
        mVisit2 = new ClusterVisit(
                1.0F, mGurl2, "Title 2", "bar.com", new ArrayList<>(), new ArrayList<>());

        mActivityScenario =
                ActivityScenario.launch(ChromeTabbedActivity.class).onActivity(activity -> {
                    mActivity = activity;
                    mHistoryClustersCoordinator = new HistoryClustersCoordinator(
                            mProfile, activity, mTemplateUrlService, mHistoryClustersDelegate);
                });
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
        resetStaticState();
    }

    @Test
    public void testOpenHistoryClustersUi() {
        mHistoryClustersCoordinator.openHistoryClustersUi("pandas");
        Intent intent = shadowOf(mActivity).peekNextStartedActivity();

        assertEquals(intent, mHistoryActivityIntent);
        assertTrue(intent.hasExtra(HistoryClustersConstants.EXTRA_SHOW_HISTORY_CLUSTERS));
        assertTrue(intent.hasExtra(HistoryClustersConstants.EXTRA_HISTORY_CLUSTERS_QUERY));
        assertTrue(intent.getBooleanExtra(
                HistoryClustersConstants.EXTRA_SHOW_HISTORY_CLUSTERS, false));
        assertEquals(intent.getStringExtra(HistoryClustersConstants.EXTRA_HISTORY_CLUSTERS_QUERY),
                "pandas");
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    public void testOpenHistoryClustersUiTablet() {
        mHistoryClustersCoordinator.openHistoryClustersUi("pandas");
        verify(mTab).loadUrl(argThat(
                HistoryClustersMediatorTest.hasSameUrl("chrome://history/journeys?q=pandas")));
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

    @Test
    public void testOpenInNewTabMenuItem() {
        HistoryClustersToolbar toolbar = mHistoryClustersCoordinator.getActivityContentView()
                                                 .findViewById(R.id.selectable_list)
                                                 .findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        mHistoryClustersCoordinator.getSelectionDelegateForTesting().setSelectedItems(
                new HashSet<>(Arrays.asList(mVisit1, mVisit2)));
        mHistoryClustersCoordinator.onMenuItemClick(
                toolbar.getMenu().findItem(R.id.selection_mode_open_in_new_tab));

        assertTrue(mOpenUrlIntent.hasExtra(NEW_TAB_EXTRA));
        assertTrue(mOpenUrlIntent.hasExtra(INCOGNITO_EXTRA));
        assertTrue(mOpenUrlIntent.getBooleanExtra(NEW_TAB_EXTRA, false));
        assertFalse(mOpenUrlIntent.getBooleanExtra(INCOGNITO_EXTRA, true));
    }

    @Test
    public void testOpenInNewIncognitoTabMenuItem() {
        HistoryClustersToolbar toolbar = mHistoryClustersCoordinator.getActivityContentView()
                                                 .findViewById(R.id.selectable_list)
                                                 .findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        mHistoryClustersCoordinator.getSelectionDelegateForTesting().setSelectedItems(
                new HashSet<>(Arrays.asList(mVisit1, mVisit2)));
        mHistoryClustersCoordinator.onMenuItemClick(
                toolbar.getMenu().findItem(R.id.selection_mode_open_in_incognito));

        assertTrue(mOpenUrlIntent.hasExtra(NEW_TAB_EXTRA));
        assertTrue(mOpenUrlIntent.hasExtra(INCOGNITO_EXTRA));
        assertTrue(mOpenUrlIntent.getBooleanExtra(NEW_TAB_EXTRA, false));
        assertTrue(mOpenUrlIntent.getBooleanExtra(INCOGNITO_EXTRA, false));
    }

    @Test
    public void testToggleInfoMenuItem() {
        HistoryClustersToolbar toolbar = mHistoryClustersCoordinator.getActivityContentView()
                                                 .findViewById(R.id.selectable_list)
                                                 .findViewById(R.id.action_bar);
        ShadowLooper.idleMainLooper();
        assertNotNull(toolbar);
        assertTrue(mHistoryClustersDelegate.shouldShowPrivacyDisclaimerSupplier().get());
        MenuItem menuItem = toolbar.getMenu().findItem(R.id.info_menu_id);
        assertEquals(menuItem.getTitle(), mActivity.getResources().getString(R.string.hide_info));

        mHistoryClustersCoordinator.onMenuItemClick(menuItem);
        assertFalse(mHistoryClustersDelegate.shouldShowPrivacyDisclaimerSupplier().get());

        assertEquals(menuItem.getTitle(), mActivity.getResources().getString(R.string.show_info));

        mHistoryClustersCoordinator.onMenuItemClick(menuItem);
        assertTrue(mHistoryClustersDelegate.shouldShowPrivacyDisclaimerSupplier().get());
        assertEquals(menuItem.getTitle(), mActivity.getResources().getString(R.string.hide_info));
    }

    private static void resetStaticState() {
        DisplayAndroidManager.resetInstanceForTesting();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }
}
