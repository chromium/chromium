// Copyright 2022 The Chromium Authors
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

import android.Manifest;
import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;
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
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeController;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraPreviewOverlay;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraScanModel;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker.AssistantQrCodeImagePickerModel;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionCallback;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionCoordinator;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionModel;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.permissions.PermissionConstants;

/**
 * Tests for QR Code Scanning.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AssistantQrCodeTest {
    private static final String TAG = "AssistantQrCodeTest";

    // UI strings.
    private static final String TOOLBAR_TITLE = "Scan QR Code";
    private static final String PERMISSION_TEXT = "Please provide the permission";
    private static final String PERMISSION_BUTTON_TEXT = "Continue";
    private static final String OPEN_SETTINGS_TEXT = "Please enable permissions in device settings";
    private static final String OPEN_SETTINGS_BUTTON_TEXT = "Open Settings";
    private static final String OVERLAY_INSTRUCTION_TEXT = "Focus the QR Code inside the box";
    private static final String OVERLAY_SECURITY_TEXT =
            "The details will be safely shared with the website";

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

    @Mock
    public AssistantQrCodePermissionCallback mAssistantQrCodePermissionCallback;

    /**
     * Dummy Implementation of |AssistantQrCodeDelegate| that prints user interaction logs to
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

    interface AccessChildViewUtils {
        String getChildViewText(View view);
    }

    /** Checks whether the child view has the given text. */
    static TypeSafeMatcher<View> hasChildViewWithText(
            final AccessChildViewUtils accessChildViewUtils, final String text) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                return accessChildViewUtils.getChildViewText(view).equals(text);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Has child view with Text \'" + text + "\'");
            }
        };
    }

    @Before
    public void setUp() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), BLANK_URL));
    }

    /**
     * Test that the permission view is displayed when the component does not have permission
     * but can prompt for permission.
     */
    @Test
    @MediumTest
    public void testQrCodePermissionView() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodePermissionModel permissionModel = new AssistantQrCodePermissionModel();
            permissionModel.setPermissionText(PERMISSION_TEXT);
            permissionModel.setPermissionButtonText(PERMISSION_BUTTON_TEXT);
            permissionModel.setHasPermission(false);
            permissionModel.setCanPromptForPermission(true);

            AssistantQrCodePermissionCoordinator permissionCoordinator =
                    new AssistantQrCodePermissionCoordinator(getActivity(),
                            getActivity().getWindowAndroid(), permissionModel,
                            AssistantQrCodePermissionType.CAMERA,
                            mAssistantQrCodePermissionCallback);

            CoordinatorLayout.LayoutParams layoutParams = new CoordinatorLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            ViewGroup chromeCoordinatorView = getActivity().findViewById(R.id.coordinator);
            chromeCoordinatorView.addView(permissionCoordinator.getView(), layoutParams);
        });

        // Verify that view asking the user to give permission is displayed.
        onView(withText(PERMISSION_TEXT)).check(matches(isDisplayed()));
        onView(withText(PERMISSION_BUTTON_TEXT)).check(matches(isDisplayed()));
        onView(withId(R.id.permission_image)).check(matches(isDisplayed()));
    }

    /**
     * Test the view asking the user to give permission by opening settings is displayed when the
     * component does not have permission and cannot prompt for the permission.
     */
    @Test
    @MediumTest
    public void testQrCodePermissionSettingsView() {
        Intents.init();
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodePermissionModel permissionModel = new AssistantQrCodePermissionModel();
            permissionModel.setOpenSettingsText(OPEN_SETTINGS_TEXT);
            permissionModel.setOpenSettingsButtonText(OPEN_SETTINGS_BUTTON_TEXT);
            permissionModel.setHasPermission(false);
            permissionModel.setCanPromptForPermission(false);

            AssistantQrCodePermissionCoordinator permissionCoordinator =
                    new AssistantQrCodePermissionCoordinator(getActivity(),
                            getActivity().getWindowAndroid(), permissionModel,
                            AssistantQrCodePermissionType.CAMERA,
                            mAssistantQrCodePermissionCallback);

            CoordinatorLayout.LayoutParams layoutParams = new CoordinatorLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            ViewGroup chromeCoordinatorView = getActivity().findViewById(R.id.coordinator);
            chromeCoordinatorView.addView(permissionCoordinator.getView(), layoutParams);
        });

        // Verify that view asking the user to give permission by opening settings is displayed.
        onView(withText(OPEN_SETTINGS_TEXT)).check(matches(isDisplayed()));
        onView(withText(OPEN_SETTINGS_BUTTON_TEXT)).check(matches(isDisplayed()));
        onView(withId(R.id.permission_image)).check(matches(isDisplayed()));

        // Verify that an |ACTION_APPLICATION_DETAILS_SETTINGS| intent is started when
        // 'Open Settings' button is clicked.
        onView(withText(OPEN_SETTINGS_BUTTON_TEXT)).perform(click());
        intended(hasAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS));

        Intents.release();
    }

    /**
     * Test the title bar is displayed when QR Code scanning via Camera Preview is triggered.
     * Verifies the call to the delegate when the Cancel button is clicked.
     */
    @Test
    @MediumTest
    public void testCameraScanToolbar() {
        // Trigger QR Code Scanning via Camera Preview.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeCameraScanModel cameraScanModel =
                    getAssistantQrCodeCameraScanModelInstance();

            AssistantQrCodeController.promptQrCodeCameraScan(
                    getActivity(), getActivity().getWindowAndroid(), cameraScanModel);
        });

        // Verify that toolbar title is displayed.
        onView(withText(TOOLBAR_TITLE)).check(matches(isDisplayed()));

        // Click Cancel button and verify call to delegate.
        onView(withId(R.id.close_button)).perform(click());
        verify(mAssistantQrCodeDelegateMock).onScanCancelled();
    }

    /**
     * Tests permission view is displayed initially when camera permission is not granted. Once, the
     * permission is granted, it should open camera.
     */
    @Test
    @MediumTest
    public void testQrCodeCameraScanPermissionGranted() throws Exception {
        triggerAndTestQrCodeCameraPermissionFlow(/* grantPermission= */ true);

        // Verify camera preview overlay is displayed with the required strings.
        onView(withId(R.id.autofill_assistant_qr_code_camera_preview_overlay))
                .check(matches(isDisplayed()));
        onView(withId(R.id.autofill_assistant_qr_code_camera_preview_overlay))
                .check(matches(
                        hasChildViewWithText((view)
                                                     -> ((AssistantQrCodeCameraPreviewOverlay) view)
                                                                .getInstructionText(),
                                OVERLAY_INSTRUCTION_TEXT)));
        onView(withId(R.id.autofill_assistant_qr_code_camera_preview_overlay))
                .check(matches(hasChildViewWithText(
                        (view)
                                -> ((AssistantQrCodeCameraPreviewOverlay) view).getSecurityText(),
                        OVERLAY_SECURITY_TEXT)));

        // Click Cancel button and verify call to delegate.
        onView(withId(R.id.close_button)).perform(click());
        verify(mAssistantQrCodeDelegateMock).onScanCancelled();
    }

    /**
     * Tests permission view is displayed initially when camera permission is not granted. Once, the
     * permission is not granted and we cannot prompt more, it should open view asking the user to
     * give permission by opening settings.
     */
    @Test
    @MediumTest
    public void testQrCodeCameraScanPermissionDenied() throws Exception {
        Intents.init();
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        triggerAndTestQrCodeCameraPermissionFlow(/* grantPermission= */ false);

        // Verify that view asking the user to give permission by opening settings is displayed.
        onView(withText(OPEN_SETTINGS_TEXT)).check(matches(isDisplayed()));
        onView(withText(OPEN_SETTINGS_BUTTON_TEXT)).check(matches(isDisplayed()));
        onView(withId(R.id.permission_image)).check(matches(isDisplayed()));

        // Verify that an |ACTION_APPLICATION_DETAILS_SETTINGS| intent is started when
        // 'Open Settings' button is clicked.
        onView(withText(OPEN_SETTINGS_BUTTON_TEXT)).perform(click());
        intended(hasAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS));

        Intents.release();
    }

    /**
     * Tests the intent to open the Android image picker is triggered.
     */
    @Test
    @MediumTest
    public void testImagePickerCreatesActionPickIntent() {
        // Initializes Intents and begins recording intents. Must be called prior to triggering
        // any actions that send out intents which need to be verified or stubbed.
        Intents.init();

        // Stub all external intents. By default Espresso does not stub any Intent. Note that in
        // this case, all external calls will be blocked.
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        // Trigger QR Code Scanning via Image Picker.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeImagePickerModel imagePickerModel =
                    new AssistantQrCodeImagePickerModel();
            imagePickerModel.setDelegate(mAssistantQrCodeDelegateMock);

            AssistantQrCodeController.promptQrCodeImagePicker(
                    getActivity(), getActivity().getWindowAndroid(), imagePickerModel);
        });

        // Verify that an |ACTION_PICK| intent is started.
        intended(hasAction(Intent.ACTION_PICK));

        // Clears Intents state.
        Intents.release();
    }

    /**
     * Tests QR Code scanning via Image Picker. Tests permission view is displayed initially when
     * the permission to read images is not granted. Once, the permission is granted, it should open
     * the Android image picker.
     */
    @Test
    @MediumTest
    public void testQrCodeImagePickerPermissionGranted() throws Exception {
        Intents.init();
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        triggerAndTestQrCodeImagePickerPermissionFlow(/* grantPermission= */ true);

        // Verify that an |ACTION_PICK| intent is started.
        intended(hasAction(Intent.ACTION_PICK));

        Intents.release();
    }

    /**
     * Tests permission view is displayed initially when the permission to read images is not
     * granted. Once, the permission is not granted and we cannot prompt more, it should open view
     * asking the user to give permission by opening settings.
     */
    @Test
    @MediumTest
    public void testQrCodeImagePickerPermissionDenied() throws Exception {
        Intents.init();
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        triggerAndTestQrCodeImagePickerPermissionFlow(/* grantPermission= */ false);

        // Verify that view asking the user to give permission by opening settings is displayed.
        onView(withText(OPEN_SETTINGS_TEXT)).check(matches(isDisplayed()));
        onView(withText(OPEN_SETTINGS_BUTTON_TEXT)).check(matches(isDisplayed()));
        onView(withId(R.id.permission_image)).check(matches(isDisplayed()));

        // Verify that an |ACTION_APPLICATION_DETAILS_SETTINGS| intent is started when
        // 'Open Settings' button is clicked.
        onView(withText(OPEN_SETTINGS_BUTTON_TEXT)).perform(click());
        intended(hasAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS));

        Intents.release();
    }

    /** Manual Test to prompt QR Code Camera Scan and wait for any user interaction. */
    @Test
    @DisabledTest(message = "Only for local testing, not for automated testing")
    public void testpromptQrCodeCameraScan() throws Exception {
        MockAssistantQrCodeDelegate delegate = new MockAssistantQrCodeDelegate(mRunnableMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeCameraScanModel cameraScanModel =
                    getAssistantQrCodeCameraScanModelInstance();
            cameraScanModel.setDelegate(delegate);

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
                    getAssistantQrCodeImagePickerModelInstance();
            imagePickerModel.setDelegate(delegate);

            AssistantQrCodeController.promptQrCodeImagePicker(
                    getActivity(), getActivity().getWindowAndroid(), imagePickerModel);
        });

        verify(mRunnableMock, timeout(/* millis= */ 60000)).run();
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * Returns new instance of |AssistantQrCodeCameraScanModel|. Sets the UI strings and mock
     * |AssistantQrCodeDelegate|.
     */
    private AssistantQrCodeCameraScanModel getAssistantQrCodeCameraScanModelInstance() {
        AssistantQrCodeCameraScanModel cameraScanModel = new AssistantQrCodeCameraScanModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cameraScanModel.setDelegate(mAssistantQrCodeDelegateMock);

            // Set UI strings in model.
            cameraScanModel.setToolbarTitle(TOOLBAR_TITLE);
            cameraScanModel.setPermissionText(PERMISSION_TEXT);
            cameraScanModel.setPermissionButtonText(PERMISSION_BUTTON_TEXT);
            cameraScanModel.setOpenSettingsText(OPEN_SETTINGS_TEXT);
            cameraScanModel.setOpenSettingsButtonText(OPEN_SETTINGS_BUTTON_TEXT);
            cameraScanModel.setOverlayInstructionText(OVERLAY_INSTRUCTION_TEXT);
            cameraScanModel.setOverlaySecurityText(OVERLAY_SECURITY_TEXT);
        });

        return cameraScanModel;
    }

    /**
     * Returns new instance of |AssistantQrCodeImagePickerModel|. Sets the UI strings and mock
     * |AssistantQrCodeDelegate|.
     */
    private AssistantQrCodeImagePickerModel getAssistantQrCodeImagePickerModelInstance() {
        AssistantQrCodeImagePickerModel imagePickerModel = new AssistantQrCodeImagePickerModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            imagePickerModel.setDelegate(mAssistantQrCodeDelegateMock);

            // Set UI strings in model.
            imagePickerModel.setToolbarTitle(TOOLBAR_TITLE);
            imagePickerModel.setPermissionText(PERMISSION_TEXT);
            imagePickerModel.setPermissionButtonText(PERMISSION_BUTTON_TEXT);
            imagePickerModel.setOpenSettingsText(OPEN_SETTINGS_TEXT);
            imagePickerModel.setOpenSettingsButtonText(OPEN_SETTINGS_BUTTON_TEXT);
        });

        return imagePickerModel;
    }

    /**
     * Triggers QR Code scanning via Camera Preview and tests the permission flow. Mocks the
     * |AndroidPermissionDelegate| of |WindowAndroid|. Camera permission is not given initially.
     * Tests that permission view asking the user for camera permissions is displayed. Based on
     * |grantPermission| param, will then grant the permission or deny the permission forever.
     */
    private void triggerAndTestQrCodeCameraPermissionFlow(Boolean grantPermission) {
        String[] requestablePermission = new String[] {Manifest.permission.CAMERA};

        // Make sure that camera permission is not granted initially.
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testAndroidPermissionDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(requestablePermission,
                        RuntimePermissionTestUtils.RuntimePromptResponse.DENY);
        getActivity().getWindowAndroid().setAndroidPermissionDelegate(
                testAndroidPermissionDelegate);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeCameraScanModel cameraScanModel =
                    getAssistantQrCodeCameraScanModelInstance();

            // Trigger QR Code Scanning via Camera Preview.
            AssistantQrCodeController.promptQrCodeCameraScan(
                    getActivity(), getActivity().getWindowAndroid(), cameraScanModel);
        });

        // Verify that view asking the user to give permission is displayed.
        onView(withText(TOOLBAR_TITLE)).check(matches(isDisplayed()));
        onView(withText(PERMISSION_TEXT)).check(matches(isDisplayed()));
        onView(withText(PERMISSION_BUTTON_TEXT)).check(matches(isDisplayed()));
        onView(withId(R.id.permission_image)).check(matches(isDisplayed()));

        // Either grant the permission or deny it forever.
        testAndroidPermissionDelegate.setResponse(grantPermission
                        ? RuntimePermissionTestUtils.RuntimePromptResponse.GRANT
                        : RuntimePermissionTestUtils.RuntimePromptResponse.NEVER_ASK_AGAIN);
        onView(withText(PERMISSION_BUTTON_TEXT)).perform(click());
    }

    /**
     * Triggers QR Code scanning via Image Picker and tests the permission flow. Mocks the
     * |AndroidPermissionDelegate| of |WindowAndroid|. Read images permission is not given
     * initially. Tests that permission view asking the user for the permissions is displayed.
     * Based on |grantPermission| param, will then grant the permission or deny the permission
     * forever.
     */
    private void triggerAndTestQrCodeImagePickerPermissionFlow(Boolean grantPermission) {
        String[] requestablePermission = new String[] {
                Manifest.permission.READ_EXTERNAL_STORAGE, PermissionConstants.READ_MEDIA_IMAGES};

        // Make sure that the permission is not granted initially.
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testAndroidPermissionDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(requestablePermission,
                        RuntimePermissionTestUtils.RuntimePromptResponse.DENY);
        getActivity().getWindowAndroid().setAndroidPermissionDelegate(
                testAndroidPermissionDelegate);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantQrCodeImagePickerModel imagePickerModel =
                    getAssistantQrCodeImagePickerModelInstance();

            // Trigger QR Code Scanning via Image Picker.
            AssistantQrCodeController.promptQrCodeImagePicker(
                    getActivity(), getActivity().getWindowAndroid(), imagePickerModel);
        });

        // Verify that view asking the user to give permission is displayed.
        onView(withText(TOOLBAR_TITLE)).check(matches(isDisplayed()));
        onView(withText(PERMISSION_TEXT)).check(matches(isDisplayed()));
        onView(withText(PERMISSION_BUTTON_TEXT)).check(matches(isDisplayed()));
        onView(withId(R.id.permission_image)).check(matches(isDisplayed()));

        // Either grant the permission or deny it forever.
        testAndroidPermissionDelegate.setResponse(grantPermission
                        ? RuntimePermissionTestUtils.RuntimePromptResponse.GRANT
                        : RuntimePermissionTestUtils.RuntimePromptResponse.NEVER_ASK_AGAIN);
        onView(withText(PERMISSION_BUTTON_TEXT)).perform(click());
    }
}
