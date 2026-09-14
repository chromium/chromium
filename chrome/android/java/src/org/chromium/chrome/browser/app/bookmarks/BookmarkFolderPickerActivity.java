// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Color;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.ContextCompat;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.bookmarks.BookmarkAddNewFolderCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderPickerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderPickerMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderPickerMetrics.BookmarkFolderPickerOutcome;
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

    /**
     * The intent extra specifying if the folder picker was launched from another bookmark dialog.
     */
    public static final String INTENT_IS_FROM_BOOKMARK_DIALOG =
            "BookmarkFolderPickerActivity.IsFromBookmarkDialog";

    /** Result code sent when the picker wishes to dismiss all bookmark dialogs in the backstack. */
    public static final int RESULT_DISMISS_ALL = RESULT_FIRST_USER + 1;

    private @Nullable BookmarkFolderPickerCoordinator mCoordinator;
    private @Nullable BookmarkUiPrefs mBookmarkUiPrefs;
    private boolean mOutcomeRecorded;

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
        mBookmarkUiPrefs = new BookmarkUiPrefs(ChromeSharedPreferences.getInstance());
        ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(profile);
        boolean isFromBookmarkDialog =
                IntentUtils.safeGetBooleanExtra(getIntent(), INTENT_IS_FROM_BOOKMARK_DIALOG, false);
        // TODO(crbug.com/40278746): Consider initializing this in #onCreateOptionsMenu to avoid the
        // possibility that the menu is null when the first parent is set.
        mCoordinator =
                new BookmarkFolderPickerCoordinator(
                        this,
                        bookmarkModel,
                        bookmarkIds,
                        () -> {
                            recordOutcome(BookmarkFolderPickerOutcome.MOVED);
                            setResult(RESULT_OK);
                            finish();
                            if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
                                overridePendingTransition(0, 0);
                            }
                        },
                        addNewFolderCoordinator,
                        mBookmarkUiPrefs,
                        new ImprovedBookmarkRowCoordinator(
                                this,
                                bookmarkImageFetcher,
                                bookmarkModel,
                                mBookmarkUiPrefs,
                                shoppingService),
                        shoppingService,
                        isFromBookmarkDialog);

        getOnBackPressedDispatcher()
                .addCallback(
                        this,
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                onBackPressFromRoot();
                            }
                        });
        BackPressHelper.create(this, getOnBackPressedDispatcher(), mCoordinator);

        Toolbar toolbar = mCoordinator.getToolbar();
        if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
            toolbar.inflateMenu(R.menu.bookmark_folder_picker_menu_desktop);
            toolbar.setOnMenuItemClickListener(this::onOptionsItemSelected);
            setFinishOnTouchOutside(true);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            int scrimColor = ContextCompat.getColor(this, R.color.modal_dialog_scrim_color_lff);
            getWindow().setDimAmount(Color.alpha(scrimColor) / 255.0f);
        } else {
            setSupportActionBar(toolbar);
            assumeNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);
        }
        setContentView(mCoordinator.getView());

        View cancelButton = findViewById(R.id.cancel_button);
        if (cancelButton != null) {
            cancelButton.setOnClickListener(
                    v -> {
                        recordOutcome(BookmarkFolderPickerOutcome.CLOSED);
                        finish();
                    });
        }
    }

    private void recordOutcome(@BookmarkFolderPickerOutcome int outcome) {
        if (mOutcomeRecorded) return;
        mOutcomeRecorded = true;
        BookmarkFolderPickerMetrics.recordOutcome(outcome);
    }

    void onBackPressFromRoot() {
        recordOutcome(BookmarkFolderPickerOutcome.DISMISSED);
        finish();
        if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
            overridePendingTransition(0, 0);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (!BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
            getMenuInflater().inflate(R.menu.bookmark_folder_picker_menu, menu);
            assumeNonNull(mCoordinator).updateToolbarButtons();
            return super.onCreateOptionsMenu(menu);
        }
        return false;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id) {
            recordOutcome(BookmarkFolderPickerOutcome.CLOSED);
            setResult(RESULT_DISMISS_ALL);
            finish();
            if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
                overridePendingTransition(0, 0);
            }
            return true;
        } else if (item.getItemId() == android.R.id.home) {
            if (mCoordinator == null || !mCoordinator.onBackPressed()) {
                onBackPressFromRoot();
            }
            return true;
        }
        if (assumeNonNull(mCoordinator).optionsItemSelected(item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN
                && BookmarkUtils.isDesktopBookmarksDialogEnabled()
                && isOutOfBounds(event)) {
            recordOutcome(BookmarkFolderPickerOutcome.DISMISSED);
            setResult(RESULT_DISMISS_ALL);
            finish();
            overridePendingTransition(0, 0);
            return true;
        }
        return super.onTouchEvent(event);
    }

    private boolean isOutOfBounds(MotionEvent event) {
        int x = (int) event.getX();
        int y = (int) event.getY();
        int slop = ViewConfiguration.get(this).getScaledWindowTouchSlop();
        View decorView = getWindow().getDecorView();
        return x < -slop
                || y < -slop
                || x > (decorView.getWidth() + slop)
                || y > (decorView.getHeight() + slop);
    }

    @Override
    protected void onDestroy() {
        if (!mOutcomeRecorded) {
            recordOutcome(BookmarkFolderPickerOutcome.DISMISSED);
        }

        if (mCoordinator != null) {
            mCoordinator.destroy();
        }

        if (mBookmarkUiPrefs != null) {
            mBookmarkUiPrefs.destroy();
            mBookmarkUiPrefs = null;
        }

        super.onDestroy();
    }
}
