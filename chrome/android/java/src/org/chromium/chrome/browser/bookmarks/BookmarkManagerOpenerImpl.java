// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry.isBookmarksPageOverridden;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

@NullMarked
public class BookmarkManagerOpenerImpl implements BookmarkManagerOpener {

    // TODO(crbug.com/): Figure out what dependencies can be injected here.
    public BookmarkManagerOpenerImpl() {}

    @Override
    public void showBookmarkManager(
            Activity activity, @Nullable Tab tab, Profile profile, @Nullable BookmarkId folderId) {
        ThreadUtils.assertOnUiThread();
        String url = getFirstUrlToLoad(folderId, profile);

        if (ChromeSharedPreferences.getInstance()
                .contains(ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL)) {
            RecordUserAction.record("MobileBookmarkManagerReopenBookmarksInSameSession");
        }

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            showBookmarkManagerOnTablet(tab, url, profile);
        } else {
            showBookmarkManagerOnPhone(activity, url, profile);
        }
    }

    @Override
    public void startEditActivity(Context context, Profile profile, BookmarkId bookmarkId) {
        RecordUserAction.record("MobileBookmarkManagerEditBookmark");
        Intent intent = getEditActivityIntent(context, profile, bookmarkId);
        if (context instanceof BookmarkActivity bookmarkActivity) {
            bookmarkActivity.startActivityForResult(
                    intent, BookmarkActivity.EDIT_BOOKMARK_REQUEST_CODE);
        } else {
            context.startActivity(intent);
        }
    }

    @Override
    public void startFolderPickerActivity(
            Context context, Profile profile, BookmarkId... bookmarkIds) {
        Intent intent = new Intent(context, BookmarkFolderPickerActivity.class);
        intent.putStringArrayListExtra(
                BookmarkFolderPickerActivity.INTENT_BOOKMARK_IDS,
                BookmarkUtils.bookmarkIdsToStringList(bookmarkIds));
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        context.startActivity(intent);
    }

    @Override
    public void finishActivityOnPhone(Context context) {
        if (context instanceof BookmarkActivity bookmarkActivity) {
            bookmarkActivity.finish();
        }
    }

    @Override
    public String getLastUsedUrl(Profile profile) {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(profile);
        return ChromeSharedPreferences.getInstance()
                .readString(
                        ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL,
                        resolver.getBookmarksPageUrl());
    }

    private void showBookmarkManagerOnPhone(Activity activity, String url, Profile profile) {
        Intent intent =
                new Intent(
                        activity == null ? ContextUtils.getApplicationContext() : activity,
                        BookmarkActivity.class);
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        intent.setData(Uri.parse(url));
        if (activity != null) {
            // Start from an existing activity.
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            activity.startActivity(intent);
        } else {
            // Start a new task.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(intent);
        }
    }

    private void showBookmarkManagerOnTablet(@Nullable Tab tab, String url, Profile profile) {
        // History shows up as a tab on tablets.
        LoadUrlParams params = new LoadUrlParams(url);
        ChromeAsyncTabLauncher delegate =
                new ChromeAsyncTabLauncher(/* incognito= */ profile.isOffTheRecord());
        delegate.launchNewTab(params, TabLaunchType.FROM_CHROME_UI, /* parent= */ tab);
    }

    // Returns the first URL to load.
    private String getFirstUrlToLoad(@Nullable BookmarkId folderId, Profile profile) {
        String url;
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(profile);
        if (isBookmarksPageOverridden(profile.isIncognitoBranded())) {
            url = resolver.getBookmarksPageUrl();
        } else if (folderId == null) {
            // Load most recently visited bookmark folder.
            url = getLastUsedUrl(profile);
        } else {
            // Load a specific folder.
            url = BookmarkUiState.createFolderUrl(folderId).toString();
        }

        return TextUtils.isEmpty(url) ? resolver.getNtpUrl() : url;
    }

    private Intent getEditActivityIntent(Context context, Profile profile, BookmarkId bookmarkId) {
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        return intent;
    }
}
