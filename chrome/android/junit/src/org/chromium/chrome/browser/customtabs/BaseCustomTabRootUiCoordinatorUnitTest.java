// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
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
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;
import java.util.function.BooleanSupplier;

/** JUnit tests for BaseCustomTabRootUiCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public final class BaseCustomTabRootUiCoordinatorUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private ActivityTabProvider mTabProvider;
    @Mock private ObservableSupplier<Profile> mProfileSupplier;
    @Mock private ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    @Mock private ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private Supplier<Long> mLastUserInteractionTimeSupplier;
    @Mock private BrowserControlsManager mBrowserControlsManager;

    @Mock
    private BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate;

    @Mock private ActivityWindowAndroid mWindowAndroid;
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
    @Mock private Supplier<CustomTabActivityNavigationController> mCustomTabNavigationController;
    @Mock private Supplier<BrowserServicesIntentDataProvider> mIntentDataProvider;
    @Mock private BrowserServicesIntentDataProvider mBrowserServicesIntentDataProvider;
    @Mock private BackPressManager mBackPressManager;
    @Mock private Supplier<CustomTabActivityTabController> mTabController;
    @Mock private Supplier<CustomTabMinimizeDelegate> mMinimizeDelegateSupplier;
    @Mock private Supplier<CustomTabFeatureOverridesManager> mFeatureOverridesManagerSupplier;
    @Mock private View mBaseChromeLayout;
    @Mock private Profile mProfile;
    @Mock private GoogleBottomBarCoordinator mGoogleBottomBarCoordinator;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;

    private AppCompatActivity mActivity;
    private BaseCustomTabRootUiCoordinator mBaseCustomTabRootUiCoordinator;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);

        // Setup the shopping service.
        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        mJniMocker.mock(ShoppingServiceFactoryJni.TEST_HOOKS, mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mIntentDataProvider.get()).thenReturn(mBrowserServicesIntentDataProvider);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserStateBrowserControlsVisibilityDelegate);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mBaseCustomTabRootUiCoordinator =
                new BaseCustomTabRootUiCoordinator(
                        mActivity,
                        mShareDelegateSupplier,
                        mTabProvider,
                        mProfileSupplier,
                        mBookmarkModelSupplier,
                        mTabBookmarkerSupplier,
                        mTabModelSelectorSupplier,
                        mLastUserInteractionTimeSupplier,
                        mBrowserControlsManager,
                        mWindowAndroid,
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
                        mIntentRequestTracker,
                        mCustomTabToolbarCoordinator,
                        mCustomTabNavigationController,
                        mIntentDataProvider,
                        mBackPressManager,
                        mTabController,
                        mMinimizeDelegateSupplier,
                        mFeatureOverridesManagerSupplier,
                        mBaseChromeLayout) {

                    @Nullable
                    @Override
                    public GoogleBottomBarCoordinator getGoogleBottomBarCoordinator() {
                        return mGoogleBottomBarCoordinator;
                    }
                };
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
    @EnableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgeForMediaViewer() {
        doReturn(true)
                .when(mBrowserServicesIntentDataProvider)
                .shouldEnableEmbeddedMediaExperience();
        assertTrue(mBaseCustomTabRootUiCoordinator.supportsEdgeToEdge());
    }

    @Test
    @Config(sdk = 30)
    @DisableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgeForMediaViewer_DisabledFeatures() {
        doReturn(true)
                .when(mBrowserServicesIntentDataProvider)
                .shouldEnableEmbeddedMediaExperience();
        assertFalse(
                "Not drawing E2E when feature disabled.",
                mBaseCustomTabRootUiCoordinator.supportsEdgeToEdge());
    }

    @Test
    @Config(sdk = 30)
    @EnableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgeForMediaViewer_NotMediaViewer() {
        doReturn(false)
                .when(mBrowserServicesIntentDataProvider)
                .shouldEnableEmbeddedMediaExperience();
        assertFalse(
                "Not drawing E2E when not in media viewer.",
                mBaseCustomTabRootUiCoordinator.supportsEdgeToEdge());
    }
}
