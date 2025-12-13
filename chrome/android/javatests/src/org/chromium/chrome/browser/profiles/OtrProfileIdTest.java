// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertSame;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * This test class checks if OtrProfileId works correctly for regular profile, primary
 * off-the-record profile and non primary off-the-record profiles. Also this checks whether
 * serialization and deserialization work correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OtrProfileIdTest {
    private static final String TEST_OTR_PROFILE_ID_ONE = "Test::SerializationOne";
    private static final String TEST_OTR_PROFILE_ID_TWO = "Test::SerializationTwo";

    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mActivityTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.start();
    }

    @Test
    @MediumTest
    public void testOtrProfileIdForRegularProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();

                    assertSame(profile, mPage.loadedTabElement.value().getProfile());

                    // OtrProfileId should be null for regular profile.
                    assertThat(profile.getOtrProfileId()).isNull();
                });
    }

    @Test
    @MediumTest
    public void testOtrProfileIdForPrimaryOtrProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOtrProfile(/* createIfNeeded= */ true);

                    // OtrProfileId should not be null for primary OTR profile and it should be the
                    // id of primary OTR profile.
                    assertThat(profile.getOtrProfileId()).isNotNull();
                    assertThat(profile.getOtrProfileId().isPrimaryOtrId()).isTrue();
                });
    }

    @Test
    @MediumTest
    public void testOtrProfileIdForNonPrimaryOtrProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId otrProfileId = new OtrProfileId(TEST_OTR_PROFILE_ID_ONE);
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileId, /* createIfNeeded= */ true);

                    // OtrProfileId should not be null for non-primary OTR profile and it should not
                    // be the id of primary OTR profile.
                    assertThat(profile.getOtrProfileId()).isNotNull();
                    assertThat(profile.getOtrProfileId().isPrimaryOtrId()).isFalse();
                });
    }

    @Test
    @MediumTest
    public void testSerializationAndDeserialization_success() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId otrProfileId = new OtrProfileId(TEST_OTR_PROFILE_ID_ONE);
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileId, /* createIfNeeded= */ true);
                    String serializedId = OtrProfileId.serialize(profile.getOtrProfileId());

                    // Check whether deserialized version from serialized version equals with the
                    // original OtrProfileId.
                    OtrProfileId deserializedId = OtrProfileId.deserialize(serializedId);
                    assertThat(deserializedId).isEqualTo(profile.getOtrProfileId());
                });
    }

    @Test
    @MediumTest
    public void testSerializationAndDeserialization_fail() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId otrProfileId = new OtrProfileId(TEST_OTR_PROFILE_ID_ONE);
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileId, /* createIfNeeded= */ true);
                    String serializedId = OtrProfileId.serialize(profile.getOtrProfileId());

                    // Break serialized id by adding a char to test failure scenario.
                    String brokenSerializedId = serializedId + "}";
                    // Try to deserialize from the broken serialized version. This should throw
                    // exception.
                    try {
                        OtrProfileId.deserialize(brokenSerializedId);
                        Assert.fail(
                                "The exception should be thrown since given OtrProfileId is not"
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
                    // Create first OtrProfileId and profile for TEST_OTR_PROFILE_ID_ONE
                    OtrProfileId otrProfileIdOne = new OtrProfileId(TEST_OTR_PROFILE_ID_ONE);
                    Profile profileOne =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIdOne, /* createIfNeeded= */ true);
                    String serializedIdOne = OtrProfileId.serialize(profileOne.getOtrProfileId());

                    // Create second OtrProfileId and profile for TEST_OTR_PROFILE_ID_TWO
                    OtrProfileId otrProfileIdTwo = new OtrProfileId(TEST_OTR_PROFILE_ID_TWO);
                    Profile profileTwo =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIdTwo, /* createIfNeeded= */ true);
                    String serializedIdTwo = OtrProfileId.serialize(profileTwo.getOtrProfileId());

                    // Deserialize the profile ids from serialized version.
                    OtrProfileId deserializedIdOne = OtrProfileId.deserialize(serializedIdOne);
                    OtrProfileId deserializedIdTwo = OtrProfileId.deserialize(serializedIdTwo);
                    // Check whether deserialized version of TEST_OTR_PROFILE_ID_ONE and
                    // TEST_OTR_PROFILE_ID_TWO are not equal.
                    assertThat(deserializedIdOne).isNotEqualTo(deserializedIdTwo);
                });
    }

    @Test
    @MediumTest
    public void testSerializationForRegularProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Deserialize the profile ids from serialized version.
                    OtrProfileId deserializedNullValue = OtrProfileId.deserialize(null);
                    assertThat(deserializedNullValue).isNull();

                    OtrProfileId deserializedEmptyValue = OtrProfileId.deserialize("");
                    assertThat(deserializedEmptyValue).isNull();
                });
    }

    @Test
    @SmallTest
    public void testOtrProfileIdsAreEqualOnJavaAndNative() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId otrProfileIdJava = OtrProfileId.getPrimaryOtrProfileId();
                    OtrProfileId otrProfileIdNative = OtrProfileIdJni.get().getPrimaryId();
                    assertThat(otrProfileIdJava).isEqualTo(otrProfileIdNative);

                    Profile profileJava =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIdJava, /* createIfNeeded= */ true);
                    Profile profileNative =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileIdNative, /* createIfNeeded= */ true);
                    assertThat(profileJava).isEqualTo(profileNative);

                    ProfileKey profileKeyJava = profileJava.getProfileKey();
                    ProfileKey profileKeyNative = profileNative.getProfileKey();
                    assertThat(profileKeyJava).isEqualTo(profileKeyNative);
                });
    }
}
