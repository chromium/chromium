// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtra;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtraWithKey;

import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.net.Uri;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;

/** Tests for SendTabToSelf related notifications. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SendTabToSelfNotificationTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private Profile mProfile;

    private final MockNotificationManagerProxy mMockNotificationManager =
            new MockNotificationManagerProxy();

    private static final String GUID = "guid";
    private static final String URL = "https://www.google.com/";
    private static final String TEXT_FRAGMENT = "selector";

    @Before
    public void setUp() {
        Intents.init();
        intending(anyIntent()).respondWith(new ActivityResult(Activity.RESULT_OK, null));
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        // markEntryOpened and dismissEntry are called by NotificationManager.
        doNothing().when(mNativeMock).markEntryOpened(any(), any());
        doNothing().when(mNativeMock).dismissEntry(any(), any());
    }

    @After
    public void tearDown() {
        Intents.release();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testNotificationTap_WithScrollToTextFragment() {
        Intent intent = new Intent();
        intent.setAction("send_tab_to_self.tap");
        intent.setData(Uri.parse(URL));
        intent.putExtra("send_tab_to_self.notification.guid", GUID);
        intent.putExtra(IntentHandler.EXTRA_SCROLL_TO_TEXT_FRAGMENT, TEXT_FRAGMENT);

        ThreadUtils.runOnUiThreadBlocking(() -> NotificationManager.handleIntent(intent));

        intended(hasComponent(ChromeLauncherActivity.class.getName()));
        intended(hasData(URL));
        intended(hasExtra(IntentHandler.EXTRA_SCROLL_TO_TEXT_FRAGMENT, TEXT_FRAGMENT));
        verify(mNativeMock).markEntryOpened(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testNotificationTap_WithoutScrollToTextFragment() {
        Intent intent = new Intent();
        intent.setAction("send_tab_to_self.tap");
        intent.setData(Uri.parse(URL));
        intent.putExtra("send_tab_to_self.notification.guid", GUID);

        ThreadUtils.runOnUiThreadBlocking(() -> NotificationManager.handleIntent(intent));

        intended(hasComponent(ChromeLauncherActivity.class.getName()));
        intended(hasData(URL));
        intended(not(hasExtraWithKey(IntentHandler.EXTRA_SCROLL_TO_TEXT_FRAGMENT)));
        verify(mNativeMock).markEntryOpened(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testNotificationTap_FeatureDisabled() {
        Intent intent = new Intent();
        intent.setAction("send_tab_to_self.tap");
        intent.setData(Uri.parse(URL));
        intent.putExtra("send_tab_to_self.notification.guid", GUID);
        intent.putExtra(IntentHandler.EXTRA_SCROLL_TO_TEXT_FRAGMENT, TEXT_FRAGMENT);

        ThreadUtils.runOnUiThreadBlocking(() -> NotificationManager.handleIntent(intent));

        intended(hasComponent(ChromeLauncherActivity.class.getName()));
        intended(hasData(URL));
        intended(not(hasExtraWithKey(IntentHandler.EXTRA_SCROLL_TO_TEXT_FRAGMENT)));
        verify(mNativeMock).markEntryOpened(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testNotificationDismiss() {
        Intent intent = new Intent();
        intent.setAction("send_tab_to_self.dismiss");
        intent.putExtra("send_tab_to_self.notification.guid", GUID);

        ThreadUtils.runOnUiThreadBlocking(() -> NotificationManager.handleIntent(intent));

        verify(mNativeMock).dismissEntry(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testNotificationTimeout() {
        Intent intent = new Intent();
        intent.setAction("send_tab_to_self.timeout");
        intent.putExtra("send_tab_to_self.notification.guid", GUID);

        ThreadUtils.runOnUiThreadBlocking(() -> NotificationManager.handleIntent(intent));

        verify(mNativeMock).dismissEntry(eq(mProfile), eq(GUID));
    }
}
