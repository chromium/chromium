// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.isEmptyString;

import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for ProfileResolver. Profile resolution must run on the UI thread and may invoke the given
 * callback asynchronously. This means test cases cannot block the UI thread while waiting for this
 * callback. This complicates writing each test, as care must be taken to avoid staying on the UI
 * thread even though most operations should be performed on it.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests initialization")
public class ProfileResolverTest {
    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ProfileResolver mProfileResolver;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mProfileResolver = new ProfileResolver();
    }

    private Profile getLastUsedRegularProfileOnUiThread() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> ProfileManager.getLastUsedRegularProfile());
    }

    private Profile getPrimaryOtrProfileOnUiThread() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ProfileManager.getLastUsedRegularProfile()
                                .getPrimaryOtrProfile(/* createIfNeeded= */ true));
    }

    private Profile newOtrProfileOnUiThread(String profileIdPrefix) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile regularProfile = ProfileManager.getLastUsedRegularProfile();
                    OtrProfileId otrProfileId = OtrProfileId.createUnique(profileIdPrefix);
                    return regularProfile.getOffTheRecordProfile(
                            otrProfileId, /* createIfNeeded= */ true);
                });
    }

    private String tokenizeOnUiThread(Profile profile) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> mProfileResolver.tokenize(profile));
    }

    private Profile resolveProfileSync(String token) {
        PayloadCallbackHelper<Profile> callbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileResolver.resolveProfile(
                            token, (Profile p) -> callbackHelper.notifyCalled(p));
                });
        return callbackHelper.getOnlyPayloadBlocking();
    }

    private static Intent createSelfTargetedIntent() {
        return new Intent().setPackage(ContextUtils.getApplicationContext().getPackageName());
    }

    private Profile retrieveProfileFromIntentSync(Intent intent) {
        PayloadCallbackHelper<Profile> callbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ProfileTokenUtils.retrieveProfileFromIntent(
                            intent, (Profile p) -> callbackHelper.notifyCalled(p));
                });
        return callbackHelper.getOnlyPayloadBlocking();
    }

    @Test
    @SmallTest
    public void testResolveProfile() throws ExecutionException {
        Profile profile = getLastUsedRegularProfileOnUiThread();

        String firstToken = tokenizeOnUiThread(profile);
        Assert.assertEquals(
                "Round tripping should result in the same Profile object",
                profile,
                resolveProfileSync(firstToken));

        String secondToken = tokenizeOnUiThread(profile);
        Assert.assertEquals(
                "Round tripping should result in the same Profile object",
                profile,
                resolveProfileSync(secondToken));
        Assert.assertEquals("Tokens should be identical", firstToken, secondToken);
    }

    @Test
    @SmallTest
    public void testResolveOtrProfile() throws ExecutionException {
        Profile profile = getLastUsedRegularProfileOnUiThread();
        Profile primaryOtrProfile = getPrimaryOtrProfileOnUiThread();
        Profile newOtrProfile = newOtrProfileOnUiThread("foo");

        String primaryOtrToken = tokenizeOnUiThread(primaryOtrProfile);
        Profile resolvedPrimaryOtrProfile = resolveProfileSync(primaryOtrToken);
        Assert.assertEquals(
                "Round tripped primary otr profile should match",
                primaryOtrProfile,
                resolvedPrimaryOtrProfile);
        Assert.assertNotEquals(
                "Round tripped primary OTR profile should be different from original Profile",
                profile,
                resolvedPrimaryOtrProfile);

        String newOtrToken = tokenizeOnUiThread(newOtrProfile);
        Profile resolvedNewOtrProfile = resolveProfileSync(newOtrToken);
        Assert.assertEquals(
                "Round tripped new otr profile should match", newOtrProfile, resolvedNewOtrProfile);
        Assert.assertNotEquals(
                "Round tripped new OTR profile should be different from original Profile",
                profile,
                resolvedNewOtrProfile);
        Assert.assertNotEquals(
                "Round tripped new OTR profile should be different from original OTR Profile",
                primaryOtrProfile,
                resolvedNewOtrProfile);
    }

    @Test
    @SmallTest
    public void testTokenizeNulls() throws ExecutionException {

        Profile profile = null;
        assertThat(
                "Tokenizing a null profile should not work",
                tokenizeOnUiThread(profile),
                isEmptyString());
    }

    @Test
    @SmallTest
    public void testResolveBadTokens() throws ExecutionException {
        List<String> badTokens = Arrays.asList(null, "", "abcdef");

        for (String token : badTokens) {
            Assert.assertNull(
                    "#resolveProfile() did not resolve null for " + token,
                    resolveProfileSync(token));
        }
    }

    @Test
    @SmallTest
    public void testProfileTokenUtils_RegularProfileAndNotificationTag() throws ExecutionException {
        Profile profile = getLastUsedRegularProfileOnUiThread();

        Intent intent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Intent i = createSelfTargetedIntent();
                            ProfileTokenUtils.addProfileToIntent(profile, i);
                            return i;
                        });
        Assert.assertTrue(ProfileTokenUtils.hasProfileToken(intent));
        Assert.assertEquals(
                "Round tripping through ProfileTokenUtils should result in the same Profile object",
                profile,
                retrieveProfileFromIntentSync(intent));

        String expectedTag =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> ProfileTokenUtils.getNotificationTag("TestPrefix", profile));
        Assert.assertEquals(
                "Notification tag from intent should match tag from profile",
                expectedTag,
                ProfileTokenUtils.getNotificationTagFromIntent("TestPrefix", intent));
    }

    @Test
    @SmallTest
    public void testProfileTokenUtils_OtrProfiles() throws ExecutionException {
        Profile regularProfile = getLastUsedRegularProfileOnUiThread();
        Profile primaryOtrProfile = getPrimaryOtrProfileOnUiThread();
        Profile uniqueOtrProfile = newOtrProfileOnUiThread("foo");

        Intent regularIntent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Intent i = createSelfTargetedIntent();
                            ProfileTokenUtils.addProfileToIntent(regularProfile, i);
                            return i;
                        });
        Intent primaryOtrIntent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Intent i = createSelfTargetedIntent();
                            ProfileTokenUtils.addProfileToIntent(primaryOtrProfile, i);
                            return i;
                        });
        Intent uniqueOtrIntent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Intent i = createSelfTargetedIntent();
                            ProfileTokenUtils.addProfileToIntent(uniqueOtrProfile, i);
                            return i;
                        });

        Profile resolvedPrimaryOtr = retrieveProfileFromIntentSync(primaryOtrIntent);
        Assert.assertEquals(
                "Round tripped primary OTR profile should match",
                primaryOtrProfile,
                resolvedPrimaryOtr);
        Assert.assertNotEquals(regularProfile, resolvedPrimaryOtr);

        Profile resolvedUniqueOtr = retrieveProfileFromIntentSync(uniqueOtrIntent);
        Assert.assertEquals(
                "Round tripped unique OTR profile should match",
                uniqueOtrProfile,
                resolvedUniqueOtr);
        Assert.assertNotEquals(regularProfile, resolvedUniqueOtr);
        Assert.assertNotEquals(primaryOtrProfile, resolvedUniqueOtr);

        String regularToken = regularIntent.getStringExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA);
        String primaryOtrToken =
                primaryOtrIntent.getStringExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA);
        String uniqueOtrToken =
                uniqueOtrIntent.getStringExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA);
        Assert.assertNotNull(regularToken);
        Assert.assertNotNull(primaryOtrToken);
        Assert.assertNotNull(uniqueOtrToken);
        Assert.assertNotEquals(regularToken, primaryOtrToken);
        Assert.assertNotEquals(regularToken, uniqueOtrToken);
        Assert.assertNotEquals(primaryOtrToken, uniqueOtrToken);
    }

    @Test
    @SmallTest
    public void testProfileTokenUtils_LegacyIntent() throws ExecutionException {
        Profile profile = getLastUsedRegularProfileOnUiThread();

        String legacyRawToken = tokenizeOnUiThread(profile);
        Intent legacyIntent = createSelfTargetedIntent();
        legacyIntent.putExtra(ProfileTokenUtils.LEGACY_PROFILE_TOKEN_EXTRA, legacyRawToken);
        IntentUtils.addTrustedIntentExtras(legacyIntent);

        Assert.assertTrue(ProfileTokenUtils.hasProfileToken(legacyIntent));
        Assert.assertNull(
                ProfileTokenUtils.getNotificationTagFromIntent("TestPrefix", legacyIntent));
        Assert.assertEquals(
                "Legacy raw token intent should resolve to the same Profile object",
                profile,
                retrieveProfileFromIntentSync(legacyIntent));
    }
}
