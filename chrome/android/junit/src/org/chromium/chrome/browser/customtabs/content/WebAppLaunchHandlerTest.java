// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.trusted.LaunchHandlerClientMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.url.JUnitTestGURLs;

import java.util.Objects;

/** Tests for {@link WebAppLaunchHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER})
public class WebAppLaunchHandlerTest {

    @Test
    public void getClientMode() {
        int clientMode =
                WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.NAVIGATE_EXISTING);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_EXISTING, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.FOCUS_EXISTING);
        Assert.assertEquals(LaunchHandlerClientMode.FOCUS_EXISTING, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.NAVIGATE_NEW);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_NEW, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.AUTO);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_EXISTING, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(45); // Invalid argument
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_EXISTING, clientMode);
    }

    @Test
    public void getStartNewNavigation() {
        String url = JUnitTestGURLs.INITIAL_URL.getSpec();
        String packageName = null;
        WebAppLaunchHandler launchHandler =
                new WebAppLaunchHandler(
                        LaunchHandlerClientMode.NAVIGATE_EXISTING, url, packageName);
        Assert.assertTrue(launchHandler.getStartNewNavigation());

        launchHandler =
                new WebAppLaunchHandler(LaunchHandlerClientMode.FOCUS_EXISTING, url, packageName);
        Assert.assertFalse(launchHandler.getStartNewNavigation());

        launchHandler =
                new WebAppLaunchHandler(LaunchHandlerClientMode.NAVIGATE_NEW, url, packageName);
        Assert.assertTrue(launchHandler.getStartNewNavigation());

        launchHandler = new WebAppLaunchHandler(LaunchHandlerClientMode.AUTO, url, packageName);
        Assert.assertTrue(launchHandler.getStartNewNavigation());

        launchHandler = new WebAppLaunchHandler(65, url, packageName);
        Assert.assertTrue(launchHandler.getStartNewNavigation());
    }

    void doTestClientModeStartNewTask(Integer clientMode, int startActivityExpectedCallsNumber) {
        final String exampleUrl = "https://www.example.com";

        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, exampleUrl);

        // If a custom tab intent has a client mode it will be resent with flags to start a new task
        Activity mockActivity = mock(Activity.class);

        if (clientMode != null) {
            intent.putExtra(
                    TrustedWebActivityIntentBuilder.EXTRA_LAUNCH_HANDLER_CLIENT_MODE, clientMode);
        }

        CustomTabsSessionToken sessionToken =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        SessionHolder<CustomTabsSessionToken> session = new SessionHolder<>(sessionToken);
        CustomTabsConnection.getInstance().newSession(sessionToken);
        CustomTabsConnection.getInstance()
                .overridePackageNameForSessionForTesting(session, "com.test_package");
        LaunchIntentDispatcher.dispatchToCustomTabActivity(mockActivity, intent);

        verify(mockActivity, times(startActivityExpectedCallsNumber))
                .startActivity(
                        argThat(
                                params -> {
                                    if (params == null
                                            || params.getData() == null
                                            || params.getAction() == null) {
                                        return false;
                                    }
                                    return Objects.equals(params.getAction(), Intent.ACTION_VIEW)
                                            && exampleUrl.equals(intent.getData().toString())
                                            && (params.getFlags()
                                                            & Intent.FLAG_ACTIVITY_MULTIPLE_TASK)
                                                    != 0
                                            && (params.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK)
                                                    != 0;
                                }));
    }

    @Test
    public void navigateNewStartNewTask() {
        doTestClientModeStartNewTask(
                LaunchHandlerClientMode.NAVIGATE_NEW, /* startActivityExpectedCallsNumber= */ 1);
    }

    @Test
    public void navigateNewStartNewTask_noClientMode() {
        doTestClientModeStartNewTask(
                /* clientMode= */ null, /* startActivityExpectedCallsNumber= */ 0);
    }

    @Test
    public void navigateNewStartNewTask_anotherClientMode() {
        doTestClientModeStartNewTask(
                LaunchHandlerClientMode.NAVIGATE_EXISTING,
                /* startActivityExpectedCallsNumber= */ 0);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER})
    public void navigateNewStartNewTask_featureIsDisabled() {
        doTestClientModeStartNewTask(
                LaunchHandlerClientMode.NAVIGATE_NEW, /* startActivityExpectedCallsNumber= */ 0);
    }
}
