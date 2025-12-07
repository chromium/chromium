// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION;

import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelper;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelperJni;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link HomeModulesMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int MODULE_TYPES = 3;
    @Mock private Profile mProfile;
    @Mock private Runnable mOnHomeModulesChangedCallback;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private ModuleDelegateHost mModuleDelegateHost;
    @Mock private ModuleConfigChecker mModuleConfigChecker;
    @Mock private HomeModulesRankingHelper.Natives mHomeModulesRankingHelperJniMock;
    @Spy private ModelList mModel;

    private int[] mModuleTypeList;
    private ListItem[] mListItems;
    private ModuleProviderBuilder[] mModuleProviderBuilderList;
    private ModuleProvider[] mModuleProviders;
    private HomeModulesConfigManager mHomeModulesConfigManager;
    private HomeModulesMediator mMediator;

    @Before
    public void setUp() {
        mModel = Mockito.spy(new ModelList());
        mModuleTypeList = new int[MODULE_TYPES];
        mListItems = new ListItem[MODULE_TYPES];
        mModuleProviderBuilderList = new ModuleProviderBuilder[MODULE_TYPES];
        mModuleProviders = new ModuleProvider[MODULE_TYPES];
        ObservableSupplierImpl<Profile> profileSupplier = new ObservableSupplierImpl<>();
        profileSupplier.set(mProfile);

        registerModule(0, ModuleType.SINGLE_TAB);
        registerModule(1, ModuleType.PRICE_CHANGE);
        registerModule(2, ModuleType.SAFETY_HUB);

        FeatureOverrides.newBuilder().disable(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR).apply();
        mHomeModulesConfigManager = HomeModulesConfigManager.getInstance();
        assertEquals(0, mHomeModulesConfigManager.getEnabledModuleSet().size());
        mMediator =
                new HomeModulesMediator(
                        profileSupplier,
                        mModel,
                        mModuleRegistry,
                        mModuleDelegateHost,
                        mHomeModulesConfigManager);
        when(mModuleConfigChecker.isEligible()).thenReturn(true);
        HomeModulesRankingHelperJni.setInstanceForTesting(mHomeModulesRankingHelperJniMock);
    }

    @After
    public void tearDown() {
        for (int i = 0; i < ModuleType.NUM_ENTRIES; i++) {
            HomeModulesUtils.resetFreshnessCountAsFresh(i);
            HomeModulesUtils.resetFreshnessTimeStampForTesting(i);
        }
        mHomeModulesConfigManager.cleanupForTesting();
    }

    @Test
    @SmallTest
    public void testShowModule_CacheRanking() {
        List<Integer> moduleList = List.of(mModuleTypeList[2], mModuleTypeList[0]);
        mMediator.cacheRanking(moduleList);
        Map<Integer, Integer> moduleTypeToRankingIndexMap =
                mMediator.getModuleTypeToRankingIndexMapForTesting();
        assertEquals(2, moduleTypeToRankingIndexMap.size());
        assertEquals(0, (int) moduleTypeToRankingIndexMap.get(mModuleTypeList[2]));
        assertEquals(1, (int) moduleTypeToRankingIndexMap.get(mModuleTypeList[0]));
    }

    @Test
    @SmallTest
    public void testShowModule_BuildWithoutRegisteredModules() {
        List<Integer> moduleList = List.of(mModuleTypeList[2], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 2; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(false);
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        // Verifies that don't wait for module loading if there isn't any modules can be built.
        assertFalse(mMediator.getIsFetchingModulesForTesting());
    }

    @Test
    @SmallTest
    public void testShowModule_BuildWithUnRegisteredModule() {
        List<Integer> moduleList = List.of(mModuleTypeList[2], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 2; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        // The magic stack is waiting for modules to be load.
        assertTrue(mMediator.getIsFetchingModulesForTesting());
        Boolean[] moduleFetchResultsIndicator =
                mMediator.getModuleFetchResultsIndicatorForTesting();

        // Verifies that false is set immediately for a module which isn't registered. "0" is the
        // ranking of mModuleTypeList[2] which isn't registered in ModuleRegistry.
        assertFalse(moduleFetchResultsIndicator[0]);
        // Verifies that the waiting response index increases.
        assertEquals(1, mMediator.getModuleResultsWaitingIndexForTesting());
        assertNull(moduleFetchResultsIndicator[1]);
    }

    @Test
    @SmallTest
    public void testShowModule_WaitHighestRankingModule() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[0], mModuleTypeList[1]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < MODULE_TYPES; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        Boolean[] moduleFetchResultsIndicator =
                mMediator.getModuleFetchResultsIndicatorForTesting();
        ListItem[] moduleFetchResultsCache = mMediator.getModuleFetchResultsCacheForTesting();
        verify(mModel, never()).add(any());

        // Verifies that the response of a low ranking module is cached.
        PropertyModel propertyModel1 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[1], propertyModel1);
        assertTrue(moduleFetchResultsIndicator[2]);
        assertEquals(propertyModel1, moduleFetchResultsCache[2].model);
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mModel, never()).add(any());

        // Verifies that the response of a low ranking module is cached.
        PropertyModel propertyModel0 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], propertyModel0);
        assertTrue(moduleFetchResultsIndicator[1]);
        assertEquals(propertyModel0, moduleFetchResultsCache[1].model);
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mModel, never()).add(any());
        verify(mOnHomeModulesChangedCallback, never()).run();

        // Verifies that cached results will be added to the magic stack once the response of the
        // highest ranking modules arrive.
        PropertyModel propertyModel2 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[2], propertyModel2);
        verify(mModel, times(3)).add(any());
        assertEquals(3, mMediator.getModuleResultsWaitingIndexForTesting());
    }

    @Test
    @SmallTest
    public void testShowModule_HighestRankingModuleDoesNotHaveData() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[0], mModuleTypeList[1]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        Boolean[] moduleFetchResultsIndicator =
                mMediator.getModuleFetchResultsIndicatorForTesting();
        ListItem[] moduleFetchResultsCache = mMediator.getModuleFetchResultsCacheForTesting();
        verify(mModel, never()).add(any());

        // Calls onModuleBuilt() to add ModuleProviders to the map.
        for (int i = 0; i < 3; i++) {
            mMediator.onModuleBuilt(mModuleTypeList[i], mModuleProviders[i]);
        }

        // The response of the second highest ranking module arrives first.
        PropertyModel propertyModel0 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], propertyModel0);
        assertTrue(moduleFetchResultsIndicator[1]);
        assertEquals(propertyModel0, moduleFetchResultsCache[1].model);
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mModel, never()).add(any());
        verify(mOnHomeModulesChangedCallback, never()).run();

        mMediator.addToRecyclerViewOrCache(mModuleTypeList[2], null);
        // Verifies that the RecyclerView becomes visible as soon as no-data response of the
        // highest ranking modules arrive.
        verify(mModel, times(1)).add(any());
        verify(mOnHomeModulesChangedCallback).run();
        verify(mModuleProviders[2]).hideModule();
        assertEquals(2, mMediator.getModuleResultsWaitingIndexForTesting());

        // Verifies that the callback to notify a module being added is called again.
        PropertyModel propertyModel1 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[1], propertyModel1);
        verify(mModel, times(2)).add(any());
        assertEquals(3, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mOnHomeModulesChangedCallback, times(2)).run();
    }

    @Test
    @SmallTest
    public void testUpdateModules() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[0], mModuleTypeList[1], mModuleTypeList[2]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }

        // Calls buildModulesAndShow() to show the magic stack.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        verify(mModuleRegistry).build(eq(mModuleTypeList[0]), eq(mModuleDelegate), any());
        verify(mModuleRegistry).build(eq(mModuleTypeList[1]), eq(mModuleDelegate), any());
        verify(mModuleRegistry).build(eq(mModuleTypeList[2]), eq(mModuleDelegate), any());

        // Calls onModuleBuilt() to add ModuleProviders to the map.
        for (int i = 0; i < 3; i++) {
            mMediator.onModuleBuilt(mModuleTypeList[i], mModuleProviders[i]);
        }
        // Adds modules to the recyclerview and show.
        PropertyModel propertyModel0 = Mockito.mock(PropertyModel.class);
        PropertyModel propertyModel2 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], propertyModel0);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[1], null);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[2], propertyModel2);
        verify(mOnHomeModulesChangedCallback, times(2)).run();

        // Calls buildModulesAndShow() again when the magic stack is still visible.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);

        // Verifies that magic stack asks each modules being shown to update their data.
        verify(mModuleProviders[0]).updateModule();
        verify(mModuleProviders[1], never()).updateModule();
        verify(mModuleProviders[2]).updateModule();

        // Verifies that all of the modules aren't built again.
        verify(mModuleRegistry).build(eq(mModuleTypeList[0]), eq(mModuleDelegate), any());
        verify(mModuleRegistry).build(eq(mModuleTypeList[1]), eq(mModuleDelegate), any());
        verify(mModuleRegistry).build(eq(mModuleTypeList[2]), eq(mModuleDelegate), any());
    }

    @Test
    @SmallTest
    public void testHide() {
        // Adds 3 modules' data to the magic stack's RecyclerView.
        List<Integer> moduleList =
                List.of(mModuleTypeList[0], mModuleTypeList[1], mModuleTypeList[2]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);

        ModuleProvider[] moduleProviders = new ModuleProvider[MODULE_TYPES];
        for (int i = 0; i < MODULE_TYPES; i++) {
            moduleProviders[i] = Mockito.mock(ModuleProvider.class);
            // Modules are built successfully.
            mMediator.onModuleBuilt(mModuleTypeList[i], moduleProviders[i]);
        }
        for (int i = 0; i < MODULE_TYPES; i++) {
            mMediator.addToRecyclerViewOrCache(
                    mModuleTypeList[i], Mockito.mock(PropertyModel.class));
        }
        verify(mOnHomeModulesChangedCallback, times(3)).run();

        mMediator.hide();

        // Verifies that all modules are hiding.
        for (int i = 0; i < MODULE_TYPES; i++) {
            verify(moduleProviders[i]).hideModule();
        }

        // Verifies that all of the cached values are cleaned up.
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        assertNull(mMediator.getModuleFetchResultsIndicatorForTesting());
        assertNull(mMediator.getModuleFetchResultsCacheForTesting());

        assertEquals(0, mMediator.getModuleTypeToModuleProviderMapForTesting().size());
        assertEquals(0, mMediator.getModuleTypeToRankingIndexMapForTesting().size());

        assertEquals(0, mModel.size());
        verify(mOnHomeModulesChangedCallback, times(4)).run();
    }

    @Test
    @SmallTest
    public void testHide_NoModuleRepliedBeforeTimeOut() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[0], mModuleTypeList[1], mModuleTypeList[2]);
        int size = moduleList.size();
        // Registers modules to the ModuleRegistry.
        for (int i = 0; i < size; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }

        ModuleProvider[] moduleProviders = new ModuleProvider[size];
        for (int i = 0; i < size; i++) {
            moduleProviders[i] = Mockito.mock(ModuleProvider.class);
            // Modules are built successfully.
            mMediator.onModuleBuilt(mModuleTypeList[i], moduleProviders[i]);
        }

        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        assertTrue(mMediator.getIsFetchingModulesForTesting());
        assertTrue(mMediator.getIsShownForTesting());

        mMediator.onModuleFetchTimeOut();
        assertFalse(mMediator.getIsFetchingModulesForTesting());
        assertFalse(mMediator.getIsShownForTesting());
        verify(mModel).clear();
    }

    @Test
    @SmallTest
    public void testHide_NoModuleCanBeBuilt() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[0], mModuleTypeList[1], mModuleTypeList[2]);

        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        assertFalse(mMediator.getIsFetchingModulesForTesting());
        assertFalse(mMediator.getIsShownForTesting());
        verify(mModel).clear();
    }

    @Test
    @SmallTest
    public void testFindModuleIndexInRecyclerView() {
        // The ranking of the modules are 0, 1, 2, but type 1 doesn't have data and thus isn't
        // added.
        doReturn(2).when(mModel).size();
        doReturn(mListItems[0]).when(mModel).get(0);
        doReturn(mListItems[2]).when(mModel).get(1);

        // Verifies that the real position of a module on the RecyclerView are returned.
        assertEquals(0, mMediator.findModuleIndexInRecyclerView(mModuleTypeList[0], 0));
        assertEquals(1, mMediator.findModuleIndexInRecyclerView(mModuleTypeList[2], 2));
        assertEquals(-1, mMediator.findModuleIndexInRecyclerView(mModuleTypeList[1], 1));
    }

    @Test
    @SmallTest
    public void testAppend() {
        List<Integer> moduleList = List.of(mModuleTypeList[0], mModuleTypeList[1]);
        int size = moduleList.size();
        // Registers modules to the ModuleRegistry.
        for (int i = 0; i < size; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        ModuleProvider[] moduleProviders = new ModuleProvider[2];
        for (int i = 0; i < size; i++) {
            moduleProviders[i] = Mockito.mock(ModuleProvider.class);
            // Modules are built successfully.
            mMediator.onModuleBuilt(mModuleTypeList[i], moduleProviders[i]);
        }

        // Verifies that the RecyclerView is changed to be visible when the first item is added.
        mMediator.append(mListItems[0]);
        verify(mModel, times(1)).add(eq(mListItems[0]));
        verify(mOnHomeModulesChangedCallback).run();

        // Verifies that the callback to notify a module being added is called again.
        mMediator.append(mListItems[1]);
        verify(mModel, times(1)).add(eq(mListItems[1]));
        verify(mOnHomeModulesChangedCallback, times(2)).run();
    }

    @Test
    @SmallTest
    public void testRemove() {
        List<Integer> moduleList = List.of(mModuleTypeList[0]);
        when(mModuleRegistry.build(eq(mModuleTypeList[0]), eq(mModuleDelegate), any()))
                .thenReturn(true);
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);

        ModuleProvider moduleProvider = Mockito.mock(ModuleProvider.class);
        mMediator.onModuleBuilt(mModuleTypeList[0], moduleProvider);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], Mockito.mock(PropertyModel.class));

        // Case for removing a module which isn't added to the RecyclerView.
        mMediator.remove(mModuleTypeList[1]);
        verify(mModel, never()).removeAt(anyInt());

        mMediator.remove(mModuleTypeList[0]);
        // Verifies that the existing module is removed from the RecyclerView.
        verify(mModel, times(1)).removeAt(eq(0));
        verify(moduleProvider).hideModule();
    }

    @Test
    @SmallTest
    public void testTimeOut() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[1], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        // Calls onModuleBuilt() to add ModuleProviders to the map.
        for (int i = 0; i < 3; i++) {
            mMediator.onModuleBuilt(mModuleTypeList[i], mModuleProviders[i]);
        }
        Boolean[] moduleFetchResultsIndicator =
                mMediator.getModuleFetchResultsIndicatorForTesting();
        ListItem[] moduleFetchResultsCache = mMediator.getModuleFetchResultsCacheForTesting();
        verify(mModel, never()).add(any());
        // The magic stack is waiting for modules to be load.
        assertTrue(mMediator.getIsFetchingModulesForTesting());

        // The second highest ranking module returns a failed result.
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[1], null);
        assertFalse(moduleFetchResultsIndicator[1]);
        verify(mModel, never()).add(any());
        verify(mOnHomeModulesChangedCallback, never()).run();
        verify(mModuleProviders[1]).hideModule();

        // The third ranking module returns a successful result.
        PropertyModel propertyModel0 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], propertyModel0);
        assertTrue(moduleFetchResultsIndicator[2]);
        assertEquals(propertyModel0, moduleFetchResultsCache[2].model);
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mModel, never()).add(any());
        verify(mOnHomeModulesChangedCallback, never()).run();
        verify(mModuleProviders[0], never()).hideModule();

        mMediator.onModuleFetchTimeOut();
        verify(mModel, times(1)).add(any());
        assertEquals(3, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mOnHomeModulesChangedCallback, times(1)).run();
        // The magic stack is no longer waiting for modules to be load.
        assertFalse(mMediator.getIsFetchingModulesForTesting());
        verify(mModuleProviders[0], never()).hideModule();
        // Verifies that #hideModule() is called for the module which doesn't respond.
        verify(mModuleProviders[2]).hideModule();
        // Verifies that #hideModule() won't be called again for the module which has responded
        // without any data to show.
        verify(mModuleProviders[1]).hideModule();

        PropertyModel propertyModel2 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[2], propertyModel2);
        // Verifies that there isn't any new module added to the recyclerview.
        verify(mModel, times(1)).add(any());
        // Verifies that #hideModule() won't be called again for the module which responds after
        // the timeout. This is because #hideModule() has been called in onModuleFetchTimeOut().
        verify(mModuleProviders[2]).hideModule();
        verify(mModuleProviders[0], never()).hideModule();
        verify(mModuleProviders[1]).hideModule();
    }

    @Test
    @SmallTest
    public void testTimeOutCalledAfterHide() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[1], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(true);
        }
        ModuleProvider[] moduleProviders = new ModuleProvider[MODULE_TYPES];
        for (int i = 0; i < MODULE_TYPES; i++) {
            moduleProviders[i] = Mockito.mock(ModuleProvider.class);
            // Modules are built successfully.
            mMediator.onModuleBuilt(mModuleTypeList[i], moduleProviders[i]);
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mOnHomeModulesChangedCallback);
        assertNotNull(mMediator.getModuleFetchResultsIndicatorForTesting());

        mMediator.hide();
        assertNull(mMediator.getModuleFetchResultsIndicatorForTesting());

        // After calling onModuleFetchTimeOut(), the mediator shouldn't throw any exception.
        mMediator.onModuleFetchTimeOut();
    }

    @Test
    @SmallTest
    public void testGetFilteredEnabledModuleSet() {
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mHomeModulesConfigManager.registerModuleEligibilityChecker(
                ModuleType.SINGLE_TAB, mModuleConfigChecker);

        assertTrue(mMediator.getFilteredEnabledModuleSet().contains(ModuleType.SINGLE_TAB));
    }

    @Test
    @SmallTest
    public void testGetFilteredEnabledModuleSet_AllModules() {
        ChromeFeatureList.sMagicStackAndroidShowAllModules.setForTesting(true);
        Set<Integer> activeModules = HomeModulesMetricsUtils.getAllActiveModulesForTesting();
        for (@ModuleType int moduleType : activeModules) {
            mHomeModulesConfigManager.registerModuleEligibilityChecker(
                    moduleType, mModuleConfigChecker);
        }

        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        Set<Integer> expectedModuleSet =
                Set.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SINGLE_TAB,
                        ModuleType.SAFETY_HUB,
                        ModuleType.AUXILIARY_SEARCH,
                        ModuleType.DEFAULT_BROWSER_PROMO,
                        ModuleType.TAB_GROUP_PROMO,
                        ModuleType.TAB_GROUP_SYNC_PROMO,
                        ModuleType.QUICK_DELETE_PROMO,
                        ModuleType.HISTORY_SYNC_PROMO,
                        ModuleType.TIPS_NOTIFICATIONS_PROMO);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());

        // Verifies that the single tab module isn't shown if it isn't the home surface even with
        // "show all modules" parameter is enabled.
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(false);
        expectedModuleSet =
                Set.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SAFETY_HUB,
                        ModuleType.AUXILIARY_SEARCH,
                        ModuleType.DEFAULT_BROWSER_PROMO,
                        ModuleType.TAB_GROUP_PROMO,
                        ModuleType.TAB_GROUP_SYNC_PROMO,
                        ModuleType.QUICK_DELETE_PROMO,
                        ModuleType.HISTORY_SYNC_PROMO,
                        ModuleType.TIPS_NOTIFICATIONS_PROMO);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateOptions_FlagEnabled() {
        assertTrue(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2));

        // Verifies that createPredictionOptions() returns ondemand prediction options.
        PredictionOptions actualOptions = mMediator.createPredictionOptions();
        PredictionOptions expectedOptions =
                new PredictionOptions(
                        /* onDemandExecution= */ true,
                        /* canUpdateCacheForFutureRequests= */ true,
                        /* fallbackAllowed= */ true);
        assertEquals(expectedOptions, actualOptions);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER})
    @DisableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2})
    public void testCreateOptions_FlagDisabled() {
        assertFalse(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2));

        // Verifies that createPredictionOptions() returns cache prediction options.
        PredictionOptions actualOptions = mMediator.createPredictionOptions();
        PredictionOptions expectedOptions = new PredictionOptions(false);
        assertEquals(expectedOptions, actualOptions);
    }

    @Test
    @SmallTest
    public void testFilterEnabledModuleList() {
        ClassificationResult classificationResult =
                new ClassificationResult(
                        org.chromium.components.segmentation_platform.prediction_status
                                .PredictionStatus.SUCCEEDED,
                        new String[] {"SafetyHub", "SingleTab", "PriceChange"},
                        /* requestId= */ 0);
        Set<Integer> filteredEnabledModuleSet = new HashSet<>();
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.add(ModuleType.PRICE_CHANGE);
        filteredEnabledModuleSet.add(ModuleType.SAFETY_HUB);

        // Verifies that result of #filterEnabledModuleList() is used if the segmentation
        // service returns a valid result.
        List<Integer> expectedModuleList =
                List.of(ModuleType.SAFETY_HUB, ModuleType.SINGLE_TAB, ModuleType.PRICE_CHANGE);
        assertEquals(
                expectedModuleList,
                mMediator.filterEnabledModuleList(
                        classificationResult.orderedLabels, filteredEnabledModuleSet));

        // Verifies that the disabled module will be removed from the result of the segmentation
        // service.
        filteredEnabledModuleSet.remove(ModuleType.SAFETY_HUB);
        expectedModuleList = List.of(ModuleType.SINGLE_TAB, ModuleType.PRICE_CHANGE);
        assertEquals(
                expectedModuleList,
                mMediator.filterEnabledModuleList(
                        classificationResult.orderedLabels, filteredEnabledModuleSet));
    }

    @Test
    @SmallTest
    public void testFilterEnabledModuleList_withInvalidType() {
        ClassificationResult classificationResult =
                new ClassificationResult(
                        org.chromium.components.segmentation_platform.prediction_status
                                .PredictionStatus.SUCCEEDED,
                        new String[] {"TabResumption"},
                        /* requestId= */ 0);
        Set<Integer> filteredEnabledModuleSet = new HashSet<>();
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.add(ModuleType.PRICE_CHANGE);
        filteredEnabledModuleSet.add(ModuleType.SAFETY_HUB);

        assertTrue(
                mMediator
                        .filterEnabledModuleList(
                                classificationResult.orderedLabels, filteredEnabledModuleSet)
                        .isEmpty());
    }

    @Test
    @SmallTest
    public void testInput() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String expectedFreshnessString = "single_tab_freshness";
        TextUtils.equals(
                expectedFreshnessString,
                HomeModulesUtils.getFreshnessInputContextString(moduleType));
    }

    @Test
    @SmallTest
    public void testOnModuleViewCreated() {
        @ModuleType int moduleType1 = ModuleType.TAB_GROUP_PROMO;
        @ModuleType int moduleType2 = ModuleType.TAB_GROUP_SYNC_PROMO;

        mMediator.onModuleViewCreated(moduleType1);
        assertEquals(1, HomeModulesUtils.getImpressionCountBeforeInteraction(moduleType1));
        assertEquals(
                INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION,
                HomeModulesUtils.getImpressionCountBeforeInteraction(moduleType2));

        mMediator.onModuleViewCreated(moduleType1);
        assertEquals(2, HomeModulesUtils.getImpressionCountBeforeInteraction(moduleType1));
        assertEquals(
                INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION,
                HomeModulesUtils.getImpressionCountBeforeInteraction(moduleType2));
    }

    @Test
    @SmallTest
    public void testGetFilteredEnabledModuleSet_withRefactorEnabled() {
        FeatureOverrides.newBuilder().enable(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR).apply();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, false);

        List<Integer> moduleList = List.of(mModuleTypeList[2], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 2; i++) {
            when(mModuleRegistry.build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any()))
                    .thenReturn(false);
        }
        assertEquals(Set.of(), mMediator.getFilteredEnabledModuleSet());
    }

    /**
     * Helps to register a module.
     *
     * @param index The index of the module on the list.
     * @param type The module type.
     */
    private void registerModule(int index, @ModuleType int type) {
        mModuleTypeList[index] = type;
        mModuleProviderBuilderList[index] = Mockito.mock(ModuleProviderBuilder.class);
        doReturn(true).when(mModuleProviderBuilderList[index]).build(eq(mModuleDelegate), any());
        mListItems[index] = new ListItem(mModuleTypeList[index], Mockito.mock(PropertyModel.class));
        mModuleProviders[index] = Mockito.mock(ModuleProvider.class);
    }
}
