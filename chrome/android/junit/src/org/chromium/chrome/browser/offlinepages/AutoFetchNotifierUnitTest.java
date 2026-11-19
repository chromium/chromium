// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.service.notification.StatusBarNotification;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.offlinepages.AutoFetchNotifier.InProgressCancelReceiver;
import org.chromium.chrome.browser.offlinepages.AutoFetchNotifier.NotificationAction;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AutoFetchNotifier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutoFetchNotifierUnitTest {
    private static final byte[] PROFILE_TOKEN = "profile_token".getBytes(StandardCharsets.UTF_8);
    private static final byte[] OTHER_PROFILE_TOKEN =
            "other_profile_token".getBytes(StandardCharsets.UTF_8);
    private static final String IN_PROGRESS_TAG = "OfflinePageAutoFetchInProgressNotification";
    private static final String CANCEL_PREF_KEY =
            ChromePreferenceKeys.OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mOtherProfile;
    @Mock private Profile mLastUsedProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private AutoFetchNotifier.Natives mAutoFetchNotifierNatives;
    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock private OfflinePageBridge mOfflinePageBridge;

    private Context mContext;
    private String mProfileTag;
    private String mOtherProfileTag;
    private final List<Intent> mCancelButtonIntents = new ArrayList<>();
    private final List<Intent> mDeleteIntents = new ArrayList<>();
    private final List<Intent> mCompleteClickIntents = new ArrayList<>();

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();

        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        doReturn(PROFILE_TOKEN).when(mProfileResolverNatives).tokenizeProfileToBytes(mProfile);
        doReturn(OTHER_PROFILE_TOKEN)
                .when(mProfileResolverNatives)
                .tokenizeProfileToBytes(mOtherProfile);
        mProfileTag = ProfileTokenUtils.getNotificationTag(IN_PROGRESS_TAG, mProfile);
        mOtherProfileTag = ProfileTokenUtils.getNotificationTag(IN_PROGRESS_TAG, mOtherProfile);
        assertNotEquals(mProfileTag, mOtherProfileTag);
        setResolvedProfile(PROFILE_TOKEN, mProfile);
        setResolvedProfile(OTHER_PROFILE_TOKEN, mOtherProfile);

        AutoFetchNotifierJni.setInstanceForTesting(mAutoFetchNotifierNatives);
        ProfileManager.setLastUsedProfileForTesting(mLastUsedProfile);

        // Chrome is running in full browser by default.
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        doAnswer(
                        invocation -> {
                            invocation.<Runnable>getArgument(0).run();
                            return null;
                        })
                .when(mChromeBrowserInitializer)
                .runNowOrAfterFullBrowserStarted(any());

        AutoFetchNotifier.setTestHooksForTesting(
                new AutoFetchNotifier.TestHooks() {
                    @Override
                    public void inProgressNotificationShown(
                            Intent cancelButtonIntent, Intent deleteIntent) {
                        mCancelButtonIntents.add(cancelButtonIntent);
                        mDeleteIntents.add(deleteIntent);
                    }

                    @Override
                    public void completeNotificationShown(Intent clickIntent, Intent deleteIntent) {
                        mCompleteClickIntents.add(clickIntent);
                    }
                });
    }

    private void setResolvedProfile(byte[] token, Profile profile) {
        doAnswer(
                        invocation -> {
                            invocation.<Callback<Profile>>getArgument(1).onResult(profile);
                            return null;
                        })
                .when(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(token), any());
    }

    private void showInProgressNotification(Profile profile, int count) {
        AutoFetchNotifier.showInProgressNotification(profile, count);
        ShadowLooper.idleMainLooper();
    }

    private void updateInProgressNotificationCount(Profile profile, int count) {
        AutoFetchNotifier.updateInProgressNotificationCountIfShowing(profile, count);
        ShadowLooper.idleMainLooper();
    }

    /** Shows a notification like those posted before notifications were per-profile. */
    private void showLegacyInProgressNotification() {
        NotificationManager manager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.notify(
                IN_PROGRESS_TAG,
                0,
                new Notification.Builder(mContext, "legacy")
                        .setSmallIcon(android.R.drawable.ic_dialog_info)
                        .build());
    }

    /** Returns an intent like those of notifications posted before profile tokens were added. */
    private Intent createLegacyCancelIntent() {
        Intent legacyIntent = new Intent(mContext, InProgressCancelReceiver.class);
        legacyIntent.setPackage(mContext.getPackageName());
        legacyIntent.putExtra("notification_action", NotificationAction.CANCEL_PRESSED);
        return legacyIntent;
    }

    private List<String> getActiveInProgressNotificationTags() {
        NotificationManager manager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        List<String> tags = new ArrayList<>();
        for (StatusBarNotification notification : manager.getActiveNotifications()) {
            String tag = notification.getTag();
            if (tag != null && tag.startsWith(IN_PROGRESS_TAG)) tags.add(tag);
        }
        return tags;
    }

    private int readCancelPref() {
        return ChromeSharedPreferences.getInstance()
                .readInt(CANCEL_PREF_KEY, NotificationAction.NUM_ENTRIES);
    }

    private void receive(Intent intent) {
        new InProgressCancelReceiver().onReceive(mContext, intent);
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testShowInProgressNotification_usesPerProfileTagAndAttachesProfileToIntents() {
        showInProgressNotification(mProfile, /* count= */ 2);

        assertEquals(List.of(mProfileTag), getActiveInProgressNotificationTags());
        assertEquals(1, mCancelButtonIntents.size());
        Intent cancelIntent = mCancelButtonIntents.get(0);
        Intent deleteIntent = mDeleteIntents.get(0);
        assertTrue(ProfileTokenUtils.hasProfileToken(cancelIntent));
        assertTrue(ProfileTokenUtils.hasProfileToken(deleteIntent));
        assertEquals(
                mProfileTag,
                ProfileTokenUtils.getNotificationTagFromIntent(IN_PROGRESS_TAG, cancelIntent));
        assertEquals(
                mProfileTag,
                ProfileTokenUtils.getNotificationTagFromIntent(IN_PROGRESS_TAG, deleteIntent));
    }

    @Test
    public void testTwoProfiles_haveSeparateNotificationsAndIntents() {
        showInProgressNotification(mProfile, /* count= */ 1);
        showInProgressNotification(mOtherProfile, /* count= */ 2);

        List<String> tags = getActiveInProgressNotificationTags();
        assertEquals(2, tags.size());
        assertTrue(tags.contains(mProfileTag));
        assertTrue(tags.contains(mOtherProfileTag));
        // The profile-scoped identity keeps the PendingIntents of the two profiles apart.
        assertFalse(mCancelButtonIntents.get(0).filterEquals(mCancelButtonIntents.get(1)));
        assertFalse(mDeleteIntents.get(0).filterEquals(mDeleteIntents.get(1)));
    }

    @Test
    public void testCancelButton_closesNotificationAndCancelsForProfileFromIntent() {
        showInProgressNotification(mProfile, /* count= */ 1);
        showInProgressNotification(mOtherProfile, /* count= */ 1);

        receive(mCancelButtonIntents.get(0));

        // The pref is written until native reports cancellationComplete().
        assertEquals(NotificationAction.CANCEL_PRESSED, readCancelPref());
        verify(mProfileResolverNatives).resolveProfileFromBytes(aryEq(PROFILE_TOKEN), any());
        verify(mAutoFetchNotifierNatives).cancelInProgress(mProfile);
        verify(mAutoFetchNotifierNatives, never()).cancelInProgress(mOtherProfile);
        verify(mAutoFetchNotifierNatives, never()).cancelInProgress(mLastUsedProfile);
        // Only the canceled profile's notification is closed.
        assertEquals(List.of(mOtherProfileTag), getActiveInProgressNotificationTags());
    }

    @Test
    public void testDismiss_cancelsForProfileFromIntent() {
        showInProgressNotification(mOtherProfile, /* count= */ 1);

        receive(mDeleteIntents.get(0));

        assertEquals(NotificationAction.DISMISSED, readCancelPref());
        verify(mAutoFetchNotifierNatives).cancelInProgress(mOtherProfile);
        verify(mAutoFetchNotifierNatives, never()).cancelInProgress(mLastUsedProfile);
        assertTrue(getActiveInProgressNotificationTags().isEmpty());
    }

    @Test
    public void testCancel_beforeFullBrowserStarted_recordsPrefAndClosesNotification() {
        doAnswer(invocation -> null)
                .when(mChromeBrowserInitializer)
                .runNowOrAfterFullBrowserStarted(any());
        showInProgressNotification(mProfile, /* count= */ 1);

        receive(mCancelButtonIntents.get(0));

        assertEquals(NotificationAction.CANCEL_PRESSED, readCancelPref());
        verify(mAutoFetchNotifierNatives, never()).cancelInProgress(any());
        assertTrue(getActiveInProgressNotificationTags().isEmpty());
    }

    @Test
    public void testCancel_profileNoLongerExists_clearsPrefWithoutCanceling() {
        setResolvedProfile(PROFILE_TOKEN, null);
        showInProgressNotification(mProfile, /* count= */ 1);

        receive(mCancelButtonIntents.get(0));

        verify(mProfileResolverNatives).resolveProfileFromBytes(aryEq(PROFILE_TOKEN), any());
        verify(mAutoFetchNotifierNatives, never()).cancelInProgress(any());
        // Otherwise the initial profile would cancel its requests on next startup.
        assertFalse(ChromeSharedPreferences.getInstance().contains(CANCEL_PREF_KEY));
        assertTrue(getActiveInProgressNotificationTags().isEmpty());
    }

    @Test
    public void testCancel_intentWithoutProfileToken_fallsBackToLastUsedProfile() {
        showLegacyInProgressNotification();

        receive(createLegacyCancelIntent());

        verify(mProfileResolverNatives, never()).resolveProfileFromBytes(any(), any());
        verify(mAutoFetchNotifierNatives).cancelInProgress(mLastUsedProfile);
        assertTrue(getActiveInProgressNotificationTags().isEmpty());
    }

    @Test
    public void testUpdateCountToZero_closesLegacyNotificationForInitialProfileOnly() {
        doReturn(true).when(mProfile).isInitialProfile();
        doReturn(false).when(mOtherProfile).isInitialProfile();
        showInProgressNotification(mOtherProfile, /* count= */ 1);
        showLegacyInProgressNotification();

        // A non-initial profile closes only its own notification.
        updateInProgressNotificationCount(mOtherProfile, /* count= */ 0);
        assertEquals(List.of(IN_PROGRESS_TAG), getActiveInProgressNotificationTags());

        // The initial profile also closes the legacy notification, which only it could have
        // posted.
        updateInProgressNotificationCount(mProfile, /* count= */ 0);
        assertTrue(getActiveInProgressNotificationTags().isEmpty());
    }

    @Test
    public void testCancellationPref_initialProfileWithPrefSet_returnsTrue() {
        doReturn(true).when(mProfile).isInitialProfile();
        ChromeSharedPreferences.getInstance()
                .writeInt(CANCEL_PREF_KEY, NotificationAction.CANCEL_PRESSED);

        assertTrue(AutoFetchNotifier.autoFetchInProgressNotificationCanceled(mProfile));
    }

    @Test
    public void testCancellationPref_nonInitialProfileWithPrefSet_returnsFalse() {
        doReturn(false).when(mOtherProfile).isInitialProfile();
        ChromeSharedPreferences.getInstance()
                .writeInt(CANCEL_PREF_KEY, NotificationAction.CANCEL_PRESSED);

        assertFalse(AutoFetchNotifier.autoFetchInProgressNotificationCanceled(mOtherProfile));
    }

    @Test
    public void testCancellationPref_prefUnset_returnsFalse() {
        doReturn(true).when(mProfile).isInitialProfile();

        assertFalse(AutoFetchNotifier.autoFetchInProgressNotificationCanceled(mProfile));
    }

    @Test
    public void testShowCompleteNotification_usesPassedProfile() {
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        doReturn(mOfflinePageBridge).when(mOfflinePageUtils).getOfflinePageBridge(mProfile);
        doAnswer(
                        invocation -> {
                            invocation
                                    .<Callback<LoadUrlParams>>getArgument(2)
                                    .onResult(new LoadUrlParams("https://example.com/"));
                            return null;
                        })
                .when(mOfflinePageBridge)
                .getLoadUrlParamsByOfflineId(eq(42L), eq(LaunchLocation.NOTIFICATION), any());

        AutoFetchNotifier.showCompleteNotification(
                mProfile,
                "Title",
                "https://example.com/",
                "https://example.com/",
                /* tabId= */ 1,
                /* offlineId= */ 42L);
        ShadowLooper.idleMainLooper();

        verify(mOfflinePageUtils).getOfflinePageBridge(mProfile);
        verify(mOfflinePageUtils, never()).getOfflinePageBridge(mLastUsedProfile);
        verify(mOfflinePageBridge).getLoadUrlParamsByOfflineId(eq(42L), anyInt(), any());
        assertEquals(1, mCompleteClickIntents.size());
        assertTrue(ProfileTokenUtils.hasProfileToken(mCompleteClickIntents.get(0)));
    }
}
