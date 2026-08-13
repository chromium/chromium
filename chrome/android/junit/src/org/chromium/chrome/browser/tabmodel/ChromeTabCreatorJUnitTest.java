// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link ChromeTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabCreatorJUnitTest {
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
                        new LoadUrlParams("https://primary.com"),
                        List.of("https://url1.com", "https://url2.com"),
                        TabLaunchType.FROM_CHROME_UI,
                        null,
                        false,
                        null));
    }

    @Test
    public void testCreateNewTabs_WithTabGroup_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTabs(
                        new LoadUrlParams("https://primary.com"),
                        List.of("https://url1.com", "https://url2.com"),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        null,
                        true,
                        null));
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
