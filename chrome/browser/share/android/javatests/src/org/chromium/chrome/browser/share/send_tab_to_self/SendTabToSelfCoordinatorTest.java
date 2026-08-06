// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import static org.chromium.url.JUnitTestGURLs.HTTP_URL;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for SendTabToSelfCoordinator */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SendTabToSelfCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    private long mSetUpTimeMs;

    @Before
    public void setUp() {
        // Skip device lock UI on automotive.
        doAnswer(
                        invocation -> {
                            WindowAndroid.IntentCallback callback =
                                    (WindowAndroid.IntentCallback) invocation.getArguments()[4];
                            callback.onIntentCompleted(Activity.RESULT_OK, null);
                            return null;
                        })
                .when(mDeviceLockActivityLauncher)
                .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());

        // Setting a recent timestamp here is necessary, otherwise the device will be considered
        // expired and won't be displayed.
        mSetUpTimeMs = System.currentTimeMillis();
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity("CacheGuid", "Device", mSetUpTimeMs, mSetUpTimeMs);
    }

    private void signInAndShowDevicePicker() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));
        buildAndShowCoordinator();
    }

    private void expandBottomSheetToFullState() {
        BottomSheetController controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                BottomSheetControllerProvider.from(
                                        mSyncTestRule.getActivity().getWindowAndroid()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> new BottomSheetTestSupport(controller).setSheetState(SheetState.FULL, false));
        BottomSheetTestSupport.waitForState(controller, SheetState.FULL);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/40215923")
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    // TODO(crbug.com/448227402): Remove this test once the migration to the activity-less sign-in
    // flow is complete.
    public void testShowDeviceListIfSignedIn() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        buildAndShowCoordinator();

        waitForViewShown(R.id.device_picker_list);
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    // This test asserts the old bottom sheet UI and must run with the enhanced
    // bottom sheet feature disabled.
    @DisableFeatures({ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET})
    public void testShowDeviceListIfSignedIn_activitylessSignin() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        // 2 corresponds to SendTabToSelfDeviceCount::kOneDevice
                        .expectIntRecord("Sharing.SendTabToSelf.TargetDeviceCount", 2)
                        .build();

        buildAndShowCoordinator();

        onView(withId(R.id.device_picker_list)).check(matches(isDisplayed()));
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    // TODO(crbug.com/40825119): Flaky on Nexus 5x (bullhead).
    @DisableIf.Build(hardware_is = "bullhead")
    // This test asserts the old bottom sheet UI and must run with the enhanced
    // bottom sheet feature disabled.
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    // TODO(crbug.com/448227402): Remove this test once the migration to the activity-less sign-in
    // flow is complete.
    public void testShowSigninPromoIfSignedOut() {
        // An account must be added to the device so the promo is offered.
        mSyncTestRule.addAccount(TestAccounts.ACCOUNT1);
        buildAndShowCoordinator();

        // Check the promo is displayed, in particular the sign-in button.
        waitForViewShown(R.id.account_picker_continue_as_button);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getBottomSheetView()
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });

        waitForViewShown(R.id.device_picker_list);
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    // This test asserts the old bottom sheet UI and must run with the enhanced
    // bottom sheet feature disabled.
    @DisableFeatures({ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET})
    public void testShowSigninPromoIfSignedOut_activitylessSignin() {
        // An account must be added to the device so the promo is offered.
        mSyncTestRule.addAccount(TestAccounts.ACCOUNT1);
        // Two samples are expected because:
        // 1. Initial invocation while signed out records 0 (kNoTargetDevicesBecauseSignedOut).
        // 2. Sign-in completion automatically triggers a second show() invocation, which
        //    records 2 (kOneDevice) since 1 test device is active.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        // 0 corresponds to
                        // SendTabToSelfDeviceCount::kNoTargetDevicesBecauseSignedOut
                        .expectIntRecord("Sharing.SendTabToSelf.TargetDeviceCount", 0)
                        // 2 corresponds to SendTabToSelfDeviceCount::kOneDevice
                        .expectIntRecord("Sharing.SendTabToSelf.TargetDeviceCount", 2)
                        .build();
        buildAndShowCoordinator();

        // Check the promo is displayed, in particular the sign-in button.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .perform(click());

        onView(withId(R.id.device_picker_list)).check(matches(isDisplayed()));
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testShowEnhancedDeviceListIfSignedIn_activitylessSignin() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        buildAndShowCoordinator();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        // Verify the primary Send button is enabled by default due to auto-selection.
        onView(withId(R.id.send_button)).check(matches(isEnabled()));

        // Simulate clicking the primary bottom Send confirmation button.
        onView(withId(R.id.send_button)).perform(click());

        // Verify the sheet is closed/hidden.
        waitForViewHidden(R.id.sheet_item_list);
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_multipleDevicesSelection() {
        // Inject two more devices (in addition to the one in setUp) with an older timestamp,
        // so that the default device ("Device") is sorted first and auto-selected.
        long olderTime = mSetUpTimeMs - 1000;
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity("Guid1", "Device 1", olderTime, olderTime);
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity("Guid2", "Device 2", olderTime, olderTime);

        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        // 4 corresponds to SendTabToSelfDeviceCount::kThreeDevices
                        .expectIntRecord("Sharing.SendTabToSelf.TargetDeviceCount", 4)
                        .build();

        buildAndShowCoordinator();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));

        // Verify all devices are displayed
        onView(withText("Device")).check(matches(isDisplayed()));
        onView(withText("Device 1")).check(matches(isDisplayed()));
        onView(withText("Device 2")).check(matches(isDisplayed()));

        // Verify "Device" has check icon, others don't
        onView(allOf(withId(R.id.check_icon), isDescendantOfA(deviceRowMatcher("Device"))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.check_icon), isDescendantOfA(deviceRowMatcher("Device 1"))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.check_icon), isDescendantOfA(deviceRowMatcher("Device 2"))))
                .check(matches(not(isDisplayed())));

        // Click on the second device
        onView(withText("Device 2")).perform(click());

        // Verify the selection updated visually
        onView(allOf(withId(R.id.check_icon), isDescendantOfA(deviceRowMatcher("Device 2"))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.check_icon), isDescendantOfA(deviceRowMatcher("Device"))))
                .check(matches(not(isDisplayed())));

        // Verify the Send button is still enabled and proceed
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
        onView(withId(R.id.send_button)).perform(click());
        histogramWatcher.assertExpected();
    }

    /**
     * Tests that when many devices are present in HALF state, the bottom sheet overflows and the
     * Send button remains pinned and visible at the bottom without device list truncation.
     */
    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_halfStateOverflowWithManyDevices() {
        // Inject 6 more devices (in addition to the one in setUp) with an older timestamp,
        // so there are 7 devices total - more than will fit on a regular screen.
        for (int i = 1; i <= 6; i++) {
            long olderTime = mSetUpTimeMs - i * 1000;
            mSyncTestRule
                    .getFakeServerHelper()
                    .injectDeviceInfoEntity("Guid" + i, "Device " + i, olderTime, olderTime);
        }

        signInAndShowDevicePicker();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.sheet_item_list))
                .check(
                        (view, noViewFoundException) -> {
                            assertTrue(
                                    "Vertical fading edge should be enabled",
                                    view.isVerticalFadingEdgeEnabled());
                        });

        // Verify the newest device and (at least) the next 2 older devices are displayed.
        onView(withText("Device")).check(matches(isDisplayed()));
        onView(withText("Device 1")).check(matches(isDisplayed()));
        onView(withText("Device 2")).check(matches(isDisplayed()));

        // Verify that in SheetState.HALF, the Send button inside the bottom actions block
        // remains pinned, visible, and enabled without requiring expansion to SheetState.FULL.
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
    }

    /**
     * Tests that with exactly four target devices in the enhanced bottom sheet half state, the list
     * overflow threshold triggers and the Send action button remains pinned, visible, and enabled.
     */
    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_exactlyFourDevicesOverflowBoundary() {
        for (int i = 1; i <= 3; i++) {
            long olderTime = mSetUpTimeMs - i * 1000;
            mSyncTestRule
                    .getFakeServerHelper()
                    .injectDeviceInfoEntity("Guid" + i, "Device " + i, olderTime, olderTime);
        }

        signInAndShowDevicePicker();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.sheet_item_list))
                .check(
                        (view, noViewFoundException) -> {
                            assertTrue(
                                    "Vertical fading edge should be enabled",
                                    view.isVerticalFadingEdgeEnabled());
                        });
        onView(withText("Device")).check(matches(isDisplayed()));
        onView(withText("Device 1")).check(matches(isDisplayed()));
        onView(withText("Device 2")).check(matches(isDisplayed()));

        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
        onView(withId(R.id.manage_devices_link)).check(matches(not(isDisplayed())));
    }

    /**
     * Tests that transitioning an overflowing target device list from half state to full state
     * unsuppresses the list height to wrap content, displays all devices and the manage devices
     * link, and keeps the Send action button pinned, visible, and enabled.
     */
    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_overflowTransitionToFullState() {
        for (int i = 1; i <= 5; i++) {
            long olderTime = mSetUpTimeMs - i * 1000;
            mSyncTestRule
                    .getFakeServerHelper()
                    .injectDeviceInfoEntity("Guid" + i, "Device " + i, olderTime, olderTime);
        }

        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        buildAndShowCoordinator();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
        onView(withId(R.id.manage_devices_link)).check(matches(not(isDisplayed())));

        expandBottomSheetToFullState();

        onView(withId(R.id.manage_devices_link)).check(matches(isDisplayed()));
        onView(withId(R.id.manage_devices_divider)).check(matches(isDisplayed()));
        onView(withId(R.id.sheet_item_list)).perform(RecyclerViewActions.scrollToPosition(4));
        onView(withText("Device 4")).check(matches(isDisplayed()));
        onView(withId(R.id.sheet_item_list)).perform(RecyclerViewActions.scrollToPosition(5));
        onView(withText("Device 5")).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));

        onView(withId(R.id.send_button)).perform(click());
        waitForViewHidden(R.id.sheet_item_list);
    }

    /** Tests fallback to manage account devices link when zero target devices are available. */
    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_emptyListFallback() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        SendTabToSelfAndroidBridge.Natives nativeMock =
                mock(SendTabToSelfAndroidBridge.Natives.class);
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(nativeMock);
        try {
            doReturn(EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE)
                    .when(nativeMock)
                    .getEntryPointDisplayReason(any(), any());
            doReturn(new java.util.ArrayList<TargetDeviceInfo>())
                    .when(nativeMock)
                    .getAllTargetDeviceInfos(any());

            buildAndShowCoordinator();

            onView(withId(R.id.manage_account_devices_link)).check(matches(isDisplayed()));
        } finally {
            SendTabToSelfAndroidBridgeJni.setInstanceForTesting(null);
        }
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_manageDevicesClick() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        buildAndShowCoordinator();

        BottomSheetController controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return BottomSheetControllerProvider.from(
                                    mSyncTestRule.getActivity().getWindowAndroid());
                        });
        ThreadUtils.runOnUiThreadBlocking(
                () -> new BottomSheetTestSupport(controller).setSheetState(SheetState.FULL, false));
        BottomSheetTestSupport.waitForState(controller, SheetState.FULL);

        onView(withId(R.id.manage_devices_link)).check(matches(isDisplayed()));
        Intents.init();
        try {
            onView(withId(R.id.manage_devices_link)).perform(click());

            // Verify the coordinator started the intent
            intended(
                    allOf(
                            hasAction(Intent.ACTION_VIEW),
                            hasData(UrlConstants.GOOGLE_ACCOUNT_DEVICE_ACTIVITY_URL),
                            hasComponent(ChromeLauncherActivity.class.getName())));
        } finally {
            Intents.release();
        }

        // Verify the sheet is closed/hidden.
        waitForViewHidden(R.id.sheet_item_list);
    }

    private void buildAndShowCoordinator() {
        ChromeTabbedActivity activity = mSyncTestRule.getActivity();
        WindowAndroid windowAndroid = activity.getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SendTabToSelfCoordinator coordinator =
                            new SendTabToSelfCoordinator(
                                    activity,
                                    windowAndroid,
                                    HTTP_URL.getSpec(),
                                    "Page",
                                    BottomSheetControllerProvider.from(windowAndroid),
                                    ProfileManager.getLastUsedRegularProfile(),
                                    mDeviceLockActivityLauncher,
                                    activity::getActivityTab,
                                    activity,
                                    SigninAndHistorySyncActivityLauncherImpl.get(),
                                    activity.getActivityResultTracker(),
                                    activity.getModalDialogManagerSupplier(),
                                    activity.getSnackbarManager(),
                                    ShareEntryPoint.SHARE_SHEET);
                    coordinator.show();
                });
    }

    // TODO(crbug.com/448227402): Remove this method once the migration to the activity-less sign-in
    // flow is complete.
    private @Nullable View getBottomSheetView() {
        WindowAndroid windowAndroid = mSyncTestRule.getActivity().getWindowAndroid();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var content =
                            BottomSheetControllerProvider.from(windowAndroid)
                                    .getCurrentSheetContent();
                    return content != null ? content.getContentView() : null;
                });
    }

    // TODO(crbug.com/448227402): Remove this method once the migration to the activity-less sign-in
    // flow is complete.
    private void waitForViewShown(@IdRes int id) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View view = getBottomSheetView();
                    if (view == null) return false;
                    View target = view.findViewById(id);
                    return target != null && target.getVisibility() == View.VISIBLE;
                });
    }

    private void waitForViewHidden(@IdRes int id) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View view = getBottomSheetView();
                    if (view == null) return true; // Sheet is closed
                    View target = view.findViewById(id);
                    return target == null || target.getVisibility() != View.VISIBLE;
                });
    }

    private static Matcher<View> deviceRowMatcher(String deviceName) {
        return allOf(
                withParent(withId(R.id.sheet_item_list)),
                hasDescendant(allOf(withId(R.id.device_name), withText(deviceName))));
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET,
        ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST
    })
    public void testSnackbarShownAfterSend() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        buildAndShowCoordinator();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Snackbar.Shown", Snackbar.UMA_SEND_TAB_TO_SELF)
                        .build();

        // Simulate clicking the primary bottom Send confirmation button.
        onView(withId(R.id.send_button)).perform(click());

        // Verify the sheet is closed/hidden.
        waitForViewHidden(R.id.sheet_item_list);

        // Verify that the snackbar is shown.
        ChromeTabbedActivity activity = mSyncTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager = activity.getSnackbarManager();
                    Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
                    Criteria.checkThat(snackbar, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbar.getIdentifierForTesting(),
                            Matchers.is(Snackbar.UMA_SEND_TAB_TO_SELF));

                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbarMessage.getText().toString(),
                            Matchers.is("Sent to Chrome on your Device • test@gmail.com"));
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST
    })
    public void testSnackbarShownAfterSend_WebContentsNull() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        // Trigger sendTabToDevice directly with null WebContents. This simulates the case where the
        // send is triggered from the system Share Sheet.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SendTabToSelfAndroidBridge.sendTabToDevice(
                            ProfileManager.getLastUsedRegularProfile(),
                            null,
                            "CacheGuid",
                            "Device",
                            HTTP_URL.getSpec(),
                            "Title",
                            ShareEntryPoint.SHARE_SHEET);
                });

        // Verify that the snackbar is shown on the activity.
        ChromeTabbedActivity activity = mSyncTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager = activity.getSnackbarManager();
                    Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
                    Criteria.checkThat(snackbar, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbar.getIdentifierForTesting(),
                            Matchers.is(Snackbar.UMA_SEND_TAB_TO_SELF));

                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbarMessage.getText().toString(),
                            Matchers.is("Sent to Chrome on your Device • test@gmail.com"));
                });
    }

    @Test
    @LargeTest
    // On Automotive, the way this test detects the Toast (via a11y APIs) doesn't work.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST
    })
    public void testToastShownAfterSend_SnackbarManagerUnavailable() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        // Start BlankUiTestActivity to take focus. It does not implement SnackbarManageable. This
        // simulates the case where the system Share Sheet is triggered from another app, and there
        // is no visible Chrome activity.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Intent intent = new Intent(context, BlankUiTestActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        BlankUiTestActivity blankActivity =
                (BlankUiTestActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        // Register accessibility listener to capture the Toast.
        var automation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        java.util.concurrent.atomic.AtomicBoolean toastDisplayed =
                new java.util.concurrent.atomic.AtomicBoolean(false);
        automation.setOnAccessibilityEventListener(
                event -> {
                    if (event.getEventType()
                            == AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED) {
                        String className =
                                event.getClassName() != null ? event.getClassName().toString() : "";
                        String eventText = event.getText().toString();
                        if (className.contains("android.widget.Toast")
                                && eventText.contains(
                                        "Sent to Chrome on your Device • test@gmail.com")) {
                            toastDisplayed.set(true);
                        }
                    }
                });

        // Trigger sendTabToDevice directly with null WebContents. This simulates the send being
        // triggered from the system Share Sheet. It should check the focused activity
        // (BlankUiTestActivity), find that it doesn't implement SnackbarManageable, and fallback to
        // Toast.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SendTabToSelfAndroidBridge.sendTabToDevice(
                            ProfileManager.getLastUsedRegularProfile(),
                            null,
                            "CacheGuid",
                            "Device",
                            HTTP_URL.getSpec(),
                            "Title",
                            ShareEntryPoint.SHARE_SHEET);
                });

        // Wait for the toast to be displayed.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(toastDisplayed.get(), Matchers.is(true));
                });

        // Clean up.
        automation.setOnAccessibilityEventListener(null);
        blankActivity.finish();
    }

    /**
     * Tests that show() returns early without crashing when getEntryPointDisplayReason returns
     * null.
     */
    @Test
    @LargeTest
    public void testShow_nullDisplayReasonDoesNotCrash() {
        ChromeTabbedActivity activity = mSyncTestRule.getActivity();
        WindowAndroid windowAndroid = activity.getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(
                            SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                    ProfileManager.getLastUsedRegularProfile(), "chrome://flags"));
                    SendTabToSelfCoordinator coordinator =
                            new SendTabToSelfCoordinator(
                                    activity,
                                    windowAndroid,
                                    "chrome://flags",
                                    "Flags",
                                    BottomSheetControllerProvider.from(windowAndroid),
                                    ProfileManager.getLastUsedRegularProfile(),
                                    mDeviceLockActivityLauncher,
                                    activity::getActivityTab,
                                    activity,
                                    SigninAndHistorySyncActivityLauncherImpl.get(),
                                    activity.getActivityResultTracker(),
                                    activity.getModalDialogManagerSupplier(),
                                    activity.getSnackbarManager(),
                                    ShareEntryPoint.SHARE_SHEET);
                    coordinator.show();
                });
    }

    /**
     * Tests that when there are too many target devices to fit in the full state, the list view
     * overflows and both the Send button and manage devices link remain displayed and visible.
     */
    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/543459081
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_fullStateOverflowWithManyDevices() {
        for (int i = 1; i <= 20; i++) {
            long olderTime = mSetUpTimeMs - i * 1000;
            mSyncTestRule
                    .getFakeServerHelper()
                    .injectDeviceInfoEntity("Guid" + i, "Device " + i, olderTime, olderTime);
        }

        signInAndShowDevicePicker();

        expandBottomSheetToFullState();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.manage_devices_link)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
        onView(withId(R.id.sheet_item_list)).perform(RecyclerViewActions.scrollToPosition(20));
        onView(withText("Device 20")).check(matches(isDisplayed()));
        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
    }

    /**
     * Tests that the enhanced target device picker displays correctly in landscape mode with all
     * target devices and the Send button visible and accessible.
     */
    @Test
    @LargeTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SEND_TAB_TO_SELF_ENHANCED_BOTTOMSHEET
    })
    public void testEnhancedDevicePicker_landscapeMode() {
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity("Guid1", "My Phone", mSetUpTimeMs, mSetUpTimeMs);
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity("Guid2", "My Laptop", mSetUpTimeMs, mSetUpTimeMs);

        Activity activity = mSyncTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                });

        signInAndShowDevicePicker();

        onView(withId(R.id.sheet_item_list)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isDisplayed()));
        onView(withId(R.id.send_button)).check(matches(isEnabled()));
    }
}
