// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.PopupBlockedMessageFacility;
import org.chromium.chrome.test.transit.testhtmls.PopupOnClickPageStation;
import org.chromium.chrome.test.transit.testhtmls.PopupOnLoadPageStation;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;

/** Tests whether popup windows appear or get blocked as expected. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PopupPTTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mActivityTestRule);

    private static final String METADATA_FOR_ABUSIVE_ENFORCEMENT =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_absv\":\"\"}]}";

    private PageStation mEntryPage;

    @Before
    public void setUp() {
        SafeBrowsingApiBridge.setSafetyNetApiHandler(new MockSafetyNetApiHandler());
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
        mEntryPage = mEntryPoints.startOnBlankPage();
    }

    @After
    public void tearDown() {
        // TODO(crbug.com/329302688): Use ResettersForTesting instead.
        MockSafetyNetApiHandler.clearMockResponses();
        MockSafeBrowsingApiHandler.clearMockResponses();
        SafeBrowsingApiBridge.clearHandlerForTesting();
    }

    @Test
    @MediumTest
    public void testPopupOnLoadBlocked() {
        var pair =
                PopupOnLoadPageStation.loadInCurrentTabExpectBlocked(mActivityTestRule, mEntryPage);
        PopupOnLoadPageStation page = pair.first;
        PopupBlockedMessageFacility popupBlockedMessage = pair.second;

        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
        TransitAsserts.assertFinalDestination(page, popupBlockedMessage);
    }

    @Test
    @MediumTest
    public void testSafeGestureTabNotBlocked() {
        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(mActivityTestRule, mEntryPage);
        page = page.clickLinkToOpenPopup();

        assertEquals(2, mActivityTestRule.tabsCount(/* incognito= */ false));
        TransitAsserts.assertFinalDestination(page);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.SAFE_BROWSING_NEW_GMS_API_FOR_SUBRESOURCE_FILTER_CHECK)
    public void testAbusiveGesturePopupBlocked() {
        MockSafeBrowsingApiHandler.addMockResponse(
                mActivityTestRule.getTestServer().getURL(PopupOnClickPageStation.PATH),
                MockSafeBrowsingApiHandler.ABUSIVE_EXPERIENCE_VIOLATION_CODE);

        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(mActivityTestRule, mEntryPage);
        PopupBlockedMessageFacility popupBlockedMessage =
                page.clickLinkAndExpectPopupBlockedMessage();

        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
        TransitAsserts.assertFinalDestination(page, popupBlockedMessage);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SAFE_BROWSING_NEW_GMS_API_FOR_SUBRESOURCE_FILTER_CHECK)
    public void testAbusiveGesturePopupBlocked_NewGmsApiDisabled() {
        MockSafetyNetApiHandler.addMockResponse(
                mActivityTestRule.getTestServer().getURL(PopupOnClickPageStation.PATH),
                METADATA_FOR_ABUSIVE_ENFORCEMENT);

        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(mActivityTestRule, mEntryPage);
        PopupBlockedMessageFacility popupBlockedMessage =
                page.clickLinkAndExpectPopupBlockedMessage();

        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
        TransitAsserts.assertFinalDestination(page, popupBlockedMessage);
    }

    @Test
    @MediumTest
    public void testPopupWindowsAppearWhenAllowed() {
        PopupBlockedMessageFacility popupBlockedMessage =
                PopupOnLoadPageStation.loadInCurrentTabExpectBlocked(mActivityTestRule, mEntryPage)
                        .second;
        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));

        // Click the "Always allow" button.
        PageStation poppedUpPage = popupBlockedMessage.clickAlwaysAllow();
        assertEquals(3, mActivityTestRule.tabsCount(/* incognito= */ false));

        // Test that revisiting the original page makes pop-up windows show immediately.
        // The second pop-up opens navigate/page_two.html.
        PageStation pageTwo =
                PopupOnLoadPageStation.loadInCurrentTabExpectPopups(
                        mActivityTestRule, poppedUpPage);

        assertEquals(5, mActivityTestRule.tabsCount(/* incognito= */ false));
        TransitAsserts.assertFinalDestination(pageTwo);
    }
}
