// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.support.test.filters.MediumTest;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.DropdownPopupWindowInterface;
import org.chromium.ui.R;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for interaction of the AutofillPopup and a keyboard.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillPopupWithKeyboardTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() {
        // TODO(crbug.com/894428) - fix this suite to use the embedded test server instead of
        // data urls.
        Features.getInstance().enable(ChromeFeatureList.AUTOFILL_ALLOW_NON_HTTP_ACTIVATION);
    }

    /**
     * Test that showing autofill popup and keyboard will not hide the autofill popup.
     */
    @Test
    @MediumTest
    @Feature({"autofill-keyboard"})
    @RetryOnFailure
    @DisabledTest
    public void testShowAutofillPopupAndKeyboardimultaneously() throws TimeoutException {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" /><br>"
                + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\" /><br>"
                + "<textarea id=\"sa\" autocomplete=\"street-address\"></textarea><br>"
                + "<input type=\"text\" id=\"a1\" autocomplete=\"address-line1\" /><br>"
                + "<input type=\"text\" id=\"a2\" autocomplete=\"address-line2\" /><br>"
                + "<input type=\"text\" id=\"ct\" autocomplete=\"locality\" /><br>"
                + "<input type=\"text\" id=\"zc\" autocomplete=\"postal-code\" /><br>"
                + "<input type=\"text\" id=\"em\" autocomplete=\"email\" /><br>"
                + "<input type=\"text\" id=\"ph\" autocomplete=\"tel\" /><br>"
                + "<input type=\"text\" id=\"fx\" autocomplete=\"fax\" /><br>"
                + "<select id=\"co\" autocomplete=\"country\"><br>"
                + "<option value=\"BR\">Brazil</option>"
                + "<option value=\"US\">United States</option>"
                + "</select>"
                + "<input type=\"submit\" />"
                + "</form></body></html>"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "John Smith", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "", "94102", "",
                "US", "(415) 888-9999", "john@acme.inc", "en"));
        final AtomicReference<WebContents> webContentsRef = new AtomicReference<WebContents>();
        final AtomicReference<ViewGroup> viewRef = new AtomicReference<ViewGroup>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            webContentsRef.set(mActivityTestRule.getActivity().getCurrentWebContents());
            viewRef.set(mActivityTestRule.getActivity().getActivityTab().getContentView());
        });
        DOMUtils.waitForNonZeroNodeBounds(webContentsRef.get(), "fn");

        // Click on the unfocused input element for the first time to focus on it. This brings up
        // the autofill popup and shows the keyboard at the same time. Showing the keyboard should
        // not hide the autofill popup.
        DOMUtils.clickNode(webContentsRef.get(), "fn");

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(new Criteria("Keyboard was never shown.") {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        mActivityTestRule.getActivity(),
                        mActivityTestRule.getActivity().getActivityTab().getContentView());
            }
        });

        // Verify that the autofill popup is showing.
        CriteriaHelper.pollUiThread(
                new Criteria("Autofill Popup anchor view was never added.") {
                    @Override
                    public boolean isSatisfied() {
                        return viewRef.get().findViewById(R.id.dropdown_popup_window) != null;
                    }
                });
        Object popupObject = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> viewRef.get().findViewById(R.id.dropdown_popup_window).getTag());
        Assert.assertTrue(popupObject instanceof DropdownPopupWindowInterface);
        final DropdownPopupWindowInterface popup = (DropdownPopupWindowInterface) popupObject;
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup was never shown.") {
            @Override
            public boolean isSatisfied() {
                return popup.isShowing();
            }
        });
    }
}
