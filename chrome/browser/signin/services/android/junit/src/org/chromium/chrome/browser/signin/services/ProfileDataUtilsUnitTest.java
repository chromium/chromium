// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.TestDisplayableProfileData;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ProfileDataUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileDataUtilsUnitTest {
    private static final DisplayableProfileData PROFILE_DATA1 = TestDisplayableProfileData.ACCOUNT1;
    private static final DisplayableProfileData PROFILE_DATA2 = TestDisplayableProfileData.ACCOUNT2;
    private static final List<DisplayableProfileData> PROFILE_DATA_LIST =
            Arrays.asList(PROFILE_DATA1, PROFILE_DATA2);

    @Test
    public void testGetProfileDataIfFulfilledOrEmpty_unfulfilledPromise() {
        Promise<List<DisplayableProfileData>> promise = new Promise<>();
        assertEquals(
                Collections.emptyList(),
                ProfileDataUtils.getProfileDataIfFulfilledOrEmpty(promise));
    }

    @Test
    public void testGetProfileDataIfFulfilledOrEmpty_fulfilledPromise() {
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(PROFILE_DATA_LIST);
        assertEquals(PROFILE_DATA_LIST, ProfileDataUtils.getProfileDataIfFulfilledOrEmpty(promise));
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
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(PROFILE_DATA_LIST);
        assertEquals(PROFILE_DATA1, ProfileDataUtils.getFirstIfFulfilledAndNotEmpty(promise));
    }

    @Test
    public void testGetPreferredOrFirstIfFulfilledAndNotEmpty_withMatchingPreference() {
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(PROFILE_DATA_LIST);
        assertEquals(
                PROFILE_DATA2,
                ProfileDataUtils.getPreferredOrFirstIfFulfilledAndNotEmpty(
                        promise, TestAccounts.ACCOUNT2.getGaiaId()));
    }

    @Test
    public void testGetPreferredOrFirstIfFulfilledAndNotEmpty_withNoPreference() {
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(PROFILE_DATA_LIST);
        assertEquals(
                PROFILE_DATA1,
                ProfileDataUtils.getPreferredOrFirstIfFulfilledAndNotEmpty(
                        promise, /* preferredGaiaId= */ null));
    }

    @Test
    public void testGetPreferredOrFirstIfFulfilledAndNotEmpty_preferenceNotFoundInCache() {
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(PROFILE_DATA_LIST);
        assertEquals(
                PROFILE_DATA1,
                ProfileDataUtils.getPreferredOrFirstIfFulfilledAndNotEmpty(
                        promise, new GaiaId("unknown-gaia-id")));
    }

    @Test
    public void testGetPreferredOrFirstIfFulfilledAndNotEmpty_unfulfilledPromise() {
        Promise<List<DisplayableProfileData>> promise = new Promise<>();
        assertNull(
                ProfileDataUtils.getPreferredOrFirstIfFulfilledAndNotEmpty(
                        promise, TestAccounts.ACCOUNT2.getGaiaId()));
    }

    @Test
    public void testGetPreferredOrFirstIfFulfilledAndNotEmpty_emptyList() {
        Promise<List<DisplayableProfileData>> promise = Promise.fulfilled(Collections.emptyList());
        assertNull(
                ProfileDataUtils.getPreferredOrFirstIfFulfilledAndNotEmpty(
                        promise, TestAccounts.ACCOUNT2.getGaiaId()));
    }
}
