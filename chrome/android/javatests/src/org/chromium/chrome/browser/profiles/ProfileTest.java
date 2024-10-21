// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.profile_metrics.BrowserProfileType;

/** This test class checks if incognito and non-incognito OTR profiles can be distinctly created. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ProfileTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    public Profile mRegularProfile;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRegularProfile =
                            sActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getModel(false)
                                    .getProfile();
                });
    }

    /** Test if two calls for incognito profile return the same object. */
    @Test
    @LargeTest
    public void testIncognitoProfileConsistency() throws Exception {
        Assert.assertNull(mRegularProfile.getOtrProfileId());
        // Open an new Incognito Tab page to create a new primary OTR profile.
        sActivityTestRule.loadUrlInNewTab("about:blank", true);

        Profile incognitoProfile1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ true));
        Assert.assertTrue(
                "isOffTheRecord should be true for Incognito profiles",
                incognitoProfile1.isOffTheRecord());
        Assert.assertTrue(
                "isIncognitoBranded should be true for Incognito profiles",
                incognitoProfile1.isIncognitoBranded());
        Assert.assertTrue(
                "isPrimaryOtrProfile should be true for Incognito profiles",
                incognitoProfile1.isPrimaryOtrProfile());
        Assert.assertTrue(
                "isNativeInitialized should be true for Incognito profiles",
                incognitoProfile1.isNativeInitialized());
        Assert.assertTrue(
                "The regular profile should be the Incognito profile",
                mRegularProfile.hasPrimaryOtrProfile());

        Profile incognitoProfile2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ true));
        Assert.assertSame(
                "Two calls to get incognito profile should return the same object.",
                incognitoProfile1,
                incognitoProfile2);
    }

    /** Test if two calls to get non-primary profile with the same id return the same object. */
    @Test
    @LargeTest
    public void testNonPrimaryProfileConsistency() throws Exception {
        OtrProfileId profileId = new OtrProfileId("test::OtrProfile");
        Profile nonPrimaryOtrProfile1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId, /* createIfNeeded= */ true));

        Assert.assertTrue(
                "isOffTheRecord should be true for non-primary OTR profiles",
                nonPrimaryOtrProfile1.isOffTheRecord());
        Assert.assertFalse(
                "isIncognitoBranded should be false for non-primary, non-iCCT OTR profiles",
                nonPrimaryOtrProfile1.isIncognitoBranded());
        Assert.assertFalse(
                "isPrimaryOtrProfile should be false for non-primary OTR profiles",
                nonPrimaryOtrProfile1.isPrimaryOtrProfile());
        Assert.assertTrue(
                "isNativeInitialized should be true for non-primary OTR profiles",
                nonPrimaryOtrProfile1.isNativeInitialized());
        Assert.assertTrue(
                "The regular profile should return the OTR profile from the OTR profile id",
                mRegularProfile.hasOffTheRecordProfile(profileId));
        Assert.assertFalse(
                "hasPrimaryOtrProfile should be false for non-primary, non-incognito profiles",
                mRegularProfile.hasPrimaryOtrProfile());

        Assert.assertEquals(
                "OTR profile id should be returned as it is set.",
                nonPrimaryOtrProfile1.getOtrProfileId(),
                profileId);

        Profile nonPrimaryOtrProfile2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        new OtrProfileId("test::OtrProfile"),
                                        /* createIfNeeded= */ true));

        Assert.assertSame(
                "Two calls to get non-primary OTR profile with the same ID "
                        + "should return the same object.",
                nonPrimaryOtrProfile1,
                nonPrimaryOtrProfile2);
    }

    /** Test if creating two non-primary profiles result in different objects. */
    @Test
    @LargeTest
    public void testCreatingTwoNonPrimaryProfiles() throws Exception {
        OtrProfileId profileId1 = new OtrProfileId("test::OtrProfile-1");
        Profile nonPrimaryOtrProfile1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId1, /* createIfNeeded= */ true));

        OtrProfileId profileId2 = new OtrProfileId("test::OtrProfile-2");
        Profile nonPrimaryOtrProfile2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId2, /* createIfNeeded= */ true));

        Assert.assertTrue(
                "isOffTheRecord should be true for non-primary OTR profiles",
                nonPrimaryOtrProfile1.isOffTheRecord());
        Assert.assertFalse(
                "isIncognitoBranded should be false for non-primary, non-iCCT OTR profiles",
                nonPrimaryOtrProfile1.isIncognitoBranded());
        Assert.assertFalse(
                "isPrimaryOtrProfile should be false for non-primary OTR profiles",
                nonPrimaryOtrProfile1.isPrimaryOtrProfile());
        Assert.assertTrue(
                "isNativeInitialized should be true for non-primary OTR profiles",
                nonPrimaryOtrProfile1.isNativeInitialized());
        Assert.assertTrue(
                "The regular profile should return the OTR profile from the OTR profile id",
                mRegularProfile.hasOffTheRecordProfile(profileId1));

        Assert.assertTrue(
                "isOffTheRecord should be true for non-primary OTR profiles",
                nonPrimaryOtrProfile2.isOffTheRecord());
        Assert.assertFalse(
                "isIncognitoBranded should be false for non-primary, non-iCCT OTR profiles",
                nonPrimaryOtrProfile2.isIncognitoBranded());
        Assert.assertFalse(
                "isPrimaryOtrProfile should be false for non-primary OTR profiles",
                nonPrimaryOtrProfile2.isPrimaryOtrProfile());
        Assert.assertTrue(
                "isNativeInitialized should be true for non-primary OTR profiles",
                nonPrimaryOtrProfile2.isNativeInitialized());
        Assert.assertTrue(
                "The regular profile should return the OTR profile from the OTR profile id",
                mRegularProfile.hasOffTheRecordProfile(profileId2));

        Assert.assertNotSame(
                "Two calls to get non-primary OTR profile with different IDs"
                        + "should return different objects.",
                nonPrimaryOtrProfile1,
                nonPrimaryOtrProfile2);
    }

    /** Test if creating unique otr profile ids works as expected. */
    @Test
    @LargeTest
    public void testCreatingUniqueOtrProfileIds() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId profileId1 = OtrProfileId.createUnique("test::OtrProfile");
                    OtrProfileId profileId2 = OtrProfileId.createUnique("test::OtrProfile");

                    Assert.assertNotSame(
                            "Two calls to OtrProfileId.CreateUnique with the same prefix"
                                    + "should return different objects.",
                            profileId1,
                            profileId2);
                });
    }

    /** Test if creating unique iCCT profile ids works as expected. */
    @Test
    @LargeTest
    public void testCreatingUniqueIncognitoCCTOtrProfileIds() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId incognitoCctId1 = OtrProfileId.createUniqueIncognitoCctId();
                    OtrProfileId incognitoCctId2 = OtrProfileId.createUniqueIncognitoCctId();

                    Assert.assertNotSame(
                            "Two calls to OtrProfileId.createUniqueIncognitoCctId"
                                    + "should return different objects.",
                            incognitoCctId1,
                            incognitoCctId2);
                    Assert.assertTrue(incognitoCctId1.isIncognitoCCId());
                    Assert.assertTrue(incognitoCctId2.isIncognitoCCId());
                });
    }

    /** Tests creating iCCT profile. */
    @Test
    @LargeTest
    public void testIncognitoCctProfileCreation() throws Exception {
        OtrProfileId incognitoCctId = OtrProfileId.createUniqueIncognitoCctId();
        Profile incognitoCctProfile =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        incognitoCctId, /* createIfNeeded= */ true));

        Assert.assertTrue(
                "isOffTheRecord should be true for Incognito CCT profiles",
                incognitoCctProfile.isOffTheRecord());
        Assert.assertTrue(
                "isIncognitoBranded should be true for Incognito CCT profiles",
                incognitoCctProfile.isIncognitoBranded());
        Assert.assertFalse(
                "isPrimaryOtrProfile should be false for Incognito CCT profiles",
                incognitoCctProfile.isPrimaryOtrProfile());
        Assert.assertTrue(
                "isNativeInitialized should be true for Incognito CCT profiles",
                incognitoCctProfile.isNativeInitialized());
        Assert.assertTrue(
                "The regular profile should return the Incognito CCT profile from the OTR profile"
                        + " id",
                mRegularProfile.hasOffTheRecordProfile(incognitoCctId));
    }

    @Test
    @LargeTest
    public void testBrowserProfileTypeFromRegularProfile() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            BrowserProfileType.REGULAR,
                            Profile.getBrowserProfileTypeFromProfile(mRegularProfile));
                });
    }

    @Test
    @LargeTest
    public void testBrowserProfileTypeFromPrimaryOtrProfile() throws Exception {
        // Open an new Incognito Tab page to create a new primary OTR profile.
        sActivityTestRule.loadUrlInNewTab("about:blank", true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile primaryOtrProfile =
                            mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ true);
                    Assert.assertEquals(
                            BrowserProfileType.INCOGNITO,
                            Profile.getBrowserProfileTypeFromProfile(primaryOtrProfile));
                });
    }

    @Test
    @LargeTest
    public void testBrowserProfileTypeFromNonPrimaryOtrProfile() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId otrProfileId = new OtrProfileId("test::OtrProfile");
                    Profile nonPrimaryOtrProfile =
                            mRegularProfile.getOffTheRecordProfile(
                                    otrProfileId, /* createIfNeeded= */ true);
                    Assert.assertEquals(
                            BrowserProfileType.OTHER_OFF_THE_RECORD_PROFILE,
                            Profile.getBrowserProfileTypeFromProfile(nonPrimaryOtrProfile));
                });
    }

    /** Tests createIfNeeded parameter of getOffTheRecordProfile. */
    @Test
    @LargeTest
    @RequiresRestart(
            "crbug/1161449 - Other tests create profiles which invalidate the first assertion.")
    public void testGetOffTheRecordProfile() throws Exception {
        OtrProfileId profileId = new OtrProfileId("test::OtrProfile");

        // Ask for a non-existing profile with createIfNeeded set to false, and expect null.
        Profile profile1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId, /* createIfNeeded= */ false));
        Assert.assertNull(profile1);
        Assert.assertFalse(mRegularProfile.hasOffTheRecordProfile(profileId));

        // Ask for a non-existing profile with createIfNeeded set to true and expect creation.
        Profile profile2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId, /* createIfNeeded= */ true));
        Assert.assertNotNull(profile2);
        Assert.assertTrue(mRegularProfile.hasOffTheRecordProfile(profileId));

        // Ask for an existing profile with createIfNeeded set to false and expect getting the
        // existing profile.
        Profile profile3 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId, /* createIfNeeded= */ false));
        Assert.assertNotNull(profile3);
        Assert.assertSame(profile2, profile3);

        // Ask for an existing profile with createIfNeeded set to true and expect getting the
        // existing profile.
        Profile profile4 =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileId, /* createIfNeeded= */ true));
        Assert.assertNotNull(profile4);
        Assert.assertSame(profile2, profile4);
    }

    /** Tests createIfNeeded parameter of getPrimaryOtrProfile. */
    @Test
    @LargeTest
    public void testGetPrimaryOtrProfile() throws Exception {
        // Ask for a non-existing profile with createIfNeeded set to false, and expect null.
        Profile profile1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ false));
        Assert.assertNull(profile1);
        Assert.assertFalse(mRegularProfile.hasPrimaryOtrProfile());

        // Ask for a non-existing profile with createIfNeeded set to true and expect creation.
        Profile profile2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ true));
        Assert.assertNotNull(profile2);
        Assert.assertTrue(mRegularProfile.hasPrimaryOtrProfile());

        // Ask for an existing profile with createIfNeeded set to false and expect getting the
        // existing profile.
        Profile profile3 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ false));
        Assert.assertNotNull(profile3);
        Assert.assertSame(profile2, profile3);

        // Ask for an existing profile with createIfNeeded set to true and expect getting the
        // existing profile.
        Profile profile4 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOtrProfile(/* createIfNeeded= */ true));
        Assert.assertNotNull(profile4);
        Assert.assertSame(profile2, profile4);
    }
}
