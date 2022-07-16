// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.hamcrest.Matchers.lessThan;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewStub;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Test of {@link ContinuousSearchContainerCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchContainerCoordinatorTest {
    private static final String TEST_QUERY = "Foo";
    private static final int TEST_RESULT_TYPE = 1;
    private static final long FAKE_NATIVE_ADDR = 123456L;
    private static final int STUB_ID = 123;
    private static final int INFLATED_ID = 456;

    @Mock
    private Tab mTabMock;
    @Mock
    private LayoutManager mLayoutManagerMock;
    @Mock
    private ResourceManager mResourceManagerMock;
    @Mock
    private DynamicResourceLoader mResourceLoaderMock;
    @Mock
    private BrowserControlsStateProvider mStateProviderMock;
    @Mock
    private ThemeColorProvider mThemeColorProviderMock;
    @Mock
    private SearchUrlHelper.Natives mSearchUrlHelperJniMock;
    @Mock
    private ContinuousSearchSceneLayer.Natives mContinuousSearchSceneLayerJniMock;
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    private ObservableSupplierImpl<Tab> mTabSupplier = new ObservableSupplierImpl<Tab>();

    private Supplier<Boolean> mAnimateNativeControls;
    private boolean mAnimateNativeControlsValue;

    private Supplier<Integer> mDefaultTopHeight;
    private int mDefaultTopHeightValue;

    private boolean mAnimateHidingState;
    private int mAnimateHidingStateCount;
    private GURL mSrpUrl;
    private UserDataHost mUserDataHost = new UserDataHost();
    private ContinuousSearchContainerCoordinator mCoordinator;
    private ContinuousNavigationUserDataImpl mUserData;
    private LinearLayout mRoot;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTINUOUS_SEARCH, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchListMediator.TRIGGER_MODE_PARAM, "0");
        FeatureList.setTestValues(testValues);
        mSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
        mJniMocker.mock(SearchUrlHelperJni.TEST_HOOKS, mSearchUrlHelperJniMock);
        mJniMocker.mock(
                ContinuousSearchSceneLayerJni.TEST_HOOKS, mContinuousSearchSceneLayerJniMock);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        doReturn(TEST_QUERY).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mSrpUrl));
        doReturn(TEST_RESULT_TYPE)
                .when(mSearchUrlHelperJniMock)
                .getSrpPageCategoryFromUrl(eq(mSrpUrl));
        doReturn(FAKE_NATIVE_ADDR).when(mContinuousSearchSceneLayerJniMock).init(any());
        doReturn(mResourceLoaderMock).when(mResourceManagerMock).getDynamicResourceLoader();

        Context context =
                new ContextThemeWrapper(ContextUtils.getApplicationContext(), R.style.ColorOverlay);
        mRoot = new LinearLayout(context);
        mRoot.setLayoutParams(new LinearLayout.LayoutParams(100, 100));
        mRoot.setOrientation(LinearLayout.VERTICAL);
        ViewStub viewStub = new ViewStub(context);
        viewStub.setId(STUB_ID);
        viewStub.setInflatedId(INFLATED_ID);
        viewStub.setLayoutResource(
                org.chromium.chrome.browser.continuous_search.R.layout.continuous_search_container);
        mRoot.addView(viewStub);
        mAnimateNativeControls = () -> {
            return mAnimateNativeControlsValue;
        };
        mDefaultTopHeight = () -> {
            return mDefaultTopHeightValue;
        };

        doReturn(mUserDataHost).when(mTabMock).getUserDataHost();
        mUserData = ContinuousNavigationUserDataImpl.getOrCreateForTab(mTabMock);
        mUserData.mAllowNativeUrlChecks = false;
        mCoordinator = new ContinuousSearchContainerCoordinator(viewStub, mLayoutManagerMock,
                mResourceManagerMock, mTabSupplier, mStateProviderMock, mAnimateNativeControls,
                mDefaultTopHeight, mThemeColorProviderMock, context, (state) -> {
                    mAnimateHidingState = state;
                    mAnimateHidingStateCount++;
                });

        Assert.assertEquals(ContinuousSearchSceneLayer.class,
                ContinuousSearchContainerCoordinator.getSceneOverlayClass());
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    public void testShowAndDismiss() {
        Assert.assertNotNull(mRoot.findViewById(STUB_ID));
        Assert.assertNull(mRoot.findViewById(INFLATED_ID));

        // Create data.
        GURL resultUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        List<PageGroup> groups = new ArrayList<PageGroup>();
        List<PageItem> results1 = new ArrayList<PageItem>();
        results1.add(new PageItem(resultUrl, "Red 1"));
        groups.add(new PageGroup("Red Group", false, results1));
        ContinuousNavigationMetadata metadata =
                new ContinuousNavigationMetadata(mSrpUrl, TEST_QUERY, getProvider(), groups);

        mTabSupplier.set(mTabMock);
        mUserData.updateData(metadata, mSrpUrl);
        mUserData.updateCurrentUrl(resultUrl);
        ContinuousSearchViewResourceFrameLayout rootView = mCoordinator.getRootViewForTesting();
        Assert.assertNotNull(rootView);
        rootView.layout(0, 0, 100, 100);
        View view = mock(View.class);
        when(view.getHeight())
                .thenReturn(40 + mCoordinator.getRootViewForTesting().getShadowHeight());
        mRoot.findViewById(INFLATED_ID).layout(0, 0, 100, 100);
        mCoordinator.onLayoutChange(view, 0, 0, 0, 0, 0, 0, 0, 0);

        // Just return the Url verbatim in this test since we are primarily concerned with the UI
        // inflating.
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenAnswer((invocation) -> {
                    return ((GURL) invocation.getArguments()[0]).getSpec();
                });
        RecyclerView recyclerView = rootView.findViewById(R.id.recycler_view);
        Assert.assertNotNull(recyclerView);
        recyclerView.layout(0, 0, 100, 100);

        // View should now be inflated.
        Assert.assertNull(mRoot.findViewById(STUB_ID));
        Assert.assertNotNull(mRoot.findViewById(INFLATED_ID));
        // UI is still in flux so just assert that the items exist.
        Assert.assertEquals(1, recyclerView.getAdapter().getItemCount());

        // Invalidate.
        mUserData.invalidateData();
        mCoordinator.getMediatorForTesting().runOnFinishedHide();
        Assert.assertEquals(0, recyclerView.getAdapter().getItemCount());
    }

    @Test
    public void testObserveHeightChanges() {
        ContinuousSearchContainerCoordinator.HeightObserver observer =
                mock(ContinuousSearchContainerCoordinator.HeightObserver.class);
        InOrder inOrder = inOrder(observer);
        mCoordinator.addHeightObserver(observer);
        mTabSupplier.set(mTabMock);

        GURL resultUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        List<PageGroup> groups = new ArrayList<PageGroup>();
        List<PageItem> results1 = new ArrayList<PageItem>();
        results1.add(new PageItem(resultUrl, "Red 1"));
        groups.add(new PageGroup("Red Group", false, results1));
        ContinuousNavigationMetadata metadata =
                new ContinuousNavigationMetadata(mSrpUrl, TEST_QUERY, getProvider(), groups);

        mUserData.updateData(metadata, mSrpUrl);
        mUserData.updateCurrentUrl(resultUrl);
        View view = mock(View.class);
        when(view.getHeight())
                .thenReturn(5 + mCoordinator.getRootViewForTesting().getShadowHeight());
        mCoordinator.onLayoutChange(view, 0, 0, 0, 0, 0, 0, 0, 0);
        inOrder.verify(observer).onHeightChange(5, false);
        mCoordinator.removeHeightObserver(observer);

        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that {@link ContinuousSearchViewResourceFrameLayout} captures correctly.
     */
    @Test
    public void testCapture() {
        GURL resultUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        List<PageGroup> groups = new ArrayList<PageGroup>();
        List<PageItem> results1 = new ArrayList<PageItem>();
        results1.add(new PageItem(resultUrl, "Red 1"));
        groups.add(new PageGroup("Red Group", false, results1));
        ContinuousNavigationMetadata metadata =
                new ContinuousNavigationMetadata(mSrpUrl, TEST_QUERY, getProvider(), groups);

        mTabSupplier.set(mTabMock);
        mUserData.updateData(metadata, mSrpUrl);
        mUserData.updateCurrentUrl(resultUrl);
        View view = mock(View.class);
        ContinuousSearchViewResourceFrameLayout rootView = mCoordinator.getRootViewForTesting();
        when(view.getHeight()).thenReturn(5 + rootView.getShadowHeight());
        mCoordinator.onLayoutChange(view, 0, 0, 0, 0, 0, 0, 0, 0);

        // Force a resize since robolectric view sizes are technically 0x0 by default.
        rootView.layout(0, 0, 100, 100);
        ViewResourceAdapter adapter = rootView.createResourceAdapter();
        adapter.invalidate(new Rect(0, rootView.getHeight() - rootView.getShadowHeight(),
                rootView.getWidth(), rootView.getHeight()));
        Bitmap bitmap = adapter.getBitmap();

        // This isn't intended to be a pixel test so just check that the content was actually
        // captured.
        Assert.assertNotNull(bitmap);
        Assert.assertThat(1, lessThan(bitmap.getHeight()));
        Assert.assertThat(1, lessThan(bitmap.getWidth()));
    }

    private ContinuousNavigationMetadata.Provider getProvider() {
        return new ContinuousNavigationMetadata.Provider(TEST_RESULT_TYPE, null, 0);
    }
}
