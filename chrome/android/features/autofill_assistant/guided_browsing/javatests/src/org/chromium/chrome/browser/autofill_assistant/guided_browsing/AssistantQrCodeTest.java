// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.guided_browsing;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.isInternal;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Log;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeController;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraScanModel;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker.AssistantQrCodeImagePickerModel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for QR Code Scanning.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AssistantQrCodeTest {
    private static final String TAG = "AssistantQrCodeTest";

    // URL to open a blank page in custom chrome tab.
    private static final String BLANK_URL = "about://blank";

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    public Runnable mRunnableMock;

    @Mock
    public AssistantQrCodeDelegate mAssistantQrCodeDelegateMock;

    /**
     * Dummy Implementation of AssistantQrCodeDelegate that prints user interaction logs to
     * STDOUT for manual testing.
     */
    class MockAssistantQrCodeDelegate implements AssistantQrCodeDelegate {
        public static final String TAG = "MockAssistantQrCodeDelegate";

        private Runnable mOnUserInteractionComplete;

        /**
         * Constructor for MockAssistantQrCodeDelegate.
         *
         * @param onUserInteractionComplete Run when any of the delegate function is called.
         *         Used to notify the completion of user interaction.
         */
        public MockAssistantQrCodeDelegate(Runnable onUserInteractionComplete) {
            mOnUserInteractionComplete = onUserInteractionComplete;
        }

        @Override
        public void onScanResult(String value) {
            Log.i(TAG, "Scan Result: " + value);
            mOnUserInteractionComplete.run();
        }

        @Override
        public void onScanCancelled() {
            Log.i(TAG, "onScanCancelled");
            mOnUserInteractionComplete.run();
        }

        @Override
        public void onScanFailure() {
            Log.i(TAG, "onScanFailure");
            mOnUserInteractionComplete.run();
        }

        @Override
        public void onCameraError() {
            Log.i(TAG, "onCameraError");
            mOnUserInteractionComplete.run();
        }
    }

    @Before
    public void setUp() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), BLANK_URL));
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    public void testCameraScanToolbar() {
        // Trigger QR Code Scanning via Camera Preview
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeCameraScanModel cameraScanModel = new AssistantQrCodeCameraScanModel();
            cameraScanModel.setDelegate(mAssistantQrCodeDelegateMock);
            cameraScanModel.setToolbarTitle("Scan QR Code");

            AssistantQrCodeController.promptQrCodeCameraScan(
                    getActivity(), getActivity().getWindowAndroid(), cameraScanModel);
        });

        // Verify that toolbar title is displayed
        onView(withText("Scan QR Code")).check(matches(isDisplayed()));

        // Click Cancel button and verify call to delegate
        onView(withId(R.id.close_button)).perform(click());
        verify(mAssistantQrCodeDelegateMock).onScanCancelled();
    }

    @Test
    @MediumTest
    public void testImagePickerCreatesActionPickIntent() {
        // Initializes Intents and begins recording intents. Must be called prior to triggering
        // any actions that send out intents which need to be verified or stubbed.
        Intents.init();

        // Stub all external intents. By default Espresso does not stub any Intent. Note that in
        // this case, all external calls will be blocked.
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        // Trigger QR Code Scanning via Image Picker
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeImagePickerModel imagePickerModel =
                    new AssistantQrCodeImagePickerModel();
            imagePickerModel.setDelegate(mAssistantQrCodeDelegateMock);

            AssistantQrCodeController.promptQrCodeImagePicker(
                    getActivity(), getActivity().getWindowAndroid(), imagePickerModel);
        });

        // Verify that an ACTION_PICK intent is started.
        intended(hasAction(Intent.ACTION_PICK));

        // Clears Intents state.
        Intents.release();
    }

    /** Manual Test to prompt QR Code Camera Scan and wait for any user interaction. */
    @Test
    @DisabledTest(message = "Only for local testing, not for automated testing")
    public void testpromptQrCodeCameraScan() throws Exception {
        MockAssistantQrCodeDelegate delegate = new MockAssistantQrCodeDelegate(mRunnableMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeCameraScanModel cameraScanModel = new AssistantQrCodeCameraScanModel();
            cameraScanModel.setDelegate(delegate);

            // Set UI strings in model.
            cameraScanModel.setToolbarTitle("Scan QR Code");
            cameraScanModel.setPermissionText("Please provide camera permissions");
            cameraScanModel.setPermissionButtonText("Continue");
            cameraScanModel.setOpenSettingsText(
                    "Please enable camera permissions in device settings");
            cameraScanModel.setOpenSettingsButtonText("Open Settings");
            cameraScanModel.setOverlayInstructionText("Focus the QR Code inside the box");
            cameraScanModel.setOverlaySecurityText(
                    "The details will be safely shared with the website");

            AssistantQrCodeController.promptQrCodeCameraScan(
                    getActivity(), getActivity().getWindowAndroid(), cameraScanModel);
        });

        verify(mRunnableMock, timeout(/* millis= */ 60000)).run();
    }

    /** Manual Test to prompt QR Code Image Picker and wait for any user interaction. */
    @Test
    @DisabledTest(message = "Only for local testing, not for automated testing")
    public void testpromptQrCodeImagePicker() throws Exception {
        MockAssistantQrCodeDelegate delegate = new MockAssistantQrCodeDelegate(mRunnableMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeImagePickerModel imagePickerModel =
                    new AssistantQrCodeImagePickerModel();
            imagePickerModel.setDelegate(delegate);

            // Set UI strings in model.
            imagePickerModel.setToolbarTitle("Scan QR Code");
            imagePickerModel.setPermissionText("Please provide permissions to access images");
            imagePickerModel.setPermissionButtonText("Continue");
            imagePickerModel.setOpenSettingsText(
                    "Please enable media permissions in device settings");
            imagePickerModel.setOpenSettingsButtonText("Open Settings");

            AssistantQrCodeController.promptQrCodeImagePicker(
                    getActivity(), getActivity().getWindowAndroid(), imagePickerModel);
        });

        verify(mRunnableMock, timeout(/* millis= */ 60000)).run();
    }
}