// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.components.webapps.ShortcutSource;

/** Proxy that responds to tapping on the Bookmarks widget. */
public class BookmarkWidgetProxy extends Activity {
    private static final String TAG = "BookmarkWidgetProxy";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent intent = getIntent();

        if (BookmarkWidgetServiceImpl.getChangeFolderAction().equals(intent.getAction())) {
            BookmarkWidgetServiceImpl.changeFolder(intent);
        } else {
            Intent view = new Intent(intent);
            view.setClass(this, ChromeLauncherActivity.class);
            view.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.BOOKMARK_NAVIGATOR_WIDGET);
            view.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
            startActivity(this, view);
        }

        finish();
    }

    void startActivity(Context context, Intent intent) {
        try {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } catch (Exception e) {
            Log.w(TAG, "Failed to start intent activity", e);
        }
    }
}
