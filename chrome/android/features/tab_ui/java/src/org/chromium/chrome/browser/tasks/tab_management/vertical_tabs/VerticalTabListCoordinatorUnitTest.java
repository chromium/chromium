// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link VerticalTabListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY
})
public class VerticalTabListCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mSelectorObserverCaptor;

    private Activity mActivity;
    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final List<TabGroupObserver> mTabGroupObservers = new ArrayList<>();
    private VerticalTabListCoordinator mCoordinator;

    @Before
    public void setUp() {
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        ServiceStatus serviceStatus = mock(ServiceStatus.class);
        when(mCollaborationService.getServiceStatus()).thenReturn(serviceStatus);
        when(serviceStatus.isAllowedToJoin()).thenReturn(false);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mCurrentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.isTabModelRestored()).thenReturn(true);
        when(mTabModel.iterator()).thenReturn(java.util.Collections.emptyIterator());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        doAnswer(
                        invocation -> {
                            mTabGroupObservers.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mTabModel)
                .addTabGroupObserver(any(TabGroupObserver.class));
    }

    private Tab prepareMockTab(int id) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.getTitle()).thenReturn("Tab " + id);
        GURL gurl = new GURL("https://google.com");
        when(tab.getUrl()).thenReturn(gurl);
        return tab;
    }

    @Test
    @SmallTest
    public void testConstructor() {
        doNothing().when(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        assertNotNull(mCoordinator.getView());

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        assertNotNull(recyclerView);
        assertNotNull(recyclerView.getAdapter());
        assertNotNull(recyclerView.getLayoutManager());

        GridLayoutManager layoutManager = (GridLayoutManager) recyclerView.getLayoutManager();
        assertEquals(4, layoutManager.getSpanCount());

        assertNotNull(mSelectorObserverCaptor.getValue());
        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        doNothing().when(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);

        TabModelSelectorObserver observer = mSelectorObserverCaptor.getValue();
        assertNotNull(observer);

        mCoordinator.destroy();
        verify(mTabModelSelector).removeObserver(observer);
    }

    @Test
    @SmallTest
    public void testDestroy_RemovesSupplierObserver() {
        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        mCoordinator.destroy();

        TabModel newTabModel = mock(TabModel.class);
        when(newTabModel.getProfile()).thenReturn(mProfile);
        when(newTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(789);
        when(newTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));

        mCurrentTabModelSupplier.set(newTabModel);
        assertEquals(0, adapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testAdapterInterceptionAndSpanLookup() {
        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();
        assertNotNull(recycler.getLayoutManager());
        GridLayoutManager.SpanSizeLookup lookup =
                ((GridLayoutManager) recycler.getLayoutManager()).getSpanSizeLookup();

        PropertyModel reg = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        PropertyModel pin = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        PropertyModel group = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        pin.set(TabProperties.IS_PINNED, true);
        group.set(TabProperties.TAB_GROUP_HEADER_ID, new Token(1L, 2L));

        assertNotNull(adapter);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, reg));
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, pin));
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, group));

        assertEquals(UiType.TAB, adapter.getItemViewType(0));
        assertEquals(UiType.PINNED_TAB, adapter.getItemViewType(1));
        assertEquals(UiType.TAB_GROUP, adapter.getItemViewType(2));
        assertEquals(4, lookup.getSpanSize(0));
        assertEquals(1, lookup.getSpanSize(1));
        assertEquals(4, lookup.getSpanSize(2));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Expand() {
        Tab tab123 = prepareMockTab(123);
        Token tabGroupId123 = new Token(1L, 2L);
        when(tab123.getTabGroupId()).thenReturn(tabGroupId123);

        when(mTabModel.getTabById(anyInt())).thenReturn(tab123);
        when(mTabModel.getTabsInGroup(tabGroupId123)).thenReturn(List.of(tab123));
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab123));
        when(mTabModel.getGroupLastShownTabId(tabGroupId123)).thenReturn(123);
        when(mTabModel.getRelatedTabList(123)).thenReturn(List.of(tab123));
        when(mTabModel.isTabInTabGroup(tab123)).thenReturn(true);

        final boolean[] collapsedState = {true};
        doAnswer(invocation -> collapsedState[0])
                .when(mTabModel)
                .getTabGroupCollapsed(any(Token.class));
        doAnswer(
                        invocation -> {
                            collapsedState[0] = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        invocation.getArgument(0), collapsedState[0], false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(any(Token.class), anyBoolean(), anyBoolean());

        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertTrue(groupModel.get(TabProperties.IS_COLLAPSED));
        assertNull(groupModel.get(TabProperties.TAB_ACTION_BUTTON_DATA));

        mCoordinator.toggleTabGroupExpansion(123);
        assertFalse(
                "Tab group should be expanded (IS_COLLAPSED = false) after first toggle click.",
                groupModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Collapse() {
        Tab tab123 = prepareMockTab(123);
        Token tabGroupId123 = new Token(1L, 2L);
        when(tab123.getTabGroupId()).thenReturn(tabGroupId123);

        when(mTabModel.getTabById(anyInt())).thenReturn(tab123);
        when(mTabModel.getTabsInGroup(tabGroupId123)).thenReturn(List.of(tab123));
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab123));
        when(mTabModel.getGroupLastShownTabId(tabGroupId123)).thenReturn(123);
        when(mTabModel.getRelatedTabList(123)).thenReturn(List.of(tab123));
        when(mTabModel.isTabInTabGroup(tab123)).thenReturn(true);

        final boolean[] collapsedState = {false};
        doAnswer(invocation -> collapsedState[0])
                .when(mTabModel)
                .getTabGroupCollapsed(any(Token.class));
        doAnswer(
                        invocation -> {
                            collapsedState[0] = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        invocation.getArgument(0), collapsedState[0], false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(any(Token.class), anyBoolean(), anyBoolean());

        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(2, adapter.getModelList().size());
        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertFalse(groupModel.get(TabProperties.IS_COLLAPSED));

        mCoordinator.toggleTabGroupExpansion(123);
        assertTrue(
                "Tab group should be collapsed (IS_COLLAPSED = true) after toggle click from"
                        + " expanded state.",
                groupModel.get(TabProperties.IS_COLLAPSED));
        assertEquals(1, adapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_RegularTabCannotToggle() {
        Tab tab456 = prepareMockTab(456);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab456);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab456));
        when(mTabModel.isTabInTabGroup(tab456)).thenReturn(false);

        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        mCoordinator.toggleTabGroupExpansion(456);
        assertFalse(tabModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testTabSelection_SelectsTabInSelector() {
        Tab tab456 = prepareMockTab(456);
        when(mTabModelSelector.getModelForTabId(456)).thenReturn(mTabModel);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab456);
        when(mTabModel.indexOf(tab456)).thenReturn(0);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab456));
        when(mTabModel.isTabInTabGroup(tab456)).thenReturn(false);
        when(mTabModel.iterator()).thenReturn(List.of(tab456).iterator());

        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        TabActionListener clickListener = tabModel.get(TabProperties.TAB_CLICK_LISTENER);
        assertNotNull("Tab click listener should be bound to model", clickListener);
        clickListener.run(null, 456, null);

        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    @SmallTest
    public void testTabModelSwap_ResetsTabs() {
        mCoordinator = new VerticalTabListCoordinator(mActivity, mTabModelSelector, mProfile);
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        TabModel newTabModel = mock(TabModel.class);
        when(newTabModel.getProfile()).thenReturn(mProfile);
        when(newTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(789);
        when(newTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));
        when(newTabModel.iterator()).thenReturn(List.of(newTab).iterator());
        when(newTabModel.getTabById(789)).thenReturn(newTab);

        mCurrentTabModelSupplier.set(newTabModel);

        assertEquals(1, adapter.getModelList().size());
        assertEquals(789, adapter.getModelList().get(0).model.get(TabProperties.TAB_ID));
    }

    // TODO(crbug.com/518001737): Add tests for footer's new tab button
}
