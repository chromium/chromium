// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.content_creation.notes.fonts.GoogleFontService;
import org.chromium.chrome.browser.content_creation.notes.top_bar.TopBarCoordinator;
import org.chromium.chrome.browser.content_creation.notes.top_bar.TopBarDelegate;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Responsible for notes main UI and its subcomponents.
 */
public class NoteCreationCoordinatorImpl implements NoteCreationCoordinator, TopBarDelegate {
    private static final String NOTE_FILENAME_PREFIX = "Note";
    private static final String PNG_MIME_TYPE = "image/PNG";

    private final Activity mActivity;
    private final Tab mTab;
    private final ModelList mListModel;
    private final NoteCreationMediator mMediator;
    private final NoteCreationDialog mDialog;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mShareUrl;

    private TopBarCoordinator mTopBarCoordinator;

    public NoteCreationCoordinatorImpl(Activity activity, Tab tab, NoteService noteService,
            ChromeOptionShareCallback chromeOptionShareCallback, String shareUrl,
            String selectedText) {
        mActivity = activity;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mShareUrl = shareUrl;

        mListModel = new ModelList();

        mMediator =
                new NoteCreationMediator(mListModel, new GoogleFontService(mActivity), noteService);

        mDialog = new NoteCreationDialog();
        mDialog.initDialog(this::onViewCreated, selectedText);
    }

    @Override
    public void showDialog() {
        FragmentActivity fragmentActivity = (FragmentActivity) mActivity;
        mDialog.show(fragmentActivity.getSupportFragmentManager(), null);
    }

    /**
     * Dismiss the main dialog.
     */
    @Override
    public void dismiss() {
        mDialog.dismiss();
    }

    /**
     * Share the currently selected note.
     */
    @Override
    public void executeAction() {
        View noteView = mDialog.getNoteViewAt(mDialog.getSelectedItemIndex());

        assert noteView != null;

        Bitmap bitmap = Bitmap.createBitmap(
                noteView.getWidth(), noteView.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        noteView.draw(canvas);

        ShareImageFileUtils.generateTemporaryUriFromBitmap(
                NOTE_FILENAME_PREFIX, bitmap, (imageUri) -> {
                    // TODO(crbug.com/2912180): Change to localized title with
                    // date.
                    final String sheetTitle = "Stylized Note";
                    ShareParams params = new ShareParams
                                                 .Builder(mTab.getWindowAndroid(),
                                                         /*title=*/sheetTitle, mShareUrl)
                                                 .setFileUris(new ArrayList<>(
                                                         Collections.singletonList(imageUri)))
                                                 .setFileContentType(PNG_MIME_TYPE)
                                                 .build();

                    long shareStartTime = System.currentTimeMillis();
                    ChromeShareExtras extras = new ChromeShareExtras.Builder().build();

                    // Dismiss current dialog before showing the share sheet.
                    this.dismiss();
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
}