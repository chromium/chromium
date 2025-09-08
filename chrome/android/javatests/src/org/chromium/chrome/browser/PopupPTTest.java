// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.FixMethodOrder;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.PopupBlockedMessageFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.BlankPopupOnLoadPageStation;
import org.chromium.chrome.test.transit.testhtmls.PopupOnClickPageStation;
import org.chromium.chrome.test.transit.testhtmls.PopupOnLoadPageStation;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;

/** Tests whether popup windows appear or get blocked as expected. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class PopupPTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private static final String METADATA_FOR_ABUSIVE_ENFORCEMENT =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_absv\":\"\"}]}";

    private WebPageStation mEntryPage;

    @BeforeClass
    public static void setUpClass() {
        // TODO(crbug.com/329302688): Use ResettersForTesting instead.
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
    }

    @AfterClass
    public static void tearDownClass() {
        SafeBrowsingApiBridge.clearHandlerForTesting();
    }

    @Before
    public void setUp() {
        mEntryPage = mCtaTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        MockSafeBrowsingApiHandler.clearMockResponses();
    }

    @Test
    @MediumTest
    public void test010PopupOnLoadBlocked() {
        var pair =
                PopupOnLoadPageStation.loadInCurrentTabExpectBlocked(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);
        PopupOnLoadPageStation page = pair.first;
        PopupBlockedMessageFacility popupBlockedMessage = pair.second;

        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ false));

        popupBlockedMessage.dismiss();

        // Test that dismissing does not allow the popups and they get blocked again
        pair =
                PopupOnLoadPageStation.loadInCurrentTabExpectBlocked(
                        mCtaTestRule.getActivityTestRule(), page);
        page = pair.first;
        popupBlockedMessage = pair.second;

        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertFinalDestination(page, popupBlockedMessage);
    }

    @Test
    @MediumTest
    public void test020SafeGestureTabNotBlocked() {
        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);
        page = page.clickLinkToOpenPopup();

        assertEquals(2, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertFinalDestination(page);
    }

    // Run last in the batch because clicking "Always allow" will cause the popups to be allowed
    // in the next tests as well.
    @Test
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER)
    @MediumTest
    public void test900PopupWindowsAppearWhenAllowed() {
        PopupBlockedMessageFacility popupBlockedMessage =
                PopupOnLoadPageStation.loadInCurrentTabExpectBlocked(
                                mCtaTestRule.getActivityTestRule(), mEntryPage)
                        .second;
        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ false));

        // Click the "Always allow" button.
        WebPageStation poppedUpPage = popupBlockedMessage.clickAlwaysAllow();
        assertEquals(3, mCtaTestRule.tabsCount(/* incognito= */ false));

        // Test that revisiting the original page makes pop-up windows show immediately.
        // The second pop-up opens navigate/page_two.html.
        WebPageStation pageTwo =
                PopupOnLoadPageStation.loadInCurrentTabExpectPopups(
                        mCtaTestRule.getActivityTestRule(), poppedUpPage);

        assertEquals(5, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertFinalDestination(pageTwo);
    }

    @Test
    @MediumTest
    public void test030AbusiveGesturePopupBlocked() {
        MockSafeBrowsingApiHandler.addMockResponse(
                mCtaTestRule.getTestServer().getURL(PopupOnClickPageStation.PATH),
                MockSafeBrowsingApiHandler.ABUSIVE_EXPERIENCE_VIOLATION_CODE);

        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);
        PopupBlockedMessageFacility popupBlockedMessage =
                page.clickLinkAndExpectPopupBlockedMessage();

        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertFinalDestination(page, popupBlockedMessage);
    }

    // Regression test for crbug.com/413341816.
    @Test
    @MediumTest
    @RequiresRestart
    public void testBlankPopupLaunchedFromBlockedChip() {
        PopupBlockedMessageFacility popupBlockedMessage =
                BlankPopupOnLoadPageStation.loadInCurrentTabExpectBlocked(
                                mCtaTestRule.getActivityTestRule(), mEntryPage)
                        .second;
        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ false));

        // Click the "Always allow" button.
        WebPageStation poppedUpPage = popupBlockedMessage.clickAlwaysAllow();
        assertEquals(2, mCtaTestRule.tabsCount(/* incognito= */ false));

        assertFinalDestination(poppedUpPage);
    }
}
