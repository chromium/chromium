// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;

/** Unit tests for BookmarkManagerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
@Features.EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
public class BookmarkManagerCoordinatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    SnackbarManager mSnackbarManager;
    @Mock
    Profile mProfile;
    @Mock
    LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock
    SyncService mSyncService;
    @Mock
    IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    SigninManager mSigninManager;
    @Mock
    AccountManagerFacade mAccountManagerFacade;
    @Mock
    IdentityManager mIdentityManager;
    @Mock
    BookmarkModel mBookmarkModel;
    @Mock
    BookmarkUiPrefs mBookmarkUiPrefs;

    private Activity mActivity;
    private BookmarkManagerCoordinator mCoordinator;

    @Before
    public void setUp() {
        // Setup JNI mocks.
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);

        // Setup service mocks.
        SyncService.overrideForTests(mSyncService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        Mockito.doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        Mockito.doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);

        mActivityScenarioRule.getScenario().onActivity((activity) -> {
            mActivity = activity;
            mCoordinator = new BookmarkManagerCoordinator(mActivity,
                    /*openBookmarkComponentName=*/null,
                    /*isDialogUi=*/!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity),
                    /*isIncognito=*/false, mSnackbarManager, mProfile, mBookmarkUiPrefs);
            mActivity.setContentView(mCoordinator.getView());
        });
    }

    @Test
    public void testGetView() {
        View mainView = mCoordinator.getView();

        Assert.assertNotNull(mainView);
        Assert.assertNotNull(mainView.findViewById(R.id.selectable_list));
        Assert.assertNotNull(mainView.findViewById(R.id.action_bar));
    }

    @Test
    public void testCreateView() {
        FrameLayout parent = new FrameLayout(mActivity);
        Assert.assertNotNull(mCoordinator.buildPersonalizedPromoView(parent));
        Assert.assertNotNull(mCoordinator.buildLegacyPromoView(parent));
        Assert.assertNotNull(BookmarkManagerCoordinator.buildSectionHeaderView(parent));
        Assert.assertNotNull(mCoordinator.buildAndInitBookmarkFolderView(parent));
        Assert.assertNotNull(mCoordinator.buildAndInitBookmarkItemRow(parent));
        Assert.assertNotNull(mCoordinator.buildAndInitShoppingItemView(parent));
        Assert.assertNotNull(BookmarkManagerCoordinator.buildDividerView(parent));
        Assert.assertNotNull(BookmarkManagerCoordinator.buildShoppingFilterView(parent));
    }
}