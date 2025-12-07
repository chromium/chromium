// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.contacts_picker;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.WebContents;

/** TestSuite for Chrome's Contacts Picker implementation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContactsPickerLauncherTest {
    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

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
                            webContents.getMainFrame().getLastCommittedOrigin().getScheme(),
                            /* contactsFetcher= */ null);
                });
    }

    @Test
    @LargeTest
    public void testHandleNavigation() throws Exception {
        WebPageStation firstPage = mActivityTestRule.startOnBlankPage();
        WebContents webContents = firstPage.webContentsElement.value();

        // Switch to a new tab before the picker is launched.
        firstPage.openFakeLinkToWebPage(mActivityTestRule.getTestServer().getURL(FILE_PATH));

        // Check that the picker with the previous WebContents can't launch.
        Assert.assertFalse(ContactsPicker.canShowContactsPicker(webContents));
        Assert.assertFalse(showContactsPicker(webContents));
    }
}
