// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

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
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleMediatorUnitTest extends TestSupport {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private TabResumptionDataProvider mDataProvider;
    @Mock private UrlImageProvider mUrlImageProvider;
    @Mock private TabListFaviconProvider mFaviconProvider;
    @Mock private ThumbnailProvider mThumbnailProvider;
    @Mock private SuggestionClickCallbacks mClickCallbacks;

    @Captor private ArgumentCaptor<Callback<SuggestionsResult>> mFetchSuggestionCallbackCaptor;

    private PropertyModel mModel;
    private TabResumptionModuleMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);

        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);

        mMediator =
                new TabResumptionModuleMediator(
                        context,
                        mModuleDelegate,
                        mModel,
                        mDataProvider,
                        mUrlImageProvider,
                        mFaviconProvider,
                        mThumbnailProvider,
                        mClickCallbacks) {
                    @Override
                    long getCurrentTimeMs() {
                        return CURRENT_TIME_MS;
                    }
                };

        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                mUrlImageProvider, mModel.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        Assert.assertEquals(
                mFaviconProvider, mModel.get(TabResumptionModuleProperties.FAVICON_PROVIDER));
        Assert.assertEquals(
                mThumbnailProvider, mModel.get(TabResumptionModuleProperties.THUMBNAIL_PROVIDER));
        // `mClickCallback` may get wrapped, so just check for non-null.
        Assert.assertNotNull(mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK));
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        mModel = null;
        mMediator = null;
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
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(16, 0, 0),
                        /* id= */ 45);
        // Invalid due to empty title.
        SuggestionEntry entryInvalid =
                new SuggestionEntry(
                        "Desktop", JUnitTestGURLs.RED_2, "", makeTimestamp(17, 0, 0), 123);

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
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(16, 0, 0),
                        /* id= */ 45);
        SuggestionEntry entryNewer =
                new SuggestionEntry(
                        "Phone", JUnitTestGURLs.RED_2, "Red 2", makeTimestamp(13, 0, 0), 3);
        SuggestionEntry entryOldest =
                new SuggestionEntry(
                        "Desktop", JUnitTestGURLs.BLUE_1, "Blue 1", makeTimestamp(12, 0, 0), 1000);

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
        List<SuggestionEntry> stableSuggestions1 = Arrays.asList(makeValidEntry(0));

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
        List<SuggestionEntry> tentativeSuggestions = Arrays.asList(makeValidEntry(1));
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
        List<SuggestionEntry> tentativeSuggestions = Arrays.asList(makeValidEntry(0));
        List<SuggestionEntry> stableSuggestions1 =
                Arrays.asList(makeValidEntry(1), makeValidEntry(0));
        List<SuggestionEntry> stableSuggestions2 = Arrays.asList(makeValidEntry(0));
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

    private SuggestionEntry makeValidEntry(int index) {
        assert index == 0 || index == 1;
        GURL[] urlChoices = {JUnitTestGURLs.GOOGLE_URL_DOG, JUnitTestGURLs.GOOGLE_URL_CAT};
        String[] titleChoices = {"Google Dog", "Google Cat"};
        return new SuggestionEntry(
                /* sourceName= */ "Desktop",
                /* url= */ urlChoices[index],
                /* title= */ titleChoices[index],
                /* timestamp= */ makeTimestamp(16, 0, 0),
                /* id= */ 45);
    }

    private void checkModuleState(
            boolean isVisible,
            int expectOnDataReadyCalls,
            int expectOnDataFetchFailedCalls,
            int expectRemoveModuleCalls) {
        Assert.assertEquals(isVisible, mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        verify(mModuleDelegate, times(expectOnDataReadyCalls)).onDataReady(anyInt(), any());
        verify(mModuleDelegate, times(expectOnDataFetchFailedCalls)).onDataFetchFailed(anyInt());
        verify(mModuleDelegate, times(expectRemoveModuleCalls)).removeModule(anyInt());
    }

    private SuggestionBundle getSuggestionBundle() {
        return (SuggestionBundle) mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
    }
}
