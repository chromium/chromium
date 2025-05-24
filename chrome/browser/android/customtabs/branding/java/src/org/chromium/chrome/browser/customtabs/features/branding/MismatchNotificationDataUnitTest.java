// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;

/** Unit test for {@link MismatchNotificationData} */
@RunWith(BaseRobolectricTestRunner.class)
public class MismatchNotificationDataUnitTest {
    @Rule public MockitoRule mTestRule = MockitoJUnit.rule();

    @Test
    public void testIsEmpty() {
        MismatchNotificationData.AppUiData appData = new MismatchNotificationData.AppUiData();
        assertTrue("AppData should be empty", appData.isEmpty());

        appData.showCount = 2;
        assertFalse("AppData should not be empty", appData.isEmpty());
    }

    @Test
    public void getAppData_returnsEmptyDataForNonExistingEntry() {
        MismatchNotificationData data = new MismatchNotificationData();
        MismatchNotificationData.AppUiData appData = data.getAppData("no-account", "no-app");
        assertTrue("Non-null, empty data should be returned", appData.isEmpty());
    }

    @Test
    public void setAppData() {
        MismatchNotificationData data = new MismatchNotificationData();
        MismatchNotificationData.AppUiData appData = new MismatchNotificationData.AppUiData();
        appData.showCount = 15;
        appData.closeType = CloseType.DISMISSED.getNumber();
        appData.userActCount = 7;
        data.setAppData("good-account", "org.goodapp", appData);
        var retrievedAppData = data.getAppData("good-account", "org.goodapp");
        assertEquals("Retrieved data is not correct", appData, retrievedAppData);
    }

    @Test
    public void testSerialize() {
        MismatchNotificationData data = new MismatchNotificationData();
        MismatchNotificationData.AppUiData appData = new MismatchNotificationData.AppUiData();
        appData.showCount = 15;
        appData.closeType = CloseType.DISMISSED.getNumber();
        appData.userActCount = 7;
        data.setAppData("good-account", "org.goodapp", appData);

        String s = data.toBase64();
        var restoredData = MismatchNotificationData.fromBase64(s);
        var restoredAppData = restoredData.getAppData("good-account", "org.goodapp");
        assertEquals("Serial/Deserialization failed", appData, restoredAppData);
    }
}
