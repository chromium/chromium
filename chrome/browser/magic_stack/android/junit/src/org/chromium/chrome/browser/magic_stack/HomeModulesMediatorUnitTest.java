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

import static org.chromium.chrome.browser.magic_stack.HomeModulesMediator.INVALID_FRESHNESS_SCORE;

import android.os.SystemClock;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

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
    @Mock private Runnable mOnHomeModulesChangedCallback;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private ModuleDelegateHost mModuleDelegateHost;
    @Mock private ModuleConfigChecker mModuleConfigChecker;
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
        for (int i = 0; i < MODULE_TYPES; i++) {
            mModuleTypeList[i] = i;
            mModuleProviderBuilderList[i] = Mockito.mock(ModuleProviderBuilder.class);
            doReturn(true).when(mModuleProviderBuilderList[i]).build(eq(mModuleDelegate), any());
            mListItems[i] = new ListItem(mModuleTypeList[i], Mockito.mock(PropertyModel.class));
            mModuleProviders[i] = Mockito.mock(ModuleProvider.class);
        }
        mHomeModulesConfigManager = HomeModulesConfigManager.getInstance();
        mMediator =
                new HomeModulesMediator(
                        mModel, mModuleRegistry, mModuleDelegateHost, mHomeModulesConfigManager);
        when(mModuleConfigChecker.isEligible()).thenReturn(true);
    }

    @After
    public void tearDown() {
        for (int i = 0; i < ModuleType.NUM_ENTRIES; i++) {
            mHomeModulesConfigManager.resetFreshnessCount(i);
            mHomeModulesConfigManager.resetFreshnessTimeStampForTesting(i);
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
        assertEquals(0, (int) moduleTypeToRankingIndexMap.get(2));
        assertEquals(1, (int) moduleTypeToRankingIndexMap.get(0));
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
        SimpleRecyclerViewAdapter.ListItem[] moduleFetchResultsCache =
                mMediator.getModuleFetchResultsCacheForTesting();
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
        SimpleRecyclerViewAdapter.ListItem[] moduleFetchResultsCache =
                mMediator.getModuleFetchResultsCacheForTesting();
        verify(mModel, never()).add(any());

        // Calls onModuleBuilt() to add ModuleProviders to the map.
        for (int i = 0; i < 3; i++) {
            mMediator.onModuleBuilt(i, mModuleProviders[i]);
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
            mMediator.onModuleBuilt(i, mModuleProviders[i]);
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
            mMediator.onModuleBuilt(i, mModuleProviders[i]);
        }
        Boolean[] moduleFetchResultsIndicator =
                mMediator.getModuleFetchResultsIndicatorForTesting();
        SimpleRecyclerViewAdapter.ListItem[] moduleFetchResultsCache =
                mMediator.getModuleFetchResultsCacheForTesting();
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

        Set<Integer> expectedModuleSet = Set.of(ModuleType.SINGLE_TAB);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());
    }

    @Test
    @SmallTest
    public void testGetFilteredEnabledModuleSet_AllModules() {
        HomeModulesMetricsUtils.HOME_MODULES_SHOW_ALL_MODULES.setForTesting(true);
        for (@ModuleType int i = 0; i < ModuleType.NUM_ENTRIES; i++) {
            mHomeModulesConfigManager.registerModuleEligibilityChecker(i, mModuleConfigChecker);
        }

        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        Set<Integer> expectedModuleSet =
                Set.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SINGLE_TAB,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB,
                        ModuleType.EDUCATIONAL_TIP);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());

        // Verifies that the single tab module isn't shown if it isn't the home surface even with
        // "show all modules" parameter is enabled.
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(false);
        expectedModuleSet =
                Set.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB,
                        ModuleType.EDUCATIONAL_TIP);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
    public void testGetFilteredEnabledModuleSet_CombineTabs_TabResumptionEnabled() {
        HomeModulesMetricsUtils.TAB_RESUMPTION_COMBINE_TABS.setForTesting(true);
        for (@ModuleType int i = 0; i < ModuleType.NUM_ENTRIES; i++) {
            mHomeModulesConfigManager.registerModuleEligibilityChecker(i, mModuleConfigChecker);
        }
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);

        // Verifies that the tab resumption module will be added to the list without the single tab
        // module.
        Set<Integer> expectedModuleSet =
                Set.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB,
                        ModuleType.EDUCATIONAL_TIP);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
    })
    public void testGetFilteredEnabledModuleSet_CombineTabs_TabResumptionDisabled() {
        HomeModulesMetricsUtils.TAB_RESUMPTION_COMBINE_TABS.setForTesting(true);
        mHomeModulesConfigManager.registerModuleEligibilityChecker(
                ModuleType.PRICE_CHANGE, mModuleConfigChecker);
        mHomeModulesConfigManager.registerModuleEligibilityChecker(
                ModuleType.SINGLE_TAB, mModuleConfigChecker);

        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        // Verifies that the single tab module will be added to the set if the tab resumption
        // feature flag is disabled.
        Set<Integer> expectedModuleSet = Set.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());

        when(mModuleDelegateHost.isHomeSurface()).thenReturn(false);
        // Verifies that the single tab module won't be added to the list if it isn't on home
        // surface.
        expectedModuleSet = Set.of(ModuleType.PRICE_CHANGE);
        assertEquals(expectedModuleSet, mMediator.getFilteredEnabledModuleSet());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateContextInputEnabled_Empty() {
        assertTrue(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2));

        // Verifies that createInputContext() returns an empty one with invalid score value.
        InputContext inputContext = mMediator.createInputContext();
        verifyEmptyInputContext(inputContext);
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
        actualOptions.equals(expectedOptions);
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
        actualOptions.equals(expectedOptions);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER})
    @DisableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2})
    public void testCreateContextInputEnabled_NoFreshnessScore() {
        assertFalse(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2));
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        // Verifies that createInputContext() returns an empty one with invalid score value if the
        // freshness score is invalid or not added.
        mHomeModulesConfigManager.setFreshnessCountForTesting(
                moduleType, HomeModulesConfigManager.INVALID_FRESHNESS_SCORE);
        InputContext inputContext = mMediator.createInputContext();
        verifyEmptyInputContext(inputContext);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateContextInputEnabled() {
        assertTrue(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2));
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        // Verifies that if the logged time is longer than the threshold, the freshness score is
        // invalid.
        int expectedScore = 100;
        long scoreLoggedTime =
                SystemClock.elapsedRealtime() - HomeModulesMediator.FRESHNESS_THRESHOLD_MS - 10;
        mHomeModulesConfigManager.setFreshnessScoreTimeStamp(moduleType, scoreLoggedTime);
        mHomeModulesConfigManager.setFreshnessCountForTesting(moduleType, expectedScore);
        InputContext inputContext = mMediator.createInputContext();
        verifyEmptyInputContext(inputContext);

        // Verifies that the freshness score will be used if the logging time is less than the
        // threshold.
        scoreLoggedTime = SystemClock.elapsedRealtime() - 10;
        mHomeModulesConfigManager.setFreshnessScoreTimeStamp(moduleType, scoreLoggedTime);
        mHomeModulesConfigManager.setFreshnessCountForTesting(moduleType, expectedScore);
        int[] scores = new int[] {-1, expectedScore, -1, -1};
        inputContext = mMediator.createInputContext();
        verifyInputContext(inputContext, scores);

        // Verifies that if the freshness score becomes invalid or removed, there isn't any entry
        // added to the InputContext.
        mHomeModulesConfigManager.setFreshnessCountForTesting(moduleType, INVALID_FRESHNESS_SCORE);
        inputContext = mMediator.createInputContext();
        verifyEmptyInputContext(inputContext);
    }

    // newly added:

    @Test
    @SmallTest
    public void testGetFixedModuleList() {
        Set<Integer> filteredEnabledModuleSet = new HashSet<>();
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.add(ModuleType.PRICE_CHANGE);
        filteredEnabledModuleSet.add(ModuleType.TAB_RESUMPTION);
        filteredEnabledModuleSet.add(ModuleType.SAFETY_HUB);
        filteredEnabledModuleSet.add(ModuleType.EDUCATIONAL_TIP);

        // Verifies the orders of modules match the heuristic logic when all modules are present.
        List<Integer> expectedModuleList =
                List.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SINGLE_TAB,
                        ModuleType.EDUCATIONAL_TIP,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));

        // Verifies that Price change module is always placed as the first module.
        filteredEnabledModuleSet.remove(ModuleType.SINGLE_TAB);
        expectedModuleList =
                List.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.EDUCATIONAL_TIP,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));

        // Verifies that single tab module is placed before the educational tip module and tab
        // resumption module.
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.remove(ModuleType.PRICE_CHANGE);
        expectedModuleList =
                List.of(
                        ModuleType.SINGLE_TAB,
                        ModuleType.EDUCATIONAL_TIP,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));

        // Verifies that the safety hub module becomes the first module when the first two modules
        // are disabled.
        filteredEnabledModuleSet.remove(ModuleType.SINGLE_TAB);
        expectedModuleList =
                List.of(
                        ModuleType.EDUCATIONAL_TIP,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));

        // Verifies that the educational tip module becomes the last module when the tab resumption
        // module is disabled.
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.add(ModuleType.PRICE_CHANGE);
        filteredEnabledModuleSet.remove(ModuleType.TAB_RESUMPTION);
        expectedModuleList =
                List.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SINGLE_TAB,
                        ModuleType.SAFETY_HUB,
                        ModuleType.EDUCATIONAL_TIP);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));

        // Verifies that the tab resumption module stays as the last module when the educational tip
        // module is disabled.
        filteredEnabledModuleSet.add(ModuleType.TAB_RESUMPTION);
        filteredEnabledModuleSet.remove(ModuleType.EDUCATIONAL_TIP);
        expectedModuleList =
                List.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SINGLE_TAB,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SAFETY_HUB);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));
    }

    @Test
    @SmallTest
    public void testOnGetClassificationResult_GetFixedModuleList() {
        ClassificationResult classificationResult =
                new ClassificationResult(
                        PredictionStatus.FAILED,
                        new String[] {"TabResumption", "SingleTab", "PriceChange"});
        Set<Integer> filteredEnabledModuleSet = new HashSet<>();
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.add(ModuleType.PRICE_CHANGE);
        filteredEnabledModuleSet.add(ModuleType.TAB_RESUMPTION);

        // Verifies that the fixed module list is returned when the segmentation result is invalid.
        List<Integer> expectedModuleList =
                List.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB, ModuleType.TAB_RESUMPTION);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));
        assertEquals(
                expectedModuleList,
                mMediator.onGetClassificationResult(
                        classificationResult, filteredEnabledModuleSet));

        // Verifies that the fixed module list is returned when the segmentation result is empty.
        classificationResult =
                new ClassificationResult(PredictionStatus.SUCCEEDED, new String[] {});
        expectedModuleList =
                List.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB, ModuleType.TAB_RESUMPTION);
        assertEquals(expectedModuleList, mMediator.getFixedModuleList(filteredEnabledModuleSet));
        assertEquals(
                expectedModuleList,
                mMediator.onGetClassificationResult(
                        classificationResult, filteredEnabledModuleSet));
    }

    @Test
    @SmallTest
    public void testOnGetClassificationResult_FilterEnabledModuleList() {
        ClassificationResult classificationResult =
                new ClassificationResult(
                        org.chromium.components.segmentation_platform.prediction_status
                                .PredictionStatus.SUCCEEDED,
                        new String[] {"TabResumption", "SingleTab", "PriceChange"});
        Set<Integer> filteredEnabledModuleSet = new HashSet<>();
        filteredEnabledModuleSet.add(ModuleType.SINGLE_TAB);
        filteredEnabledModuleSet.add(ModuleType.PRICE_CHANGE);
        filteredEnabledModuleSet.add(ModuleType.TAB_RESUMPTION);
        filteredEnabledModuleSet.add(ModuleType.EDUCATIONAL_TIP);

        // Verifies that result of #filterEnabledModuleList() is used if the segmentation
        // service returns a valid result.
        List<Integer> expectedModuleList =
                List.of(
                        ModuleType.EDUCATIONAL_TIP,
                        ModuleType.TAB_RESUMPTION,
                        ModuleType.SINGLE_TAB,
                        ModuleType.PRICE_CHANGE);
        assertEquals(
                expectedModuleList,
                mMediator.filterEnabledModuleList(
                        classificationResult.orderedLabels, filteredEnabledModuleSet));
        assertEquals(
                expectedModuleList,
                mMediator.onGetClassificationResult(
                        classificationResult, filteredEnabledModuleSet));

        // Verifies that the disabled module will be removed from the result of the segmentation
        // service.
        filteredEnabledModuleSet.remove(ModuleType.TAB_RESUMPTION);
        expectedModuleList =
                List.of(ModuleType.SINGLE_TAB, ModuleType.PRICE_CHANGE, ModuleType.EDUCATIONAL_TIP);
        assertEquals(
                expectedModuleList,
                mMediator.filterEnabledModuleList(
                        classificationResult.orderedLabels, filteredEnabledModuleSet));
        assertEquals(
                expectedModuleList,
                mMediator.onGetClassificationResult(
                        classificationResult, filteredEnabledModuleSet));
    }

    @Test
    @SmallTest
    public void testInput() {
        @ModuleType int moduleType = ModuleType.TAB_RESUMPTION;
        String expectedFreshnessString = "tab_resumption_freshness";
        TextUtils.equals(
                expectedFreshnessString,
                HomeModulesMetricsUtils.getFreshnessInputContextString(moduleType));
    }

    private void verifyInputContext(InputContext inputContext, int[] scores) {
        assertEquals(ModuleType.NUM_ENTRIES - 1, inputContext.getSizeForTesting());

        int j = 0;
        for (int i = 0; i < ModuleType.NUM_ENTRIES; i++) {
            if (i == ModuleType.EDUCATIONAL_TIP) {
                continue;
            }

            assertEquals(
                    scores[j],
                    inputContext.getEntryForTesting(
                                    HomeModulesMetricsUtils.getFreshnessInputContextString(i))
                            .floatValue,
                    0.01);
            j = j + 1;
        }
    }

    private void verifyEmptyInputContext(InputContext inputContext) {
        assertEquals(ModuleType.NUM_ENTRIES - 1, inputContext.getSizeForTesting());
        for (int i = 0; i < ModuleType.NUM_ENTRIES; i++) {
            if (i == ModuleType.EDUCATIONAL_TIP) {
                continue;
            }

            assertEquals(
                    INVALID_FRESHNESS_SCORE,
                    inputContext.getEntryForTesting(
                                    HomeModulesMetricsUtils.getFreshnessInputContextString(i))
                            .floatValue,
                    0.01);
        }
    }
}
