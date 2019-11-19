// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
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
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.DropdownPopupWindowInterface;
import org.chromium.ui.R;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the AutofillPopup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@DisableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillPopupTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String FIRST_NAME = "John";
    private static final String LAST_NAME = "Smith";
    private static final String COMPANY_NAME = "Acme Inc.";
    private static final String ADDRESS_LINE1 = "1 Main";
    private static final String ADDRESS_LINE2 = "Apt A";
    private static final String STREET_ADDRESS_TEXTAREA = ADDRESS_LINE1 + "\n" + ADDRESS_LINE2;
    private static final String CITY = "San Francisco";
    private static final String DEPENDENT_LOCALITY = "";
    private static final String STATE = "CA";
    private static final String ZIP_CODE = "94102";
    private static final String SORTING_CODE = "";
    private static final String COUNTRY = "US";
    private static final String PHONE_NUMBER = "4158889999";
    private static final String EMAIL = "john@acme.inc";
    private static final String LANGUAGE_CODE = "";
    private static final String ORIGIN = "https://www.example.com";

    private static final String BASIC_PAGE_DATA = UrlUtils.encodeHtmlDataUri("<html><head>"
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
            + "</form></body></html>");

    private static final String INITIATING_ELEMENT_FILLED = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
            + "<body><form method=\"POST\">"
            + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" value=\"J\"><br>"
            + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\"><br>"
            + "<input type=\"text\" id=\"em\" autocomplete=\"email\"><br>"
            + "<select id=\"co\" autocomplete=\"country\"><br>"
            + "<option value=\"US\">United States</option>"
            + "<option value=\"BR\">Brazil</option>"
            + "</select>"
            + "<input type=\"submit\" />"
            + "</form></body></html>");

    private static final String ANOTHER_ELEMENT_FILLED = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
            + "<body><form method=\"POST\">"
            + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\"><br>"
            + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\"><br>"
            + "<input type=\"text\" id=\"em\" autocomplete=\"email\" value=\"foo@example.com\"><br>"
            + "<select id=\"co\" autocomplete=\"country\"><br>"
            + "<option></option>"
            + "<option value=\"BR\">Brazil</option>"
            + "<option value=\"US\">United States</option>"
            + "</select>"
            + "<input type=\"submit\" />"
            + "</form></body></html>");

    private static final String INVALID_OPTION = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
            + "<body><form method=\"POST\">"
            + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" value=\"J\"><br>"
            + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\"><br>"
            + "<input type=\"text\" id=\"em\" autocomplete=\"email\"><br>"
            + "<select id=\"co\" autocomplete=\"country\"><br>"
            + "<option value=\"GB\">Great Britain</option>"
            + "<option value=\"BR\">Brazil</option>"
            + "</select>"
            + "<input type=\"submit\" />"
            + "</form></body></html>");

    private AutofillTestHelper mHelper;
    private List<AutofillLogger.LogEntry> mAutofillLoggedEntries;

    @Before
    public void setUp() {
        mAutofillLoggedEntries = new ArrayList<AutofillLogger.LogEntry>();
        AutofillLogger.setLoggerForTesting(
                logEntry -> mAutofillLoggedEntries.add(logEntry)
        );
        // TODO(crbug.com/894428) - fix this suite to use the embedded test server instead of
        // data urls.
        Features.getInstance().enable(ChromeFeatureList.AUTOFILL_ALLOW_NON_HTTP_ACTIVATION);
    }

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void loadForm(final String formDataUrl, final String inputText,
            @Nullable Callback<Activity> updateActivity) throws TimeoutException {
        mActivityTestRule.startMainActivityWithURL(formDataUrl);
        if (updateActivity != null) {
            updateActivity.onResult(mActivityTestRule.getActivity());
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        }

        // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard is never
        // brought up.
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();
        final ImeAdapter imeAdapter = WebContentsUtils.getImeAdapter(webContents);
        TestInputMethodManagerWrapper immw = TestInputMethodManagerWrapper.create(imeAdapter);
        imeAdapter.setInputMethodManagerWrapper(immw);

        // Add an Autofill profile.
        mHelper = new AutofillTestHelper();
        AutofillProfile profile = new AutofillProfile(
                "" /* guid */, ORIGIN, FIRST_NAME + " " + LAST_NAME, COMPANY_NAME,
                STREET_ADDRESS_TEXTAREA,
                STATE, CITY, DEPENDENT_LOCALITY,
                ZIP_CODE, SORTING_CODE, COUNTRY, PHONE_NUMBER, EMAIL,
                LANGUAGE_CODE);
        mHelper.setProfile(profile);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesToSuggest());

        // Click the input field for the first name.
        DOMUtils.waitForNonZeroNodeBounds(webContents, "fn");
        DOMUtils.clickNode(webContents, "fn");

        waitForKeyboardShowRequest(immw, 1);

        imeAdapter.setComposingTextForTest(inputText, 1);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void loadAndFillForm(final String formDataUrl, final String inputText,
            @Nullable Callback<Activity> updateActivity) throws TimeoutException {
        loadForm(formDataUrl, inputText, updateActivity);

        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();
        final ViewGroup view = webContents.getViewAndroidDelegate().getContainerView();
        waitForAnchorViewAdd(view);
        View anchorView = view.findViewById(R.id.dropdown_popup_window);

        Assert.assertTrue(anchorView.getTag() instanceof DropdownPopupWindowInterface);
        final DropdownPopupWindowInterface popup =
                (DropdownPopupWindowInterface) anchorView.getTag();

        waitForAutofillPopupShow(popup);

        TouchCommon.singleClickView(popup.getListView(), 10, 10);

        waitForInputFieldFill();
    }

    private void loadAndFillForm(final String formDataUrl, final String inputText)
            throws TimeoutException {
        loadAndFillForm(formDataUrl, inputText, null);
    }

    /**
     * Tests that bringing up an Autofill and clicking on the first entry fills out the expected
     * Autofill information.
     */
    @Test
    @MediumTest
    @Feature({"autofill"})
    @DisabledTest(message = "Flaky. crbug.com/936183")
    public void testClickAutofillPopupSuggestion() throws TimeoutException {
        loadAndFillForm(BASIC_PAGE_DATA, "J");
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        Assert.assertEquals(
                "First name did not match", FIRST_NAME, DOMUtils.getNodeValue(webContents, "fn"));
        Assert.assertEquals(
                "Last name did not match", LAST_NAME, DOMUtils.getNodeValue(webContents, "ln"));
        Assert.assertEquals("Street address (textarea) did not match", STREET_ADDRESS_TEXTAREA,
                DOMUtils.getNodeValue(webContents, "sa"));
        Assert.assertEquals("Address line 1 did not match", ADDRESS_LINE1,
                DOMUtils.getNodeValue(webContents, "a1"));
        Assert.assertEquals("Address line 2 did not match", ADDRESS_LINE2,
                DOMUtils.getNodeValue(webContents, "a2"));
        Assert.assertEquals("City did not match", CITY, DOMUtils.getNodeValue(webContents, "ct"));
        Assert.assertEquals(
                "Zip code did not match", ZIP_CODE, DOMUtils.getNodeValue(webContents, "zc"));
        Assert.assertEquals(
                "Country did not match", COUNTRY, DOMUtils.getNodeValue(webContents, "co"));
        Assert.assertEquals("Email did not match", EMAIL, DOMUtils.getNodeValue(webContents, "em"));
        Assert.assertEquals("Phone number did not match", PHONE_NUMBER,
                DOMUtils.getNodeValue(webContents, "ph"));

        final String profileFullName = FIRST_NAME + " " + LAST_NAME;
        final int loggedEntries = 10;
        Assert.assertEquals("Mismatched number of logged entries", loggedEntries,
                mAutofillLoggedEntries.size());
        assertLogged(FIRST_NAME, profileFullName);
        assertLogged(LAST_NAME, profileFullName);
        assertLogged(STREET_ADDRESS_TEXTAREA, profileFullName);
        assertLogged(ADDRESS_LINE1, profileFullName);
        assertLogged(ADDRESS_LINE2, profileFullName);
        assertLogged(CITY, profileFullName);
        assertLogged(ZIP_CODE, profileFullName);
        assertLogged(COUNTRY, profileFullName);
        assertLogged(EMAIL, profileFullName);
        assertLogged(PHONE_NUMBER, profileFullName);
    }

    /**
     * Tests that bringing up an Autofill and clicking on the partially filled first
     * element will still fill the entire form (including the initiating element itself).
     */
    @Test
    @MediumTest
    @Feature({"autofill"})
    public void testLoggingInitiatedElementFilled() throws TimeoutException {
        loadAndFillForm(INITIATING_ELEMENT_FILLED, "o");
        final String profileFullName = FIRST_NAME + " " + LAST_NAME;
        final int loggedEntries = 4;
        Assert.assertEquals("Mismatched number of logged entries", loggedEntries,
                mAutofillLoggedEntries.size());
        assertLogged(FIRST_NAME, profileFullName);
        assertLogged(LAST_NAME, profileFullName);
        assertLogged(EMAIL, profileFullName);
        assertLogged(COUNTRY, profileFullName);
    }

    /**
     * Tests that bringing up an Autofill and clicking on the empty first element
     * will fill the all other elements except the previously filled email.
     */
    @Test
    @MediumTest
    @Feature({"autofill"})
    public void testLoggingAnotherElementFilled() throws TimeoutException {
        loadAndFillForm(ANOTHER_ELEMENT_FILLED, "J");
        final String profileFullName = FIRST_NAME + " " + LAST_NAME;
        final int loggedEntries = 3;
        Assert.assertEquals("Mismatched number of logged entries", loggedEntries,
                mAutofillLoggedEntries.size());
        assertLogged(FIRST_NAME, profileFullName);
        assertLogged(LAST_NAME, profileFullName);
        assertLogged(COUNTRY, profileFullName);
        // Email will not be logged since it already had some data.
    }

    /**
     * Tests that selecting a value not present in <option> will not be filled.
     */
    @Test
    @MediumTest
    @Feature({"autofill"})
    public void testNotLoggingInvalidOption() throws TimeoutException {
        loadAndFillForm(INVALID_OPTION, "o");
        final String profileFullName = FIRST_NAME + " " + LAST_NAME;
        final int loggedEntries = 3;
        Assert.assertEquals("Mismatched number of logged entries", loggedEntries,
                mAutofillLoggedEntries.size());
        assertLogged(FIRST_NAME, profileFullName);
        assertLogged(LAST_NAME, profileFullName);
        assertLogged(EMAIL, profileFullName);
        // Country will not be logged since "US" is not a valid <option>.
    }

    @Test
    @MediumTest
    @Feature({"autofill"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_REFRESH_STYLE_ANDROID)
    public void testScreenOrientationPortrait() throws TimeoutException {
        runTestScreenOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    @Test
    @MediumTest
    @Feature({"autofill"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_REFRESH_STYLE_ANDROID)
    public void testScreenOrientationLandscape() throws TimeoutException {
        runTestScreenOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    private void runTestScreenOrientation(int orientation) throws TimeoutException {
        // TODO(crbug.com/905081): Also test different screen sizes.
        loadForm(BASIC_PAGE_DATA, "J", activity -> activity.setRequestedOrientation(orientation));

        ChromeActivity activity = mActivityTestRule.getActivity();
        final WebContents webContents = activity.getCurrentWebContents();
        final Configuration config = activity.getResources().getConfiguration();
        final boolean shouldShowPopup = config.orientation == Configuration.ORIENTATION_PORTRAIT
                || config.isLayoutSizeAtLeast(Configuration.SCREENLAYOUT_SIZE_XLARGE);
        final ViewGroup view = webContents.getViewAndroidDelegate().getContainerView();
        if (shouldShowPopup) {
            waitForAnchorViewAdd(view);
        } else {
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        }
        final View popup = view.findViewById(R.id.dropdown_popup_window);

        final String message = "Mismatched dropdown_popup_window for orientation: "
                + (orientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE ? "landscape"
                                                                            : "portrait");
        if (shouldShowPopup) {
            Assert.assertNotNull(message, popup);
        } else {
            Assert.assertNull(message, popup);
        }
    }

    // Wait and assert helper methods -------------------------------------------------------------

    private void waitForKeyboardShowRequest(final TestInputMethodManagerWrapper immw,
            final int count) {
        CriteriaHelper.pollUiThread(
                Criteria.equals(count, () -> immw.getShowSoftInputCounter()));
    }

    private void waitForAnchorViewAdd(final ViewGroup view) {
        CriteriaHelper.pollUiThread(new Criteria(
                "Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                return view.findViewById(R.id.dropdown_popup_window) != null;
            }
        });
    }

    private void waitForAutofillPopupShow(final DropdownPopupWindowInterface popup) {
        CriteriaHelper.pollUiThread(
                new Criteria("Autofill Popup anchor view was never added.") {
                    @Override
                    public boolean isSatisfied() {
                        // Wait until the popup is showing and onLayout() has happened.
                        return popup.isShowing() && popup.getListView() != null
                                && popup.getListView().getHeight() != 0;
                    }
                });
    }

    private void waitForInputFieldFill() {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("First name field was never filled.") {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            return TextUtils.equals(FIRST_NAME,
                                    DOMUtils.getNodeValue(
                                            mActivityTestRule.getActivity().getCurrentWebContents(),
                                            "fn"));
                        } catch (TimeoutException e) {
                            return false;
                        }
                    }
                });
    }

    private void assertLogged(String autofilledValue, String profileFullName) {
        for (AutofillLogger.LogEntry entry : mAutofillLoggedEntries) {
            if (entry.getAutofilledValue().equals(autofilledValue)
                    && entry.getProfileFullName().equals(profileFullName)) {
                return;
            }
        }
        Assert.fail("Logged entry not found [" + autofilledValue + "," + profileFullName + "]");
    }
}
