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

import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.NUM_ENTRIES;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;

import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesMediatorUnitTest.ShadowHomeModulesMetricsUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;
import java.util.Map;

/** Unit tests for {@link HomeModulesMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowHomeModulesMetricsUtils.class})
public class HomeModulesMediatorUnitTest {
    @Implements(HomeModulesMetricsUtils.class)
    static class ShadowHomeModulesMetricsUtils {
        @Implementation
        public static String getModuleName(@ModuleType int moduleType) {
            switch (moduleType) {
                case SINGLE_TAB:
                    return "SingleTab";
                case (PRICE_CHANGE):
                    return "PriceChange";
                case (NUM_ENTRIES):
                    return "ForTesting";
                default:
                    assert false : "Module type not supported!";
                    return null;
            }
        }
    }

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int MODULE_TYPES = 3;
    @Mock private Callback<Boolean> mSetVisibilityCallback;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private ModelList mModel;

    private int[] mModuleTypeList;
    private ListItem[] mListItems;
    private ModuleProviderBuilder[] mModuleProviderBuilderList;

    private @HostSurface int mHostSurface = HostSurface.START_SURFACE;
    private HomeModulesMediator mMediator;

    @Before
    public void setUp() {
        mModuleTypeList = new int[MODULE_TYPES];
        mListItems = new ListItem[MODULE_TYPES];
        mModuleProviderBuilderList = new ModuleProviderBuilder[MODULE_TYPES];
        for (int i = 0; i < MODULE_TYPES; i++) {
            mModuleTypeList[i] = i;
            mModuleProviderBuilderList[i] = Mockito.mock(ModuleProviderBuilder.class);
            doReturn(true).when(mModuleProviderBuilderList[i]).build(eq(mModuleDelegate), any());
            mListItems[i] = new ListItem(mModuleTypeList[i], Mockito.mock(PropertyModel.class));
        }
        when(mModuleDelegate.getHostSurfaceType()).thenReturn(mHostSurface);
        mMediator = new HomeModulesMediator(mModel, mModuleRegistry);
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
            doReturn(false)
                    .when(mModuleRegistry)
                    .build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any());
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);
        // Verifies that don't wait for module loading if there isn't any modules can be built.
        assertFalse(mMediator.getIsFetchingModulesForTesting());
    }

    @Test
    @SmallTest
    public void testShowModule_BuildWithUnRegisteredModule() {
        List<Integer> moduleList = List.of(mModuleTypeList[2], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 2; i++) {
            doReturn(true)
                    .when(mModuleRegistry)
                    .build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any());
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);
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
            doReturn(true)
                    .when(mModuleRegistry)
                    .build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any());
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);
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
        verify(mSetVisibilityCallback, never()).onResult(true);

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
        int duration = 100;
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[0], mModuleTypeList[1]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            doReturn(true)
                    .when(mModuleRegistry)
                    .build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any());
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        SystemClock.setCurrentTimeMillis(0);
        // Calls buildModulesAndShow() to initialize ranking index map.
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);
        Boolean[] moduleFetchResultsIndicator =
                mMediator.getModuleFetchResultsIndicatorForTesting();
        SimpleRecyclerViewAdapter.ListItem[] moduleFetchResultsCache =
                mMediator.getModuleFetchResultsCacheForTesting();
        verify(mModel, never()).add(any());

        // The response of the second highest ranking module arrives first.
        PropertyModel propertyModel0 = Mockito.mock(PropertyModel.class);
        SystemClock.setCurrentTimeMillis(duration);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], propertyModel0);
        assertTrue(moduleFetchResultsIndicator[1]);
        assertEquals(propertyModel0, moduleFetchResultsCache[1].model);
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mModel, never()).add(any());
        verify(mSetVisibilityCallback, never()).onResult(true);
        // Verifies that the duration of fetching data successfully is logged.
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS
                        + HomeModulesMetricsUtils.getModuleName(mModuleTypeList[0]);
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataDuration(mHostSurface, mModuleTypeList[0], duration);
        histogramWatcher.assertExpected();

        doReturn(1).when(mModel).size();
        duration++;
        SystemClock.setCurrentTimeMillis(duration);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[2], null);
        // Verifies that the RecyclerView becomes visible as soon as no-data response of the
        // highest ranking modules arrive.
        verify(mModel, times(1)).add(any());
        verify(mSetVisibilityCallback).onResult(true);
        assertEquals(2, mMediator.getModuleResultsWaitingIndexForTesting());
        // Verifies that the duration of a failed response is logged.
        histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS
                        + ShadowHomeModulesMetricsUtils.getModuleName(mModuleTypeList[2]);
        histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataFailedDuration(
                mHostSurface, mModuleTypeList[2], duration);
        histogramWatcher.assertExpected();
        // Verifies that the duration of the recyclerview to be visible is logged.
        histogramName = "MagicStack.Clank.StartSurface" + HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS;
        histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFirstModuleShownDuration(mHostSurface, duration);
        histogramWatcher.assertExpected();

        // Verifies that the callback to change the visibility isn't called again.
        doReturn(2).when(mModel).size();
        PropertyModel propertyModel1 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[1], propertyModel1);
        verify(mModel, times(2)).add(any());
        assertEquals(3, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mSetVisibilityCallback).onResult(true);
    }

    @Test
    @SmallTest
    public void testHide() {
        // Adds 3 modules' data to the magic stack's RecyclerView.
        List<Integer> moduleList =
                List.of(mModuleTypeList[0], mModuleTypeList[1], mModuleTypeList[2]);
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);

        ModuleProvider[] moduleProviders = new ModuleProvider[MODULE_TYPES];
        for (int i = 0; i < MODULE_TYPES; i++) {
            moduleProviders[i] = Mockito.mock(ModuleProvider.class);
            // Modules are built successfully.
            mMediator.onModuleBuilt(mModuleTypeList[i], moduleProviders[i]);
        }
        doReturn(3).when(mModel).size();
        for (int i = 0; i < 3; i++) {
            doReturn(mListItems[i]).when(mModel).get(i);
        }

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

        verify(mModel).clear();
        verify(mSetVisibilityCallback).onResult(false);
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
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);

        // Verifies that the RecyclerView is changed to be visible when the first item is added.
        doReturn(1).when(mModel).size();
        mMediator.append(mListItems[0]);
        verify(mModel, times(1)).add(eq(mListItems[0]));
        verify(mSetVisibilityCallback).onResult(true);

        // Verifies that the callback to change visibility isn't called again when more items are
        // added.
        doReturn(2).when(mModel).size();
        mMediator.append(mListItems[1]);
        verify(mModel, times(1)).add(eq(mListItems[1]));
        verify(mSetVisibilityCallback).onResult(true);
    }

    @Test
    @SmallTest
    public void testRemove() {
        List<Integer> moduleList = List.of(mModuleTypeList[0]);
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);

        ModuleProvider moduleProvider = Mockito.mock(ModuleProvider.class);
        mMediator.onModuleBuilt(mModuleTypeList[0], moduleProvider);
        doReturn(1).when(mModel).size();
        doReturn(mListItems[0]).when(mModel).get(0);

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
        int duration = 100;
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[1], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            doReturn(true)
                    .when(mModuleRegistry)
                    .build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any());
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        SystemClock.setCurrentTimeMillis(0);
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);
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
        verify(mSetVisibilityCallback, never()).onResult(true);

        // The third ranking module returns a successful result.
        PropertyModel propertyModel0 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[0], propertyModel0);
        assertTrue(moduleFetchResultsIndicator[2]);
        assertEquals(propertyModel0, moduleFetchResultsCache[2].model);
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mModel, never()).add(any());
        verify(mSetVisibilityCallback, never()).onResult(true);

        when(mModel.size()).thenReturn(1);
        mMediator.onModuleFetchTimeOut();
        verify(mModel, times(1)).add(any());
        assertEquals(3, mMediator.getModuleResultsWaitingIndexForTesting());
        verify(mSetVisibilityCallback).onResult(true);
        // The magic stack is no longer waiting for modules to be load.
        assertFalse(mMediator.getIsFetchingModulesForTesting());
        // Verifies that the type of the module which didn't respond before timeout is logged.
        String histogramName =
                "MagicStack.Clank.StartSurface" + HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE;
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, mModuleTypeList[2]);
        HomeModulesMetricsUtils.recordFetchDataTimeOutType(mHostSurface, mModuleTypeList[2]);
        histogramWatcher.assertExpected();

        SystemClock.setCurrentTimeMillis(duration);
        PropertyModel propertyModel2 = Mockito.mock(PropertyModel.class);
        mMediator.addToRecyclerViewOrCache(mModuleTypeList[2], propertyModel2);
        // Verifies that there isn't any new module added to the recyclerview.
        verify(mModel, times(1)).add(any());
        // Verifies that the duration of a timeout response is logged.
        histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS
                        + ShadowHomeModulesMetricsUtils.getModuleName(mModuleTypeList[2]);
        histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataTimeOutDuration(
                mHostSurface, mModuleTypeList[2], duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testTimeOutCalledAfterHide() {
        List<Integer> moduleList =
                List.of(mModuleTypeList[2], mModuleTypeList[1], mModuleTypeList[0]);
        // Registers three modules to the ModuleRegistry.
        for (int i = 0; i < 3; i++) {
            doReturn(true)
                    .when(mModuleRegistry)
                    .build(eq(mModuleTypeList[i]), eq(mModuleDelegate), any());
        }
        assertEquals(0, mMediator.getModuleResultsWaitingIndexForTesting());

        // Calls buildModulesAndShow() to initialize ranking index map.
        SystemClock.setCurrentTimeMillis(0);
        mMediator.buildModulesAndShow(moduleList, mModuleDelegate, mSetVisibilityCallback);
        assertNotNull(mMediator.getModuleFetchResultsIndicatorForTesting());

        mMediator.hide();
        assertNull(mMediator.getModuleFetchResultsIndicatorForTesting());

        // After calling onModuleFetchTimeOut(), the mediator shouldn't throw any exception.
        mMediator.onModuleFetchTimeOut();
    }
}
