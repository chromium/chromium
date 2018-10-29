// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Notification;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.content_public.common.MediaMetadata;

/**
 * JUnit tests for checking MediaNotificationManager presents correct notification to Android
 * NotificationManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        // Remove this after updating to a version of Robolectric that supports
        // notification channel creation. crbug.com/774315
        sdk = Build.VERSION_CODES.N_MR1, shadows = MediaNotificationTestShadowResources.class)
public class MediaNotificationManagerNotificationTest extends MediaNotificationManagerTestBase {
    @Test
    public void updateNotificationBuilderDisplaysCorrectMetadata_PreN_NonEmptyArtistAndAlbum() {
        MediaNotificationManager.sOverrideIsRunningNForTesting = false;

        mMediaNotificationInfoBuilder.setMetadata(new MediaMetadata("title", "artist", "album"));
        mMediaNotificationInfoBuilder.setOrigin("https://example.com/");

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        boolean userDataIsHidden = info.isPrivate
                && ChromeFeatureList.isEnabled(
                           ChromeFeatureList.HIDE_USER_DATA_FROM_INCOGNITO_NOTIFICATIONS);
        if (userDataIsHidden) {
            assertNotEquals("title", shadowNotification.getContentTitle());
            assertNotEquals("artist - album", shadowNotification.getContentText());
            if (hasNApis()) {
                assertNull(notification.extras.getString(Notification.EXTRA_SUB_TEXT));
            }
        } else {
            assertEquals("title", shadowNotification.getContentTitle());
            assertEquals("artist - album", shadowNotification.getContentText());

            if (hasNApis()) {
                assertEquals("https://example.com/",
                        notification.extras.getString(Notification.EXTRA_SUB_TEXT));
            }
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMetadata_PreN_EmptyArtistAndAlbum() {
        MediaNotificationManager.sOverrideIsRunningNForTesting = false;

        mMediaNotificationInfoBuilder.setMetadata(new MediaMetadata("title", "", ""));
        mMediaNotificationInfoBuilder.setOrigin("https://example.com/");

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        boolean userDataIsHidden = info.isPrivate
                && ChromeFeatureList.isEnabled(
                           ChromeFeatureList.HIDE_USER_DATA_FROM_INCOGNITO_NOTIFICATIONS);
        if (userDataIsHidden) {
            assertNotEquals(info.metadata.getTitle(), shadowNotification.getContentTitle());
            assertNull(shadowNotification.getContentText());
        } else {
            assertEquals(info.metadata.getTitle(), shadowNotification.getContentTitle());
            assertEquals(info.origin, shadowNotification.getContentText());
        }
        if (hasNApis()) {
            assertEquals(null, notification.extras.getString(Notification.EXTRA_SUB_TEXT));
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMetadata_AtLeastN_EmptyArtistAndAlbum() {
        MediaNotificationManager.sOverrideIsRunningNForTesting = true;

        mMediaNotificationInfoBuilder.setMetadata(new MediaMetadata("title", "", ""));
        mMediaNotificationInfoBuilder.setOrigin("https://example.com/");

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        boolean userDataIsHidden = info.isPrivate
                && ChromeFeatureList.isEnabled(
                           ChromeFeatureList.HIDE_USER_DATA_FROM_INCOGNITO_NOTIFICATIONS);
        if (userDataIsHidden) {
            assertNotEquals(info.metadata.getTitle(), shadowNotification.getContentTitle());
            assertNull(shadowNotification.getContentText());
            if (hasNApis()) {
                assertNotEquals(
                        info.origin, notification.extras.getString(Notification.EXTRA_SUB_TEXT));
            }
        } else {
            assertEquals(info.metadata.getTitle(), shadowNotification.getContentTitle());
            assertEquals("", shadowNotification.getContentText());
            if (hasNApis()) {
                assertEquals(
                        info.origin, notification.extras.getString(Notification.EXTRA_SUB_TEXT));
            }
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_WithLargeIcon() {
        Bitmap largeIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mMediaNotificationInfoBuilder.setNotificationLargeIcon(largeIcon);

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        if (hasNApis()) {
            boolean userDataIsHidden = info.isPrivate
                    && ChromeFeatureList.isEnabled(
                               ChromeFeatureList.HIDE_USER_DATA_FROM_INCOGNITO_NOTIFICATIONS);
            if (userDataIsHidden) {
                assertNull(notification.getLargeIcon());
            } else {
                assertTrue(largeIcon.sameAs(iconToBitmap(notification.getLargeIcon())));
            }
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_WithoutLargeIcon_AtLeastN() {
        mMediaNotificationInfoBuilder.setNotificationLargeIcon(null);

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        if (hasNApis()) {
            assertNull(notification.getLargeIcon());
        }
        assertNull(getManager().mDefaultNotificationLargeIcon);
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_WithoutLargeIcon_PreN() {
        MediaNotificationManager.sOverrideIsRunningNForTesting = false;
        assertNull(getManager().mDefaultNotificationLargeIcon);

        mMediaNotificationInfoBuilder.setNotificationLargeIcon(null);

        MediaNotificationInfo info =
                mMediaNotificationInfoBuilder
                        .setDefaultNotificationLargeIcon(R.drawable.audio_playing_square)
                        .build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        assertNotNull(getManager().mDefaultNotificationLargeIcon);
        if (hasNApis()) {
            assertTrue(getManager().mDefaultNotificationLargeIcon.sameAs(
                    iconToBitmap(notification.getLargeIcon())));
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_DontSupportPlayPause() {
        Bitmap largeIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mMediaNotificationInfoBuilder.setNotificationLargeIcon(largeIcon).setActions(0);

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        if (hasNApis()) {
            assertNull(notification.getLargeIcon());
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMiscInfo() {
        mMediaNotificationInfoBuilder.setNotificationSmallIcon(1 /* resId */)
                .setActions(0)
                .setContentIntent(new Intent());
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        assertFalse(shadowNotification.isWhenShown());
        assertFalse(shadowNotification.isOngoing());
        if (hasNApis()) {
            assertNotNull(notification.getSmallIcon());
            assertFalse((notification.flags & Notification.FLAG_AUTO_CANCEL) != 0);
            assertTrue((notification.flags & Notification.FLAG_LOCAL_ONLY) != 0);
            assertEquals(NOTIFICATION_GROUP_NAME, notification.getGroup());
            assertTrue(notification.isGroupSummary());
            assertNull(notification.deleteIntent);
            assertNotNull(notification.contentIntent);
            assertEquals(Notification.VISIBILITY_PRIVATE, notification.visibility);
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMiscInfo_SupportsSwipeAway() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        assertTrue(shadowNotification.isOngoing());
        if (hasNApis()) {
            assertNotNull(notification.deleteIntent);
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMiscInfo_Private() {
        mMediaNotificationInfoBuilder.setPrivate(false);
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        if (hasNApis()) {
            assertEquals(Notification.VISIBILITY_PUBLIC, notification.visibility);
        }
    }

    private Notification updateNotificationBuilderAndBuild(MediaNotificationInfo info) {
        getManager().mMediaNotificationInfo = info;

        // This is the fake implementation to ensure |mMediaSession| is non-null.
        //
        // TODO(zqzhang): move around code so that updateNotification() doesn't need a MediaSession.
        getManager().updateMediaSession();
        getManager().updateNotificationBuilder();

        return getManager().mNotificationBuilder.build();
    }

    private boolean hasNApis() {
        return RuntimeEnvironment.getApiLevel() >= Build.VERSION_CODES.N;
    }
}
