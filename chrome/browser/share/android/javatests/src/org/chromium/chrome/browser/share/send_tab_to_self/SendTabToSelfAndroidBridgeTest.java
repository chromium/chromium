// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.PersistedTabDataConfiguration;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
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
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ManagedMessageDispatcher mMessageDispatcher;
    @Mock private ChromeTabbedActivity mTabbedActivity;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    private WebContents mWebContents;

    @Before
    public void setUp() {
        // Required to allow SendTabToSelfTabCardLabelData to be initialized.
        PersistedTabDataConfiguration.setUseTestConfig(true);

        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        mWebContents = mock(WebContents.class);
        mWindowAndroid = mock(WindowAndroid.class);
        mSnackbarManager = mock(SnackbarManager.class);
        mMessageDispatcher = mock(ManagedMessageDispatcher.class);
        mTabbedActivity = mock(ChromeTabbedActivity.class);

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mWindowAndroid.getContext())
                .thenReturn(new WeakReference<>(RuntimeEnvironment.getApplication()));
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mTabbedActivity));
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mMessageDispatcher);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);
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
    public void testSendTabToDevice_ShowsSuccessSnackbar() {
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                "Sent to Chrome on your Pixel 10 • test@gmail.com",
                snackbarCaptor.getValue().getTextForTesting().toString());
    }

    // Tests that the post-send success snackbar shows the fallback message without an email when
    // the user's primary account email address is not available.
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessSnackbar_NoEmail() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(null);
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                "Sent to Chrome on your Pixel 10",
                snackbarCaptor.getValue().getTextForTesting().toString());
    }

    // Tests that the post-send success snackbar shows the fallback message without an email when
    // the user's primary account email address cannot be displayed (e.g. supervised child account).
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessSnackbar_NonDisplayableEmail() {
        // Supervised child accounts are not allowed to display email addresses in the UI.
        when(mIdentityManager.getPrimaryAccountInfo())
                .thenReturn(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL);
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                "Sent to Chrome on your Pixel 10",
                snackbarCaptor.getValue().getTextForTesting().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessSnackbar_Throttled() {
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                "Already sent to Chrome on your Pixel 10",
                snackbarCaptor.getValue().getTextForTesting().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSnackbarFromActivity_WhenWebContentsNull() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        // Set up an activity to be returned from ApplicationStatus.getLastTrackedFocusedActivity()
        // (which is what the bridge uses as a fallback if the WebContents is null).
        ChromeTabbedActivity activity = mock(ChromeTabbedActivity.class);
        when(activity.getSnackbarManager()).thenReturn(mSnackbarManager);
        when(activity.getString(
                        eq(R.string.send_tab_to_self_post_send_success_toast_android),
                        any(Object[].class)))
                .thenReturn("Sent to Chrome on your Pixel 10 • test@gmail.com");

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                null,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(null),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                "Sent to Chrome on your Pixel 10 • test@gmail.com",
                snackbarCaptor.getValue().getTextForTesting().toString());

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsFailureSnackbar_OnFailure() {
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                expectedMessage, snackbarCaptor.getValue().getTextForTesting().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsNoInternetSnackbar_OnNoInternetConnection() {
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                expectedMessage, snackbarCaptor.getValue().getTextForTesting().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsNoInternetSnackbar_OnCommitTimeout() {
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

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                expectedMessage, snackbarCaptor.getValue().getTextForTesting().toString());
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

        verify(mSnackbarManager, never()).showSnackbar(any());
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
        // Trigger the banner display logic.
        SendTabToSelfAndroidBridge.showMessageBanner(mWebContents, "Pixel 10", 1);

        // Capture the enqueued PropertyModel to verify its content and action callbacks.
        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(messageCaptor.capture(), eq(false));

        // Verify the static properties of the banner.
        PropertyModel model = messageCaptor.getValue();
        Assert.assertEquals(
                MessageIdentifier.SEND_TAB_TO_SELF,
                model.get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals("Link received", model.get(MessageBannerProperties.TITLE));
        Assert.assertEquals("From Pixel 10", model.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals("Open", model.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                R.drawable.send_tab, model.get(MessageBannerProperties.ICON_RESOURCE_ID));

        // Verify the ON_PRIMARY_ACTION callback behavior.
        Supplier<Integer> onPrimaryAction = model.get(MessageBannerProperties.ON_PRIMARY_ACTION);

        // Set up a mock ChromeTabbedActivity and LayoutManager to verify that the action attempts
        // to open the tab switcher.
        LayoutManagerChrome layoutManager = mock(LayoutManagerChrome.class);
        when(mTabbedActivity.getLayoutManager()).thenReturn(layoutManager);
        // Register the mock activity with ApplicationStatus so getLastTrackedFocusedActivity()
        // returns it.
        ApplicationStatus.onStateChangeForTesting(mTabbedActivity, ActivityState.CREATED);

        // Execute the primary action.
        int result = onPrimaryAction.get();

        // Verify that the banner dismisses immediately and showLayout is called to show the tab
        // switcher.
        Assert.assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, result);
        verify(layoutManager).showLayout(LayoutType.HUB, true);

        // Clean up global static state.
        ApplicationStatus.onStateChangeForTesting(mTabbedActivity, ActivityState.DESTROYED);
        MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
    }

    @Test
    @SmallTest
    public void testShowMessageBanner_ClickActionSingleTab_OpensTab() {
        // Trigger message banner display.
        SendTabToSelfAndroidBridge.showMessageBanner(mWebContents, "Pixel 10", 1);

        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(messageCaptor.capture(), eq(false));

        Supplier<Integer> onPrimaryAction =
                messageCaptor.getValue().get(MessageBannerProperties.ON_PRIMARY_ACTION);

        // Mock Activity elements.
        LayoutManagerChrome layoutManager = mock(LayoutManagerChrome.class);
        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        TabModel normalTabModel = mock(TabModel.class);

        when(mTabbedActivity.getLayoutManager()).thenReturn(layoutManager);
        when(mTabbedActivity.getTabModelSelector()).thenReturn(tabModelSelector);
        when(tabModelSelector.getModel(false)).thenReturn(normalTabModel);

        // Create a single tab matching search criteria.
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(100);
        UserDataHost userDataHost = new UserDataHost();
        when(tab.getUserDataHost()).thenReturn(userDataHost);
        SendTabToSelfAndroidBridge.attachTabLabel(tab, "guid", "Pixel 10");

        when(normalTabModel.getCount()).thenReturn(1);
        when(normalTabModel.getTabAt(0)).thenReturn(tab);

        ApplicationStatus.onStateChangeForTesting(mTabbedActivity, ActivityState.CREATED);

        // Execute primary action.
        int result = onPrimaryAction.get();

        // Verify result: return immediately to stay on the page.
        Assert.assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, result);
        // Verify tab is selected (index 0 for id 100).
        verify(normalTabModel).setIndex(eq(0), eq(TabSelectionType.FROM_USER));
        // Verify that the tab switcher is not opened.
        verify(layoutManager, never()).showLayout(any(Integer.class), any(Boolean.class));

        // Clean up.
        ApplicationStatus.onStateChangeForTesting(mTabbedActivity, ActivityState.DESTROYED);
        MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
    }

    @Test
    @SmallTest
    public void testShowMessageBanner_ClickActionMultipleTabs_OpensNewestTab() {
        // Trigger message banner display.
        SendTabToSelfAndroidBridge.showMessageBanner(mWebContents, "Pixel 10", 2);

        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(messageCaptor.capture(), eq(false));

        Assert.assertEquals(
                "2 links received",
                messageCaptor.getValue().get(MessageBannerProperties.TITLE));

        Supplier<Integer> onPrimaryAction =
                messageCaptor.getValue().get(MessageBannerProperties.ON_PRIMARY_ACTION);

        // Mock Activity elements.
        LayoutManagerChrome layoutManager = mock(LayoutManagerChrome.class);
        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        TabModel normalTabModel = mock(TabModel.class);

        when(mTabbedActivity.getLayoutManager()).thenReturn(layoutManager);
        when(mTabbedActivity.getTabModelSelector()).thenReturn(tabModelSelector);
        when(tabModelSelector.getModel(false)).thenReturn(normalTabModel);

        // Create two tabs matching search criteria, with different timestamps.
        // The second tab was added later (higher timestamp).
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(101);
        UserDataHost userDataHost1 = new UserDataHost();
        when(tab1.getUserDataHost()).thenReturn(userDataHost1);
        SendTabToSelfAndroidBridge.attachTabLabel(tab1, "guid1", "Pixel 10");
        // Manipulate timestamp to make it older (e.g. 10s ago).
        SendTabToSelfTabCardLabelData data1 =
                userDataHost1.getUserData(SendTabToSelfTabCardLabelData.class);
        data1.setAdditionTimestampMsForTesting(System.currentTimeMillis() - 10000);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(102);
        UserDataHost userDataHost2 = new UserDataHost();
        when(tab2.getUserDataHost()).thenReturn(userDataHost2);
        SendTabToSelfAndroidBridge.attachTabLabel(tab2, "guid2", "Pixel 10");
        // Maintain a newer timestamp on tab2 (e.g. 5s ago).
        SendTabToSelfTabCardLabelData data2 =
                userDataHost2.getUserData(SendTabToSelfTabCardLabelData.class);
        data2.setAdditionTimestampMsForTesting(System.currentTimeMillis() - 5000);

        when(normalTabModel.getCount()).thenReturn(2);
        when(normalTabModel.getTabAt(0)).thenReturn(tab1);
        when(normalTabModel.getTabAt(1)).thenReturn(tab2);

        ApplicationStatus.onStateChangeForTesting(mTabbedActivity, ActivityState.CREATED);

        // Execute primary action.
        int result = onPrimaryAction.get();

        // Verify result.
        Assert.assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, result);
        // Verify newest tab is selected (index 1 / id 102).
        verify(normalTabModel).setIndex(eq(1), eq(TabSelectionType.FROM_USER));
        // Verify older tab (id 101) is NEVER selected.
        verify(normalTabModel, never()).setIndex(eq(0), any(Integer.class));
        // Verify that the tab switcher is not opened.
        verify(layoutManager, never()).showLayout(any(Integer.class), any(Boolean.class));

        // Clean up.
        ApplicationStatus.onStateChangeForTesting(mTabbedActivity, ActivityState.DESTROYED);
        MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_SUPPORT_AUTO_OPEN_IN_TAB_GRID)
    public void testShowMessageBanner_InOverviewMode_DoesNotShow() {
        when(mTabbedActivity.isInOverviewMode()).thenReturn(true);

        // Trigger the banner display logic.
        SendTabToSelfAndroidBridge.showMessageBanner(mWebContents, "Pixel 10", 1);

        // Verify banner was never enqueued.
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), eq(false));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_SUPPORT_AUTO_OPEN_IN_TAB_GRID)
    public void testLabelObservers() {
        Tab tab = mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        when(tab.getUserDataHost()).thenReturn(userDataHost);

        SendTabToSelfAndroidBridge.LabelObjectObserver observer =
                mock(SendTabToSelfAndroidBridge.LabelObjectObserver.class);

        SendTabToSelfAndroidBridge.addLabelObserver(observer);
        try {
            SendTabToSelfAndroidBridge.attachTabLabel(tab, "guid", "Example Phone");
            verify(observer).onLabelAttached(tab);
        } finally {
            SendTabToSelfAndroidBridge.removeLabelObserver(observer);
        }
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_SUPPORT_AUTO_OPEN_IN_TAB_GRID)
    public void testLabelObservers_FeatureDisabled() {
        Tab tab = mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        when(tab.getUserDataHost()).thenReturn(userDataHost);

        SendTabToSelfAndroidBridge.LabelObjectObserver observer =
                mock(SendTabToSelfAndroidBridge.LabelObjectObserver.class);

        SendTabToSelfAndroidBridge.addLabelObserver(observer);
        try {
            SendTabToSelfAndroidBridge.attachTabLabel(tab, "guid", "Example Phone");
            verify(observer, never()).onLabelAttached(any());
        } finally {
            SendTabToSelfAndroidBridge.removeLabelObserver(observer);
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsToast_WhenSnackbarManagerUnavailable() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                null,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(null),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);

        Assert.assertEquals(
                "Sent to Chrome on your Pixel 10 • test@gmail.com",
                ShadowToast.getTextOfLatestToast());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsToast_OnFailure_WhenSnackbarManagerUnavailable() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                null,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                ShareEntryPoint.SHARE_SHEET);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(null),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture(),
                        eq(ShareEntryPoint.SHARE_SHEET));

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.FAILURE_INVALID_URL);

        Assert.assertEquals("Something went wrong. Try again.", ShadowToast.getTextOfLatestToast());
    }
}
