// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.contacts_picker;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.atomic.AtomicBoolean;

/** TestSuite for Chrome's Contacts Picker implementation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContactsPickerLauncherTest {
    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        loadNative();
    }

    // Based on BackgroundMetricsTest.java
    private void loadNative() {
        final AtomicBoolean mNativeLoaded = new AtomicBoolean();
        final BrowserParts parts =
                new EmptyBrowserParts() {
                    @Override
                    public void finishNativeInitialization() {
                        mNativeLoaded.set(true);
                    }
                };
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    ChromeBrowserInitializer.getInstance()
                            .handlePreNativeStartupAndLoadLibraries(parts);
                    ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
                });
        CriteriaHelper.pollUiThread(
                () -> mNativeLoaded.get(), "Failed while waiting for starting native.");
    }

    private boolean showContactsPicker(WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ContactsPicker.showContactsPicker(
                            webContents,
                            /* listener= */ null,
                            /* allowMultiple= */ true,
                            /* includeNames= */ true,
                            /* includeEmails= */ true,
                            /* includeTel= */ true,
                            /* includeAddresses= */ true,
                            /* includeIcons= */ true,
                            webContents.getMainFrame().getLastCommittedOrigin().getScheme());
                });
    }

    @Test
    @LargeTest
    public void testHandleNavigation() throws Exception {
        String url = mTestServer.getURL(FILE_PATH);

        mActivityTestRule.loadUrlInNewTab(url);
        WebContents webContents = mActivityTestRule.getWebContents();

        // Switch to a new tab before the picker is launched.
        mActivityTestRule.loadUrlInNewTab(url);

        // Check that the picker with the previous WebContents can't launch.
        Assert.assertFalse(ContactsPicker.canShowContactsPicker(webContents));
        Assert.assertFalse(showContactsPicker(webContents));
    }
}
