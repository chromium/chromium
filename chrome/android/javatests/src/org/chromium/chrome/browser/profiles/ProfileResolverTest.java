// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.isEmptyString;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.ReducedModeNativeTestRule;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.content_public.browser.BrowserContextHandle;

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
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ProfileResolverTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public ReducedModeNativeTestRule mReducedModeNativeTestRule =
            new ReducedModeNativeTestRule(/* autoLoadNative= */ false);

    private ProfileResolver mProfileResolver;

    @Before
    public void setUp() {
        mProfileResolver = new ProfileResolver();
    }

    private void initToFullMode() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    private void initToReducedMode() {
        mReducedModeNativeTestRule.loadNative();
    }

    private Profile getLastUsedRegularProfileOnUiThread() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> ProfileManager.getLastUsedRegularProfile());
    }

    private Profile getPrimaryOtrProfileOnUiThread() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ProfileManager.getLastUsedRegularProfile()
                                .getPrimaryOTRProfile(/* createIfNeeded= */ true));
    }

    private Profile newOtrProfileOnUiThread(String profileIdPrefix) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile regularProfile = ProfileManager.getLastUsedRegularProfile();
                    OTRProfileID otrProfileId = OTRProfileID.createUnique(profileIdPrefix);
                    return regularProfile.getOffTheRecordProfile(
                            otrProfileId, /* createIfNeeded= */ true);
                });
    }

    private ProfileKey getPrimaryProfileKeyOnUiThread() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ProfileManager.getLastUsedRegularProfile().getProfileKey());
    }

    private String tokenizeOnUiThread(Profile profile) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> mProfileResolver.tokenize(profile));
    }

    private String tokenizeOnUiThread(ProfileKey profileKey) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> mProfileResolver.tokenize(profileKey));
    }

    private String tokenizeOnUiThread(BrowserContextHandle browserContext)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> mProfileResolver.tokenize(browserContext));
    }

    private String tokenizeOnUiThread(SimpleFactoryKeyHandle simpleFactoryKey)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> mProfileResolver.tokenize(simpleFactoryKey));
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

    private ProfileKey resolveProfileKeySync(String token) {
        PayloadCallbackHelper<ProfileKey> callbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileResolver.resolveProfileKey(
                            token, (ProfileKey p) -> callbackHelper.notifyCalled(p));
                });
        return callbackHelper.getOnlyPayloadBlocking();
    }

    private BrowserContextHandle resolveBrowserContextSync(String token) {
        PayloadCallbackHelper<BrowserContextHandle> callbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileResolver.resolveBrowserContext(
                            token, (BrowserContextHandle p) -> callbackHelper.notifyCalled(p));
                });
        return callbackHelper.getOnlyPayloadBlocking();
    }

    private SimpleFactoryKeyHandle resolveSimpleFactoryKeySync(String token) {
        PayloadCallbackHelper<SimpleFactoryKeyHandle> callbackHelper =
                new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileResolver.resolveSimpleFactoryKey(
                            token, (SimpleFactoryKeyHandle p) -> callbackHelper.notifyCalled(p));
                });
        return callbackHelper.getOnlyPayloadBlocking();
    }

    @Test
    @SmallTest
    public void testResolveProfile() throws ExecutionException {
        initToFullMode();
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
        initToFullMode();
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
    public void testResolveProfileKey() throws ExecutionException {
        initToFullMode();
        ProfileKey key = getPrimaryProfileKeyOnUiThread();

        String token = tokenizeOnUiThread(key);
        ProfileKey resolvedKey = resolveProfileKeySync(token);

        Assert.assertEquals("Round tripped profile key should match", key, resolvedKey);
    }

    @Test
    @SmallTest
    public void testResolveProfileKeyBeforeProfileInit() throws ExecutionException {
        initToReducedMode();
        ProfileKey key =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> ProfileKeyUtil.getLastUsedRegularProfileKey());

        String token = tokenizeOnUiThread(key);
        ProfileKey resolvedKey = resolveProfileKeySync(token);

        Assert.assertEquals("Round tripped profile key should match", key, resolvedKey);
    }

    @Test
    @SmallTest
    public void testMixProfileAndProfileKey() throws ExecutionException {
        initToFullMode();
        Profile profile = getLastUsedRegularProfileOnUiThread();
        ProfileKey key = getPrimaryProfileKeyOnUiThread();

        String keyToken = tokenizeOnUiThread(key);
        Profile resolvedProfile = resolveProfileSync(keyToken);
        Assert.assertEquals("Round tripped profile should match", profile, resolvedProfile);
        Assert.assertEquals(
                "Round tripped profile key should match", key, resolvedProfile.getProfileKey());

        String profileToken = tokenizeOnUiThread(profile);
        ProfileKey resolvedKey = resolveProfileKeySync(profileToken);
        Assert.assertEquals("Round tripped profile key should match", key, resolvedKey);
        Assert.assertEquals("Tokens should be identical", keyToken, profileToken);
    }

    @Test
    @SmallTest
    public void testResolveBrowserContext() throws ExecutionException {
        initToFullMode();
        BrowserContextHandle handle = getLastUsedRegularProfileOnUiThread();

        String token = tokenizeOnUiThread(handle);
        BrowserContextHandle resolvedHandle = resolveBrowserContextSync(token);

        Assert.assertEquals(
                "Round tripping should result in the same BrowserContextHandle object",
                handle,
                resolvedHandle);
    }

    @Test
    @SmallTest
    public void testResolveSimpleFactoryKey() throws ExecutionException {
        initToFullMode();
        SimpleFactoryKeyHandle handle = getPrimaryProfileKeyOnUiThread();

        String token = tokenizeOnUiThread(handle);
        SimpleFactoryKeyHandle resolvedHandle = resolveSimpleFactoryKeySync(token);

        Assert.assertEquals(
                "Round tripping should result in the same SimpleFactoryKeyHandle object",
                handle,
                resolvedHandle);
    }

    @Test
    @SmallTest
    public void testTokenizeNulls() throws ExecutionException {
        initToFullMode();

        // Put nulls into variables first to get correct overloaded methods.
        Profile profile = null;
        assertThat(
                "Tokenizing a null profile should not work",
                tokenizeOnUiThread(profile),
                isEmptyString());

        ProfileKey profileKey = null;
        assertThat(
                "Tokenizing a null profile key should not work",
                tokenizeOnUiThread(profileKey),
                isEmptyString());
    }

    @Test
    @SmallTest
    public void testResolveBadTokens() throws ExecutionException {
        initToFullMode();
        List<String> badTokens = Arrays.asList(null, "", "abcdef");

        for (String token : badTokens) {
            Assert.assertNull(
                    "#resolveProfile() did not resolve null for " + token,
                    resolveProfileSync(token));
            Assert.assertNull(
                    "#resolveProfileKey() did not resolve null for " + token,
                    resolveProfileKeySync(token));
        }
    }
}
