// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.os.Build;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;
import org.chromium.components.sync_device_info.RemoteDeviceInfo;

import java.util.List;

/** Unit tests for {@link SendTabToSelfShareTargetActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
@EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_DYNAMIC_SHORTCUTS)
public class SendTabToSelfShareTargetActivityTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String DEVICE_GUID = "guid1";
    private static final String SHORTCUT_ID =
            OtherDevicesShortcutController.SHORTCUT_ID_PREFIX + DEVICE_GUID;
    private static final String URL = "https://example.com";
    private static final String TITLE = "Title";

    @Mock private Profile mProfile;
    @Mock private ChromeBrowserInitializer mBrowserInitializer;
    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private SendTabToSelfMetricsRecorder.Natives mMetricsRecorderMock;
    @Mock private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        doAnswer(
                        invocation -> {
                            BrowserParts parts = invocation.getArgument(0);
                            parts.preInflationStartup();
                            return null;
                        })
                .when(mBrowserInitializer)
                .handlePreNativeStartupAndLoadLibraries(org.mockito.ArgumentMatchers.any());
        ChromeBrowserInitializer.setForTesting(mBrowserInitializer);

        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        SendTabToSelfMetricsRecorderJni.setInstanceForTesting(mMetricsRecorderMock);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    @After
    public void tearDown() {
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        FirstRunStatus.setFirstRunFlowComplete(false);
    }

    private Intent createShareTargetIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_SHORTCUT_ID, SHORTCUT_ID);
        intent.putExtra(Intent.EXTRA_TEXT, URL);
        intent.putExtra(Intent.EXTRA_SUBJECT, TITLE);
        intent.setType("text/plain");
        return intent;
    }

    private SendTabToSelfShareTargetActivity createActivity(Intent intent) {
        ActivityController<SendTabToSelfShareTargetActivity> controller =
                Robolectric.buildActivity(SendTabToSelfShareTargetActivity.class, intent);
        controller.get().setTheme(R.style.Theme_BrowserUI_Translucent_NoTitleBar);
        return controller.create().get();
    }

    @Test
    public void testWindowIsTransparentAndNotTouchableOrFocusable() {
        SendTabToSelfShareTargetActivity activity = createActivity(createShareTargetIntent());

        WindowManager.LayoutParams params = activity.getWindow().getAttributes();
        assertNotEquals(0, params.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
        assertNotEquals(0, params.flags & WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
        assertEquals(0f, params.alpha, 0f);
    }

    @Test
    public void testFinishesOnlyAfterSendCompletes() {
        when(mNativeMock.isModelReady(mProfile)).thenReturn(true);
        when(mNativeMock.getAllTargetDeviceInfos(mProfile))
                .thenReturn(
                        List.of(
                                new RemoteDeviceInfo(
                                        "Device 1",
                                        DEVICE_GUID,
                                        FormFactor.PHONE,
                                        OsType.ANDROID,
                                        "Just now")));

        // Populate ShortcutManager with the target device shortcut.
        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();
        controller.destroy();

        SendTabToSelfShareTargetActivity activity = createActivity(createShareTargetIntent());
        activity.finishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback> callbackCaptor =
                ArgumentCaptor.forClass(
                        SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);
        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(null),
                        eq(DEVICE_GUID),
                        eq(URL),
                        eq(TITLE),
                        callbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET_DIRECT_SHARE));

        // Activity must remain alive while waiting for commit confirmation.
        assertFalse(activity.isFinishing());

        callbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testFinishesImmediatelyOnInvalidIntent() {
        SendTabToSelfShareTargetActivity activity = createActivity(new Intent(Intent.ACTION_VIEW));
        activity.finishNativeInitialization();
        assertTrue(activity.isFinishing());
    }
}
