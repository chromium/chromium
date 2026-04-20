// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;

import android.graphics.Color;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

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
    private TabBottomSheetManager mManager;

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
                    mCoBrowseViews =
                            new CoBrowseViews(
                                    mActivity,
                                    TabBottomSheetClientType.UNKNOWN,
                                    null,
                                    null,
                                    Color.WHITE);
                    mManager = tabbedRootUiCoordinator.getTabBottomSheetManagerForTesting();
                });
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mManager.tryToCloseBottomSheet(/* animate= */ true));
        }
    }

    @Test
    @SmallTest
    public void testTryToShowBottomSheet_Success_NativeInterfaceDelegateRegistered() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ true,
                            /* startsExpanded= */ true);
                });
        assertEquals(mManager.getNativeInterfaceDelegateForTesting(), mDelegate);
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
                                        TabBottomSheetClientType.UNKNOWN));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.getNavigationController().loadUrl(new LoadUrlParams(url));

                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            coBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

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
                                        TabBottomSheetClientType.UNKNOWN));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.getNavigationController().loadUrl(new LoadUrlParams(url));

                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            coBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });
        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());

        // Open tab switcher
        RegularTabSwitcherStation tabSwitcher = mInitialStation.openRegularTabSwitcher();

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Close tab switcher
        tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());

        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testBottomSheetHiddenOnToolbarSwipe() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });
        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());

        // Trigger toolbar swipe layout
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.TOOLBAR_SWIPE, false));

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Restore to browsing layout
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.BROWSING, false));

        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
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
                    mManager.setReadAloudActivePlaybackTabSupplierForTesting(readAloudTabSupplier);
                    mManager.tryToShowBottomSheet(
                            mDelegate,
                            mCoBrowseViews,
                            /* animate= */ false,
                            /* startsExpanded= */ true);
                });
        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());

        // Fake start read aloud
        ThreadUtils.runOnUiThreadBlocking(
                () -> readAloudTabSupplier.set(mActivity.getActivityTab()));

        CriteriaHelper.pollUiThread(() -> !mManager.isSheetShowing());

        // Stop read aloud
        ThreadUtils.runOnUiThreadBlocking(() -> readAloudTabSupplier.set(null));

        CriteriaHelper.pollUiThread(() -> mManager.isSheetShowing());
    }
}
