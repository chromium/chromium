// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
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
@EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
public class BookmarkManagerCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock private SyncService mSyncService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSigninManager;
    @Mock private AccountManagerFacade mAccountManagerFacade;
    @Mock private IdentityManager mIdentityManager;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private ShoppingService mShoppingService;

    private Activity mActivity;
    private BookmarkManagerCoordinator mCoordinator;

    @Before
    public void setUp() {
        // Setup JNI mocks.
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);

        // Setup service mocks.
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        Mockito.doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        Mockito.doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);

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
                                            /* isIncognito= */ false,
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
        assertNotNull(BookmarkManagerCoordinator.buildSectionHeaderView(parent));
        assertNotNull(mCoordinator.buildAndInitBookmarkFolderView(parent));
        assertNotNull(mCoordinator.buildAndInitBookmarkItemRow(parent));
        assertNotNull(mCoordinator.buildAndInitShoppingItemView(parent));
        assertNotNull(BookmarkManagerCoordinator.buildDividerView(parent));
        assertNotNull(BookmarkManagerCoordinator.buildShoppingFilterView(parent));
        assertNotNull(mCoordinator.buildCompactImprovedBookmarkRow(parent));
        assertNotNull(mCoordinator.buildVisualImprovedBookmarkRow(parent));
        assertNotNull(mCoordinator.buildSearchBoxRow(parent));
    }
}
