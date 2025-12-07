// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP;

import android.content.ComponentName;
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

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({UNO_PHASE_2_FOLLOW_UP, ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES})
public class BookmarkPageUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    // Arguments for BookmarkPage constructor.
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private NativePageHost mNativePageHost;
    @Mock private ComponentName mComponentName;

    // Other dependencies.
    @Mock private BookmarkModel mModel;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SigninManager mSigninManager;
    @Mock private SyncService mSyncService;
    @Mock private ReauthenticatorBridge mReauthenticatorBridge;
    @Mock private Destroyable mMarginAdapater;
    @Mock private BackPressManager mBackPressManager;

    // Mock native methods.
    @Mock private BookmarkBridge.Natives mBookmarkNatives;
    @Mock private ShoppingServiceFactory.Natives mShoppingNatives;
    @Mock private ImageServiceBridge.Natives mImageNatives;
    @Mock private FaviconHelper.Natives mFaviconNatives;

    // Needed to test edge-to-edge behavior.
    private @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;
    private @Mock EdgeToEdgeController mEdgeToEdgeController;
    private final ObservableSupplierImpl<@Nullable EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    private BookmarkPage mBookmarkPage;

    @Before
    public void setup() {
        BookmarkModel.setInstanceForTesting(mModel);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        BookmarkBridgeJni.setInstanceForTesting(mBookmarkNatives);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingNatives);
        ImageServiceBridgeJni.setInstanceForTesting(mImageNatives);
        FaviconHelperJni.setInstanceForTesting(mFaviconNatives);
        when(mFaviconNatives.init()).thenReturn(1L); // Can't be 0 otherwise assertion fails.
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        mActivityScenarios
                .getScenario()
                .onActivity(activity -> when(mNativePageHost.getContext()).thenReturn(activity));
        when(mNativePageHost.createDefaultMarginAdapter(any())).thenReturn(mMarginAdapater);
        when(mNativePageHost.createEdgeToEdgePadAdjuster(any()))
                .thenAnswer(
                        invocation ->
                                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                                        invocation.getArgument(0), mEdgeToEdgeSupplier));
        mBookmarkPage =
                new BookmarkPage(
                        mSnackbarManager,
                        mProfile,
                        mNativePageHost,
                        mComponentName,
                        mBackPressManager);
    }

    @Test
    public void testBackPressManagerWiredWhenFeatureEnabled() {
        assertNotNull(
                "BackPressManager should be set on coordinator when feature is enabled.",
                mBookmarkPage.getManagerForTesting().getBackPressManagerForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
    public void testBackPressManagerNotWiredWhenFeatureDisabled() {
        assertNull(
                "BackPressManager should NOT be set on coordinator when feature is disabled.",
                mBookmarkPage.getManagerForTesting().getBackPressManagerForTesting());
    }

    @Test
    public void testEdgeToEdge() {
        assertTrue("Bookmark page should support E2E.", mBookmarkPage.supportsEdgeToEdge());

        // This should call SimpleEdgeToEdgePadAdjuster#mControllerChangedCallback.
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();

        padAdjuster.overrideBottomInset(100);
        ViewGroup recyclerView = mBookmarkPage.getManagerForTesting().getRecyclerViewForTesting();
        assertEquals(
                "Bottom insets should have been applied.", 100, recyclerView.getPaddingBottom());
        assertFalse(recyclerView.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals("Bottom insets should have been reset.", 0, recyclerView.getPaddingBottom());
        assertTrue(recyclerView.getClipToPadding());

        mBookmarkPage.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }
}
