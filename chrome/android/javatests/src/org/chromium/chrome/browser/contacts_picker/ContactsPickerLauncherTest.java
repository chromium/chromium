// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.contacts_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.ContactsFetcher;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/** TestSuite for Chrome's Contacts Picker implementation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContactsPickerLauncherTest {
    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private boolean showContactsPicker(WebContents webContents) {
        ContactsFetcher fetcher =
                new ContactsFetcher() {
                    @Override
                    public @Nullable AsyncTask fetchContacts(
                            boolean includeNames,
                            boolean includeEmails,
                            boolean includeTel,
                            boolean includeAddresses,
                            ContactsRetrievedCallback callback) {
                        callback.contactsRetrieved(new ArrayList<>());
                        return null;
                    }

                    @Override
                    public @Nullable AsyncTask fetchIcon(
                            String id, int iconSize, IconRetrievedCallback callback) {
                        callback.iconRetrieved(null, id);
                        return null;
                    }
                };

        ContactsPickerListener listener =
                new ContactsPickerListener() {
                    @Override
                    public void onContactsPickerUserAction(
                            int action,
                            @Nullable List<Contact> contacts,
                            int percentageShared,
                            int propertiesSiteRequested,
                            int propertiesUserRejected) {
                        // Do nothing
                    }
                };

        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ContactsPicker.showContactsPicker(
                            webContents,
                            listener,
                            /* allowMultiple= */ true,
                            /* includeNames= */ true,
                            /* includeEmails= */ true,
                            /* includeTel= */ true,
                            /* includeAddresses= */ true,
                            /* includeIcons= */ true,
                            "https://example.com",
                            fetcher);
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

    @Test
    @LargeTest
    public void testPickerDismissedOnNavigation() throws Exception {
        WebPageStation firstPage = mActivityTestRule.startOnBlankPage();
        WebContents webContents = firstPage.webContentsElement.value();

        // Launch the picker.
        Assert.assertTrue(showContactsPicker(webContents));
        onView(withId(R.id.selectable_list)).check(matches(isDisplayed()));

        // Switch to a new page/tab.
        firstPage.openFakeLinkToWebPage(mActivityTestRule.getTestServer().getURL(FILE_PATH));

        // Verify the picker is dismissed and the observer is reset to null.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(withId(R.id.selectable_list)).check(doesNotExist());
                        Assert.assertNull(ContactsPicker.getObserverForTesting());
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }
}
