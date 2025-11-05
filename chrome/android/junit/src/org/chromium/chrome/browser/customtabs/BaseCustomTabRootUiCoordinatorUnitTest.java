// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.text.format.DateUtils;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** JUnit tests for BaseCustomTabRootUiCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public final class BaseCustomTabRootUiCoordinatorUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private ActivityTabProvider mTabProvider;
    @Mock private CustomTabActivityTabProvider mCustomTabProvider;
    @Mock private ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    @Mock private ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    @Mock private ObservableSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    @Mock private BrowserControlsManager mBrowserControlsManager;

    @Mock
    private BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate;

    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private OneshotSupplier<ChromeAndroidTask> mChromeAndroidTask;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ObservableSupplier<LayoutManagerImpl> mLayoutManagerSupplier;
    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private Supplier<Integer> mActivityThemeColorSupplier;
    @Mock private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private AppMenuBlocker mAppMenuBlocker;
    @Mock private BooleanSupplier mSupportsAppMenuSupplier;
    @Mock private BooleanSupplier mSupportsFindInPage;
    @Mock private Supplier<TabCreatorManager> mTabCreatorManagerSupplier;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ObservableSupplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private Supplier<SnackbarManager> mSnackbarManagerSupplier;
    @Mock private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    @Mock private Supplier<Boolean> mIsInOverviewModeSupplier;
    @Mock private AppMenuDelegate mAppMenuDelegate;
    @Mock private StatusBarColorProvider mStatusBarColorProvider;
    @Mock private IntentRequestTracker mIntentRequestTracker;
    @Mock private Supplier<CustomTabToolbarCoordinator> mCustomTabToolbarCoordinator;
    @Mock private Supplier<BrowserServicesIntentDataProvider> mIntentDataProvider;
    @Mock private BrowserServicesIntentDataProvider mBrowserServicesIntentDataProvider;
    @Mock private BackPressManager mBackPressManager;
    @Mock private Supplier<CustomTabActivityTabController> mTabController;
    @Mock private Supplier<CustomTabMinimizeDelegate> mMinimizeDelegateSupplier;
    @Mock private Profile mProfile;
    @Mock private GoogleBottomBarCoordinator mGoogleBottomBarCoordinator;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private EdgeToEdgeManager mEdgeToEdgeManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Supplier<BrowserServicesThemeColorProvider> mBrowserServicesColorProviderSupplier;

    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private AppCompatActivity mActivity;
    private BaseCustomTabRootUiCoordinator mBaseCustomTabRootUiCoordinator;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        // Setup the shopping service.
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mIntentDataProvider.get()).thenReturn(mBrowserServicesIntentDataProvider);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserStateBrowserControlsVisibilityDelegate);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        when(mFullscreenManager.getPersistentFullscreenModeSupplier())
                .thenReturn(new ObservableSupplierImpl<Boolean>(false));

        mProfileSupplier = new ObservableSupplierImpl();

        mBaseCustomTabRootUiCoordinator =
                new BaseCustomTabRootUiCoordinator(
                        mActivity,
                        mShareDelegateSupplier,
                        mTabProvider,
                        mCustomTabProvider,
                        mProfileSupplier,
                        mBookmarkModelSupplier,
                        mTabBookmarkerSupplier,
                        mTabModelSelectorSupplier,
                        mBrowserControlsManager,
                        mWindowAndroid,
                        mChromeAndroidTask,
                        mActivityLifecycleDispatcher,
                        mLayoutManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mActivityThemeColorSupplier,
                        mModalDialogManagerSupplier,
                        mAppMenuBlocker,
                        mSupportsAppMenuSupplier,
                        mSupportsFindInPage,
                        mTabCreatorManagerSupplier,
                        mFullscreenManager,
                        mCompositorViewHolderSupplier,
                        mTabContentManagerSupplier,
                        mSnackbarManagerSupplier,
                        mEdgeToEdgeControllerSupplier,
                        ActivityType.CUSTOM_TAB,
                        mIsInOverviewModeSupplier,
                        mAppMenuDelegate,
                        mStatusBarColorProvider,
                        mEphemeralTabCoordinatorSupplier,
                        mIntentRequestTracker,
                        mCustomTabToolbarCoordinator,
                        mIntentDataProvider,
                        mBackPressManager,
                        mTabController,
                        mMinimizeDelegateSupplier,
                        CallbackUtils.emptyRunnable(),
                        mEdgeToEdgeManager,
                        mDesktopWindowStateManager,
                        mBrowserServicesColorProviderSupplier,
                        null) {

                    @Nullable
                    @Override
                    public GoogleBottomBarCoordinator getGoogleBottomBarCoordinator() {
                        return mGoogleBottomBarCoordinator;
                    }
                };
    }

    @After
    public void tearDown() {
        mFakeTimeTestRule.resetTimes();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR)
    public void testGoogleBottomBarEnabled_cctGoogleBottomBarTrue() throws Exception {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        assertTrue(
                "Google Bottom Bar should be enabled",
                BaseCustomTabRootUiCoordinator.isGoogleBottomBarEnabled(null));

        // The method should return false if any one of the conditions is not met .

        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(false);
        assertFalse(
                "Google Bottom Bar should be disabled",
                BaseCustomTabRootUiCoordinator.isGoogleBottomBarEnabled(null));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR)
    public void testGoogleBottomBarEnabled_cctGoogleBottomBarFalse() throws Exception {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);

        // The method returns false if the flag is set to false

        assertFalse(
                "Google Bottom Bar should be disabled",
                BaseCustomTabRootUiCoordinator.isGoogleBottomBarEnabled(null));
    }

    @Test
    @MediumTest
    public void testInitProfileDependantFeatures_callsInitDefaultSearchEngine() {
        mBaseCustomTabRootUiCoordinator.initProfileDependentFeatures(mProfile);

        verify(mGoogleBottomBarCoordinator).initDefaultSearchEngine(mProfile);
    }

    @Test
    @Config(sdk = 30)
    public void testEdgeToEdgeForMediaViewer() {
        doReturn(true)
                .when(mBrowserServicesIntentDataProvider)
                .shouldEnableEmbeddedMediaExperience();
        assertTrue(mBaseCustomTabRootUiCoordinator.supportsEdgeToEdge());
    }

    @Test
    @Config(sdk = 30)
    public void testEdgeToEdgeForMediaViewer_NotMediaViewer() {
        doReturn(false)
                .when(mBrowserServicesIntentDataProvider)
                .shouldEnableEmbeddedMediaExperience();
        assertFalse(
                "Not drawing E2E when not in media viewer.",
                mBaseCustomTabRootUiCoordinator.supportsEdgeToEdge());
    }

    @Test
    public void testCreateMismatchNotificationChecker() {
        HistogramWatcher freCompletedRecentlyWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.CctAccountMismatchNoticeSuppressed",
                        MismatchNotificationController.SuppressedReason.FRE_COMPLETED_RECENTLY);
        HistogramWatcher mismatchNoticeSuppressedWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.CctAccountMismatchNoticeSuppressed",
                                MismatchNotificationController.SuppressedReason
                                        .FRE_COMPLETED_RECENTLY,
                                MismatchNotificationController.SuppressedReason
                                        .CCT_IS_OFF_THE_RECORD)
                        .build();
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.isAppForAccountMismatchNotification(any())).thenReturn(true);
        mProfileSupplier.set(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        assertNotNull(
                "Should create a checker",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker("app-id"));

        // Not the right app
        when(connection.isAppForAccountMismatchNotification(any())).thenReturn(false);
        assertNull(
                "Should NOT create a checker for a wrong app",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker("app-id"));
        when(connection.isAppForAccountMismatchNotification(any())).thenReturn(true);

        // Nulled-out app ID
        assertNull(
                "Should NOT create checker for no app ID",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker(null));

        // FRE was recently completed
        SigninPreferencesManager.getInstance()
                .setCctMismatchNoticeSuppressionPeriodStart(TimeUtils.currentTimeMillis());
        assertNull(
                "Should NOT create checker when the FRE was recently completed",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker("app-id"));
        freCompletedRecentlyWatcher.assertExpected();

        // Advance the clock so that the suppression period start is no longer recent.
        mFakeTimeTestRule.advanceMillis(DateUtils.WEEK_IN_MILLIS * 10);
        assertNotNull(
                "Should create a checker",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker("app-id"));
        Assert.assertEquals(
                "The pref saving the FRE completion time should have been cleared",
                0L,
                SigninPreferencesManager.getInstance()
                        .getCctMismatchNoticeSuppressionPeriodStart());

        // Off the record profile
        when(mProfile.isOffTheRecord()).thenReturn(true);
        assertNull(
                "Should NOT create checker for an OTR session",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker("app-id"));
        mismatchNoticeSuppressedWatcher.assertExpected();

        // No profile
        mProfileSupplier.set(null);
        assertNull(
                "Should NOT create checker for no profile",
                mBaseCustomTabRootUiCoordinator.createMismatchNotificationChecker("app-id"));
    }
}
