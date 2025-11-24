// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactoryJni;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.concurrent.atomic.AtomicReference;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
public class DownloadPageUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    // Arguments for DownloadPage constructor.
    @Mock private Activity mActivity;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private OtrProfileId mOtrProfileId;
    @Mock private NativePageHost mNativePageHost;

    // Dependencies for DownloadManagerCoordinatorFactory.create.
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private FaviconHelper.Natives mFaviconHelperJni;
    @Mock private OfflineContentAggregatorFactory.Natives mOfflineContentAggregatorFactoryJni;
    @Mock private OfflineContentProvider mOfflineContentProvider;
    @Mock private Destroyable mMarginAdapter;

    // Needed to test edge-to-edge behavior.
    private @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;
    private @Captor ArgumentCaptor<BackPressHandler> mHandlerCaptor;
    private @Mock EdgeToEdgeController mEdgeToEdgeController;
    private @Mock BackPressManager mMockBackPressManager;
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    private DownloadPage mDownloadPage;

    @Before
    public void setup() {
        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
        when(mNativePageHost.getContext()).thenReturn(mActivity);
        when(mNativePageHost.createDefaultMarginAdapter(any())).thenReturn(mMarginAdapter);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mProfile.getOffTheRecordProfile(mOtrProfileId, true)).thenReturn(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        when(mFaviconHelperJni.init()).thenReturn(1L); // Can't be 0 otherwise assertion fails.
        OfflineContentAggregatorFactoryJni.setInstanceForTesting(
                mOfflineContentAggregatorFactoryJni);
        OfflineContentAggregatorFactory.setOfflineContentProviderForTests(mOfflineContentProvider);
        when(mNativePageHost.createEdgeToEdgePadAdjuster(any()))
                .thenAnswer(
                        invocation ->
                                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                                        invocation.getArgument(0), mEdgeToEdgeSupplier));
        mDownloadPage =
                new DownloadPage(
                        mActivity,
                        mSnackbarManager,
                        mModalDialogManager,
                        mOtrProfileId,
                        mNativePageHost,
                        mMockBackPressManager);
    }

    @Test
    public void testEdgeToEdge() {
        assertTrue("Download page should support E2E.", mDownloadPage.supportsEdgeToEdge());

        // This should call SimpleEdgeToEdgePadAdjuster#mControllerChangedCallback.
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();

        padAdjuster.overrideBottomInset(100);
        ViewGroup listView = mDownloadPage.getListViewForTesting();
        assertEquals("Bottom insets should have been applied.", 100, listView.getPaddingBottom());
        assertFalse(listView.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals("Bottom insets should have been reset.", 0, listView.getPaddingBottom());
        assertTrue(listView.getClipToPadding());

        mDownloadPage.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }

    @Test
    public void testBackPressHandler_IsManagedByBasicNativePage() {
        final AtomicReference<ViewGroup> decorViewRef = new AtomicReference<>();
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity ->
                                decorViewRef.set((ViewGroup) activity.getWindow().getDecorView()));
        ViewGroup decorView = decorViewRef.get();
        View downloadPageView = mDownloadPage.getView();

        ThreadUtils.runOnUiThreadBlocking(() -> decorView.addView(downloadPageView));

        verify(mMockBackPressManager)
                .addHandler(mHandlerCaptor.capture(), eq(BackPressHandler.Type.NATIVE_PAGE));
        assertNotNull(
                "The handler passed to BackPressManager should not be null.",
                mHandlerCaptor.getValue());

        ThreadUtils.runOnUiThreadBlocking(() -> decorView.removeView(downloadPageView));

        verify(mMockBackPressManager).removeHandler(mHandlerCaptor.getValue());
    }
}
