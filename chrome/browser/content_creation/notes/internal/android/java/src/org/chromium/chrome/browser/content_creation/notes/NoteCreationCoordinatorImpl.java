// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;
import android.content.ComponentName;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.content_creation.internal.R;
import org.chromium.chrome.browser.content_creation.notes.fonts.GoogleFontService;
import org.chromium.chrome.browser.content_creation.notes.images.ImageService;
import org.chromium.chrome.browser.content_creation.notes.top_bar.TopBarCoordinator;
import org.chromium.chrome.browser.content_creation.notes.top_bar.TopBarDelegate;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
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

/** Responsible for notes main UI and its subcomponents. */
public class NoteCreationCoordinatorImpl implements NoteCreationCoordinator, TopBarDelegate {
    private static final String PNG_MIME_TYPE = "image/PNG";

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mListModel;
    private final NoteCreationMediator mMediator;
    private final NoteCreationDialog mDialog;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mShareUrl;
    private final String mSelectedText;

    private long mCreationStartTime;

    private TopBarCoordinator mTopBarCoordinator;

    public NoteCreationCoordinatorImpl(
            Activity activity,
            WindowAndroid windowAndroid,
            NoteService noteService,
            ChromeOptionShareCallback chromeOptionShareCallback,
            String shareUrl,
            String title,
            String selectedText) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mShareUrl = shareUrl;
        mSelectedText = selectedText;

        selectedText = addQuotes(selectedText);

        mListModel = new ModelList();

        ImageFetcher imageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.DISK_CACHE_ONLY,
                        ProfileKey.getLastUsedRegularProfileKey());
        mMediator =
                new NoteCreationMediator(
                        mListModel,
                        new GoogleFontService(mActivity),
                        noteService,
                        new ImageService(imageFetcher));

        String urlDomain =
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
                        new GURL(mShareUrl));
        mDialog = new NoteCreationDialog();
        mDialog.initDialog(
                this::onViewCreated, urlDomain, title, selectedText, this::executeAction);
    }

    @Override
    public void showDialog() {
        mCreationStartTime = System.currentTimeMillis();
        NoteCreationMetrics.recordNoteCreationSelected();

        FragmentActivity fragmentActivity = (FragmentActivity) mActivity;
        mDialog.show(fragmentActivity.getSupportFragmentManager(), null);
    }

    /** Dismiss the main dialog from top bar. */
    @Override
    public void dismiss() {
        NoteCreationMetrics.recordNoteCreationDismissed(
                getTimeElapsedSinceCreationStart(), mDialog.getNbTemplateSwitches());
        mDialog.dismiss();
    }

    /** Share the currently selected note. */
    @Override
    public void executeAction() {
        // Top bar may be loaded before notes.
        if (mListModel.size() == 0) return;
        int selectedNoteIndex = mDialog.getSelectedItemIndex();
        NoteCreationMetrics.recordNoteTemplateSelected(
                getTimeElapsedSinceCreationStart(),
                mDialog.getNbTemplateSwitches(),
                mListModel.get(selectedNoteIndex).model.get(NoteProperties.TEMPLATE).id,
                selectedNoteIndex);

        View noteView = mDialog.getNoteViewAt(selectedNoteIndex);

        assert noteView != null;

        Bitmap bitmap =
                Bitmap.createBitmap(
                        noteView.getWidth(), noteView.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        noteView.draw(canvas);

        ShareImageFileUtils.generateTemporaryUriFromBitmap(
                getNoteFilenamePrefix(),
                bitmap,
                (imageUri) -> {
                    final String sheetTitle = getShareSheetTitle();
                    var callback =
                            new ShareParams.TargetChosenCallback() {
                                @Override
                                public void onTargetChosen(ComponentName chosenComponent) {
                                    NoteCreationMetrics.recordNoteShared(
                                            getTimeElapsedSinceCreationStart(), chosenComponent);
                                }

                                @Override
                                public void onCancel() {
                                    NoteCreationMetrics.recordNoteNotShared(
                                            getTimeElapsedSinceCreationStart());
                                }
                            };
                    ShareParams params =
                            new ShareParams.Builder(mWindowAndroid, sheetTitle, mShareUrl)
                                    .setSingleImageUri(imageUri)
                                    .setImageAltText(mSelectedText)
                                    .setFileContentType(PNG_MIME_TYPE)
                                    .setCallback(callback)
                                    .build();

                    long shareStartTime = System.currentTimeMillis();
                    ChromeShareExtras extras =
                            new ChromeShareExtras.Builder()
                                    .setSkipPageSharingActions(true)
                                    .setContentUrl(new GURL(mShareUrl))
                                    .setDetailedContentType(DetailedContentType.WEB_NOTES)
                                    .build();

                    // Dismiss current dialog before showing the share sheet.
                    mDialog.dismiss();
                    mChromeOptionShareCallback.showShareSheet(params, extras, shareStartTime);
                });
    }

    /**
     * Initializes the top bar after the parent view is ready.
     * @param view A {@link View} to corresponding to the parent view for the top bar.
     */
    private void onViewCreated(View view) {
        mTopBarCoordinator = new TopBarCoordinator(mActivity, view, this);
        mDialog.createRecyclerViews(mListModel);
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

    /** Retrieves the user's preferred locale from the app's configurations. */
    private Locale getPreferredLocale() {
        return mActivity.getResources().getConfiguration().getLocales().get(0);
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

    /** Returns the time elapsed since the creation was started. */
    private long getTimeElapsedSinceCreationStart() {
        return System.currentTimeMillis() - mCreationStartTime;
    }
}
