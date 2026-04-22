// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.url.JUnitTestGURLs.HTTP_URL;

import android.app.Activity;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.base.WindowAndroid;

/** Tests for SendTabToSelfCoordinator */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SendTabToSelfCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;

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
        long now = System.currentTimeMillis();
        mSyncTestRule.getFakeServerHelper().injectDeviceInfoEntity("CacheGuid", "Device", now, now);
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
    public void testShowDeviceListIfSignedIn_activitylessSignin() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () ->
                        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        HTTP_URL.getSpec())
                                .equals(EntryPointDisplayReason.OFFER_FEATURE));

        buildAndShowCoordinator();

        onView(withId(R.id.device_picker_list)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    // TODO(crbug.com/40825119): Flaky on Nexus 5x (bullhead).
    @DisableIf.Build(hardware_is = "bullhead")
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    // TODO(crbug.com/448227402): Remove this test once the migration to the activity-less sign-in
    // flow is complete.
    public void testShowSigninPromoIfSignedOut() {
        // An account must be added to the device so the promo is offered.
        mSyncTestRule.addTestAccount();
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
    public void testShowSigninPromoIfSignedOut_activitylessSignin() {
        // An account must be added to the device so the promo is offered.
        mSyncTestRule.addTestAccount();
        buildAndShowCoordinator();

        // Check the promo is displayed, in particular the sign-in button.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .perform(click());

        onView(withId(R.id.device_picker_list)).check(matches(isDisplayed()));
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
                                    activity.getSnackbarManager());
                    coordinator.show();
                });
    }

    // TODO(crbug.com/448227402): Remove this method once the migration to the activity-less sign-in
    // flow is complete.
    private View getBottomSheetView() {
        WindowAndroid windowAndroid = mSyncTestRule.getActivity().getWindowAndroid();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return BottomSheetControllerProvider.from(windowAndroid)
                            .getCurrentSheetContent()
                            .getContentView();
                });
    }

    // TODO(crbug.com/448227402): Remove this method once the migration to the activity-less sign-in
    // flow is complete.
    private void waitForViewShown(@IdRes int id) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View view = getBottomSheetView().findViewById(id);
                    return view != null && view.getVisibility() == View.VISIBLE;
                });
    }
}
