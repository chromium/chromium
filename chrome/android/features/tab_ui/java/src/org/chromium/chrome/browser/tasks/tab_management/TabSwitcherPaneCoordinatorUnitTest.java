// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab.TitleProvider;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;

/**
 * Unit tests for {@link TabSwitcherPaneCoordinator}. These are mostly for coverage and to confirm
 * nothing will crash since the bulk of the behaviors from the coordinator are either unit tested by
 * classes hosted insider the coordinator or have to be verified in an integration test.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneCoordinatorUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupModelFilter mTabModelFilter;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TitleProvider mTitleProvider;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private ScrimCoordinator mScrimCoordinator;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabSwitcherResetHandler mResetHandler;
    @Mock private Callback<Integer> mOnTabClickedCallback;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private Tracker mTracker;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<TabModelFilter> mTabModelFilterSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsVisibleSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>();

    private MockTabModel mTabModel;
    private Activity mActivity;
    private FrameLayout mRootView;
    private FrameLayout mContainerView;
    private FrameLayout mCoordinatorView;
    private TabSwitcherPaneCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);

        TrackerFactory.setTrackerForTests(mTracker);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        mTabModel = new MockTabModel(mProfile, null);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModelFilter.isTabModelRestored()).thenReturn(true);

        mProfileProviderSupplier.set(mProfileProvider);
        mTabModelFilterSupplier.set(mTabModelFilter);
        mIsVisibleSupplier.set(false);
        mIsAnimatingSupplier.set(false);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
        mRootView = new FrameLayout(activity);
        mCoordinatorView = new FrameLayout(activity);
        mContainerView = new FrameLayout(activity);
        mRootView.addView(mContainerView);
        mCoordinatorView.setId(R.id.coordinator);
        mRootView.addView(mCoordinatorView);
        activity.setContentView(mRootView);

        mCoordinator =
                new TabSwitcherPaneCoordinator(
                        activity,
                        mLifecycleDispatcher,
                        mProfileProviderSupplier,
                        mTabModelFilterSupplier,
                        () -> mTabModel,
                        mTabContentManager,
                        mTabCreatorManager,
                        mTitleProvider,
                        mBrowserControlsStateProvider,
                        mMultiWindowModeStateDispatcher,
                        mScrimCoordinator,
                        mSnackbarManager,
                        mModalDialogManager,
                        mContainerView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        TabListMode.GRID,
                        /* supportsEmptyState= */ true);

        mCoordinator.initWithNative();

        mIsVisibleSupplier.set(true);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void testShowTabListEditor() {
        ObservableSupplier<Boolean> handlesBackPressSupplier =
                mCoordinator.getHandleBackPressChangedSupplier();
        assertFalse(handlesBackPressSupplier.get());

        mCoordinator.showTabListEditor();
        assertTrue(handlesBackPressSupplier.get());

        assertNotNull(mActivity.findViewById(R.id.selectable_list));

        assertEquals(BackPressResult.SUCCESS, mCoordinator.handleBackPress());
        assertFalse(handlesBackPressSupplier.get());

        assertNull(mActivity.findViewById(R.id.selectable_list));
    }

    @Test
    @SmallTest
    public void testSetInitialScrollIndexOffset() {
        int index = 8;
        when(mTabModelFilter.index()).thenReturn(index);
        mCoordinator.setInitialScrollIndexOffset();

        assertEquals(
                index,
                mCoordinator
                        .getContainerViewModelForTesting()
                        .get(INITIAL_SCROLL_INDEX)
                        .intValue());
    }

    @Test
    @SmallTest
    public void testRequestAccessibilityFocusOnCurrentTab() {
        int index = 2;
        when(mTabModelFilter.index()).thenReturn(index);
        mCoordinator.requestAccessibilityFocusOnCurrentTab();

        assertEquals(
                index,
                mCoordinator
                        .getContainerViewModelForTesting()
                        .get(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY)
                        .intValue());
    }

    @Test
    @SmallTest
    public void testTabGridDialogVisibilitySupplier() {
        Supplier<Boolean> tabGridDialogVisibilitySupplier =
                mCoordinator.getTabGridDialogVisibilitySupplier();

        assertFalse(tabGridDialogVisibilitySupplier.get());

        DialogController controller = mCoordinator.getTabGridDialogControllerForTesting();
        MockTab tab = MockTab.createAndInitialize(/* id= */ 1, mProfile);
        tab.setIsInitialized(true);
        int index = 0;
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        when(mTabModelFilter.indexOf(tab)).thenReturn(index);
        when(mTabModelFilter.getTabAt(index)).thenReturn(tab);
        controller.resetWithListOfTabs(Collections.singletonList(tab));
        assertTrue(tabGridDialogVisibilitySupplier.get());

        controller.hideDialog(false);
        assertFalse(tabGridDialogVisibilitySupplier.get());
    }

    @Test
    @SmallTest
    public void testCustomViewManager() {
        TabSwitcherCustomViewManager.Delegate customViewManagerDelegate =
                mCoordinator.getTabSwitcherCustomViewManagerDelegate();
        assertNotNull(customViewManagerDelegate);

        FrameLayout customView = new FrameLayout(mActivity);
        customViewManagerDelegate.addCustomView(customView, null, false);
        boolean found = false;
        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            if (mContainerView.getChildAt(i) == customView) {
                found = true;
            }
        }
        assertTrue("Did not find added custom view.", found);

        customViewManagerDelegate.removeCustomView(customView);
        found = false;
        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            if (mContainerView.getChildAt(i) == customView) {
                found = true;
            }
        }
        assertFalse("Did not remove custom view", found);
    }

    @Test
    @SmallTest
    public void testShowTab() {
        int tabId = 1;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        tab.setIsInitialized(true);
        int index = 0;
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        when(mTabModelFilter.indexOf(tab)).thenReturn(index);
        when(mTabModelFilter.getTabAt(index)).thenReturn(tab);
        when(mTabModelFilter.getCount()).thenReturn(1);
        when(mTabModelFilter.getRelatedTabList(tabId)).thenReturn(Collections.singletonList(tab));

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        doCallback(2, (Callback<Bitmap> callback) -> callback.onResult(bitmap))
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(eq(tabId), any(), any(), anyBoolean(), anyBoolean());
        mCoordinator.resetWithTabList(mTabModelFilter);

        TabListRecyclerView recyclerView =
                (TabListRecyclerView) mActivity.findViewById(R.id.tab_list_recycler_view);
        // Manually size the view so that the children get added this is to work around robolectric
        // view testing limitations.
        recyclerView.measure(0, 0);
        recyclerView.layout(0, 0, 100, 1000);

        assertEquals(1, recyclerView.getAdapter().getItemCount());
        assertEquals(1, recyclerView.getChildCount());
        // This gets called three times
        // 1) Once when the fetcher is set.
        // 2) Twice due to thumbnail size changes on initial and repeat layout.
        verify(mTabContentManager, times(3))
                .getTabThumbnailWithCallback(eq(tabId), any(), any(), anyBoolean(), anyBoolean());

        TabThumbnailView thumbnailView =
                (TabThumbnailView) mActivity.findViewById(R.id.tab_thumbnail);
        assertNotNull(thumbnailView);
        assertFalse(thumbnailView.isPlaceholder());

        mIsVisibleSupplier.set(false);

        mCoordinator.softCleanup();
        assertTrue(thumbnailView.isPlaceholder());

        mCoordinator.hardCleanup();
        assertEquals(0, recyclerView.getAdapter().getItemCount());
        // Don't assert on the actual child count, robolectric isn't removing the child view for
        // some reason.
    }
}
