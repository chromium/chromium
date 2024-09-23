// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * This test class checks if OTRProfileID works correctly for regular profile, primary
 * off-the-record profile and non primary off-the-record profiles. Also this checks whether
 * serialization and deserialization work correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OTRProfileIDTest {
    private static final String TEST_OTR_PROFILE_ID_ONE = "Test::SerializationOne";
    private static final String TEST_OTR_PROFILE_ID_TWO = "Test::SerializationTwo";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        sActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void testOTRProfileIdForRegularProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();

                    // OTRProfileId should be null for regular profile.
                    assert profile.getOTRProfileID() == null;
                });
    }

    @Test
    @MediumTest
    public void testOTRProfileIdForPrimaryOTRProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOTRProfile(/* createIfNeeded= */ true);

                    // OTRProfileId should not be null for primary OTR profile and it should be the
                    // id of primary OTR profile.
                    assert profile.getOTRProfileID() != null;
                    assert profile.getOTRProfileID().isPrimaryOTRId();
                });
    }

    @Test
    @MediumTest
    public void testOTRProfileIdForNonPrimaryOTRProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = new OTRProfileID(TEST_OTR_PROFILE_ID_ONE);
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);

                    // OTRProfileId should not be null for non-primary OTR profile and it should not
                    // be the id of primary OTR profile.
                    assert profile.getOTRProfileID() != null;
                    assert !profile.getOTRProfileID().isPrimaryOTRId();
                });
    }

    @Test
    @MediumTest
    public void testSerializationAndDeserialization_success() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = new OTRProfileID(TEST_OTR_PROFILE_ID_ONE);
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                    String serializedId = OTRProfileID.serialize(profile.getOTRProfileID());

                    // Check whether deserialized version from serialized version equals with the
                    // original OTRProfileId.
                    OTRProfileID deserializedId = OTRProfileID.deserialize(serializedId);
                    assert deserializedId.equals(profile.getOTRProfileID());
                });
    }

    @Test
    @MediumTest
    public void testSerializationAndDeserialization_fail() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = new OTRProfileID(TEST_OTR_PROFILE_ID_ONE);
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                    String serializedId = OTRProfileID.serialize(profile.getOTRProfileID());

                    // Break serialized id by adding a char to test failure scenario.
                    String brokenSerializedId = serializedId + "}";
                    // Try to deserialize from the broken serialized version. This should throw
                    // exception.
                    try {
                        OTRProfileID.deserialize(brokenSerializedId);
                        Assert.fail(
                                "The exception should be thrown since given OTRProfileId is not"
                                        + " valid.");
                    } catch (IllegalStateException e) {
                        Assert.assertNotNull(e);
                    }
                });
    }

    @Test
    @MediumTest
    public void testSerializationAndDeserializationForTwoIdComparision() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create first OTRProfileID and profile for TEST_OTR_PROFILE_ID_ONE
                    OTRProfileID otrProfileIDOne = new OTRProfileID(TEST_OTR_PROFILE_ID_ONE);
                    Profile profileOne =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIDOne, /* createIfNeeded= */ true);
                    String serializedIdOne = OTRProfileID.serialize(profileOne.getOTRProfileID());

                    // Create second OTRProfileID and profile for TEST_OTR_PROFILE_ID_TWO
                    OTRProfileID otrProfileIDTwo = new OTRProfileID(TEST_OTR_PROFILE_ID_TWO);
                    Profile profileTwo =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIDTwo, /* createIfNeeded= */ true);
                    String serializedIdTwo = OTRProfileID.serialize(profileTwo.getOTRProfileID());

                    // Deserialize the profile ids from serialized version.
                    OTRProfileID deserializedIdOne = OTRProfileID.deserialize(serializedIdOne);
                    OTRProfileID deserializedIdTwo = OTRProfileID.deserialize(serializedIdTwo);
                    // Check whether deserialized version of TEST_OTR_PROFILE_ID_ONE and
                    // TEST_OTR_PROFILE_ID_TWO are not equal.
                    assert !deserializedIdOne.equals(deserializedIdTwo);
                });
    }

    @Test
    @MediumTest
    public void testSerializationForRegularProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Deserialize the profile ids from serialized version.
                    OTRProfileID deserializedNullValue = OTRProfileID.deserialize(null);
                    assert deserializedNullValue == null;

                    OTRProfileID deserializedEmptyValue = OTRProfileID.deserialize("");
                    assert deserializedEmptyValue == null;
                });
    }

    @Test
    @SmallTest
    public void testOTRProfileIDsAreEqualOnJavaAndNative() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileIDJava = OTRProfileID.getPrimaryOTRProfileID();
                    OTRProfileID otrProfileIDNative = OTRProfileIDJni.get().getPrimaryID();
                    assert otrProfileIDJava.equals(otrProfileIDNative);

                    Profile profileJava =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIDJava, /* createIfNeeded= */ true);
                    Profile profileNative =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIDNative, /* createIfNeeded= */ true);
                    assert profileJava.equals(profileNative);

                    ProfileKey profileKeyJava = profileJava.getProfileKey();
                    ProfileKey profileKeyNative = profileNative.getProfileKey();
                    assert profileKeyJava.equals(profileKeyNative);
                });
    }
}
