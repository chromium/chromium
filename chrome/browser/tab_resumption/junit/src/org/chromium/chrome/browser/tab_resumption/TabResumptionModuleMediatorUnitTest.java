// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static android.os.Looper.getMainLooper;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.visited_url_ranking.ScoredURLUserAction;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleMediatorUnitTest extends TestSupportExtended {

    /** Custom mock for TrainingInfo. */
    static class FakeTrainingInfo extends TrainingInfo {
        public String recordHistory = "";

        FakeTrainingInfo(SuggestionEntry entry, long requestId) {
            super(
                    /* nativeVisitedUrlRankingBackend= */ 0L,
                    /* visitId= */ entry.url.getSpec(),
                    /* requestId= */ requestId);
        }

        @Override
        void record(@ScoredURLUserAction int scoredUrlUserAction) {
            switch (scoredUrlUserAction) {
                case ScoredURLUserAction.SEEN:
                    recordHistory += "(SEEN)";
                    break;
                case ScoredURLUserAction.ACTIVATED:
                    recordHistory += "(ACTIVATED)";
                    break;
                case ScoredURLUserAction.DISMISSED:
                    recordHistory += "(DISMISSED)";
                    break;
                default:
                    recordHistory += "(?)";
                    break;
            }
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabResumptionDataProvider mDataProvider;
    @Mock private UrlImageProvider mUrlImageProvider;
    @Mock private SuggestionClickCallback mClickCallback;
    @Mock private TabModelSelector mTabModelSelector;

    @Captor private ArgumentCaptor<Callback<SuggestionsResult>> mFetchSuggestionCallbackCaptor;

    // Fake Tab observation helper, assumes each Tab has at most one TabObserver.
    private Map<Integer, TabObserver> mTabObserverMap;

    private TabResumptionModuleMediator mMediator;

    private int mReloadSessionCounter;
    private CallbackHelper mReloadSessionCallbackHelper;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        mTabObserverMap = new HashMap<Integer, TabObserver>();
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>();
        mTabModelSelectorSupplier.set(mTabModelSelector);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);

        TabResumptionModuleUtils.setFakeCurrentTimeMsForTesting(() -> CURRENT_TIME_MS);
        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);

        mMediator =
                new TabResumptionModuleMediator(
                        /* context= */ mContext,
                        /* moduleDelegate= */ mModuleDelegate,
                        /* tabModelSelectorSupplier= */ mTabModelSelectorSupplier,
                        /* model= */ mModel,
                        /* urlImageProvider= */ mUrlImageProvider,
                        /* reloadSessionCallback= */ () -> {
                            ++mReloadSessionCounter;
                            mReloadSessionCallbackHelper.notifyCalled();
                        },
                        /* statusChangedCallback= */ CallbackUtils.emptyRunnable(),
                        /* seeMoreLinkClickCallback= */ CallbackUtils.emptyRunnable(),
                        /* suggestionClickCallback= */ mClickCallback);
        mMediator.startSession(mDataProvider);

        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                mUrlImageProvider, mModel.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        // `mClickCallback` may get wrapped, so just check for non-null.
        Assert.assertNotNull(mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK));
    }

    @After
    public void tearDown() {
        mMediator.endSession(); // Okay even if endSession() has already been called.
        mMediator.destroy();
        mMediator = null;
        Assert.assertNull(mModel.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        mModel = null;
        TabResumptionModuleUtils.setFakeCurrentTimeMsForTesting(null);
    }

    @Test
    @SmallTest
    public void testNullSuggestions() {
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getValue()
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));

        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getValue()
                .onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));

        // Fetch skipped on next load, since module is gone.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
    }

    @Test
    @SmallTest
    public void testEmptySuggestions() {
        List<SuggestionEntry> emptySuggestions = new ArrayList<SuggestionEntry>();
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getValue()
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, emptySuggestions));
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));

        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getValue()
                .onResult(new SuggestionsResult(ResultStrength.STABLE, emptySuggestions));
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
    }

    @Test
    @SmallTest
    public void testRejectInvalidOrStale() {
        // TabResumptionDataProvider filters (including staleness) and ranks suggestions. Only test
        // the selection and filtering layer in TabResumptionModuleMediator.
        SuggestionEntry entryValid =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(16, 0, 0));
        // Invalid due to empty title.
        SuggestionEntry entryInvalid =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.RED_2,
                        /* title= */ "",
                        /* timestamp= */ makeTimestamp(17, 0, 0));

        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        suggestions.add(entryInvalid);
        suggestions.add(entryValid);
        Collections.sort(suggestions);

        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        mMediator.loadModule();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getValue()
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, suggestions));

        Assert.assertTrue((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                "Continue with this tab", mModel.get(TabResumptionModuleProperties.TITLE));

        SuggestionBundle bundle = getSuggestionBundle();
        Assert.assertEquals(CURRENT_TIME_MS, bundle.referenceTimeMs);
        Assert.assertEquals(1, bundle.entries.size());
        Assert.assertEquals(entryValid, bundle.entries.get(0));
    }

    @Test
    @SmallTest
    public void testTakeMostRecent() {
        SuggestionEntry entryNewest =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(16, 0, 0));
        SuggestionEntry entryNewer =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Phone",
                        /* url= */ JUnitTestGURLs.RED_2,
                        /* title= */ "Red 2",
                        /* timestamp= */ makeTimestamp(13, 0, 0));
        SuggestionEntry entryOldest =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.BLUE_1,
                        /* title= */ "Blue 1",
                        /* timestamp= */ makeTimestamp(12, 0, 0));

        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        suggestions.add(entryNewest);
        suggestions.add(entryNewer);
        suggestions.add(entryOldest);
        Collections.sort(suggestions);

        mMediator.loadModule();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getValue()
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, suggestions));

        Assert.assertTrue((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                "Continue with these tabs", mModel.get(TabResumptionModuleProperties.TITLE));

        SuggestionBundle bundle = getSuggestionBundle();
        Assert.assertEquals(CURRENT_TIME_MS, bundle.referenceTimeMs);
        Assert.assertEquals(2, bundle.entries.size());
        Assert.assertEquals(entryNewest, bundle.entries.get(0));
        Assert.assertEquals(entryNewer, bundle.entries.get(1));
    }

    @Test
    @SmallTest
    public void testTentativeNothingStableNothing() {
        List<SuggestionEntry> tentativeSuggestions = new ArrayList<SuggestionEntry>();
        List<SuggestionEntry> stableSuggestions1 = new ArrayList<SuggestionEntry>();

        // Tentative suggestions = nothing: Don't fail yet; wait some more.
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, tentativeSuggestions));
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 0,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);

        // Stable suggestions = nothing: Call onDataFetchFailed().
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(1)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions1));
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 0,
                /* expectOnDataFetchFailedCalls= */ 1,
                /* expectRemoveModuleCalls= */ 0);
    }

    @Test
    @SmallTest
    public void testTentativeNothingStableSomething() {
        List<SuggestionEntry> tentativeSuggestions = new ArrayList<SuggestionEntry>();
        List<SuggestionEntry> stableSuggestions1 = Arrays.asList(makeSyncDerivedSuggestion(0));

        // Tentative suggestions = nothing: Don't fail yet; wait some more.
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, tentativeSuggestions));
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 0,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);

        // Stable suggestions = something: Call onDataReady() and show.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(1)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions1));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(0).title);

        // Forced null: Call removeModule().
        mMediator.loadModule();
        verify(mDataProvider, times(3)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(2)
                .onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 1);
    }

    @Test
    @SmallTest
    public void testTentativeSomethingStableNothing() {
        List<SuggestionEntry> tentativeSuggestions = Arrays.asList(makeSyncDerivedSuggestion(1));
        List<SuggestionEntry> stableSuggestions1 = new ArrayList<SuggestionEntry>();

        // Tentative suggestions = something: Call onDataReady() and show (tentative).
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, tentativeSuggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Cat", getSuggestionBundle().entries.get(0).title);

        // Stable suggestions = nothing: Call removeModule().
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(1)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions1));
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 1);

        // Reached terminal state: Subsequent loadModule() calls do nothing.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
    }

    @Test
    @SmallTest
    public void testTentativeSomethingStableSomething() {
        List<SuggestionEntry> tentativeSuggestions = Arrays.asList(makeSyncDerivedSuggestion(0));
        List<SuggestionEntry> stableSuggestions1 =
                Arrays.asList(makeSyncDerivedSuggestion(1), makeSyncDerivedSuggestion(0));
        List<SuggestionEntry> stableSuggestions2 = Arrays.asList(makeSyncDerivedSuggestion(0));
        List<SuggestionEntry> stableSuggestions3 = new ArrayList<SuggestionEntry>();

        // Tentative suggestions = something: Call onDataReady() and show (tentative).
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, tentativeSuggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(0).title);

        // Stable suggestions 1 = something: Show stable results.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(1)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions1));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Cat", getSuggestionBundle().entries.get(0).title);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(1).title);

        // Stable suggestions 2 = something: Update shown stable results.
        mMediator.loadModule();
        verify(mDataProvider, times(3)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(2)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions2));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(0).title);

        // Stable suggestions 3 = nothing: Call removeModule().
        mMediator.loadModule();
        verify(mDataProvider, times(4)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(3)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions3));
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 1);

        // Reached terminal state: Subsequent loadModule() calls do nothing.
        mMediator.loadModule();
        verify(mDataProvider, times(4)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 1);
    }

    @Test
    @SmallTest
    public void testMaxTilesNumber_Single() {
        testMaxTilesNumberImpl(1);
    }

    @Test
    @SmallTest
    public void testMaxTilesNumber_Double() {
        testMaxTilesNumberImpl(2);
    }

    @Test
    @SmallTest
    public void testShowOnlyOneLocalTab() {
        Tab tab0 = createMockLocalTab(0);
        Tab tab1 = createMockLocalTab(1);
        bindBasicTabObservation(tab0);
        bindBasicTabObservation(tab1);
        List<SuggestionEntry> suggestions =
                Arrays.asList(
                        SuggestionEntry.createFromLocalTab(tab0),
                        SuggestionEntry.createFromLocalTab(tab1));
        mReloadSessionCallbackHelper = new CallbackHelper();

        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, suggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals(1, getSuggestionBundle().entries.size());
        Assert.assertEquals(suggestions.get(0), getSuggestionBundle().entries.get(0));

        // Examine Local Tab observation: Only the shown tab is observed.
        Assert.assertNotNull(mTabObserverMap.get(tab0.getId()));
        Assert.assertNull(mTabObserverMap.get(tab1.getId()));

        // Simulate closing the suggested Local Tab: Causes `reloadSessionCallback` to be called.
        Assert.assertEquals(0, mReloadSessionCounter);
        mTabObserverMap.get(tab0.getId()).onClosingStateChanged(tab0, /* closing= */ false);
        Assert.assertEquals(0, mReloadSessionCounter);
        mTabObserverMap.get(tab0.getId()).onClosingStateChanged(tab0, /* closing= */ true);
        try {
            shadowOf(getMainLooper()).idle();
            mReloadSessionCallbackHelper.waitForNext();
        } catch (TimeoutException e) {
            throw new AssertionError("Timed out waiting for reload callback.", e);
        }
        Assert.assertEquals(1, mReloadSessionCounter);

        // Simulate ending session: Observation should be released.
        mMediator.endSession();
        Assert.assertNull(mTabObserverMap.get(tab0.getId()));
        Assert.assertNull(mTabObserverMap.get(tab1.getId()));
    }

    @Test
    @SmallTest
    public void testShowOnlyOneLocalTab_WithForeignTab() {
        Tab tab0 = createMockLocalTab(0);
        Tab tab1 = createMockLocalTab(1);
        List<SuggestionEntry> suggestions =
                Arrays.asList(
                        SuggestionEntry.createFromLocalTab(tab0),
                        SuggestionEntry.createFromLocalTab(tab1),
                        makeSyncDerivedSuggestion(1));

        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, suggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals(2, getSuggestionBundle().entries.size());
        Assert.assertEquals(suggestions.get(0), getSuggestionBundle().entries.get(0));
        Assert.assertEquals(suggestions.get(2), getSuggestionBundle().entries.get(1));
    }

    @Test
    @SmallTest
    public void testTrainingInfoUsageTentativeClick() {
        List<SuggestionEntry> tentativeSuggestions =
                Arrays.asList(makeSyncDerivedSuggestion(0), makeSyncDerivedSuggestion(1));
        List<FakeTrainingInfo> tentativeFakeTrainingInfos =
                addFakeTrainingInfo(tentativeSuggestions, Arrays.asList(1234L, -314159265358L));

        // Tentative suggestions = something: Call onDataReady() and show (tentative).
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, tentativeSuggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(0).title);
        Assert.assertEquals("Google Cat", getSuggestionBundle().entries.get(1).title);

        // No user interactions.
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(0).recordHistory);
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(1).recordHistory);

        // User click on entry 1.
        mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK)
                .onSuggestionClicked(tentativeSuggestions.get(1));
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(0).recordHistory);
        Assert.assertEquals("(ACTIVATED)", tentativeFakeTrainingInfos.get(1).recordHistory);

        // Explicitly end session.
        mMediator.endSession();
        Assert.assertEquals("(SEEN)", tentativeFakeTrainingInfos.get(0).recordHistory);
        Assert.assertEquals("(ACTIVATED)(SEEN)", tentativeFakeTrainingInfos.get(1).recordHistory);
    }

    @Test
    @SmallTest
    public void testTrainingInfoUsageTentativeStableStable() {
        List<SuggestionEntry> tentativeSuggestions =
                Arrays.asList(makeSyncDerivedSuggestion(0), makeSyncDerivedSuggestion(1));
        List<SuggestionEntry> stableSuggestions1 =
                Arrays.asList(makeSyncDerivedSuggestion(1), makeSyncDerivedSuggestion(0));
        List<SuggestionEntry> stableSuggestions2 = Arrays.asList(makeSyncDerivedSuggestion(0));

        List<FakeTrainingInfo> tentativeFakeTrainingInfos =
                addFakeTrainingInfo(tentativeSuggestions, Arrays.asList(0L, -1L));
        List<FakeTrainingInfo> stableFakeTrainingInfos1 =
                addFakeTrainingInfo(stableSuggestions1, Arrays.asList(-2024L, -133700000000L));
        List<FakeTrainingInfo> stableFakeTrainingInfos2 =
                addFakeTrainingInfo(stableSuggestions2, Arrays.asList(9876543219876543L));

        // Tentative suggestions = something: Call onDataReady() and show (tentative).
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.TENTATIVE, tentativeSuggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(0).title);
        Assert.assertEquals("Google Cat", getSuggestionBundle().entries.get(1).title);

        // No user interactions.
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(0).recordHistory);
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(1).recordHistory);

        // Stable suggestions 1 = something: Show stable results.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(1)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions1));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Cat", getSuggestionBundle().entries.get(0).title);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(1).title);

        // No user interactions.
        Assert.assertEquals("", stableFakeTrainingInfos1.get(0).recordHistory);
        Assert.assertEquals("", stableFakeTrainingInfos1.get(1).recordHistory);

        // The *previous* suggestions don't get training info logging since they're TENTATIVE.
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(0).recordHistory);
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(1).recordHistory);

        // Stable suggestions 2 = something: Update shown stable results.
        mMediator.loadModule();
        verify(mDataProvider, times(3)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(2)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, stableSuggestions2));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals("Google Dog", getSuggestionBundle().entries.get(0).title);

        // No user interactions.
        Assert.assertEquals("", stableFakeTrainingInfos2.get(0).recordHistory);

        // The *previous* suggestions get training info logging since they're STABLE.
        Assert.assertEquals("(SEEN)", stableFakeTrainingInfos1.get(0).recordHistory);
        Assert.assertEquals("(SEEN)", stableFakeTrainingInfos1.get(1).recordHistory);

        // Explicitly end session.
        mMediator.endSession();
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(0).recordHistory);
        Assert.assertEquals("", tentativeFakeTrainingInfos.get(1).recordHistory);
        Assert.assertEquals("(SEEN)", stableFakeTrainingInfos1.get(0).recordHistory);
        Assert.assertEquals("(SEEN)", stableFakeTrainingInfos1.get(1).recordHistory);
        Assert.assertEquals("(SEEN)", stableFakeTrainingInfos2.get(0).recordHistory);
    }

    private void testMaxTilesNumberImpl(int maxTilesNumber) {
        TabResumptionModuleUtils.TAB_RESUMPTION_MAX_TILES_NUMBER.setForTesting(maxTilesNumber);
        List<SuggestionEntry> suggestions =
                Arrays.asList(makeSyncDerivedSuggestion(1), makeSyncDerivedSuggestion(0));

        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new SuggestionsResult(ResultStrength.STABLE, suggestions));
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
        Assert.assertEquals(maxTilesNumber, getSuggestionBundle().entries.size());
    }

    private SuggestionBundle getSuggestionBundle() {
        return (SuggestionBundle) mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
    }

    private List<FakeTrainingInfo> addFakeTrainingInfo(
            List<SuggestionEntry> entries, List<Long> requestIds) {
        int n = entries.size();
        assert requestIds.size() == n;
        List<FakeTrainingInfo> fakeTrainingInfos = new ArrayList<FakeTrainingInfo>();
        for (int i = 0; i < n; ++i) {
            SuggestionEntry entry = entries.get(i);
            Assert.assertNull(entry.trainingInfo);
            FakeTrainingInfo traingInfo = new FakeTrainingInfo(entry, requestIds.get(i));
            fakeTrainingInfos.add(traingInfo);
            entry.trainingInfo = traingInfo;
        }
        return fakeTrainingInfos;
    }

    /** Simulates behavior of Tab.{addObserver(),removeObserver()}, captured in `mTabObserveMap` */
    private void bindBasicTabObservation(Tab tab) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            // Enforce our restriction that a Tab can have at most one observer.
                            Assert.assertFalse(mTabObserverMap.containsKey(tab.getId()));
                            mTabObserverMap.put(
                                    tab.getId(), (TabObserver) invocation.getArguments()[0]);
                            return null;
                        })
                .when(tab)
                .addObserver(any(TabObserver.class));

        doAnswer(
                        (InvocationOnMock invocation) -> {
                            Assert.assertEquals(
                                    mTabObserverMap.get(tab.getId()),
                                    (TabObserver) invocation.getArguments()[0]);
                            mTabObserverMap.remove(tab.getId());
                            return null;
                        })
                .when(tab)
                .removeObserver(any(TabObserver.class));
    }
}
