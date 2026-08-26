// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.TestDisplayableProfileData;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ProfileDataUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ProfileDataUtilsUnitTest {

    @Test
    public void testGetProfileDataIfFulfilledOrEmpty_unfulfilledPromise() {
        Promise<List<DisplayableProfileData>> promise = new Promise<>();
        assertEquals(
                Collections.emptyList(),
                ProfileDataUtils.getProfileDataIfFulfilledOrEmpty(promise));
    }

    @Test
    public void testGetProfileDataIfFulfilledOrEmpty_fulfilledPromise() {
        DisplayableProfileData account1 = TestDisplayableProfileData.ACCOUNT1;
        DisplayableProfileData account2 = TestDisplayableProfileData.ACCOUNT2;
        List<DisplayableProfileData> accounts = Arrays.asList(account1, account2);

        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(accounts);
        assertEquals(accounts, ProfileDataUtils.getProfileDataIfFulfilledOrEmpty(promise));
    }

    @Test
    public void testGetFirstIfFulfilledAndNotEmpty_unfulfilledPromise() {
        Promise<List<DisplayableProfileData>> promise = new Promise<>();
        assertNull(ProfileDataUtils.getFirstIfFulfilledAndNotEmpty(promise));
    }

    @Test
    public void testGetFirstIfFulfilledAndNotEmpty_emptyList() {
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(Collections.emptyList());
        assertNull(ProfileDataUtils.getFirstIfFulfilledAndNotEmpty(promise));
    }

    @Test
    public void testGetFirstIfFulfilledAndNotEmpty_populatedList() {
        DisplayableProfileData account1 = TestDisplayableProfileData.ACCOUNT1;
        DisplayableProfileData account2 = TestDisplayableProfileData.ACCOUNT2;

        Promise<List<DisplayableProfileData>> promise =
                Promise.fulfilled(Arrays.asList(account1, account2));
        assertEquals(account1, ProfileDataUtils.getFirstIfFulfilledAndNotEmpty(promise));
    }
}
