// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Bundle;
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

    private static final String STATE_CURRENT_FOLDER_ID =
            "BookmarkFolderPickerActivity.CurrentFolderId";

    private @Nullable BookmarkFolderPickerCoordinator mCoordinator;
    private @Nullable BookmarkUiPrefs mBookmarkUiPrefs;
    private boolean mOutcomeRecorded;

    /** The folder that was shown before recreation, if any. */
    private @Nullable BookmarkId mRestoredFolderId;

    /** Whether this instance uses the desktop dialog rather than the full-screen presentation. */
    private boolean mIsDesktopDialog;

    /**
     * Whether the display was tablet-width when this instance chose its presentation. With the
     * desktop dialog, a fold/unfold that changes this recreates the activity to switch
     * presentation.
     */
    private boolean mIsTabletDisplay;

    @Override
    @Initializer
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        BookmarkUtils.makeTranslucentIfFloating(this);
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
                            if (mIsDesktopDialog) {
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
                        isFromBookmarkDialog,
                        mIsDesktopDialog,
                        mRestoredFolderId);

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

        if (mIsDesktopDialog) {
            setFinishOnTouchOutside(true);
            // When launched from another bookmark dialog, the parent dialog's window remains
            // visible underneath and keeps owning the dim. Requesting a second dim here would make
            // WindowManager move the dim layer to this window, which shows up as a scrim flash.
            // The floating dialog theme enables the dim by default, so it must be cleared
            // explicitly.
            if (isFromBookmarkDialog) {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            } else {
                // FLAG_DIM_BEHIND is already set by the floating dialog theme; only the dim amount
                // needs to match the modal dialog scrim.
                int scrimColor = ContextCompat.getColor(this, R.color.modal_dialog_scrim_color_lff);
                getWindow().setDimAmount(Color.alpha(scrimColor) / 255.0f);
            }
        } else {
            Toolbar toolbar = assumeNonNull(mCoordinator.getToolbar());
            setSupportActionBar(toolbar);
            assumeNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);
        }
        setContentView(mCoordinator.getView());

        if (mIsDesktopDialog) {
            getWindow()
                    .setLayout(
                            WindowManager.LayoutParams.WRAP_CONTENT,
                            WindowManager.LayoutParams.WRAP_CONTENT);
            View closeButton = findViewById(R.id.close_button);
            if (closeButton != null) {
                closeButton.setOnClickListener(
                        v -> {
                            recordOutcome(BookmarkFolderPickerOutcome.CLOSED);
                            setResult(RESULT_DISMISS_ALL);
                            finish();
                            overridePendingTransition(0, 0);
                        });
            }
        }

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
        if (mIsDesktopDialog) {
            overridePendingTransition(0, 0);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (!mIsDesktopDialog) {
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
            if (mIsDesktopDialog) {
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
                && mIsDesktopDialog
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
        // Not a user outcome; skip while recreating (e.g. to switch presentation).
        if (!mOutcomeRecorded && !isChangingConfigurations()) {
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

    @Override
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {
        super.onCreateInternal(savedInstanceState);
        String folderId =
                savedInstanceState == null
                        ? null
                        : savedInstanceState.getString(STATE_CURRENT_FOLDER_ID);
        if (folderId != null) {
            mRestoredFolderId = BookmarkId.getBookmarkIdFromString(folderId);
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // Keep the open folder across recreation (e.g. to switch presentation). Before the model
        // has loaded there is no current folder yet, so carry the restored one forward.
        BookmarkId folderId = mCoordinator == null ? null : mCoordinator.getCurrentFolderId();
        if (folderId == null) folderId = mRestoredFolderId;
        if (folderId != null) {
            outState.putString(STATE_CURRENT_FOLDER_ID, folderId.toString());
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Size changes don't relaunch this activity, so with the desktop dialog recreate it when a
        // fold/unfold crosses the tablet width to switch between the dialog and full-screen
        // presentation. Reads the live display width since smallestScreenWidthDp may be pinned.
        if (!isFinishing()
                && BookmarkUtils.isDesktopBookmarksDialogEnabled()
                && mIsTabletDisplay != BookmarkUtils.isCurrentDisplayTabletWidth(this)) {
            recreate();
        }
    }

    @Override
    protected void applyThemeOverlays() {
        super.applyThemeOverlays();
        mIsTabletDisplay = BookmarkUtils.isCurrentDisplayTabletWidth(this);
        mIsDesktopDialog = BookmarkUtils.shouldShowDesktopBookmarkDialog(this);
        if (mIsDesktopDialog) {
            applySingleThemeOverlay(R.style.ThemeOverlay_Chromium_DialogWhenLarge_ContentWrapping);
        }
    }

    @Override
    protected boolean shouldDrawEdgeToEdgeOnCreate() {
        // Floating modal dialogs should not draw edge-to-edge.
        return super.shouldDrawEdgeToEdgeOnCreate() && !BookmarkUtils.isBookmarkDialog(this);
    }

    @Override
    protected boolean wrapContentWithEdgeToEdgeLayout() {
        // Floating modal dialogs should not be wrapped edge-to-edge.
        return super.wrapContentWithEdgeToEdgeLayout() && !BookmarkUtils.isBookmarkDialog(this);
    }
}
