// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.customtabs.features.branding.SharedPreferencesBrandingTimeStorage.MAX_NON_PACKAGE_ENTRIES;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;

import java.util.function.Function;

/** Unit test for {@link SharedPreferencesBrandingTimeStorage}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
public class SharedPreferencesBrandingTimeStorageUnitTest {
    @Before
    public void setup() {
        var shadowPostTaskImpl =
                new ShadowPostTask.TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                };
        ShadowPostTask.setTestImpl(shadowPostTaskImpl);
    }

    @Test
    public void nonPackageIdEntrySizeCapped() {
        var storage = SharedPreferencesBrandingTimeStorage.getInstance();
        storage.setSharedPrefForTesting(new InMemorySharedPreferences());
        Function<Integer, Integer> brandingTime = (i) -> i * 10;
        for (int i = 0; i < MAX_NON_PACKAGE_ENTRIES; ++i) {
            storage.put("id-" + i, brandingTime.apply(i));
        }
        assertEquals(0, (float) storage.get("id-0"), brandingTime.apply(0));
        assertEquals(
                "The number of entries can't be bigger than |MAX_NON_PACKAGE_ENTRIES|",
                MAX_NON_PACKAGE_ENTRIES,
                storage.getSize());

        storage.put("id-max+1", (MAX_NON_PACKAGE_ENTRIES + 1) * 10);
        assertEquals(
                "The number of entries can't be bigger than |MAX_NON_PACKAGE_ENTRIES|",
                MAX_NON_PACKAGE_ENTRIES,
                storage.getSize());

        // The oldest entry should be evicted.
        assertEquals(BrandingChecker.BRANDING_TIME_NOT_FOUND, storage.get("id-0"));
    }

    @Test
    public void getMimDataReturnsNullDataWhenNotFound() {
        var storage = SharedPreferencesBrandingTimeStorage.getInstance();
        storage.setSharedPrefForTesting(new InMemorySharedPreferences());
        assertNull("MIM should be null", storage.getMimData());
    }

    @Test
    public void getMimDataReadWrite() {
        var storage = SharedPreferencesBrandingTimeStorage.getInstance();
        storage.setSharedPrefForTesting(new InMemorySharedPreferences());
        final String appId = "org.cities.gotham";
        final String accountId = "batman@gmail.com";
        var mimData = new MismatchNotificationData();
        var appData = new MismatchNotificationData.AppUiData();
        appData.showCount = 32;
        appData.closeType = CloseType.ACCEPTED.getNumber();
        mimData.setAppData(accountId, appId, appData);
        storage.putMimData(mimData);

        var fetchedAppData = storage.getMimData().getAppData(accountId, appId);
        assertEquals("Retrived MIM data is not correct.", appData, fetchedAppData);
    }

    @Test
    public void getSizeLeavesOutMimProperties() {
        var storage = SharedPreferencesBrandingTimeStorage.getInstance();
        storage.setSharedPrefForTesting(new InMemorySharedPreferences());

        final String appId = "org.cities.gotham";
        final String accountId = "batman@gmail.com";
        var mimData = new MismatchNotificationData();
        var appData = new MismatchNotificationData.AppUiData();
        appData.showCount = 32;
        appData.closeType = CloseType.ACCEPTED.getNumber();
        mimData.setAppData(accountId, appId, appData);

        // These 2 MIM entries should not be counted toward |getSize()|.
        storage.putMimData(mimData);
        storage.putLastShowTimeGlobal(47201);

        int entryCount = MAX_NON_PACKAGE_ENTRIES / 2;
        Function<Integer, Integer> brandingTime = (i) -> i * 10;
        for (int i = 0; i < entryCount; ++i) {
            storage.put("id-" + i, brandingTime.apply(i));
        }
        assertEquals("The number of entries is not correct", entryCount, storage.getSize());
    }
}
