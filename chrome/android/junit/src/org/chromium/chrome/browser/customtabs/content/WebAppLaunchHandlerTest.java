// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.browser.trusted.LaunchHandlerClientMode;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link WebAppLaunchHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
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
}
