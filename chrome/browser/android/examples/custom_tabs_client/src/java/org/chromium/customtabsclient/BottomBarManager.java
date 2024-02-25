// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.media.MediaPlayer;
import android.widget.RemoteViews;
import android.widget.Toast;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;

import java.lang.ref.WeakReference;

/** A {@link BroadcastReceiver} that manages the interaction with the active Custom Tab. */
public class BottomBarManager extends BroadcastReceiver {
    /**
     * A {@link BroadcastReceiver} that receives the swipe-up gesture on the Custom Tab bottom bar.
     */
    public static class SwipeUpReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Toast.makeText(context, "Swiped up!", Toast.LENGTH_SHORT).show();
            assert intent.getData() == null : "Swipe-up gesture should come without data URI";
        }
    }

    private static WeakReference<MediaPlayer> sMediaPlayerWeakRef;

    @Override
    public void onReceive(Context context, Intent intent) {
        int clickedId = intent.getIntExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_CLICKED_ID, -1);
        Toast.makeText(
                        context,
                        "Current URL " + intent.getDataString() + "\nClicked id " + clickedId,
                        Toast.LENGTH_SHORT)
                .show();

        CustomTabsSession session = SessionHelper.getCurrentSession();
        if (session == null) return;

        if (clickedId == R.id.play_pause) {
            MediaPlayer player = sMediaPlayerWeakRef.get();
            if (player != null) {
                boolean isPlaying = player.isPlaying();
                if (isPlaying) player.pause();
                else player.start();
                // Update the play/stop icon to respect the current state.
                session.setSecondaryToolbarViews(
                        createRemoteViews(context, isPlaying),
                        getClickableIDs(),
                        getOnClickPendingIntent(context));
            }
        } else if (clickedId == R.id.cover) {
            // Clicking on the cover image will dismiss the bottom bar.
            session.setSecondaryToolbarViews(null, null, null);
        }
    }

    /**
     * Creates a RemoteViews that will be shown as the bottom bar of the custom tab.
     * @param showPlayIcon If true, a play icon will be shown, otherwise show a pause icon.
     * @return The created RemoteViews instance.
     */
    public static RemoteViews createRemoteViews(Context context, boolean showPlayIcon) {
        RemoteViews remoteViews = new RemoteViews(context.getPackageName(), R.layout.remote_view);

        int iconRes = showPlayIcon ? R.drawable.ic_play : R.drawable.ic_stop;
        remoteViews.setImageViewResource(R.id.play_pause, iconRes);
        return remoteViews;
    }

    /**
     * @return A list of View ids, the onClick event of which is handled by Custom Tab.
     */
    public static int[] getClickableIDs() {
        return new int[] {R.id.play_pause, R.id.cover};
    }

    /**
     * @return The PendingIntent that will be triggered when the user clicks on the Views listed by
     * {@link BottomBarManager#getClickableIDs()}.
     */
    public static PendingIntent getOnClickPendingIntent(Context context) {
        Intent broadcastIntent = new Intent(context, BottomBarManager.class);
        return PendingIntent.getBroadcast(context, 0, broadcastIntent, PendingIntent.FLAG_MUTABLE);
    }

    /** Sets the {@link MediaPlayer} to be used when the user clicks on the RemoteViews. */
    public static void setMediaPlayer(MediaPlayer player) {
        sMediaPlayerWeakRef = new WeakReference<>(player);
    }
}
