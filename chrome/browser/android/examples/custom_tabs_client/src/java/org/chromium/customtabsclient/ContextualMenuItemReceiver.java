// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import androidx.browser.customtabs.ContentActionSelectedData;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.Toast;

/**
 * A {@link ContextualMenuItemReceiver} that handles the callback for triggered contextual menu
 * items.
 */
@NullMarked
public class ContextualMenuItemReceiver extends BroadcastReceiver {
    public static final String ACTION_IMAGE_ITEM_CLICKED = "action_image_item_clicked";
    public static final String ACTION_LINK_ITEM_CLICKED = "action_link_item_clicked";

    @Override
    public void onReceive(Context context, Intent intent) {
        ContentActionSelectedData casd = ContentActionSelectedData.fromIntent(intent);
        if (casd == null) {
            Toast.makeText(context, "No ContentActionSelectedData", Toast.LENGTH_SHORT).show();
            return;
        }
        StringBuilder toastMessage = new StringBuilder();
        if (intent.getAction() != null) {
            toastMessage.append(
                    intent.getAction().equals(ACTION_IMAGE_ITEM_CLICKED)
                            ? "Image Action"
                            : "Link Action");
        }
        toastMessage
                .append("Triggered action type: ")
                .append(casd.getClickedContentTargetType())
                .append('\n');
        toastMessage
                .append("Triggered action id: ")
                .append(casd.getTriggeredActionId())
                .append('\n');
        toastMessage.append("Page url: ").append(casd.getPageUrl());
        Toast.makeText(context, toastMessage.toString(), Toast.LENGTH_LONG).show();
    }
}
