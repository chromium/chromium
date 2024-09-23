// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.app.NotificationManager.IMPORTANCE_NONE;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS_QUIET;

import android.app.NotificationChannel;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TwaDisclosureUi;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;

/** Tests for {@link DisclosureUiPicker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O)
public class DisclosureUiPickerTest {

    @Mock public DisclosureInfobar mInfobar;
    @Mock public DisclosureSnackbar mSnackbar;
    @Mock public DisclosureNotification mNotification;

    @Mock public BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock public NotificationManagerProxy mNotificationManager;
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    private DisclosureUiPicker mPicker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mIntentDataProvider.getTwaDisclosureUi()).thenReturn(TwaDisclosureUi.DEFAULT);

        mPicker =
                new DisclosureUiPicker(
                        new FilledLazy<>(mInfobar),
                        new FilledLazy<>(mSnackbar),
                        new FilledLazy<>(mNotification),
                        mIntentDataProvider,
                        mNotificationManager,
                        mLifecycleDispatcher);
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
    public void picksSnackbar_whenAutomotive() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        setChannelEnabled(WEBAPPS, true);
        setChannelEnabled(WEBAPPS_QUIET, true);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    private void setNotificationsEnabled(boolean enabled) {
        when(mNotificationManager.areNotificationsEnabled()).thenReturn(enabled);
    }

    private void setChannelEnabled(String channelId, boolean enabled) {
        NotificationChannel channel = Mockito.mock(NotificationChannel.class);
        when(channel.getImportance()).thenReturn(enabled ? IMPORTANCE_DEFAULT : IMPORTANCE_NONE);
        when(mNotificationManager.getNotificationChannel(eq(channelId))).thenReturn(channel);
    }
}
