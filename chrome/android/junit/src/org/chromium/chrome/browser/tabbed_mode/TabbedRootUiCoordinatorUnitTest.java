// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.CallbackUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate.ActionType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityResultWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omaha.inline.InAppUpdateController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsActionDelegate;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.loading.LoadingFullscreenCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link TabbedRootUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedRootUiCoordinatorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private BrowserControlsManager mBrowserControlsManager;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private AppMenuBlocker mAppMenuBlocker;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TopInsetProvider mTopInsetProvider;
    @Mock private AppMenuDelegate mAppMenuDelegate;
    @Mock private StatusBarColorProvider mStatusBarColorProvider;
    @Mock private IntentRequestTracker mIntentRequestTracker;
    @Mock private InsetObserver mInsetObserver;
    @Mock private BackPressManager mBackPressManager;
    @Mock private EdgeToEdgeManager mEdgeToEdgeManager;
    @Mock private VerticalTabsActionDelegate mVerticalTabsActionDelegate;
    @Mock private ScrimManager mScrimManager;
    @Mock private Profile mOriginalProfile;
    @Mock private Profile mCurrentProfile;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private InAppUpdateController mMockInAppUpdateController;
    @Mock private DeferredStartupHandler mDeferredStartupHandler;
    @Mock private Tracker mTracker;

    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;

    private AppCompatActivity mActivity;
    private TabbedRootUiCoordinator mCoordinator;

    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final AtomicBoolean mIsWarmOnResume = new AtomicBoolean(true);

    @Before
    public void setUp() {
        ActivityController<TestActivity> activityController =
                Robolectric.buildActivity(TestActivity.class);
        activityController.get().setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivity = activityController.create().get();

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(
                        new BrowserStateBrowserControlsVisibilityDelegate(
                                ObservableSuppliers.alwaysFalse()));
        when(mFullscreenManager.getPersistentFullscreenModeSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);
        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        TrackerFactory.setTrackerForTests(mTracker);

        mProfileSupplier.set(mCurrentProfile);
        mCoordinator = createCoordinator(/* bottomBarHostManager= */ null);
        mCoordinator.setScrimManagerForTesting(mScrimManager);
    }

    /**
     * The loading fullscreen coordinator is created lazily on the first call to {@link
     * TabbedRootUiCoordinator#getLoadingFullscreenCoordinator()}. Its only client, the
     * collaboration controller delegate factory, can run after the coordinator was destroyed, and
     * the coordinator must not be resurrected then because {@link RootUiCoordinator#onDestroy()}
     * clears the activity reference.
     */
    @Test
    public void testGetLoadingFullscreenCoordinatorAfterDestroy() {
        AtomicInteger inflateCount = attachLoadingStub();

        mCoordinator.onDestroy();

        assertNull(mCoordinator.getLoadingFullscreenCoordinator());
        assertEquals(0, inflateCount.get());
    }

    /**
     * Once created, {@link TabbedRootUiCoordinator#onDestroy()} tears down the loading fullscreen
     * coordinator and clears the activity so later requests withhold it rather than inflating the
     * spent view stub again.
     */
    @Test
    public void testGetLoadingFullscreenCoordinatorAfterDestroy_alreadyCreated() {
        attachLoadingStub();
        assertNotNull(mCoordinator.getLoadingFullscreenCoordinator());

        mCoordinator.onDestroy();

        assertNull(mCoordinator.getLoadingFullscreenCoordinator());
    }

    /**
     * The loading fullscreen coordinator is created at most once: the loading view stub is consumed
     * by the first inflation, so every later call must hand back the existing instance instead of
     * inflating the stub a second time.
     */
    @Test
    public void testGetLoadingFullscreenCoordinator_cachesResultAfterFirstCreation() {
        AtomicInteger inflateCount = attachLoadingStub();

        LoadingFullscreenCoordinator first = mCoordinator.getLoadingFullscreenCoordinator();
        LoadingFullscreenCoordinator second = mCoordinator.getLoadingFullscreenCoordinator();

        assertNotNull(first);
        assertSame(first, second);
        assertEquals(1, inflateCount.get());
    }

    @Test
    public void testInAppUpdate_handleMenuOrKeyboardAction_updateMenuId_delegatesToHelper() {
        when(mCurrentProfile.getOriginalProfile()).thenReturn(mOriginalProfile);
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);

        boolean handled =
                mCoordinator.handleMenuOrKeyboardAction(R.id.update_menu_id, /* fromMenu= */ true);

        assertTrue(handled);
        verify(mCurrentProfile).getOriginalProfile();
        verify(mUpdateMenuItemHelper).onMenuItemClicked(mActivity, mMockInAppUpdateController);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testInAppUpdate_initObserversAndScheduleDeferredStartup_registersObserversAndAddsDeferredTask() {
        mCoordinator.scheduleInAppUpdateDeferredStartup();

        assertNotNull(mCoordinator.getActivityResultObserverForTesting());
        assertNotNull(mCoordinator.getResumeObserverForTesting());
        verify(mActivityLifecycleDispatcher)
                .register(mCoordinator.getActivityResultObserverForTesting());
        verify(mActivityLifecycleDispatcher).register(mCoordinator.getResumeObserverForTesting());
        verify(mDeferredStartupHandler).addDeferredTask(any());

        // Verify idempotency: a second call does not queue another deferred task.
        clearInvocations(mDeferredStartupHandler);
        mCoordinator.scheduleInAppUpdateDeferredStartup();
        verify(mDeferredStartupHandler, never()).addDeferredTask(any());
    }

    @Test
    public void testInAppUpdate_activityResultObserver_forwardsResultToController() {
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);
        ActivityResultWithNativeObserver observer =
                mCoordinator.getActivityResultObserverForTesting();
        assertNotNull(observer);

        // Matching request code forwards to controller.
        observer.onActivityResultWithNative(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK, null);
        verify(mMockInAppUpdateController)
                .handleActivityResult(
                        InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK);

        // Unrelated request code does not forward.
        observer.onActivityResultWithNative(99999, Activity.RESULT_CANCELED, null);
        verify(mMockInAppUpdateController, never()).handleActivityResult(eq(99999), anyInt());
    }

    @Test
    public void testInAppUpdate_resumeObserver_resumesWarmOnly() {
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);
        PauseResumeWithNativeObserver observer = mCoordinator.getResumeObserverForTesting();
        assertNotNull(observer);

        // When cold/not warm on resume, controller.onResume() is not called.
        mIsWarmOnResume.set(false);
        observer.onResumeWithNative();
        verify(mMockInAppUpdateController, never()).onResume();

        // When warm on resume, controller.onResume() is called.
        mIsWarmOnResume.set(true);
        observer.onResumeWithNative();
        verify(mMockInAppUpdateController).onResume();
    }

    @Test
    public void testInAppUpdate_destroy_unregistersObserversAndCleansUpController() {
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);
        ActivityResultWithNativeObserver activityResultObserver =
                mCoordinator.getActivityResultObserverForTesting();
        PauseResumeWithNativeObserver resumeObserver = mCoordinator.getResumeObserverForTesting();
        assertNotNull(activityResultObserver);
        assertNotNull(resumeObserver);

        mCoordinator.onDestroy();

        verify(mActivityLifecycleDispatcher).unregister(activityResultObserver);
        verify(mActivityLifecycleDispatcher).unregister(resumeObserver);
        verify(mMockInAppUpdateController).destroy();
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
        assertNull(mCoordinator.getActivityResultObserverForTesting());
        assertNull(mCoordinator.getResumeObserverForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInAppUpdate_featureDisabled_noControllerCreated() {
        mCoordinator.scheduleInAppUpdateDeferredStartup();

        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
        verify(mDeferredStartupHandler, never()).addDeferredTask(any());

        ActivityResultWithNativeObserver observer =
                mCoordinator.getActivityResultObserverForTesting();
        assertNotNull(observer);
        observer.onActivityResultWithNative(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK, null);
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());

        PauseResumeWithNativeObserver resumeObserver = mCoordinator.getResumeObserverForTesting();
        assertNotNull(resumeObserver);
        resumeObserver.onResumeWithNative();
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInAppUpdate_deferredStartupAfterDestroy_doesNotThrowOrResurrectController() {
        mCoordinator.scheduleInAppUpdateDeferredStartup();
        verify(mDeferredStartupHandler).addDeferredTask(mRunnableCaptor.capture());

        // Simulate coordinator destruction before deferred startup task executes.
        mCoordinator.onDestroy();

        // Executing the deferred task must not throw NullPointerException or resurrect the
        // controller.
        mRunnableCaptor.getValue().run();
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testGetOrCreateInAppUpdateController_afterDestroy_returnsNull() {
        mCoordinator.onDestroy();
        assertNull(mCoordinator.getOrCreateInAppUpdateController());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testGetOrCreateInAppUpdateController_whenActivityFinishingOrDestroyed_returnsNull() {
        // Active activity creates controller.
        assertNotNull(mCoordinator.getOrCreateInAppUpdateController());

        // When finishing, returns null and does not resurrect if cleared.
        mCoordinator.setInAppUpdateControllerForTesting(null);
        mActivity.finish();
        assertNull(mCoordinator.getOrCreateInAppUpdateController());
    }

    @Test
    public void testGetBackActionTypeForNonNavigableTab() {
        // 1. When mBackButtonShouldCloseTabFn returns false (e.g. destroyed tab or confirmation
        // required), returns EXIT_APP_ONLY without querying TabAssociatedApp / UserDataHost.
        Tab blockedTab = mock(Tab.class);
        when(blockedTab.isClosing()).thenReturn(false);
        assertEquals(
                ActionType.EXIT_APP_ONLY,
                mCoordinator.getBackActionTypeForNonNavigableTab(blockedTab));
        verify(blockedTab, never()).getUserDataHost();

        // 2. Tab from launcher shortcut when mBackButtonShouldCloseTabFn returns true ->
        // EXIT_APP_AND_CLOSE_TAB.
        Tab shortcutTab = mock(Tab.class);
        when(shortcutTab.isClosing()).thenReturn(true);
        when(shortcutTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(shortcutTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LAUNCHER_SHORTCUT);
        assertEquals(
                ActionType.EXIT_APP_AND_CLOSE_TAB,
                mCoordinator.getBackActionTypeForNonNavigableTab(shortcutTab));

        // 3. Tab opened from external app when mBackButtonShouldCloseTabFn returns true ->
        // EXIT_APP_AND_CLOSE_TAB.
        Tab externalTab = mock(Tab.class);
        UserDataHost externalUserDataHost = new UserDataHost();
        when(externalTab.isClosing()).thenReturn(true);
        when(externalTab.getUserDataHost()).thenReturn(externalUserDataHost);
        when(externalTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        TabAssociatedApp.from(externalTab).setAppId("com.example.external");
        assertEquals(
                ActionType.EXIT_APP_AND_CLOSE_TAB,
                mCoordinator.getBackActionTypeForNonNavigableTab(externalTab));

        // 4. Regular closeable tab (e.g. FROM_LINK) -> CLOSE_TAB.
        Tab linkTab = mock(Tab.class);
        when(linkTab.isClosing()).thenReturn(true);
        when(linkTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(linkTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LINK);
        assertEquals(
                ActionType.CLOSE_TAB, mCoordinator.getBackActionTypeForNonNavigableTab(linkTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ADAPTIVE_UI_TRANSITIONS)
    public void testAdaptiveUiController_attachedWhileAlive() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        mCoordinator.onPreInflationStartup();
        assertNotNull(AdaptiveUiController.from(mWindowAndroid));

        mCoordinator.onDestroy();
        assertNull(AdaptiveUiController.from(mWindowAndroid));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_ADAPTIVE_UI_TRANSITIONS)
    public void testAdaptiveUiController_absentWhenFlagDisabled() {
        mCoordinator.onPreInflationStartup();
        assertNull(AdaptiveUiController.from(mWindowAndroid));

        mCoordinator.onDestroy();
    }

    private TabbedRootUiCoordinator createCoordinator(
            @Nullable BottomBarHostManager bottomBarHostManager) {
        return new TabbedRootUiCoordinator(
                mActivity,
                /* onOmniboxFocusChangedListener= */ null,
                /* shareDelegateSupplier= */ ObservableSuppliers.alwaysNull(),
                new ActivityTabProvider(),
                mProfileSupplier,
                /* bookmarkModelSupplier= */ ObservableSuppliers.alwaysNull(),
                /* tabBookmarkerSupplier= */ ObservableSuppliers.alwaysNull(),
                /* tabModelSelectorSupplier= */ ObservableSuppliers.alwaysNull(),
                /* tabSwitcherSupplier= */ new OneshotSupplierImpl<>(),
                /* incognitoTabSwitcherSupplier= */ new OneshotSupplierImpl<>(),
                /* hubManagerSupplier= */ new OneshotSupplierImpl<>(),
                /* intentMetadataOneshotSupplier= */ new OneshotSupplierImpl<>(),
                /* layoutStateProviderOneshotSupplier= */ new OneshotSupplierImpl<>(),
                mBrowserControlsManager,
                mWindowAndroid,
                mActivityResultTracker,
                /* chromeAndroidTaskSupplier= */ new OneshotSupplierImpl<>(),
                mActivityLifecycleDispatcher,
                mMultiWindowModeStateDispatcher,
                /* layoutManagerSupplier= */ ObservableSuppliers.alwaysNull(),
                mMenuOrKeyboardActionController,
                /* activityThemeColorSupplier= */ () -> Color.TRANSPARENT,
                ObservableSuppliers.createNonNull(mModalDialogManager),
                mAppMenuBlocker,
                /* supportsAppMenuSupplier= */ () -> true,
                /* tabCreatorManagerSupplier= */ ObservableSuppliers.alwaysNull(),
                mFullscreenManager,
                /* compositorViewHolderSupplier= */ ObservableSuppliers.alwaysNull(),
                /* tabContentManagerSupplier= */ () -> null,
                /* snackbarManagerSupplier= */ ObservableSuppliers.alwaysNull(),
                /* edgeToEdgeSupplier= */ ObservableSuppliers.createMonotonic(),
                mTopInsetProvider,
                /* systemBarColorHelperSupplier= */ new OneshotSupplierImpl<>(),
                ActivityType.TABBED,
                /* isInOverviewModeSupplier= */ () -> false,
                mAppMenuDelegate,
                mStatusBarColorProvider,
                /* ephemeralTabCoordinatorSupplier= */ ObservableSuppliers.createMonotonic(),
                mIntentRequestTracker,
                mInsetObserver,
                /* backButtonShouldCloseTabFn= */ Tab::isClosing,
                /* sendToBackground= */ CallbackUtils.emptyCallback(),
                /* initializeUiWithIncognitoColors= */ false,
                mBackPressManager,
                /* savedInstanceState= */ null,
                /* persistentState= */ null,
                /* multiInstanceManager= */ null,
                /* manualFillingComponentSupplier= */ ObservableSuppliers.alwaysNull(),
                mEdgeToEdgeManager,
                /* bookmarkManagerOpenerSupplier= */ ObservableSuppliers.alwaysNull(),
                /* xrSpaceModeObservableSupplier= */ ObservableSuppliers.alwaysFalse(),
                /* inactivityTrackerSupplier= */ new OneshotSupplierImpl<>(),
                bottomBarHostManager,
                mVerticalTabsActionDelegate,
                /* urlBarVisibleSupplier= */ () -> false,
                /* onTabLayoutAvailable= */ CallbackUtils.emptyRunnable(),
                mIsWarmOnResume::get);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testActionRegistry_nullHostManager_settingEnabled_noRegistry() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        mCoordinator = createCoordinator(/* bottomBarHostManager= */ null);

        assertNull(mCoordinator.getActionRegistryForTesting());
    }

    private AtomicInteger attachLoadingStub() {
        FrameLayout root = new FrameLayout(mActivity);
        ViewStub loadingStub = new ViewStub(mActivity);
        loadingStub.setId(R.id.loading_stub);
        AtomicInteger inflateCount = new AtomicInteger();
        loadingStub.setOnInflateListener((_, _) -> inflateCount.incrementAndGet());
        root.addView(loadingStub);
        mActivity.setContentView(root);
        return inflateCount;
    }
}
