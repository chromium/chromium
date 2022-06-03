// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;
import android.os.Build;
import android.view.View;

import org.chromium.chrome.browser.content_creation.internal.R;
import org.chromium.chrome.browser.content_creation.notes.fonts.GoogleFontService;
import org.chromium.chrome.browser.content_creation.notes.images.ImageService;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Responsible for notes main UI and its subcomponents.
 */
public class NoteCreationCoordinatorImpl implements NoteCreationCoordinator {
    private static final String PNG_MIME_TYPE = "image/PNG";

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mListModel;
    private final NoteCreationMediator mMediator;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mShareUrl;
    private final String mSelectedText;

    private long mCreationStartTime;

    public NoteCreationCoordinatorImpl(Activity activity, WindowAndroid windowAndroid,
            NoteService noteService, ChromeOptionShareCallback chromeOptionShareCallback,
            String shareUrl, String title, String selectedText) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mShareUrl = shareUrl;
        mSelectedText = selectedText;

        selectedText = addQuotes(selectedText);

        mListModel = new ModelList();

        ImageFetcher imageFetcher = ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.DISK_CACHE_ONLY, ProfileKey.getLastUsedRegularProfileKey());
        mMediator = new NoteCreationMediator(mListModel, new GoogleFontService(mActivity),
                noteService, new ImageService(imageFetcher));

        String urlDomain = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
                new GURL(mShareUrl));
    }

    @Override
    public void showDialog() {

    }

    /**
     * Initializes the top bar after the parent view is ready.
     * @param view A {@link View} to corresponding to the parent view for the top bar.
     */
    private void onViewCreated(View view) {

    }

    /**
     * Returns the localized temporary filename's prefix. Random numbers will be
     * appended to it when the file creation happens.
     */
    private String getNoteFilenamePrefix() {
        return mActivity.getString(R.string.content_creation_note_filename_prefix);
    }

    /**
     * Creates the share sheet title based on a localized title format and the
     * current date formatted for the user's preferred locale.
     */
    private String getShareSheetTitle() {
        Date now = new Date(System.currentTimeMillis());
        String currentDateString =
                DateFormat.getDateInstance(DateFormat.SHORT, getPreferredLocale()).format(now);
        return mActivity.getString(
                R.string.content_creation_note_title_for_share, currentDateString);
    }

    /**
     * Retrieves the user's preferred locale from the app's configurations.
     */
    private Locale getPreferredLocale() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                ? mActivity.getResources().getConfiguration().getLocales().get(0)
                : mActivity.getResources().getConfiguration().locale;
    }

    private String addQuotes(String text) {
        // Split localized strings as prefix and suffix instead of full format
        // in case they are needed as split later.
        return new StringBuilder()
                .append(mActivity.getString(R.string.quotation_mark_prefix))
                .append(text)
                .append(mActivity.getString(R.string.quotation_mark_suffix))
                .toString();
    }

    /**
     * Starts the sharing flow for the newly published note.
     * @param noteUrl The url where the new note can be accessed.
     */
    private void resolvePublishedNote(String noteUrl) {

    }

    /**
     * Returns the time elapsed since the creation was started.
     */
    private long getTimeElapsedSinceCreationStart() {
        return System.currentTimeMillis() - mCreationStartTime;
    }
}
