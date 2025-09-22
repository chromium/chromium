// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProviderJni;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.TestActivity;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link BookmarkPane}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.BOOKMARK_PANE_ANDROID,
})
@DisableFeatures({
    ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP,
    ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES
})
public class BookmarkPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private BookmarkBridge.Natives mBookmarkBridgeNatives;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryNatives;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private IdentityServicesProvider.Natives mIdentityServicesProvider;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private ImageServiceBridge.Natives mImageServiceBridgeNatives;
    @Mock private FaviconHelper.Natives mFaviconHelperNatives;
    @Mock private SyncService mSyncService;

    private BookmarkPane mBookmarkPane;

    @Before
    public void setUp() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileProviderSupplier.set(mProfileProvider);
        when(mBookmarkBridgeNatives.nativeGetForProfile(any())).thenReturn(mBookmarkModel);
        BookmarkBridgeJni.setInstanceForTesting(mBookmarkBridgeNatives);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryNatives);
        when(mIdentityServicesProvider.getSigninManager(any())).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        IdentityServicesProviderJni.setInstanceForTesting(mIdentityServicesProvider);
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeNatives);
        when(mFaviconHelperNatives.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperNatives);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        mBookmarkPane =
                new BookmarkPane(
                        mOnToolbarAlphaChange,
                        Robolectric.buildActivity(TestActivity.class).setup().get(),
                        mSnackbarManager,
                        mProfileProviderSupplier);
    }

    @Test
    public void testNotifyLoadHint() {
        assertEquals(0, mBookmarkPane.getRootView().getChildCount());

        mBookmarkPane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mBookmarkPane.getRootView().getChildCount());

        mBookmarkPane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, mBookmarkPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        mBookmarkPane.notifyLoadHint(LoadHint.HOT);
        mBookmarkPane.destroy();
        assertEquals(0, mBookmarkPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        mBookmarkPane.notifyLoadHint(LoadHint.HOT);
        mBookmarkPane.notifyLoadHint(LoadHint.COLD);
        mBookmarkPane.destroy();
        assertEquals(0, mBookmarkPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        mBookmarkPane.destroy();
        assertEquals(0, mBookmarkPane.getRootView().getChildCount());
    }
}
