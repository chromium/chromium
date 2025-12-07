// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.Menu;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.bookmarks.BookmarkAddNewFolderCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderPickerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkViewUtils;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowCoordinator;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;
import java.util.List;

/**
 * The activity that enables the user to pick the parent folder for the given {@link BookmarkId}.
 * Used for the improved android bookmarks manager.
 */
@NullMarked
public class BookmarkFolderPickerActivity extends SynchronousInitializationActivity {
    /** The intent extra specifying the ID of the bookmark to be moved. */
    public static final String INTENT_BOOKMARK_IDS = "BookmarkFolderPickerActivity.BookmarkIds";

    private @Nullable BookmarkFolderPickerCoordinator mCoordinator;

    @Override
    @Initializer
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);

        List<String> bookmarkIdsAsStrings =
                IntentUtils.safeGetStringArrayListExtra(getIntent(), INTENT_BOOKMARK_IDS);
        bookmarkIdsAsStrings =
                bookmarkIdsAsStrings == null ? new ArrayList<>() : bookmarkIdsAsStrings;
        List<BookmarkId> bookmarkIds =
                BookmarkUtils.stringListToBookmarkIds(bookmarkModel, bookmarkIdsAsStrings);
        if (bookmarkIds.isEmpty()) {
            finish();
            return;
        }

        BookmarkImageFetcher bookmarkImageFetcher =
                new BookmarkImageFetcher(
                        profile,
                        this,
                        bookmarkModel,
                        ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                profile.getProfileKey(),
                                GlobalDiscardableReferencePool.getReferencePool()),
                        BookmarkViewUtils.getRoundedIconGenerator(
                                this, BookmarkRowDisplayPref.VISUAL));
        BookmarkAddNewFolderCoordinator addNewFolderCoordinator =
                new BookmarkAddNewFolderCoordinator(
                        this,
                        new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP),
                        bookmarkModel);
        BookmarkUiPrefs bookmarkUiPrefs =
                new BookmarkUiPrefs(ChromeSharedPreferences.getInstance());
        ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(profile);
        // TODO(crbug.com/40278746): Consider initializing this in #onCreateOptionsMenu to avoid the
        // possibility that the menu is null when the first parent is set.
        mCoordinator =
                new BookmarkFolderPickerCoordinator(
                        this,
                        bookmarkModel,
                        bookmarkIds,
                        this::finish,
                        addNewFolderCoordinator,
                        bookmarkUiPrefs,
                        new ImprovedBookmarkRowCoordinator(
                                this,
                                bookmarkImageFetcher,
                                bookmarkModel,
                                bookmarkUiPrefs,
                                shoppingService),
                        shoppingService);

        BackPressHelper.create(this, getOnBackPressedDispatcher(), mCoordinator);

        Toolbar toolbar = mCoordinator.getToolbar();
        setSupportActionBar(toolbar);
        assumeNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);
        setContentView(mCoordinator.getView());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.bookmark_folder_picker_menu, menu);
        assumeNonNull(mCoordinator).updateToolbarButtons();
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (assumeNonNull(mCoordinator).optionsItemSelected(item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        super.onDestroy();
    }
}
