// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.Locale;

/** Proxy that responds to tapping on the Bookmarks widget. */
@NullMarked
public class BookmarkWidgetProxy extends Activity {
    private static final String TAG = "BookmarkWidgetProxy";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent intent = getIntent();

        if (BookmarkWidgetServiceImpl.getChangeFolderAction().equals(intent.getAction())) {
            BookmarkWidgetServiceImpl.changeFolder(intent);
        } else {
            Uri data = intent.getData();
            if (isSafeBookmarkUrl(data)) {
                Intent view;
                if (data == null || Uri.EMPTY.equals(data)) {
                    view = new Intent(this, ChromeLauncherActivity.class);
                    view.setAction(Intent.ACTION_MAIN);
                    view.addCategory(Intent.CATEGORY_LAUNCHER);
                } else {
                    view = new Intent(Intent.ACTION_VIEW, data);
                    view.setClass(this, ChromeLauncherActivity.class);
                    view.addCategory(Intent.CATEGORY_BROWSABLE);
                }

                view.putExtra(
                        WebappConstants.EXTRA_SOURCE, ShortcutSource.BOOKMARK_NAVIGATOR_WIDGET);
                view.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);

                String bookmarkId =
                        IntentUtils.safeGetStringExtra(
                                intent, IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID);
                if (bookmarkId != null) {
                    view.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID, bookmarkId);
                }

                IntentUtils.addTrustedIntentExtras(view);
                startActivity(this, view);
            } else {
                Log.w(TAG, "Ignoring untrusted or unsafe bookmark widget URL: " + data);
            }
        }

        finish();
    }

    private static boolean isSafeBookmarkUrl(@Nullable Uri data) {
        if (data == null || Uri.EMPTY.equals(data)) {
            return true;
        }
        String scheme = data.getScheme();
        if (scheme == null) {
            return false;
        }
        String lowerScheme = scheme.toLowerCase(Locale.US);
        if (UrlConstants.HTTP_SCHEME.equals(lowerScheme)
                || UrlConstants.HTTPS_SCHEME.equals(lowerScheme)
                || UrlConstants.CHROME_NATIVE_SCHEME.equals(lowerScheme)) {
            return true;
        }
        if (UrlConstants.ABOUT_SCHEME.equals(lowerScheme)) {
            String uriString = data.toString();
            return ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(uriString)
                    || ContentUrlConstants.ABOUT_BLANK_URL.equals(uriString);
        }
        return false;
    }

    void startActivity(Context context, Intent intent) {
        try {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } catch (Exception e) {
            Log.w(TAG, "Failed to start intent activity", e);
        }
    }

    static PendingIntent createBookmarkProxyLaunchIntent(Context context) {
        Intent intent = new Intent(context, BookmarkWidgetProxy.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return PendingIntent.getActivity(
                context,
                0,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(true));
    }
}
