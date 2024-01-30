// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewStub;

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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleMediatorUnitTest extends TestSupport {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    @Mock private TabResumptionDataProvider mDataProvider;
    @Mock private UrlImageProvider mUrlImageProvider;
    @Mock private ViewStub mViewStub;

    @Captor private ArgumentCaptor<Callback<List<SuggestionEntry>>> mFetchSuggestionCallbackCaptor;
    @Captor private ArgumentCaptor<GURL> mFetchImagePageUrlCaptor;

    private Activity mActivity;
    private TabResumptionModuleView mModuleView;
    private TabResumptionModuleCoordinator mCoordinator;
    private PropertyModel mModel;

    private SuggestionClickCallback mClickCallback;

    private GURL mLastClickUrl;
    private int mClickCount;

    public TabResumptionModuleMediatorUnitTest() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModuleView =
                (TabResumptionModuleView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.tab_resumption_module_layout, null);
        when(mViewStub.inflate()).thenReturn(mModuleView);

        mClickCallback =
                (GURL url) -> {
                    mLastClickUrl = url;
                    ++mClickCount;
                };
        mCoordinator =
                new TabResumptionModuleCoordinator(
                        mDataProvider, mUrlImageProvider, mClickCallback, mViewStub) {
                    @Override
                    protected TabResumptionModuleMediator createMediator() {
                        return new TabResumptionModuleMediator(mModel) {
                            @Override
                            long getCurrentTimeMs() {
                                return CURRENT_TIME_MS;
                            }
                        };
                    }
                };
        mModel = mCoordinator.getModelForTesting();

        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        Assert.assertEquals(mDataProvider, mModel.get(TabResumptionModuleProperties.DATA_PROVIDER));
        Assert.assertEquals(
                mUrlImageProvider, mModel.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        // `mClickCallback` may get wrapped, so just check for non-null.
        Assert.assertNotNull(mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK));
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        mModel = null;
        mCoordinator = null;
        mModuleView = null;
        mActivity = null;
    }

    @Test
    @SmallTest
    public void testNullSuggestions() {
        mCoordinator.reload();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getValue().onResult(null);
        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
    }

    @Test
    @SmallTest
    public void testEmptySuggestions() {
        List<SuggestionEntry> emptySuggestions = new ArrayList<SuggestionEntry>();
        mCoordinator.reload();
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
                        /* timestamp= */ makeTimestamp(12, 0, 0),
                        /* id= */ 90);
        // Invalid due to empty title.
        SuggestionEntry entryInvalid =
                new SuggestionEntry(
                        "Desktop", JUnitTestGURLs.RED_2, "", makeTimestamp(16, 0, 0), 123);

        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        suggestions.add(entryInvalid);
        suggestions.add(entryValid);
        Collections.sort(suggestions);

        Assert.assertFalse((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        mCoordinator.reload();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getValue().onResult(suggestions);

        Assert.assertTrue((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));

        SuggestionBundle bundle =
                (SuggestionBundle) mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        Assert.assertEquals(CURRENT_TIME_MS, bundle.referenceTimeMs);
        Assert.assertEquals(1, bundle.entries.size());
        Assert.assertEquals(entryValid, bundle.entries.get(0));

        // Check image URL load request.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(mFetchImagePageUrlCaptor.capture(), any());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(entryValid.url, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Simulate click (without UI) by calling the stored handler directly.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        GURL clickUrl = JUnitTestGURLs.GOOGLE_URL_CAT;
        SuggestionClickCallback clickCallback =
                (SuggestionClickCallback) mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK);
        clickCallback.onSuggestionClick(clickUrl);
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(clickUrl, mLastClickUrl);
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

        mCoordinator.reload();
        verify(mDataProvider).fetchSuggestions(mFetchSuggestionCallbackCaptor.capture());
        mFetchSuggestionCallbackCaptor.getValue().onResult(suggestions);

        Assert.assertTrue((Boolean) mModel.get(TabResumptionModuleProperties.IS_VISIBLE));

        SuggestionBundle bundle =
                (SuggestionBundle) mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        Assert.assertEquals(CURRENT_TIME_MS, bundle.referenceTimeMs);
        Assert.assertEquals(2, bundle.entries.size());
        Assert.assertEquals(entryNewest, bundle.entries.get(0));
        Assert.assertEquals(entryNewer, bundle.entries.get(1));

        // Check image URL load request.
        // TODO(crbug.com/1515325): Test load of second image URL once multi-tile support is added.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(mFetchImagePageUrlCaptor.capture(), any());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(entryNewest.url, mFetchImagePageUrlCaptor.getAllValues().get(0));
    }
}
