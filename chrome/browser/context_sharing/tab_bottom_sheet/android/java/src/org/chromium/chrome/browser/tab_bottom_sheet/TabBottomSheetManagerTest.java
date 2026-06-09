// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.EmptyManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager.NativeInterfaceDelegate;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Instrumentation tests for {@link TabBottomSheetManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET)
public class TabBottomSheetManagerTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mInitialStation;
    private final NativeInterfaceDelegate mDelegate =
            new NativeInterfaceDelegate() {
                @Override
                public void onBottomSheetClosed() {}

                @Override
                public void onBottomSheetOpened(boolean isExpanded) {}

                @Override
                public void onBottomSheetSuppressed() {}
            };

    private CoBrowseViews mCoBrowseViews;
    private ChromeTabbedActivity mActivity;
    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private TabBottomSheetManagerImpl mManager;

    @Before
    public void setUp() throws InterruptedException {
        ModalDialogView.disableButtonTapProtectionForTesting();
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());

        mInitialStation = mActivityTestRule.startOnBlankPage();

        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid = mActivity.getWindowAndroid();
                    TabbedRootUiCoordinator tabbedRootUiCoordinator =
                            (TabbedRootUiCoordinator) mActivity.getRootUiCoordinatorForTesting();
                    mBottomSheetController = tabbedRootUiCoordinator.getBottomSheetController();
                    var compositorViewHolder = mActivity.getCompositorViewHolderSupplier().get();
                    View rootView =
                            LayoutInflater.from(mActivity).inflate(R.layout.tab_bottom_sheet, null);
                    mCoBrowseViews =
                            new CoBrowseViews(
                                    rootView,
                                    TabBottomSheetClientType.UNKNOWN,
                                    CoBrowseContainerType.BOTTOM_SHEET,
                                    null,
                                    null,
                                    Color.WHITE,
                                    new TestTabBottomSheetComponentProvider());
                    mManager =
                            (TabBottomSheetManagerImpl)
                                    tabbedRootUiCoordinator.getTabBottomSheetManagerForTesting();
                });
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mManager.tryToCloseBottomSheet(/* animate= */ true));
        }
    }

    private void showBottomSheetAndBlockUntilReady() {
        showBottomSheetAndBlockUntilReady(
                mDelegate, /* animate= */ false, /* startsExpanded= */ true);
    }

    private void showBottomSheetAndBlockUntilReady(NativeInterfaceDelegate delegate) {
        showBottomSheetAndBlockUntilReady(
                delegate, /* animate= */ false, /* startsExpanded= */ true);
    }

    private void showBottomSheetAndBlockUntilReady(
            NativeInterfaceDelegate delegate, boolean animate, boolean startsExpanded) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            delegate, mCoBrowseViews, animate, startsExpanded);
                });
        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
        CriteriaHelper.pollUiThread(() -> mCoBrowseViews.getView().isAttachedToWindow());
        ThreadUtils.runOnUiThreadBlocking(() -> {});
    }

    private void blockUntilSheetFullyRestored() {
        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
        CriteriaHelper.pollUiThread(() -> mCoBrowseViews.getView().isAttachedToWindow());
        ThreadUtils.runOnUiThreadBlocking(() -> {});
    }

    @Test
    @SmallTest
    public void testTryToShowBottomSheet_Success_NativeInterfaceDelegateRegistered() {
        showBottomSheetAndBlockUntilReady();
        assertEquals(mManager.getNativeInterfaceDelegateForTesting(), mDelegate);
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked") // mock(OneshotSupplier.class) returns raw OneshotSupplier.
    public void testTryToShowBottomSheet_Failed_HideContentCalled() {
        BottomSheetController mockBottomSheetController = mock(BottomSheetController.class);
        OneshotSupplier<LayoutStateProvider> mockLayoutStateProviderSupplier =
                mock(OneshotSupplier.class);
        TouchEventProvider mockTouchEventProvider = mock(TouchEventProvider.class);

        when(mockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabBottomSheetManager oldManager =
                            TabBottomSheetUtils.getManagerFromWindow(mWindowAndroid);
                    TabBottomSheetManagerImpl manager =
                            new TabBottomSheetManagerImpl(
                                    mActivity,
                                    mWindowAndroid,
                                    mockBottomSheetController,
                                    mockLayoutStateProviderSupplier,
                                    mockTouchEventProvider);

                    manager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);

                    manager.destroy();

                    TabBottomSheetUtils.attachManagerToWindow(mWindowAndroid, oldManager);
                });

        verify(mockBottomSheetController).hideContent(any(), anyBoolean(), anyInt());
    }

    @Test
    @SmallTest
    public void testOpenWebPageAndEnsureKeyboardEventsWork() {
        final String data = "<html><body><input type='text' id='input_text'></body></html>";
        final String url = "data:text/html," + data;

        WebContents webContents =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                WebContentsFactory.createWebContents(
                                        mActivityTestRule.getProfile(false), false, true));
        CoBrowseViews coBrowseViews =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CoBrowseViewFactory.buildCoBrowseViews(
                                        mWindowAndroid,
                                        webContents,
                                        TabBottomSheetClientType.UNKNOWN,
                                        CoBrowseContainerType.BOTTOM_SHEET,
                                        new TestTabBottomSheetComponentProvider()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.getNavigationController().loadUrl(new LoadUrlParams(url));

                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            coBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
        CriteriaHelper.pollUiThread(() -> coBrowseViews.getView().isAttachedToWindow());
        ThreadUtils.runOnUiThreadBlocking(() -> {});

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(webContents.isLoading(), is(false)));

        CriteriaHelper.pollUiThread(
                () -> {
                    ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(webContents);
                    Criteria.checkThat(imeAdapter.isValid(), Matchers.is(true));
                });

        ThreadUtils.runOnUiThread(
                () -> {
                    coBrowseViews.destroy();
                });
    }

    @Test
    @SmallTest
    public void testBottomSheetHandlesPermissionRequest() {
        final String url =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/content/test/data/android/geolocation.html");

        WebContents webContents =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                WebContentsFactory.createWebContents(
                                        mActivityTestRule.getProfile(false), false, true));
        CoBrowseViews coBrowseViews =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CoBrowseViewFactory.buildCoBrowseViews(
                                        mWindowAndroid,
                                        webContents,
                                        TabBottomSheetClientType.UNKNOWN,
                                        CoBrowseContainerType.BOTTOM_SHEET,
                                        new TestTabBottomSheetComponentProvider()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.getNavigationController().loadUrl(new LoadUrlParams(url));

                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            coBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
        CriteriaHelper.pollUiThread(() -> coBrowseViews.getView().isAttachedToWindow());
        ThreadUtils.runOnUiThreadBlocking(() -> {});

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(webContents.isLoading(), is(false)));

        // Trigger a geolocation request with a user gesture
        WebContentsUtils.evaluateJavaScriptWithUserGesture(
                webContents, "initiate_getCurrentPosition();", null);

        // Verify the dialog is shown
        PermissionTestRule.waitForDialog(mActivityTestRule.getActivity());

        // Exit the dialog
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mActivityTestRule.getActivity());

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(webContents.getTitle(), is("deny")),
                10000,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ThreadUtils.runOnUiThread(
                () -> {
                    coBrowseViews.destroy();
                });
    }

    @Test
    @SmallTest
    public void testBottomSheetHiddenOnTabSwitcher() {
        showBottomSheetAndBlockUntilReady();

        // Open tab switcher
        RegularTabSwitcherStation tabSwitcher = mInitialStation.openRegularTabSwitcher();

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Close tab switcher
        tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());

        blockUntilSheetFullyRestored();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testBottomSheetHiddenOnToolbarSwipe() {
        showBottomSheetAndBlockUntilReady();

        // Trigger toolbar swipe layout
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.TOOLBAR_SWIPE, false));

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Restore to browsing layout
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.BROWSING, false));

        blockUntilSheetFullyRestored();
    }

    @Test
    @SmallTest
    public void testBottomSheetHiddenOnReadAloud() {
        SettableNullableObservableSupplier<Tab> readAloudTabSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            SettableNullableObservableSupplier<Tab> supplier =
                                    ObservableSuppliers.createNullable();
                            return supplier;
                        });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.initReadAloudIntegrationForTesting(readAloudTabSupplier, () -> {});
                });
        showBottomSheetAndBlockUntilReady();

        ThreadUtils.runOnUiThreadBlocking(
                () -> readAloudTabSupplier.set(mActivity.getTabModelSelector().getCurrentTab()));

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Stop read aloud
        ThreadUtils.runOnUiThreadBlocking(() -> readAloudTabSupplier.set(null));

        blockUntilSheetFullyRestored();
    }

    @Test
    @SmallTest
    public void testReadAloudClosedOnBottomSheetShown() {
        SettableNullableObservableSupplier<Tab> readAloudTabSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return ObservableSuppliers.createNullable();
                        });
        Runnable mockStopPlaybackCallback = mock(Runnable.class);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.initReadAloudIntegrationForTesting(
                            readAloudTabSupplier, mockStopPlaybackCallback);
                });

        // Start read aloud
        ThreadUtils.runOnUiThreadBlocking(
                () -> readAloudTabSupplier.set(mActivity.getTabModelSelector().getCurrentTab()));

        // Call tabBottomSheet
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

        // Verify that stop playback callback was called
        verify(mockStopPlaybackCallback).run();
    }

    @Test
    @SmallTest
    public void testSheetEventsCallback_onBottomSheetOpened() {
        NativeInterfaceDelegate mockDelegate = mock(NativeInterfaceDelegate.class);
        showBottomSheetAndBlockUntilReady(
                mockDelegate, /* animate= */ false, /* startsExpanded= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.setSheetExpanded(true);
                });

        CriteriaHelper.pollUiThread(
                () -> verify(mockDelegate, atLeastOnce()).onBottomSheetOpened(true));
    }

    @Test
    @SmallTest
    public void testSheetEventsCallback_onBottomSheetClosed_NativeClose() {
        NativeInterfaceDelegate mockDelegate = mock(NativeInterfaceDelegate.class);
        showBottomSheetAndBlockUntilReady(mockDelegate);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToCloseBottomSheet(/* animate= */ false);
                });

        verify(mockDelegate).onBottomSheetClosed();
    }

    @Test
    @SmallTest
    public void testSheetEventsCallback_onBottomSheetClosed_Suppressed() {
        NativeInterfaceDelegate mockDelegate = mock(NativeInterfaceDelegate.class);
        showBottomSheetAndBlockUntilReady(mockDelegate);

        RegularTabSwitcherStation tabSwitcher = mInitialStation.openRegularTabSwitcher();

        verify(mockDelegate).onBottomSheetSuppressed();

        tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
    }

    @Test
    @SmallTest
    public void testSetPeekView_BeforeShow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                                    .build();
                    mManager.setPeekViewModel(model);
                });
        showBottomSheetAndBlockUntilReady();

        CriteriaHelper.pollUiThread(() -> mCoBrowseViews.hasPeekView());
    }

    @Test
    @SmallTest
    public void testSetPeekView_AfterShow() {
        showBottomSheetAndBlockUntilReady();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                                    .build();
                    mManager.setPeekViewModel(model);
                });

        CriteriaHelper.pollUiThread(() -> mCoBrowseViews.hasPeekView());
    }

    @Test
    @SmallTest
    public void testRemovePeekView() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                                    .build();
                    mManager.setPeekViewModel(model);
                });
        showBottomSheetAndBlockUntilReady();
        CriteriaHelper.pollUiThread(() -> mCoBrowseViews.hasPeekView());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.removePeekViewModel();
                });

        CriteriaHelper.pollUiThread(() -> !mCoBrowseViews.hasPeekView());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/510449718")
    public void testDetachNativeInterfaceDelegate() {
        showBottomSheetAndBlockUntilReady();
        assertEquals(mManager.getNativeInterfaceDelegateForTesting(), mDelegate);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.detachNativeInterfaceDelegate(mDelegate);
                });

        assertEquals(mManager.getNativeInterfaceDelegateForTesting(), null);
    }

    @Test
    @SmallTest
    public void testTryToCloseBottomSheet_WhenSuppressed() {
        NativeInterfaceDelegate mockDelegate = mock(NativeInterfaceDelegate.class);
        showBottomSheetAndBlockUntilReady(mockDelegate);

        RegularTabSwitcherStation tabSwitcher = mInitialStation.openRegularTabSwitcher();
        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToCloseBottomSheet(/* animate= */ false);
                });

        verify(mockDelegate).onBottomSheetClosed();

        tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
    }

    @Test
    @SmallTest
    public void testTryToShowBottomSheet_WhenAlreadyShowing() {
        NativeInterfaceDelegate mockDelegate1 = mock(NativeInterfaceDelegate.class);
        NativeInterfaceDelegate mockDelegate2 = mock(NativeInterfaceDelegate.class);

        showBottomSheetAndBlockUntilReady(mockDelegate1);

        CoBrowseViews coBrowseViews2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new CoBrowseViews(
                                        LayoutInflater.from(mActivity)
                                                .inflate(R.layout.tab_bottom_sheet, null),
                                        CoBrowseContainerType.BOTTOM_SHEET,
                                        TabBottomSheetClientType.UNKNOWN,
                                        null,
                                        null,
                                        Color.WHITE,
                                        new TestTabBottomSheetComponentProvider()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            mockDelegate2,
                            coBrowseViews2,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
        CriteriaHelper.pollUiThread(() -> coBrowseViews2.getView().isAttachedToWindow());
        ThreadUtils.runOnUiThreadBlocking(() -> {});

        verify(mockDelegate1).onBottomSheetClosed();
        assertEquals(mManager.getNativeInterfaceDelegateForTesting(), mockDelegate2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coBrowseViews2.destroy();
                });
    }

    @Test
    @SmallTest
    public void testTabSwitcherSuppression_OnlyOneObserverActive() {
        NativeInterfaceDelegate mockDelegate = mock(NativeInterfaceDelegate.class);
        showBottomSheetAndBlockUntilReady(mockDelegate);

        // 1. Open tab switcher (First suppression)
        RegularTabSwitcherStation tabSwitcher = mInitialStation.openRegularTabSwitcher();

        // Verify it was suppressed once
        verify(mockDelegate, times(1)).onBottomSheetSuppressed();

        // 2. Close tab switcher (Return to tab)
        WebPageStation pageStation =
                tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());

        // 3. Open tab switcher again (Second suppression)
        tabSwitcher = pageStation.openRegularTabSwitcher();

        // Verify that it was called exactly 2 times in total.
        verify(mockDelegate, times(2)).onBottomSheetSuppressed();

        // Clean up by leaving the tab switcher
        tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
    }

    @Test
    @SmallTest
    public void testBottomSheetHiddenOnIncognito() {
        showBottomSheetAndBlockUntilReady();

        // Switch to incognito tab model
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.getTabModelSelector().selectModel(true));

        // Verify it gets suppressed
        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Switch back to normal tab model
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.getTabModelSelector().selectModel(false));

        // Verify it gets restored
        blockUntilSheetFullyRestored();
    }

    @Test
    @SmallTest
    public void testBottomSheetSuppressedOnAccessoryRequested() {
        SettableNonNullObservableSupplier<Boolean> accessoryRequestedSupplier =
                ThreadUtils.runOnUiThreadBlocking(() -> ObservableSuppliers.createNonNull(false));
        ManualFillingComponent mockComponent =
                new TestManualFillingComponent(accessoryRequestedSupplier);

        SettableMonotonicObservableSupplier<ManualFillingComponent> supplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            SettableMonotonicObservableSupplier<ManualFillingComponent> s =
                                    ObservableSuppliers.createMonotonic();
                            s.set(mockComponent);
                            return s;
                        });

        // Register the mock supplier before showing the bottom sheet
        ThreadUtils.runOnUiThreadBlocking(
                () -> mManager.setManualFillingComponentSupplierForTesting(supplier));

        showBottomSheetAndBlockUntilReady();

        // Trigger accessory request / suppression
        ThreadUtils.runOnUiThreadBlocking(() -> accessoryRequestedSupplier.set(true));

        // Verify the sheet gets suppressed
        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());
    }

    @Test
    @SmallTest
    public void testLateComponentRegistrationHooksUpObserver() {
        SettableNonNullObservableSupplier<Boolean> accessoryRequestedSupplier =
                ThreadUtils.runOnUiThreadBlocking(() -> ObservableSuppliers.createNonNull(true));
        ManualFillingComponent mockComponent =
                new TestManualFillingComponent(accessoryRequestedSupplier);

        SettableMonotonicObservableSupplier<ManualFillingComponent> supplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            SettableMonotonicObservableSupplier<ManualFillingComponent> s =
                                    ObservableSuppliers.createMonotonic();
                            s.set(mockComponent);
                            return s;
                        });

        // Register the mock supplier
        ThreadUtils.runOnUiThreadBlocking(
                () -> mManager.setManualFillingComponentSupplierForTesting(supplier));

        // Try to show the bottom sheet
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

        // Verify the sheet is NOT showing (suppressed by autofill)
        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Stop accessory requested/suppression
        ThreadUtils.runOnUiThreadBlocking(() -> accessoryRequestedSupplier.set(false));

        // Verify the sheet is successfully restored
        blockUntilSheetFullyRestored();
    }

    @Test
    @SmallTest
    public void testBottomSheetClosedOnReaderModeNavigation() {
        showBottomSheetAndBlockUntilReady();

        // Navigate to a reader mode URL
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getTabModelSelector()
                            .getCurrentTab()
                            .loadUrl(new LoadUrlParams("chrome-distiller://some_distilled_page"));
                });

        // Verify the sheet is closed
        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());
    }

    @Test
    @SmallTest
    public void testBackPressClosesBottomSheet() {
        showBottomSheetAndBlockUntilReady();
        assertTrue(mManager.isSheetShowing());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetContent content = mBottomSheetController.getCurrentSheetContent();
                    assertNotNull(content);
                    boolean handled = content.handleBackPress();
                    assertTrue(handled);
                });

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());
    }

    private static class TestManualFillingComponent extends EmptyManualFillingComponent {
        private final NonNullObservableSupplier<Boolean> mAccessoryRequestedSupplier;

        TestManualFillingComponent(NonNullObservableSupplier<Boolean> supplier) {
            mAccessoryRequestedSupplier = supplier;
        }

        @Override
        public NonNullObservableSupplier<Boolean> getIsAccessoryRequestedSupplier() {
            return mAccessoryRequestedSupplier;
        }
    }
}
