// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import static androidx.browser.customtabs.CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE;
import static androidx.browser.customtabs.CustomTabsIntent.CONTENT_TARGET_TYPE_LINK;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.Html;
import android.text.Spanned;
import android.util.Log;

import androidx.browser.customtabs.ContentActionSelectedData;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.build.annotations.NullMarked;

/**
 * A {@link ContextualMenuItemReceiver} that handles the callback for triggered contextual menu
 * items.
 */
@NullMarked
public class ContextualMenuItemReceiver extends BroadcastReceiver {
    private static final String TAG = "CMenuItemReceiver";
    public static final String ACTION_IMAGE_ITEM_CLICKED = "action_image_item_clicked";
    public static final String ACTION_LINK_ITEM_CLICKED = "action_link_item_clicked";
    private static final int NOTIFICATION_ID = 1512;

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(TAG, "ContextualMenuItemReceiver's onReceive was TRIGGERED!");
        ContentActionSelectedData casd = ContentActionSelectedData.fromIntent(intent);
        if (casd == null) {
            Log.w(TAG, "ContentActionSelectedData was null. Cannot show notification.");
            return;
        }

        String notificationTitle = "Contextual Action Triggered";
        if (intent.getAction() != null) {
            notificationTitle =
                    intent.getAction().equals(ACTION_IMAGE_ITEM_CLICKED)
                            ? "Image Action Triggered"
                            : "Link Action Triggered";
        }

        StringBuilder htmlBuilder = new StringBuilder();

        int actionType = casd.getClickedContentTargetType();
        String actionTypeString;
        switch (actionType) {
            case CONTENT_TARGET_TYPE_IMAGE:
                actionTypeString = "Image";
                break;
            case CONTENT_TARGET_TYPE_LINK:
                actionTypeString = "Link";
                break;
            default:
                actionTypeString = "Unknown (" + actionType + ")";
                break;
        }

        htmlBuilder.append("<b>Type:</b> ").append(actionTypeString).append("<br>");
        htmlBuilder.append("<b>Action ID:</b> ").append(casd.getTriggeredActionId()).append("<br>");

        String linkUrl = casd.getLinkUrl();
        if (linkUrl != null) {
            htmlBuilder.append("<b>Link URL:</b> ").append(linkUrl).append("<br>");
        }

        String linkText = casd.getLinkText();
        if (linkText != null && !linkText.isEmpty()) {
            htmlBuilder.append("<b>Link Text:</b> \"").append(linkText).append("\"<br>");
        }

        String imageAltText = casd.getImageAltText();
        if (imageAltText != null && !imageAltText.isEmpty()) {
            htmlBuilder.append("<b>Image Alt Text:</b> \"").append(imageAltText).append("\"<br>");
        }

        Uri imageUri = casd.getImageDataUri();
        if (imageUri != null) {
            htmlBuilder.append("<b>Image URI:</b> (data URI present)<br>");
        }

        String imageUrl = casd.getImageUrl();
        if (imageUrl != null) {
            htmlBuilder.append("<b>Image URL:</b> ").append(imageUrl).append("<br>");
        }

        Uri pageUrl = casd.getPageUrl();
        if (pageUrl != null) {
            htmlBuilder.append("<b>Page URL:</b> ").append(pageUrl.toString());
        }

        Spanned notificationText =
                Html.fromHtml(htmlBuilder.toString(), Html.FROM_HTML_MODE_COMPACT);

        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(context, MyApplication.CHANNEL_ID)
                        .setSmallIcon(R.drawable.ic_notification_icon)
                        .setContentTitle(notificationTitle)
                        .setContentText("Action triggered for " + actionTypeString.toLowerCase())
                        .setStyle(new NotificationCompat.BigTextStyle().bigText(notificationText))
                        .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                        .setAutoCancel(true);

        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        notificationManager.notify(NOTIFICATION_ID, builder.build());
        Log.d(TAG, "Notification has been posted using formatted BigTextStyle.");
    }
}
