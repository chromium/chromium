// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.app.NotificationManager.IMPORTANCE_NONE;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS_QUIET;

import android.app.NotificationChannel;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TwaDisclosureUi;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link DisclosureUiPicker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisclosureUiPickerTest {

    @Mock public DisclosureInfobar mInfobar;
    @Mock public DisclosureSnackbar mSnackbar;
    @Mock public DisclosureNotification mNotification;

    @Mock public BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock public NotificationManagerProxy mNotificationManager;
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    private DisclosureUiPicker mPicker;
    private final List<NotificationChannel> mEnabledChannels = new ArrayList<>();

    @Before
    public void setUp() {

        when(mIntentDataProvider.getTwaDisclosureUi()).thenReturn(TwaDisclosureUi.DEFAULT);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManager);
        mPicker =
                new DisclosureUiPicker(
                        () -> mInfobar,
                        () -> mSnackbar,
                        () -> mNotification,
                        mIntentDataProvider,
                        mLifecycleDispatcher);
        doAnswer(
                        (invocation) -> {
                            Callback<List<NotificationChannel>> callback =
                                    invocation.getArgument(0);
                            callback.onResult(mEnabledChannels);
                            return null;
                        })
                .when(mNotificationManager)
                .getNotificationChannels(any(Callback.class));
    }

    @After
    public void tearDown() {
        mEnabledChannels.clear();
        NotificationProxyUtils.setNotificationEnabledForTest(null);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void pickInfobar_whenRequested() {
        when(mIntentDataProvider.getTwaDisclosureUi()).thenReturn(TwaDisclosureUi.V1_INFOBAR);

        mPicker.onFinishNativeInitialization();
        verify(mInfobar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksSnackbar_whenAllNotificationsDisabled() {
        setNotificationsEnabled(false);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksSnackbar_whenInitialChannelIsDisabled() {
        setNotificationsEnabled(true);
        setChannelEnabled(WEBAPPS, false);
        setChannelEnabled(WEBAPPS_QUIET, true);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksSnackbar_whenSubsequentChannelIsDisabled() {
        setNotificationsEnabled(true);
        setChannelEnabled(WEBAPPS, true);
        setChannelEnabled(WEBAPPS_QUIET, false);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksNotification() {
        setNotificationsEnabled(true);
        setChannelEnabled(WEBAPPS, true);
        setChannelEnabled(WEBAPPS_QUIET, true);

        mPicker.onFinishNativeInitialization();
        verify(mNotification).onStartWithNative();
    }

    @Test
    @Feature("TrustedWebActivities")
    @DisabledTest // This needs to be re-worked for Q.
    public void picksSnackbar_whenAutomotive() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        setChannelEnabled(WEBAPPS, true);
        setChannelEnabled(WEBAPPS_QUIET, true);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    private void setNotificationsEnabled(boolean enabled) {
        NotificationProxyUtils.setNotificationEnabledForTest(enabled);
    }

    private void setChannelEnabled(String channelId, boolean enabled) {
        NotificationChannel channel = Mockito.mock(NotificationChannel.class);
        when(channel.getImportance()).thenReturn(enabled ? IMPORTANCE_DEFAULT : IMPORTANCE_NONE);
        when(channel.getId()).thenReturn(channelId);
        mEnabledChannels.add(channel);
    }
}
