// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.components.browser_ui.share.ShareParams;

/**
 * Receives shared content broadcast from Chrome Custom Tabs and shows a share sheet to share the
 * url.
 */
@NullMarked
public final class CustomTabsShareBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        RecordUserAction.record("MobileTopToolbarShareButton");
        String url = intent.getDataString();
        Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.setType("text/plain");
        shareIntent.putExtra(Intent.EXTRA_TEXT, url);
        shareIntent.putExtra(
                ShareParams.EXTRA_SHARE_ORIGIN, ShareDelegate.ShareOrigin.CUSTOM_TAB_SHARE_BUTTON);
        Intent chooserIntent = Intent.createChooser(shareIntent, null);
        chooserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(chooserIntent);
    }
}
