// Copyright 2017 The Chromium Authors
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
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.PlaybackStateCompat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.services.media_session.MediaMetadata;
import org.chromium.services.media_session.MediaPosition;

/**
 * JUnit tests for checking MediaNotificationManager presents correct notification to Android
 * NotificationManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = MediaNotificationTestShadowResources.class)
public class MediaNotificationManagerNotificationTest extends MediaNotificationTestBase {
    @Test
    public void updateNotificationBuilderDisplaysCorrectMetadata_EmptyArtistAndAlbum() {
        mMediaNotificationInfoBuilder.setMetadata(new MediaMetadata("title", "", ""));
        mMediaNotificationInfoBuilder.setOrigin("https://example.com/");

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        if (info.isPrivate) {
            assertNotEquals(info.metadata.getTitle(), shadowNotification.getContentTitle());
            assertNull(shadowNotification.getContentText());
            assertNotEquals(
                    info.origin, notification.extras.getString(Notification.EXTRA_SUB_TEXT));
        } else {
            assertEquals(info.metadata.getTitle(), shadowNotification.getContentTitle());
            assertEquals("", shadowNotification.getContentText());
            assertEquals(info.origin, notification.extras.getString(Notification.EXTRA_SUB_TEXT));
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_WithLargeIcon() {
        Bitmap largeIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mMediaNotificationInfoBuilder.setNotificationLargeIcon(largeIcon);

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        if (info.isPrivate) {
            assertNull(notification.getLargeIcon());
        } else {
            assertTrue(largeIcon.sameAs(iconToBitmap(notification.getLargeIcon())));
        }
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_WithoutLargeIcon_AtLeastN() {
        mMediaNotificationInfoBuilder.setNotificationLargeIcon(null);

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        assertNull(notification.getLargeIcon());
        assertNull(getController().mDefaultNotificationLargeIcon);
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectLargeIcon_DontSupportPlayPause() {
        Bitmap largeIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mMediaNotificationInfoBuilder.setNotificationLargeIcon(largeIcon).setActions(0);

        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        assertNull(notification.getLargeIcon());
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMiscInfo() {
        mMediaNotificationInfoBuilder
                .setNotificationSmallIcon(/* resId= */ 1)
                .setActions(0)
                .setContentIntent(new Intent());
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        assertFalse(shadowNotification.isWhenShown());
        assertFalse(shadowNotification.isOngoing());
        assertNotNull(notification.getSmallIcon());
        assertFalse((notification.flags & Notification.FLAG_AUTO_CANCEL) != 0);
        assertTrue((notification.flags & Notification.FLAG_LOCAL_ONLY) != 0);
        assertEquals(NOTIFICATION_GROUP_NAME, notification.getGroup());
        assertTrue(notification.isGroupSummary());
        assertNotNull(notification.contentIntent);
        assertEquals(Notification.VISIBILITY_PRIVATE, notification.visibility);
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMiscInfo_SupportsSwipeAway() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        assertTrue(shadowNotification.isOngoing());
        assertNotNull(notification.deleteIntent);
    }

    @Test
    public void updateNotificationBuilderDisplaysCorrectMiscInfo_Private() {
        mMediaNotificationInfoBuilder.setPrivate(false);
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        Notification notification = updateNotificationBuilderAndBuild(info);

        assertEquals(Notification.VISIBILITY_PUBLIC, notification.visibility);
    }

    @Test
    public void mediaPosition_Present() {
        MediaPosition position = new MediaPosition(10, 5, 2.0f, 10000);
        mMediaNotificationInfoBuilder.setPaused(false);
        mMediaNotificationInfoBuilder.setMediaPosition(position);
        mMediaNotificationInfoBuilder.setPrivate(false);
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();

        MediaMetadataCompat metadata = getController().createMetadata();
        assertEquals(10, metadata.getLong(MediaMetadataCompat.METADATA_KEY_DURATION));

        PlaybackStateCompat state = getController().createPlaybackState();
        assertEquals(PlaybackStateCompat.STATE_PLAYING, state.getState());
        assertEquals(2.0f, state.getPlaybackSpeed(), 0);
        assertEquals(5, state.getPosition());
        assertEquals(10000, state.getLastPositionUpdateTime());
    }

    @Test
    public void mediaPosition_Present_Paused() {
        MediaPosition position = new MediaPosition(10, 5, 2.0f, 10000);
        mMediaNotificationInfoBuilder.setPaused(true);
        mMediaNotificationInfoBuilder.setMediaPosition(position);
        mMediaNotificationInfoBuilder.setPrivate(false);
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();

        MediaMetadataCompat metadata = getController().createMetadata();
        assertEquals(10, metadata.getLong(MediaMetadataCompat.METADATA_KEY_DURATION));

        PlaybackStateCompat state = getController().createPlaybackState();
        assertEquals(PlaybackStateCompat.STATE_PAUSED, state.getState());
        assertEquals(2.0f, state.getPlaybackSpeed(), 0);
        assertEquals(5, state.getPosition());
        assertEquals(10000, state.getLastPositionUpdateTime());
    }

    @Test
    public void mediaPosition_Missing() {
        mMediaNotificationInfoBuilder.setPaused(false);
        mMediaNotificationInfoBuilder.setPrivate(false);
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();

        MediaMetadataCompat metadata = getController().createMetadata();
        assertFalse(metadata.containsKey(MediaMetadataCompat.METADATA_KEY_DURATION));

        PlaybackStateCompat state = getController().createPlaybackState();
        assertEquals(PlaybackStateCompat.STATE_PLAYING, state.getState());
        assertEquals(1.0f, state.getPlaybackSpeed(), 0);
        assertEquals(PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, state.getPosition());
    }

    @Test
    public void mediaPosition_Missing_Paused() {
        mMediaNotificationInfoBuilder.setPaused(true);
        mMediaNotificationInfoBuilder.setPrivate(false);
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();

        MediaMetadataCompat metadata = getController().createMetadata();
        assertFalse(metadata.containsKey(MediaMetadataCompat.METADATA_KEY_DURATION));

        PlaybackStateCompat state = getController().createPlaybackState();
        assertEquals(PlaybackStateCompat.STATE_PAUSED, state.getState());
        assertEquals(1.0f, state.getPlaybackSpeed(), 0);
        assertEquals(PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, state.getPosition());
    }

    private Notification updateNotificationBuilderAndBuild(MediaNotificationInfo info) {
        getController().mMediaNotificationInfo = info;

        // This is the fake implementation to ensure |mMediaSession| is non-null.
        //
        // TODO(zqzhang): move around code so that updateNotification() doesn't need a MediaSession.
        getController().updateMediaSession();
        getController().updateNotificationBuilder();

        return getController().mNotificationBuilder.build();
    }
}
