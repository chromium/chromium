// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BookmarkManagerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
@EnableFeatures({
    SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
    ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS,
    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
})
// TODO(crbug.com/327387704): Add tests with this flag enabled.
@Features.DisableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
public class BookmarkManagerCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock private ImageServiceBridge.Natives mImageServiceBridgeJni;
    @Mock private SyncService mSyncService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSigninManager;
    @Mock private AccountManagerFacade mAccountManagerFacade;
    @Mock private IdentityManager mIdentityManager;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingService mShoppingService;

    private Activity mActivity;
    private BookmarkManagerCoordinator mCoordinator;

    @Before
    public void setUp() {
        // Setup JNI mocks.
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);
        mJniMocker.mock(ImageServiceBridgeJni.TEST_HOOKS, mImageServiceBridgeJni);
        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);

        // Setup service mocks.
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);

        // Setup bookmark model.
        doReturn(true).when(mBookmarkModel).areAccountBookmarkFoldersActive();

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mCoordinator =
                                    new BookmarkManagerCoordinator(
                                            mActivity,
                                            /* openBookmarkComponentName= */ null,
                                            /* isDialogUi= */ !DeviceFormFactor
                                                    .isNonMultiDisplayContextOnTablet(mActivity),
                                            mSnackbarManager,
                                            mProfile,
                                            mBookmarkUiPrefs);
                            mActivity.setContentView(mCoordinator.getView());
                        });
    }

    @Test
    public void testGetView() {
        View mainView = mCoordinator.getView();

        assertNotNull(mainView);
        assertNotNull(mainView.findViewById(R.id.selectable_list));
        assertNotNull(mainView.findViewById(R.id.action_bar));
    }

    @Test
    public void testCreateView() {
        FrameLayout parent = new FrameLayout(mActivity);
        assertNotNull(mCoordinator.buildPersonalizedPromoView(parent));
        assertNotNull(mCoordinator.buildLegacyPromoView(parent));
        assertNotNull(mCoordinator.buildSectionHeaderView(parent));
        assertNotNull(BookmarkManagerCoordinator.buildDividerView(parent));
        assertNotNull(BookmarkManagerCoordinator.buildCompactImprovedBookmarkRow(parent));
        assertNotNull(BookmarkManagerCoordinator.buildVisualImprovedBookmarkRow(parent));
        assertNotNull(mCoordinator.buildSearchBoxRow(parent));
    }
}
