// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OtrProfileId}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OtrProfileIdUnitTest {
    @Test
    public void testDeserializeAndVerifyChecksGivenProfile_notLastUsed() {
        TestProfile profileA = TestProfile.createRegular("Default");
        TestProfile profileB = TestProfile.createRegular("Profile 1");
        ProfileManager.setLastUsedProfileForTesting(profileB);

        OtrProfileId customId = new OtrProfileId("Test::Custom");
        TestProfile otrProfileA = profileA.getOrCreateOffTheRecordProfile(customId);
        TestProfile siblingOtrProfileA = profileA.getOrCreatePrimaryOtrProfile();
        TestProfile otrProfileB = profileB.getOrCreatePrimaryOtrProfile();
        String serialized = OtrProfileId.serialize(customId);

        assertEquals(customId, OtrProfileId.deserializeAndVerify(serialized, profileA));
        assertEquals(customId, OtrProfileId.deserializeAndVerify(serialized, otrProfileA));
        assertEquals(customId, OtrProfileId.deserializeAndVerify(serialized, siblingOtrProfileA));
        assertThrows(
                IllegalStateException.class,
                () -> OtrProfileId.deserializeAndVerify(serialized, profileB));
        assertThrows(
                IllegalStateException.class,
                () -> OtrProfileId.deserializeAndVerify(serialized, otrProfileB));
    }

    @Test
    public void testDeserializeAndVerifyPrimaryOtrIdIsolatedPerProfile() {
        TestProfile profileA = TestProfile.createRegular("Default");
        TestProfile profileB = TestProfile.createRegular("Profile 1");
        profileA.getOrCreatePrimaryOtrProfile();
        String serializedPrimary = OtrProfileId.serialize(OtrProfileId.getPrimaryOtrProfileId());

        assertEquals(
                OtrProfileId.getPrimaryOtrProfileId(),
                OtrProfileId.deserializeAndVerify(serializedPrimary, profileA));
        assertThrows(
                IllegalStateException.class,
                () -> OtrProfileId.deserializeAndVerify(serializedPrimary, profileB));
    }

    @Test
    public void testDeserializeAndVerifyNullOrEmptyReturnsNull() {
        TestProfile profile = TestProfile.createRegular("Default");
        assertNull(OtrProfileId.deserializeAndVerify(null, profile));
        assertNull(OtrProfileId.deserializeAndVerify("", profile));
    }
}
