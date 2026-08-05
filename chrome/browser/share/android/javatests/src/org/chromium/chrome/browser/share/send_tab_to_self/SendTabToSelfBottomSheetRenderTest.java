// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Render tests for the send-tab-to-self bottom sheets. */
@DoNotBatch(reason = "Night mode requires clean activity launch.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class SendTabToSelfBottomSheetRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("Default"),
                    new ParameterSet().value(true).name("NightMode"));

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_SHARING)
                    .setRevision(5)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private BottomSheetController mBottomSheetController;

    public SendTabToSelfBottomSheetRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private View createAndShowEnhancedDevicePickerView(
            List<TargetDeviceInfo> devices, @BottomSheetController.SheetState int sheetState) {
        Activity activity = mActivityTestRule.getActivity();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    EnhancedTargetDevicePickerView viewContent =
                            new EnhancedTargetDevicePickerView(activity, mBottomSheetController);
                    PropertyModel model = EnhancedTargetDevicePickerProperties.createDefaultModel();
                    model.set(EnhancedTargetDevicePickerProperties.DISMISS_CALLBACK, reason -> {});
                    new EnhancedTargetDevicePickerMediator(
                            JUnitTestGURLs.HTTP_URL.getSpec(),
                            "Title",
                            devices,
                            mProfile,
                            () -> null,
                            model,
                            ShareEntryPoint.SHARE_SHEET);
                    PropertyModelChangeProcessor.create(
                            model, viewContent, EnhancedTargetDevicePickerViewBinder::bind);
                    model.set(EnhancedTargetDevicePickerProperties.VISIBLE, true);
                    when(mBottomSheetController.getCurrentSheetContent()).thenReturn(viewContent);
                    when(mBottomSheetController.getSheetState()).thenReturn(sheetState);
                    when(mBottomSheetController.getContainerHeight())
                            .thenReturn(activity.getResources().getDisplayMetrics().heightPixels);
                    // Capture and trigger observer to simulate sheet state transition.
                    ArgumentCaptor<BottomSheetObserver> observerCaptor =
                            ArgumentCaptor.forClass(BottomSheetObserver.class);
                    verify(mBottomSheetController).addObserver(observerCaptor.capture());
                    observerCaptor
                            .getValue()
                            .onSheetStateChanged(
                                    sheetState, BottomSheetController.StateChangeReason.NONE);
                    activity.setContentView(viewContent.getContentView());
                    return viewContent.getContentView();
                });
    }

    /** Set up account data to be shown by the UI. */
    private void setUpAccountData(AccountInfo account) {
        // Set up account data to be shown by the UI.
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(account);
        when(mIdentityManager.findExtendedAccountInfoByAccountId(account.getId()))
                .thenReturn(account);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testDevicePickerBottomSheet() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, "Active today"),
                        new TargetDeviceInfo(
                                "My Computer", "guid2", FormFactor.DESKTOP, "Active 1 day ago"),
                        new TargetDeviceInfo(
                                "My Tablet", "guid3", FormFactor.TABLET, "Active 2 days ago"));
        Activity activity = mActivityTestRule.getActivity();
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            DevicePickerBottomSheetContent sheetContent =
                                    new DevicePickerBottomSheetContent(
                                            activity,
                                            JUnitTestGURLs.HTTP_URL.getSpec(),
                                            "Title",
                                            mBottomSheetController,
                                            devices,
                                            mProfile,
                                            () -> null,
                                            ShareEntryPoint.SHARE_SHEET);
                            activity.setContentView(sheetContent.getContentView());
                            return sheetContent.getContentView();
                        });
        mRenderTestRule.render(view, "device_picker");
    }

    @Test
    @MediumTest
    public void testDevicePickerBottomSheetWithNonDisplayableAccountEmail() throws Throwable {
        AccountInfo account = TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL;
        setUpAccountData(account);
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, "Active today"),
                        new TargetDeviceInfo(
                                "My Computer", "guid2", FormFactor.DESKTOP, "Active 1 day ago"),
                        new TargetDeviceInfo(
                                "My Tablet", "guid3", FormFactor.TABLET, "Active 2 days ago"));
        Activity activity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DevicePickerBottomSheetContent sheetContent =
                            new DevicePickerBottomSheetContent(
                                    activity,
                                    JUnitTestGURLs.HTTP_URL.getSpec(),
                                    "Title",
                                    mBottomSheetController,
                                    devices,
                                    mProfile,
                                    () -> null,
                                    ShareEntryPoint.SHARE_SHEET);
                    activity.setContentView(sheetContent.getContentView());
                });
        onView(withText(account.getEmail())).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoTargetDeviceBottomSheet() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        Activity activity = mActivityTestRule.getActivity();
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            NoTargetDeviceBottomSheetContent sheetContent =
                                    new NoTargetDeviceBottomSheetContent(activity, mProfile);
                            activity.setContentView(sheetContent.getContentView());
                            return sheetContent.getContentView();
                        });
        mRenderTestRule.render(view, "no_target_device_with_account");
    }

    @Test
    @MediumTest
    public void testNoTargetDeviceBottomSheetWithNonDisplayableAccountEmail() throws Throwable {
        AccountInfo account = TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL;
        setUpAccountData(account);
        Activity activity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NoTargetDeviceBottomSheetContent sheetContent =
                            new NoTargetDeviceBottomSheetContent(activity, mProfile);
                    activity.setContentView(sheetContent.getContentView());
                });
        onView(withText(account.getEmail())).check(doesNotExist());
    }

    /** Tests rendering of the enhanced target device picker bottom sheet in default HALF state. */
    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testEnhancedTargetDevicePickerBottomSheet() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, "Active today"),
                        new TargetDeviceInfo(
                                "My Computer", "guid2", FormFactor.DESKTOP, "Active 1 day ago"),
                        new TargetDeviceInfo(
                                "My Tablet", "guid3", FormFactor.TABLET, "Active 2 days ago"));
        View view =
                createAndShowEnhancedDevicePickerView(
                        devices, BottomSheetController.SheetState.HALF);
        mRenderTestRule.render(view, "enhanced_device_picker");
    }

    /**
     * Tests rendering the enhanced target device picker bottom sheet in half state with more than
     * four devices, verifying the peeking overflow fading edge and pinned Send action button.
     */
    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testEnhancedTargetDevicePickerBottomSheet_overflowHalfState() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, "Active today"),
                        new TargetDeviceInfo(
                                "My Computer", "guid2", FormFactor.DESKTOP, "Active 1 day ago"),
                        new TargetDeviceInfo(
                                "My Tablet", "guid3", FormFactor.TABLET, "Active 2 days ago"),
                        new TargetDeviceInfo(
                                "My Laptop", "guid4", FormFactor.DESKTOP, "Active 3 days ago"),
                        new TargetDeviceInfo(
                                "My Watch", "guid5", FormFactor.PHONE, "Active 4 days ago"),
                        new TargetDeviceInfo(
                                "My TV", "guid6", FormFactor.TABLET, "Active 5 days ago"));
        View view =
                createAndShowEnhancedDevicePickerView(
                        devices, BottomSheetController.SheetState.HALF);
        mRenderTestRule.render(view, "enhanced_device_picker_overflow_half");
    }

    /**
     * Tests rendering the enhanced target device picker bottom sheet in full state with more than
     * four devices, verifying the full device list, manage devices link, and pinned Send action
     * button.
     */
    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testEnhancedTargetDevicePickerBottomSheet_overflowFullState() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        List<TargetDeviceInfo> devices = new ArrayList<>();
        for (int i = 1; i <= 15; i++) {
            devices.add(
                    new TargetDeviceInfo(
                            "Device " + i,
                            "guid" + i,
                            FormFactor.PHONE,
                            "Active " + i + " days ago"));
        }
        View view =
                createAndShowEnhancedDevicePickerView(
                        devices, BottomSheetController.SheetState.FULL);
        mRenderTestRule.render(view, "enhanced_device_picker_overflow_full");
    }

    /**
     * Tests rendering the enhanced target device picker bottom sheet in half state with exactly
     * four devices, verifying the overflow threshold boundary where fading edge enabled and Send
     * button pinned.
     */
    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testEnhancedTargetDevicePickerBottomSheet_exactlyFourDevicesHalfState()
            throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, "Active today"),
                        new TargetDeviceInfo(
                                "My Computer", "guid2", FormFactor.DESKTOP, "Active 1 day ago"),
                        new TargetDeviceInfo(
                                "My Tablet", "guid3", FormFactor.TABLET, "Active 2 days ago"),
                        new TargetDeviceInfo(
                                "My Laptop", "guid4", FormFactor.DESKTOP, "Active 3 days ago"));
        View view =
                createAndShowEnhancedDevicePickerView(
                        devices, BottomSheetController.SheetState.HALF);
        mRenderTestRule.render(view, "enhanced_device_picker_exactly_four_devices_half");
    }

    /**
     * Tests rendering the enhanced target device picker bottom sheet in landscape mode with
     * multiple target devices, verifying that visual layout is consistent across screen
     * orientations.
     */
    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testEnhancedTargetDevicePickerBottomSheet_landscapeMode() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, "Active today"),
                        new TargetDeviceInfo(
                                "My Computer", "guid2", FormFactor.DESKTOP, "Active 1 day ago"),
                        new TargetDeviceInfo(
                                "My Tablet", "guid3", FormFactor.TABLET, "Active 2 days ago"),
                        new TargetDeviceInfo(
                                "My Laptop", "guid4", FormFactor.DESKTOP, "Active 3 days ago"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                });
        View view =
                createAndShowEnhancedDevicePickerView(
                        devices, BottomSheetController.SheetState.HALF);
        mRenderTestRule.render(view, "enhanced_device_picker_landscape_half");
    }
}
