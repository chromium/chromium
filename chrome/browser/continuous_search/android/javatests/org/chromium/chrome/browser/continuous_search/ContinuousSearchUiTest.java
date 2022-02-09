// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.VisibilitySettings;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for Continuous Search UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContinuousSearchUiTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private VisibilitySettings mVisibilitySettings;
    private ContinuousSearchListCoordinator mCoordinator;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private LinearLayout mLayout;

    @Mock
    private Tab mTabMock;
    @Mock
    private BrowserControlsStateProvider mStateProviderMock;
    @Mock
    private ThemeColorProvider mThemeProviderMock;
    @Mock
    private ContinuousNavigationUserDataImpl mUserDataMock;
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    @Captor
    private ArgumentCaptor<ContinuousNavigationUserDataObserver> mObserverCaptor;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sActivity = sActivityTestRule.getActivity(); });
    }

    @Before
    public void setUpTest() throws Exception {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTINUOUS_SEARCH, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchListMediator.TRIGGER_MODE_PARAM, "0");
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchListMediator.SHOW_RESULT_TITLE_PARAM, "false");

        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenAnswer((invocation) -> {
                    return ((GURL) invocation.getArguments()[0]).getSpec();
                });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureList.setTestValues(testValues);
            mTabSupplier = new ObservableSupplierImpl<Tab>();

            mCoordinator = new ContinuousSearchListCoordinator(mStateProviderMock, mTabSupplier,
                    this::updateVisibility, mThemeProviderMock, sActivity);
            mLayout = new LinearLayout(sActivity);
            mLayout.setLayoutParams(
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT));
            sActivity.setContentView(mLayout);
            mCoordinator.initializeLayout(mLayout);
            ContinuousNavigationUserDataImpl.setInstanceForTesting(mUserDataMock);
            mTabSupplier.set(mTabMock);
        });
        verify(mUserDataMock).addObserver(mObserverCaptor.capture());
    }

    /**
     * Tests that scroll to selected works without setting the scrolled condition.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1293647")
    public void testScrollToSelected() throws TimeoutException {
        GURL srpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
        GURL result1Url = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        GURL result2Url = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_2);
        GURL result3Url = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_3);
        GURL result4Url = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1);
        List<PageGroup> groups = new ArrayList<PageGroup>();
        List<PageItem> results = new ArrayList<PageItem>();
        results.add(new PageItem(result1Url, "Red 1"));
        results.add(new PageItem(result2Url, "Red 2"));
        results.add(new PageItem(result3Url, "Red 3"));
        results.add(new PageItem(result4Url, "Blue 1"));
        groups.add(new PageGroup("Red Group", false, results));
        ContinuousNavigationMetadata metadata = new ContinuousNavigationMetadata(
                srpUrl, "Foo", new ContinuousNavigationMetadata.Provider(1, null, 0), groups);

        ContinuousNavigationUserDataObserver observer = mObserverCaptor.getValue();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            observer.onUpdate(metadata);
            observer.onUrlChanged(result4Url, false);
        });

        Assert.assertNotNull(mVisibilitySettings);
        Assert.assertTrue(mVisibilitySettings.isVisible());
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = mLayout.findViewById(R.id.recycler_view);
            recyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(RecyclerView view, int newState) {
                    if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                        callbackHelper.notifyCalled();
                    }
                }
            });
            mVisibilitySettings.getOnFinishRunnable().run();
        });

        callbackHelper.waitForFirst("UI Didn't finish scroll.");
        Assert.assertFalse(mCoordinator.getMediatorForTesting().mScrolled);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = mLayout.findViewById(R.id.recycler_view);
            Assert.assertTrue("First fully visible item is unexpected.",
                    ((LinearLayoutManager) recyclerView.getLayoutManager())
                                    .findFirstCompletelyVisibleItemPosition()
                            > 0);
        });
    }

    private void updateVisibility(VisibilitySettings visibilitySettings) {
        mVisibilitySettings = visibilitySettings;
    }
}
