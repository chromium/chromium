// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;

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
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
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

    @Captor private ArgumentCaptor<Callback<List<SuggestionEntry>>> mFetchSuggestionCallbackCaptor;
    @Captor private ArgumentCaptor<GURL> mFetchImagePageUrlCaptor;

    private PropertyModel mModel;
    private TabResumptionModuleView mModuleView;
    private TabResumptionModuleMediator mMediator;

    private SuggestionClickCallback mClickCallback;

    private GURL mLastClickUrl;
    private int mClickCount;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);

        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);
        mModuleView =
                (TabResumptionModuleView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_resumption_module_layout, null);

        mClickCallback =
                (GURL url) -> {
                    mLastClickUrl = url;
                    ++mClickCount;
                };

        mMediator =
                new TabResumptionModuleMediator(
                        context,
                        mModuleDelegate,
                        mModel,
                        mDataProvider,
                        mUrlImageProvider,
                        mClickCallback) {
                    @Override
                    long getCurrentTimeMs() {
                        return CURRENT_TIME_MS;
                    }
                };

        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                mUrlImageProvider, mModel.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        // `mClickCallback` may get wrapped, so just check for non-null.
        Assert.assertNotNull(mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK));
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        mModel = null;
        mMediator = null;
        mModuleView = null;
    }

    @Test
    @SmallTest
    public void testNullSuggestions() {
        mMediator.loadModule();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getValue().onResult(null);
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
    }

    @Test
    @SmallTest
    public void testEmptySuggestions() {
        List<SuggestionEntry> emptySuggestions = new ArrayList<SuggestionEntry>();
        mMediator.loadModule();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getValue().onResult(emptySuggestions);
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
        mFetchSuggestionCallbackCaptor.getValue().onResult(suggestions);

        Assert.assertTrue((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                "Continue with this tab", mModel.get(TabResumptionModuleProperties.TITLE));

        SuggestionBundle bundle =
                (SuggestionBundle) mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
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
        mFetchSuggestionCallbackCaptor.getValue().onResult(suggestions);

        Assert.assertTrue((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(
                "Continue with these tabs", mModel.get(TabResumptionModuleProperties.TITLE));

        SuggestionBundle bundle =
                (SuggestionBundle) mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        Assert.assertEquals(CURRENT_TIME_MS, bundle.referenceTimeMs);
        Assert.assertEquals(2, bundle.entries.size());
        Assert.assertEquals(entryNewest, bundle.entries.get(0));
        Assert.assertEquals(entryNewer, bundle.entries.get(1));
    }

    @Test
    @SmallTest
    public void testInitialNothingUpdateNothing() {
        List<SuggestionEntry> initialSuggestions = new ArrayList<SuggestionEntry>();
        List<SuggestionEntry> updateSuggestions1 = new ArrayList<SuggestionEntry>();

        // Initial call --> nothing: Don't show, wait some more.
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(0).onResult(initialSuggestions);
        checkModuleState(false, 0, 0, 0);

        // If no Update call: Wait until Magic Stack timeout.

        // Update call --> nothing: Call onDataFetchFailed(), gone indefinitely.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(1).onResult(updateSuggestions1);
        checkModuleState(false, 0, 1, 0);
    }

    @Test
    @SmallTest
    public void testInitialNothingUpdateSomething() {
        List<SuggestionEntry> initialSuggestions = new ArrayList<SuggestionEntry>();
        List<SuggestionEntry> updateSuggestions1 = Arrays.asList(makeValidEntry());

        // Initial call --> nothing: Don't show, wait some more.
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(0).onResult(initialSuggestions);
        checkModuleState(false, 0, 0, 0);

        // If no Update call: Wait until Magic Stack timeout.

        // Update call --> something: Call onDataReady() and show.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(1).onResult(updateSuggestions1);
        checkModuleState(true, 1, 0, 0);
    }

    @Test
    @SmallTest
    public void testInitialSomethingUpdateNothing() {
        List<SuggestionEntry> initialSuggestions = Arrays.asList(makeValidEntry());
        List<SuggestionEntry> updateSuggestions1 = new ArrayList<SuggestionEntry>();

        // Initial call --> something: Call onDataReady() and show (tentative).
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(0).onResult(initialSuggestions);
        checkModuleState(true, 1, 0, 0);

        // If no Update call: Data is new enough, show indefinitely.

        // Update call --> nothing: Call removeModule(), gone indefinitely.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(1).onResult(updateSuggestions1);
        checkModuleState(false, 1, 0, 1);

        // Reached terminal state: Subsequent loadModule() calls do nothing.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
    }

    @Test
    @SmallTest
    public void testInitialSomethingUpdateSomething() {
        List<SuggestionEntry> initialSuggestions = Arrays.asList(makeValidEntry());
        List<SuggestionEntry> updateSuggestions1 = Arrays.asList(makeValidEntry());
        List<SuggestionEntry> updateSuggestions2 = Arrays.asList(makeValidEntry());
        List<SuggestionEntry> updateSuggestions3 = new ArrayList<SuggestionEntry>();

        // Initial call --> something: Call onDataReady() and show (tentative).
        mMediator.loadModule();
        verify(mDataProvider, times(1)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(0).onResult(initialSuggestions);
        checkModuleState(true, 1, 0, 0);

        // If no Update call: Data is new enough, show indefinitely.

        // Update call --> something: Show.
        mMediator.loadModule();
        verify(mDataProvider, times(2)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(1).onResult(updateSuggestions1);
        checkModuleState(true, 1, 0, 0);

        // Rare case of more Update call --> something: Show.
        mMediator.loadModule();
        verify(mDataProvider, times(3)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(2).onResult(updateSuggestions2);
        checkModuleState(true, 1, 0, 0);

        // Rare case of more Update call --> nothing: Call removeModule(), gone indefinitely.
        mMediator.loadModule();
        verify(mDataProvider, times(4)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getAllValues().get(3).onResult(updateSuggestions3);
        checkModuleState(false, 1, 0, 1);

        // Reached terminal state: Subsequent loadModule() calls do nothing.
        mMediator.loadModule();
        verify(mDataProvider, times(4)).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
    }

    private SuggestionEntry makeValidEntry() {
        return new SuggestionEntry(
                /* sourceName= */ "Desktop",
                /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                /* title= */ "Google Dog",
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
}
