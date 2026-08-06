// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;
import java.util.Collection;

/**
 * Unit tests for {@link BookmarkManagerCoordinator}.
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
@Features.EnableFeatures({
    ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES,
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN
})
@Features.DisableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
public class BookmarkManagerCoordinatorTest {

    @Rule(order = Rule.DEFAULT_ORDER - 1)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameters(name = "{index}_isIdentityMgr={0}")
    public static Collection parameters() {
        return Arrays.asList(false, true);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private Profile mProfile;
    @Mock private FaviconHelperJni mFaviconHelperJni;
    @Mock private ImageServiceBridge.Natives mImageServiceBridgeJni;
    @Mock private SyncService mSyncService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private ReauthenticatorBridge mReauthenticatorMock;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private PriceDropNotificationManager mPriceDropNotificationManager;

    private Activity mActivity;
    private BookmarkManagerCoordinator mCoordinator;
    private final boolean mIsIdentityManagerSourceOfAccounts;

    public BookmarkManagerCoordinatorTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
        // Setup JNI mocks.
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        doReturn(1L).when(mFaviconHelperJni).init();
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeJni);
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);

        // Setup service mocks.
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);

        // Setup bookmark model.
        doReturn(true).when(mBookmarkModel).areAccountBookmarkFoldersActive();

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mCoordinator =
                                    new BookmarkManagerCoordinator(
                                            mWindowAndroid,
                                            mActivity,
                                            /* isDialogUi= */ !DeviceFormFactor
                                                    .isNonMultiDisplayContextOnTablet(mActivity),
                                            mSnackbarManager,
                                            () -> mBottomSheetController,
                                            mActivityResultTracker,
                                            mProfile,
                                            mBookmarkUiPrefs,
                                            mBookmarkOpener,
                                            mBookmarkManagerOpener,
                                            mPriceDropNotificationManager,
                                            /* edgeToEdgePadAdjusterGenerator= */ null,
                                            /* backPressManager= */ null);
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
        assertNotNull(mCoordinator.buildBatchUploadCardView(parent));
        assertNotNull(mCoordinator.buildSectionHeaderView(parent));
        assertNotNull(BookmarkManagerCoordinator.buildDividerView(parent));
        assertNotNull(BookmarkManagerCoordinator.buildCompactImprovedBookmarkRow(parent));
        assertNotNull(BookmarkManagerCoordinator.buildVisualImprovedBookmarkRow(parent));
        assertNotNull(mCoordinator.buildSearchBoxRow(parent));
    }

    @Test
    public void testInvokeBackActionOnEscapeIsTrue() {
        assertFalse(
                "Back action should not be invoked on escape, but on non-tablet devices, the code"
                        + " flow will end up going through the back action flow.",
                mCoordinator.invokeBackActionOnEscape());
    }

    @Test
    public void testBuildEmptyStateView() {
        int parentWidth = 500;
        int parentHeight = 1000;
        int topOffset = 120;
        int paddingBottom = 80;
        int targetHeight = parentHeight - topOffset - paddingBottom;

        FrameLayout parent = new FrameLayout(mActivity);
        parent.setPadding(0, 0, 0, paddingBottom);
        parent.layout(0, 0, parentWidth, parentHeight);

        View emptyStateView = mCoordinator.buildEmptyStateView(parent);
        assertNotNull(emptyStateView);
        emptyStateView.layout(0, topOffset, parentWidth, parentHeight - paddingBottom);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(targetHeight, emptyStateView.getLayoutParams().height);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
    public void testDesktopLayoutEnabled() {
        mCoordinator.onDestroyed();
        DeviceInfo.setIsDesktopForTesting(true);
        BookmarkManagerCoordinator desktopCoordinator =
                new BookmarkManagerCoordinator(
                        mWindowAndroid,
                        mActivity,
                        /* isDialogUi= */ false,
                        mSnackbarManager,
                        () -> mBottomSheetController,
                        mActivityResultTracker,
                        mProfile,
                        mBookmarkUiPrefs,
                        mBookmarkOpener,
                        mBookmarkManagerOpener,
                        mPriceDropNotificationManager,
                        /* edgeToEdgePadAdjusterGenerator= */ null,
                        /* backPressManager= */ null);

        assertNotNull(desktopCoordinator.getView());
        assertNotNull(desktopCoordinator.getView().findViewById(R.id.navigation_pane));
        desktopCoordinator.onDestroyed();
    }

    private void recreateCoordinatorForDesktop() {
        mCoordinator.onDestroyed();
        DeviceInfo.setIsDesktopForTesting(true);
        mCoordinator =
                new BookmarkManagerCoordinator(
                        mWindowAndroid,
                        mActivity,
                        /* isDialogUi= */ false,
                        mSnackbarManager,
                        () -> mBottomSheetController,
                        mActivityResultTracker,
                        mProfile,
                        mBookmarkUiPrefs,
                        mBookmarkOpener,
                        mBookmarkManagerOpener,
                        mPriceDropNotificationManager,
                        /* edgeToEdgePadAdjusterGenerator= */ null,
                        /* backPressManager= */ null);
        mActivity.setContentView(mCoordinator.getView());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    private void assertPaddingDp(int expectedDp) {
        RecyclerView recyclerView = mCoordinator.getRecyclerViewForTesting();
        float density = mActivity.getResources().getDisplayMetrics().density;
        int expectedPx = Math.round(expectedDp * density);
        assertEquals("Padding start mismatch", expectedPx, recyclerView.getPaddingStart());
        assertEquals("Padding end mismatch", expectedPx, recyclerView.getPaddingEnd());

        View searchBoxView = mCoordinator.getView().findViewById(R.id.desktop_search_box_row);
        if (searchBoxView != null) {
            int originalMarginPx =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.search_box_embedder_margin_horizontal);
            ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) searchBoxView.getLayoutParams();
            assertEquals(
                    "Search box margin start mismatch",
                    expectedPx + originalMarginPx,
                    params.getMarginStart());
            assertEquals(
                    "Search box margin end mismatch",
                    expectedPx + originalMarginPx,
                    params.getMarginEnd());
        }
    }

    @Test
    @Config(qualifiers = "w700dp-h1000dp")
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
    public void testDesktopPadding_small() {
        recreateCoordinatorForDesktop();
        assertPaddingDp(24);
    }

    @Test
    @Config(qualifiers = "w800dp-h1000dp")
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
    public void testDesktopPadding_medium() {
        recreateCoordinatorForDesktop();
        assertPaddingDp(48);
    }

    @Test
    @Config(qualifiers = "w1000dp-h1000dp")
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
    public void testDesktopPadding_large() {
        recreateCoordinatorForDesktop();
        assertPaddingDp(72);
    }

    @Test
    @Config(qualifiers = "w1200dp-h1000dp")
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
    public void testDesktopPadding_extraLarge() {
        recreateCoordinatorForDesktop();
        assertPaddingDp(90);
    }

    @Test
    @Config(qualifiers = "w800dp-h1000dp")
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT})
    public void testDesktopPadding_resize() {
        recreateCoordinatorForDesktop();

        RecyclerView recyclerView = mCoordinator.getRecyclerViewForTesting();
        recyclerView.setVisibility(View.VISIBLE);

        // Force initial layout with 800x1000
        mCoordinator
                .getView()
                .measure(
                        View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mCoordinator.getView().layout(0, 0, 800, 1000);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertPaddingDp(48); // Initial padding for w800dp

        // Change screen width to 1200dp using Robolectric helper
        RuntimeEnvironment.setQualifiers("w1200dp-h1000dp");

        // Manually trigger the callback since Robolectric doesn't auto-dispatch it for
        // ComponentCallbacks
        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(null);

        // The padding should update immediately
        assertPaddingDp(90);

        // Force a layout pass with the new size to ensure layout works with new padding
        mCoordinator
                .getView()
                .measure(
                        View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mCoordinator.getView().layout(0, 0, 1200, 1000);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertPaddingDp(90); // Should remain 90dp
    }
}
