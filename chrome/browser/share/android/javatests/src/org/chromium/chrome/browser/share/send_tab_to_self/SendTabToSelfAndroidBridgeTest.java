// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.PersistedTabDataConfiguration;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class SendTabToSelfAndroidBridgeTest {
    private static final String URL = "https://www.google.com";
    private static final String TITLE = "Google";
    private static final String TARGET_DEVICE_SYNC_CACHE_GUID = "device_guid";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private Profile mProfile;
    private WebContents mWebContents;

    @Before
    public void setUp() {
        // Required to allow SendTabToSelfTabCardLabelData to be initialized.
        PersistedTabDataConfiguration.setUseTestConfig(true);

        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        mWebContents = new MockWebContents();
    }

    @Test
    @SmallTest
    public void testSendTabToDevice() {
        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "device_name",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);
        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        any(),
                        eq(ShareEntryPoint.SHARE_SHEET));
    }

    @Test
    @SmallTest
    public void testMarkEntryOpened() {
        String guid = "guid";
        SendTabToSelfAndroidBridge.markEntryOpened(mProfile, guid);
        verify(mNativeMock).markEntryOpened(eq(mProfile), eq(guid));
    }

    @Test
    @SmallTest
    public void testDismissEntry() {
        String guid = "guid";
        SendTabToSelfAndroidBridge.dismissEntry(mProfile, guid);
        verify(mNativeMock).dismissEntry(eq(mProfile), eq(guid));
    }

    @Test
    @SmallTest
    public void testMarkEntryActivated() {
        String guid = "guid";
        SendTabToSelfAndroidBridge.markEntryActivated(
                mProfile, guid, ShareActivatedEntryPoint.MOBILE_NOTIFICATION);
        verify(mNativeMock)
                .markEntryActivated(
                        eq(mProfile), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testGetAllTargetDeviceInfos() {
        List<TargetDeviceInfo> expected =
                List.of(
                        new TargetDeviceInfo("name1", "guid1", FormFactor.DESKTOP, "Active today"),
                        new TargetDeviceInfo("name2", "guid2", FormFactor.DESKTOP, "Active today"),
                        new TargetDeviceInfo("name3", "guid3", FormFactor.PHONE, "Active today"));
        when(mNativeMock.getAllTargetDeviceInfos(eq(mProfile))).thenReturn(expected);

        List<TargetDeviceInfo> actual =
                SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);

        verify(mNativeMock).getAllTargetDeviceInfos(eq(mProfile));
        Assert.assertEquals(3, actual.size());
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testGetEntryPointDisplayReason() {
        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, URL);
        verify(mNativeMock).getEntryPointDisplayReason(eq(mProfile), eq(URL));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessToast() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);

        Assert.assertTrue(
                ShadowToast.showedCustomToast("Sent to Chrome on your Pixel 10.", R.id.toast_text));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessToast_Throttled() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS_THROTTLED);

        Assert.assertTrue(
                ShadowToast.showedCustomToast(
                        "Already sent to Chrome on your Pixel 10", R.id.toast_text));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsFailureToast_OnFailure() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.FAILURE_INVALID_URL);

        String expectedMessage =
                ContextUtils.getApplicationContext()
                        .getString(R.string.send_tab_to_self_post_send_failure_toast);
        Assert.assertTrue(ShadowToast.showedCustomToast(expectedMessage, R.id.toast_text));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsNoInternetToast_OnNoInternetConnection() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor
                .getValue()
                .onResult(SendTabToSelfResult.FAILURE_NO_INTERNET_CONNECTION);

        String expectedMessage =
                ContextUtils.getApplicationContext()
                        .getString(R.string.send_tab_to_self_post_send_no_internet_toast);
        Assert.assertTrue(ShadowToast.showedCustomToast(expectedMessage, R.id.toast_text));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsNoInternetToast_OnCommitTimeout() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.FAILURE_COMMIT_TIMEOUT);

        String expectedMessage =
                ContextUtils.getApplicationContext()
                        .getString(R.string.send_tab_to_self_post_send_no_internet_toast);
        Assert.assertTrue(ShadowToast.showedCustomToast(expectedMessage, R.id.toast_text));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_PostSendToastFeatureDisabled() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);

        Assert.assertEquals(0, ShadowToast.shownToastCount());
    }

    @Test
    @SmallTest
    public void testAttachTabLabel() {
        UserDataHost userDataHost = new UserDataHost();
        Tab tab = mock(Tab.class);
        when(tab.getUserDataHost()).thenReturn(userDataHost);

        SendTabToSelfAndroidBridge.attachTabLabel(tab, "guid", "Example Phone");

        SendTabToSelfTabCardLabelData userData =
                userDataHost.getUserData(SendTabToSelfTabCardLabelData.class);
        Assert.assertNotNull(userData);
    }

    @Test
    @SmallTest
    // Tests that the message banner (which is shown tabs are auto-opened in the background) is
    // shown correctly and that the primary action callback is triggered correctly.
    public void testShowMessageBanner() {
        // Set up mocks for the window and messaging infrastructure needed to enqueue a banner.
        WebContents webContents = mock(WebContents.class);
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        ManagedMessageDispatcher messageDispatcher = mock(ManagedMessageDispatcher.class);

        when(windowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(webContents.getTopLevelNativeWindow()).thenReturn(windowAndroid);
        MessagesFactory.attachMessageDispatcher(windowAndroid, messageDispatcher);

        // Trigger the banner display logic.
        SendTabToSelfAndroidBridge.showMessageBanner(webContents, "Pixel 10");

        // Capture the enqueued PropertyModel to verify its content and action callbacks.
        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(messageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(),
                        eq(webContents),
                        eq(MessageScopeType.WEB_CONTENTS),
                        eq(false));

        // Verify the static properties of the banner.
        PropertyModel model = messageCaptor.getValue();
        Assert.assertEquals(
                MessageIdentifier.SEND_TAB_TO_SELF,
                model.get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals("Links received", model.get(MessageBannerProperties.TITLE));
        Assert.assertEquals("From Pixel 10", model.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals("Open", model.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                R.drawable.send_tab, model.get(MessageBannerProperties.ICON_RESOURCE_ID));

        // Verify the ON_PRIMARY_ACTION callback behavior.
        Supplier<Integer> onPrimaryAction = model.get(MessageBannerProperties.ON_PRIMARY_ACTION);

        // Set up a mock ChromeTabbedActivity and LayoutManager to verify that the action attempts
        // to open the tab switcher.
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);
        LayoutManagerChrome layoutManager = mock(LayoutManagerChrome.class);
        when(tabbedActivity.getLayoutManager()).thenReturn(layoutManager);
        // Register the mock activity with ApplicationStatus so getLastTrackedFocusedActivity()
        // returns it.
        ApplicationStatus.onStateChangeForTesting(tabbedActivity, ActivityState.CREATED);

        // Execute the primary action.
        int result = onPrimaryAction.get();

        // Verify that the banner dismisses immediately and showLayout is called to show the tab
        // switcher.
        Assert.assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, result);
        verify(layoutManager).showLayout(LayoutType.HUB, true);

        // Clean up global static state.
        ApplicationStatus.onStateChangeForTesting(tabbedActivity, ActivityState.DESTROYED);
        MessagesFactory.detachMessageDispatcher(messageDispatcher);
    }
}
