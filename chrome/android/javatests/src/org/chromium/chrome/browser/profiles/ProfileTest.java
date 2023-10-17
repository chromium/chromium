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

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
        TestThreadUtils.runOnUiThreadBlocking(
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
        Assert.assertNull(mRegularProfile.getOTRProfileID());
        // Open an new Incognito Tab page to create a new primary OTR profile.
        sActivityTestRule.loadUrlInNewTab("about:blank", true);

        Profile incognitoProfile1 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ true));
        Assert.assertTrue(incognitoProfile1.isOffTheRecord());
        Assert.assertTrue(incognitoProfile1.isPrimaryOTRProfile());
        Assert.assertTrue(incognitoProfile1.isNativeInitialized());
        Assert.assertTrue(mRegularProfile.hasPrimaryOTRProfile());

        Profile incognitoProfile2 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ true));
        Assert.assertSame(
                "Two calls to get incognito profile should return the same object.",
                incognitoProfile1,
                incognitoProfile2);
    }

    /** Test if two calls to get non-primary profile with the same id return the same object. */
    @Test
    @LargeTest
    public void testNonPrimaryProfileConsistency() throws Exception {
        OTRProfileID profileID = new OTRProfileID("test::OTRProfile");
        Profile nonPrimaryOtrProfile1 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID, /* createIfNeeded= */ true));

        Assert.assertTrue(nonPrimaryOtrProfile1.isOffTheRecord());
        Assert.assertFalse(nonPrimaryOtrProfile1.isPrimaryOTRProfile());
        Assert.assertTrue(nonPrimaryOtrProfile1.isNativeInitialized());
        Assert.assertTrue(mRegularProfile.hasOffTheRecordProfile(profileID));
        Assert.assertFalse(mRegularProfile.hasPrimaryOTRProfile());

        Assert.assertEquals(
                "OTR profile id should be returned as it is set.",
                nonPrimaryOtrProfile1.getOTRProfileID(),
                profileID);

        Profile nonPrimaryOtrProfile2 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        new OTRProfileID("test::OTRProfile"),
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
        OTRProfileID profileID1 = new OTRProfileID("test::OTRProfile-1");
        Profile nonPrimaryOtrProfile1 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID1, /* createIfNeeded= */ true));

        OTRProfileID profileID2 = new OTRProfileID("test::OTRProfile-2");
        Profile nonPrimaryOtrProfile2 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID2, /* createIfNeeded= */ true));

        Assert.assertTrue(nonPrimaryOtrProfile1.isOffTheRecord());
        Assert.assertFalse(nonPrimaryOtrProfile1.isPrimaryOTRProfile());
        Assert.assertTrue(nonPrimaryOtrProfile1.isNativeInitialized());
        Assert.assertTrue(mRegularProfile.hasOffTheRecordProfile(profileID1));

        Assert.assertTrue(nonPrimaryOtrProfile2.isOffTheRecord());
        Assert.assertFalse(nonPrimaryOtrProfile2.isPrimaryOTRProfile());
        Assert.assertTrue(nonPrimaryOtrProfile2.isNativeInitialized());
        Assert.assertTrue(mRegularProfile.hasOffTheRecordProfile(profileID2));

        Assert.assertNotSame(
                "Two calls to get non-primary OTR profile with different IDs"
                        + "should return different objects.",
                nonPrimaryOtrProfile1,
                nonPrimaryOtrProfile2);
    }

    /** Test if creating unique otr profile ids works as expected. */
    @Test
    @LargeTest
    public void testCreatingUniqueOTRProfileIDs() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID profileID1 = OTRProfileID.createUnique("test::OTRProfile");
                    OTRProfileID profileID2 = OTRProfileID.createUnique("test::OTRProfile");

                    Assert.assertNotSame(
                            "Two calls to OTRProfileID.CreateUnique with the same prefix"
                                    + "should return different objects.",
                            profileID1,
                            profileID2);
                });
    }

    @Test
    @LargeTest
    public void testBrowserProfileTypeFromRegularProfile() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            BrowserProfileType.REGULAR,
                            Profile.getBrowserProfileTypeFromProfile(mRegularProfile));
                });
    }

    @Test
    @LargeTest
    public void testBrowserProfileTypeFromPrimaryOTRProfile() throws Exception {
        // Open an new Incognito Tab page to create a new primary OTR profile.
        sActivityTestRule.loadUrlInNewTab("about:blank", true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile primaryOTRProfile =
                            mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ true);
                    Assert.assertEquals(
                            BrowserProfileType.INCOGNITO,
                            Profile.getBrowserProfileTypeFromProfile(primaryOTRProfile));
                });
    }

    @Test
    @LargeTest
    public void testBrowserProfileTypeFromNonPrimaryOTRProfile() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = new OTRProfileID("test::OTRProfile");
                    Profile nonPrimaryOtrProfile =
                            mRegularProfile.getOffTheRecordProfile(
                                    otrProfileID, /* createIfNeeded= */ true);
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
        OTRProfileID profileID = new OTRProfileID("test::OTRProfile");

        // Ask for a non-existing profile with createIfNeeded set to false, and exepct null.
        Profile profile1 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID, /* createIfNeeded= */ false));
        Assert.assertNull(profile1);
        Assert.assertFalse(mRegularProfile.hasOffTheRecordProfile(profileID));

        // Ask for a non-existing profile with createIfNeeded set to true and expect creation.
        Profile profile2 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID, /* createIfNeeded= */ true));
        Assert.assertNotNull(profile2);
        Assert.assertTrue(mRegularProfile.hasOffTheRecordProfile(profileID));

        // Ask for an existing profile with createIfNeeded set to false and expect getting the
        // existing profile.
        Profile profile3 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID, /* createIfNeeded= */ false));
        Assert.assertNotNull(profile3);
        Assert.assertSame(profile2, profile3);

        // Ask for an existing profile with createIfNeeded set to true and expect getting the
        // existing profile.
        Profile profile4 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mRegularProfile.getOffTheRecordProfile(
                                        profileID, /* createIfNeeded= */ true));
        Assert.assertNotNull(profile4);
        Assert.assertSame(profile2, profile4);
    }

    /** Tests createIfNeeded parameter of getPrimaryOTRProfile. */
    @Test
    @LargeTest
    public void testGetPrimaryOTRProfile() throws Exception {
        // Ask for a non-existing profile with createIfNeeded set to false, and exepct null.
        Profile profile1 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ false));
        Assert.assertNull(profile1);
        Assert.assertFalse(mRegularProfile.hasPrimaryOTRProfile());

        // Ask for a non-existing profile with createIfNeeded set to true and expect creation.
        Profile profile2 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ true));
        Assert.assertNotNull(profile2);
        Assert.assertTrue(mRegularProfile.hasPrimaryOTRProfile());

        // Ask for an existing profile with createIfNeeded set to false and expect getting the
        // existing profile.
        Profile profile3 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ false));
        Assert.assertNotNull(profile3);
        Assert.assertSame(profile2, profile3);

        // Ask for an existing profile with createIfNeeded set to true and expect getting the
        // existing profile.
        Profile profile4 =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularProfile.getPrimaryOTRProfile(/* createIfNeeded= */ true));
        Assert.assertNotNull(profile4);
        Assert.assertSame(profile2, profile4);
    }
}
