// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
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

import org.chromium.base.IntentUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link ChromeTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabCreatorJUnitTest {
    private static final String PRIMARY_URL = "https://primary.com";
    private static final String URL_1 = "https://url1.com";
    private static final String URL_2 = "https://url2.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Tab mParentTab;
    @Mock private Tab mFirstTab;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Captor private ArgumentCaptor<LoadUrlParams> mParamsCaptor;
    @Captor private ArgumentCaptor<Integer> mLaunchTypeCaptor;
    @Captor private ArgumentCaptor<Tab> mParentCaptor;
    @Captor private ArgumentCaptor<Integer> mPositionCaptor;

    private ChromeTabCreator mTabCreator;

    @Before
    public void setUp() {
        IntentUtils.setForceIsTrustedIntentForTesting(/* isTrusted= */ true);
        AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().clear();
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabCreator = createTabCreator(activity, /* incognito= */ false);
    }

    @After
    public void tearDown() {
        AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().clear();
    }

    @Test
    public void testCreateNewTab_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTab(
                        new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_CHROME_UI,
                        /* parent= */ null));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PRECONNECT_ON_TAB_CREATION)
    public void testCreateNewTab_ReparentingNullParams_AbortsCleanly() {
        mTabCreator.setTabModel(mock(TabModel.class));
        mTabCreator.setTabModelOrderController(mock(TabModelOrderController.class));
        int assignedTabId = 123;
        Intent intent = new Intent();
        IntentHandler.setTabId(intent, assignedTabId);

        assertNull(
                mTabCreator.createNewTab(
                        new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_REPARENTING,
                        /* parent= */ null,
                        intent));
        assertNull(
                mTabCreator.createNewTab(
                        new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_REPARENTING_BACKGROUND,
                        /* parent= */ null,
                        intent));
    }
    @Test
    public void testCreateNewTabs_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTabs(
                        new LoadUrlParams(PRIMARY_URL),
                        List.of(URL_1, URL_2),
                        TabLaunchType.FROM_CHROME_UI,
                        /* parent= */ null,
                        /* openInTabGroup= */ false,
                        /* intent= */ null));
    }

    @Test
    public void testCreateNewTabs_WithTabGroup_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTabs(
                        new LoadUrlParams(PRIMARY_URL),
                        List.of(URL_1, URL_2),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        /* parent= */ null,
                        /* openInTabGroup= */ true,
                        /* intent= */ null));
    }

    @Test
    public void testCreateNewTabs_BookmarkBarBackground() {
        when(mTabModel.indexOf(/* tab= */ null)).thenReturn(TabModel.INVALID_TAB_INDEX);
        ChromeTabCreator tabCreator = setupSpyTabCreatorForBatchTest(/* firstTabIndex= */ 2);

        Tab result =
                tabCreator.createNewTabs(
                        new LoadUrlParams(PRIMARY_URL),
                        List.of(URL_1, URL_2),
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                        /* parent= */ null,
                        /* openInTabGroup= */ true,
                        /* intent= */ null);

        assertEquals(mFirstTab, result);
        assertEquals(
                List.of(PRIMARY_URL, URL_1, URL_2),
                mParamsCaptor.getAllValues().stream().map(LoadUrlParams::getUrl).toList());
        assertEquals(
                List.of(
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND),
                mLaunchTypeCaptor.getAllValues());
        assertEquals(List.of(TabModel.INVALID_TAB_INDEX, 3, 4), mPositionCaptor.getAllValues());
        assertEquals(Arrays.asList(null, null, null), mParentCaptor.getAllValues());
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        List.of(mTab1, mTab2),
                        mFirstTab,
                        TabGroupMergeNotificationType.DONT_NOTIFY);
    }

    @Test
    public void testCreateNewTabs_ForegroundLaunch() {
        when(mTabModel.indexOf(mParentTab)).thenReturn(1);
        ChromeTabCreator tabCreator = setupSpyTabCreatorForBatchTest(/* firstTabIndex= */ 2);

        Tab result =
                tabCreator.createNewTabs(
                        new LoadUrlParams(PRIMARY_URL),
                        List.of(URL_1, URL_2),
                        TabLaunchType.FROM_CHROME_UI,
                        /* parent= */ mParentTab,
                        /* openInTabGroup= */ true,
                        /* intent= */ null);

        assertEquals(mFirstTab, result);
        assertEquals(
                List.of(PRIMARY_URL, URL_1, URL_2),
                mParamsCaptor.getAllValues().stream().map(LoadUrlParams::getUrl).toList());
        assertEquals(
                List.of(
                        TabLaunchType.FROM_CHROME_UI,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP),
                mLaunchTypeCaptor.getAllValues());
        assertEquals(List.of(2, 3, 4), mPositionCaptor.getAllValues());
        assertEquals(Arrays.asList(mParentTab, null, null), mParentCaptor.getAllValues());
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        List.of(mTab1, mTab2),
                        mFirstTab,
                        TabGroupMergeNotificationType.DONT_NOTIFY);
    }

    private ChromeTabCreator setupSpyTabCreatorForBatchTest(int firstTabIndex) {
        mTabCreator.setTabModel(mTabModel);
        when(mTabModel.indexOf(mFirstTab)).thenReturn(firstTabIndex);
        ChromeTabCreator tabCreator = spy(mTabCreator);
        doReturn(mFirstTab, mTab1, mTab2)
                .when(tabCreator)
                .createNewTab(
                        mParamsCaptor.capture(),
                        /* title= */ isNull(),
                        mLaunchTypeCaptor.capture(),
                        mParentCaptor.capture(),
                        mPositionCaptor.capture(),
                        /* intent= */ any(),
                        /* copyHistory= */ eq(false));
        return tabCreator;
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PRECONNECT_ON_TAB_CREATION)
    public void testCreateNewTab_AsyncParamsRemovedAfterAddTab_Incognito() {
        runTestAsyncParamsRemovedAfterAddTab(/* incognito= */ true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PRECONNECT_ON_TAB_CREATION)
    public void testCreateNewTab_AsyncParamsRemovedAfterAddTab_Standard() {
        runTestAsyncParamsRemovedAfterAddTab(/* incognito= */ false);
    }

    @Test
    public void testCreateFrozenTab_ReparentingDetachesBeforeAddAndRemovesParamsAfter() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        ChromeTabCreator tabCreator = createTabCreator(activity, /* incognito= */ false);
        TabModel tabModel = mock(TabModel.class);
        TabModelOrderController orderController = mock(TabModelOrderController.class);
        tabCreator.setTabModel(tabModel);
        tabCreator.setTabModelOrderController(orderController);

        int tabId = 456;
        Tab mockTab = mock(Tab.class);
        when(mockTab.getId()).thenReturn(tabId);
        when(mockTab.isIncognito()).thenReturn(false);
        when(mockTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mockTab.getUserDataHost()).thenReturn(new UserDataHost());

        boolean[] finalizeRan = new boolean[1];
        TabReparentingParams params =
                new TabReparentingParams(mockTab, () -> finalizeRan[0] = true);
        AsyncTabParamsManagerSingleton.getInstance().add(tabId, params);

        doAnswer(
                        invocation -> {
                            assertTrue(
                                    "Finalize callback must detach from old model before addTab",
                                    finalizeRan[0]);
                            assertTrue(
                                    "AsyncTabParams must remain in manager during addTab",
                                    AsyncTabParamsManagerSingleton.getInstance()
                                            .hasParamsForTabId(tabId));
                            return null;
                        })
                .when(tabModel)
                .addTab(any(Tab.class), anyInt(), anyInt(), anyInt());

        tabCreator.createFrozenTab(/* state= */ null, tabId, /* index= */ 0);

        assertFalse(
                "AsyncTabParams must be removed after createFrozenTab",
                AsyncTabParamsManagerSingleton.getInstance().hasParamsForTabId(tabId));
        verify(tabModel).addTab(eq(mockTab), eq(0), eq(TabLaunchType.FROM_RESTORE), anyInt());
    }

    @Test
    public void testRedirectTabCreator_ReparentingDestroysAsyncParams() {
        MultiInstanceOrchestrator orchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(orchestrator);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        RedirectTabCreator redirectTabCreator =
                new RedirectTabCreator(
                        activity,
                        mock(WindowAndroid.class),
                        () -> mock(TabDelegateFactory.class),
                        new OneshotSupplierImpl<>(),
                        /* incognito= */ true,
                        AsyncTabParamsManagerSingleton.getInstance(),
                        SupplierUtils.ofNull(),
                        SupplierUtils.ofNull());
        redirectTabCreator.setTabModel(mock(TabModel.class));

        int tabId = 321;
        Tab mockTab = mock(Tab.class);
        TabReparentingParams params = new TabReparentingParams(mockTab, null);
        AsyncTabParamsManagerSingleton.getInstance().add(tabId, params);

        Intent intent = new Intent();
        IntentHandler.setTabId(intent, tabId);

        redirectTabCreator.createNewTab(
                new LoadUrlParams("about:blank"),
                TabLaunchType.FROM_REPARENTING,
                /* parent= */ null,
                intent);

        assertFalse(
                "AsyncTabParams must be removed after RedirectTabCreator.createNewTab",
                AsyncTabParamsManagerSingleton.getInstance().hasParamsForTabId(tabId));
        verify(mockTab).destroy();
    }

    private ChromeTabCreator createTabCreator(Activity activity, boolean incognito) {
        Profile profile = mock(Profile.class);
        when(profile.isOffTheRecord()).thenReturn(incognito);

        ProfileProvider profileProvider = mock(ProfileProvider.class);
        when(profileProvider.getOriginalProfile()).thenReturn(profile);
        when(profileProvider.getOrCreateOffTheRecordProfile()).thenReturn(profile);
        when(profileProvider.getOffTheRecordProfile(anyBoolean())).thenReturn(profile);

        OneshotSupplierImpl<ProfileProvider> profileProviderSupplier = new OneshotSupplierImpl<>();
        profileProviderSupplier.set(profileProvider);

        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getOcclusionSupplier()).thenReturn(ObservableSuppliers.alwaysFalse());
        when(windowAndroid.getContext()).thenReturn(new WeakReference<Context>(activity));

        TabDelegateFactory tabDelegateFactory = mock(TabDelegateFactory.class);

        return new ChromeTabCreator(
                activity,
                windowAndroid,
                () -> tabDelegateFactory,
                profileProviderSupplier,
                incognito,
                AsyncTabParamsManagerSingleton.getInstance(),
                SupplierUtils.ofNull(),
                SupplierUtils.ofNull());
    }

    private void runTestAsyncParamsRemovedAfterAddTab(boolean incognito) {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        ChromeTabCreator tabCreator = createTabCreator(activity, incognito);
        TabModel tabModel = mock(TabModel.class);
        TabModelOrderController orderController = mock(TabModelOrderController.class);
        tabCreator.setTabModel(tabModel);
        tabCreator.setTabModelOrderController(orderController);

        int tabId = 789;
        Tab mockTab = mock(Tab.class);
        when(mockTab.getId()).thenReturn(tabId);
        when(mockTab.isIncognito()).thenReturn(incognito);
        when(mockTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mockTab.getUserDataHost()).thenReturn(new UserDataHost());

        TabReparentingParams params = new TabReparentingParams(mockTab, null);
        AsyncTabParamsManagerSingleton.getInstance().add(tabId, params);

        Intent intent = new Intent();
        IntentHandler.setTabId(intent, tabId);

        doAnswer(
                        invocation -> {
                            assertTrue(
                                    "AsyncTabParams must remain in manager during addTab",
                                    AsyncTabParamsManagerSingleton.getInstance()
                                            .hasParamsForTabId(tabId));
                            return null;
                        })
                .when(tabModel)
                .addTab(any(Tab.class), anyInt(), anyInt(), anyInt());

        tabCreator.createNewTab(
                new LoadUrlParams("about:blank"),
                TabLaunchType.FROM_REPARENTING,
                /* parent= */ null,
                intent);

        assertFalse(
                "AsyncTabParams must be removed after createNewTab",
                AsyncTabParamsManagerSingleton.getInstance().hasParamsForTabId(tabId));
        verify(tabModel)
                .addTab(eq(mockTab), anyInt(), eq(TabLaunchType.FROM_REPARENTING), anyInt());
    }
}
