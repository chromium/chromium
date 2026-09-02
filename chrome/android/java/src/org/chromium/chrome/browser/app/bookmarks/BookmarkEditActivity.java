// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.graphics.Color;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkEditMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkEditMetrics.BookmarkEditOutcome;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkTextInputLayout;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkViewUtils;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRow;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowCoordinator;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowViewBinder;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.UiUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.util.Objects;

/** The activity that enables the user to modify the title, url and parent folder of a bookmark. */
// TODO(crbug.com/40269559): Separate the activity from its view.
// TODO(crbug.com/40269559): Add a coordinator/mediator for business logic.
@NullMarked
public class BookmarkEditActivity extends SnackbarActivity {
    /** The intent extra specifying the ID of the bookmark to be edited. */
    public static final String INTENT_BOOKMARK_ID = "BookmarkEditActivity.BookmarkId";

    public static final int FOLDER_PICKER_REQUEST_CODE = 101;

    private BookmarkModel mModel;
    private Profile mProfile;
    private BookmarkManagerOpener mBookmarkManagerOpener;
    private BookmarkId mBookmarkId;
    private PropertyModel mFolderSelectRowModel;

    private ImprovedBookmarkRowCoordinator mFolderSelectRowCoordinator;
    private ImprovedBookmarkRow mFolderSelectRow;
    private BookmarkTextInputLayout mTitleEditText;
    private BookmarkTextInputLayout mUrlEditText;
    private @Nullable MenuItem mDeleteButton;
    private @Nullable MenuItem mCloseButton;
    private FrameLayout mFolderPickerRowContainer;

    private @Nullable String mInitialTitle;
    private @Nullable String mInitialUrl;
    private @Nullable BookmarkId mInitialParentId;
    private boolean mIsFolder;
    private boolean mOutcomeRecorded;

    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;
    private @Nullable BookmarkUiPrefs mBookmarkUiPrefs;

    private final BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver =
            new BookmarkUiPrefs.Observer() {
                @Override
                public void onBookmarkRowDisplayPrefChanged(
                        @BookmarkRowDisplayPref int displayPref) {
                    updateFolderPickerRow(displayPref);
                }
            };

    private final BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {
                    if (mModel.doesBookmarkExist(mBookmarkId)) {
                        updateViewContent(true);
                    } else {
                        finish();
                    }
                }
            };

    @Override
    @Initializer
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        mProfile = profile;
        mModel = BookmarkModel.getForProfile(profile);
        mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();
        mBookmarkId =
                BookmarkId.getBookmarkIdFromString(getIntent().getStringExtra(INTENT_BOOKMARK_ID));
        mModel.addObserver(mBookmarkModelObserver);
        BookmarkItem item = mModel.getBookmarkById(mBookmarkId);
        if (!mModel.doesBookmarkExist(mBookmarkId) || item == null) {
            finish();
            return;
        }

        mInitialTitle = item.getTitle();
        mInitialUrl = item.getUrl().getSpec();
        mInitialParentId = item.getParentId();
        mIsFolder = item.isFolder();

        boolean isDesktopDialog = BookmarkUtils.isDesktopBookmarksDialogEnabled();
        int layoutId = isDesktopDialog ? R.layout.bookmark_edit_desktop : R.layout.bookmark_edit;
        setContentView(layoutId);
        mTitleEditText = findViewById(R.id.title_text);
        mUrlEditText = findViewById(R.id.url_text);

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        assumeNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(!isDesktopDialog);

        View shadow = findViewById(R.id.shadow);
        View scrollView = findViewById(R.id.scroll_view);
        scrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () ->
                                shadow.setVisibility(
                                        scrollView.getScrollY() > 0 ? View.VISIBLE : View.GONE));

        boolean isFolder = item.isFolder();
        TextView folderTitle = findViewById(R.id.folder_title);
        folderTitle.setText(isFolder ? R.string.bookmark_parent_folder : R.string.bookmark_folder);
        mUrlEditText.setVisibility(isFolder ? View.GONE : View.VISIBLE);
        getSupportActionBar().setTitle(isFolder ? R.string.edit_folder : R.string.edit_bookmark);
        mBookmarkUiPrefs = new BookmarkUiPrefs(ChromeSharedPreferences.getInstance());
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);

        mFolderSelectRowCoordinator =
                new ImprovedBookmarkRowCoordinator(
                        this,
                        new BookmarkImageFetcher(
                                profile,
                                this,
                                mModel,
                                ImageFetcherFactory.createImageFetcher(
                                        ImageFetcherConfig.DISK_CACHE_ONLY,
                                        profile.getProfileKey()),
                                BookmarkViewUtils.getRoundedIconGenerator(
                                        this, BookmarkRowDisplayPref.VISUAL)),
                        mModel,
                        mBookmarkUiPrefs,
                        ShoppingServiceFactory.getForProfile(profile));

        mFolderPickerRowContainer = findViewById(R.id.folder_row_container);

        if (isDesktopDialog) {
            View removeButton = findViewById(R.id.remove_button);
            removeButton.setOnClickListener(
                    (v) -> {
                        recordOutcome(BookmarkEditOutcome.DELETED);
                        mModel.deleteBookmark(mBookmarkId);
                        finish();
                    });
            View saveButton = findViewById(R.id.save_button);
            saveButton.setOnClickListener(
                    (v) -> {
                        saveBookmark();
                        recordOutcome(BookmarkEditOutcome.SAVED);
                        finish();
                    });
        }

        mEdgeToEdgePadAdjuster =
                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                        scrollView, getEdgeToEdgeSupplier());
        updateViewContent(false);

        if (isDesktopDialog) {
            setFinishOnTouchOutside(true);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            int scrimColor = ContextCompat.getColor(this, R.color.modal_dialog_scrim_color_lff);
            getWindow().setDimAmount(Color.alpha(scrimColor) / 255.0f);
        }
    }

    /**
     * @param modelChanged Whether this view update is due to a model change in background.
     */
    @Initializer
    private void updateViewContent(boolean modelChanged) {
        BookmarkItem bookmarkItem = assumeNonNull(mModel.getBookmarkById(mBookmarkId));
        // While the user is editing the bookmark, do not override user's input.
        if (!modelChanged) {
            assumeNonNull(mTitleEditText.getEditText()).setText(bookmarkItem.getTitle());
            assumeNonNull(mUrlEditText.getEditText()).setText(bookmarkItem.getUrl().getSpec());
        }
        mTitleEditText.setEnabled(bookmarkItem.isEditable());
        mUrlEditText.setEnabled(bookmarkItem.isUrlEditable());
        updateFolderPickerRow(assumeNonNull(mBookmarkUiPrefs).getBookmarkRowDisplayPref());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
            mCloseButton =
                    menu.add(R.string.close)
                            .setIcon(
                                    UiUtils.getTintedDrawable(
                                            this,
                                            R.drawable.material_ic_close_24dp,
                                            R.color.default_icon_color_tint_list))
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        } else {
            mDeleteButton =
                    menu.add(R.string.bookmark_toolbar_delete)
                            .setIcon(
                                    UiUtils.getTintedDrawable(
                                            this,
                                            R.drawable.ic_delete_fill_24dp,
                                            R.color.default_icon_color_tint_list))
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item == mDeleteButton) {
            recordOutcome(BookmarkEditOutcome.DELETED);
            mModel.deleteBookmark(mBookmarkId);
            finish();
            return true;
        } else if (item == mCloseButton) {
            recordOutcome(BookmarkEditOutcome.CLOSED);
            finish();
            return true;
        } else if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void saveBookmark() {
        if (mModel.doesBookmarkExist(mBookmarkId)) {
            BookmarkItem bookmarkItem = assumeNonNull(mModel.getBookmarkById(mBookmarkId));
            final GURL originalUrl = bookmarkItem.getUrl();
            final String title = mTitleEditText.getTrimmedText();
            final String url = mUrlEditText.getTrimmedText();

            if (!mTitleEditText.isEmpty()) {
                mModel.setBookmarkTitle(mBookmarkId, title);
            }

            if (!mUrlEditText.isEmpty() && bookmarkItem.isUrlEditable()) {
                GURL fixedUrl = UrlFormatter.fixupUrl(url);
                if (fixedUrl.isValid() && !fixedUrl.equals(originalUrl)) {
                    mModel.setBookmarkUrl(mBookmarkId, fixedUrl);
                }
            }
        }
    }

    private void recordOutcome(@BookmarkEditOutcome int outcome) {
        if (mOutcomeRecorded) return;
        mOutcomeRecorded = true;
        BookmarkEditMetrics.recordOutcome(outcome, mIsFolder);
    }

    private boolean isBookmarkModified() {
        BookmarkItem item = mModel.getBookmarkById(mBookmarkId);
        if (item == null) return false;

        boolean titleChanged = !Objects.equals(mInitialTitle, mTitleEditText.getTrimmedText());
        boolean urlChanged =
                item.isUrlEditable() && !Objects.equals(mInitialUrl, mUrlEditText.getTrimmedText());
        boolean parentChanged = !Objects.equals(mInitialParentId, item.getParentId());

        return titleChanged || urlChanged || parentChanged;
    }

    @Override
    protected void onStop() {
        if (!BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
            if (isFinishing() && !mOutcomeRecorded) {
                recordOutcome(
                        isBookmarkModified()
                                ? BookmarkEditOutcome.SAVED
                                : BookmarkEditOutcome.DISMISSED);
            }
            saveBookmark();
        }

        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (!mOutcomeRecorded) {
            recordOutcome(
                    BookmarkUtils.isDesktopBookmarksDialogEnabled()
                            ? BookmarkEditOutcome.DISMISSED
                            : (isBookmarkModified()
                                    ? BookmarkEditOutcome.SAVED
                                    : BookmarkEditOutcome.DISMISSED));
        }
        mModel.removeObserver(mBookmarkModelObserver);
        if (mBookmarkUiPrefs != null) {
            mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);
            mBookmarkUiPrefs.destroy();
            mBookmarkUiPrefs = null;
        }
        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
        }
        super.onDestroy();
    }

    @VisibleForTesting
    BookmarkTextInputLayout getTitleEditText() {
        return mTitleEditText;
    }

    @VisibleForTesting
    BookmarkTextInputLayout getUrlEditText() {
        return mUrlEditText;
    }

    @VisibleForTesting
    @Nullable MenuItem getDeleteButton() {
        return mDeleteButton;
    }

    @VisibleForTesting
    @Nullable MenuItem getCloseButton() {
        return mCloseButton;
    }

    @VisibleForTesting
    @Nullable View getSaveButton() {
        return findViewById(R.id.save_button);
    }

    @VisibleForTesting
    @Nullable View getRemoveButton() {
        return findViewById(R.id.remove_button);
    }

    ScrollView getScrollViewForTesting() {
        return findViewById(R.id.scroll_view);
    }

    private void updateFolderPickerRow(@BookmarkRowDisplayPref int displayPref) {
        BookmarkItem bookmarkItem = assumeNonNull(mModel.getBookmarkById(mBookmarkId));
        mFolderSelectRowModel =
                mFolderSelectRowCoordinator.createBasePropertyModel(bookmarkItem.getParentId());

        mFolderSelectRowModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_RES, R.drawable.outline_chevron_right_24dp);
        mFolderSelectRowModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        mFolderSelectRowModel.set(
                ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER,
                () -> {
                    BookmarkEditMetrics.recordFolderPickerOpened();
                    setDialogContentVisible(false);
                    mBookmarkManagerOpener.startFolderPickerActivity(
                            /* context= */ this, mProfile, mBookmarkId);
                });

        mFolderSelectRow =
                ImprovedBookmarkRow.buildView(this, displayPref == BookmarkRowDisplayPref.VISUAL);
        PropertyModelChangeProcessor.create(
                mFolderSelectRowModel, mFolderSelectRow, ImprovedBookmarkRowViewBinder::bind);

        mFolderPickerRowContainer.removeAllViews();
        mFolderPickerRowContainer.addView(mFolderSelectRow);
    }

    @Override
    protected void onResume() {
        super.onResume();
        setDialogContentVisible(true);
    }

    /**
     * Toggles the visibility of the dialog content view on desktop.
     *
     * <p>On desktop, {@link BookmarkEditActivity} and {@link BookmarkFolderPickerActivity} are
     * layered modal dialog activities. When opening the folder picker as a child dialog, we hide
     * the edit dialog content so that if both are dismissed together (via {@code
     * RESULT_DISMISS_ALL}), the edit dialog finishes without a brief visual flash.
     */
    private void setDialogContentVisible(boolean visible) {
        if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
            findViewById(android.R.id.content)
                    .setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == FOLDER_PICKER_REQUEST_CODE
                && resultCode == BookmarkFolderPickerActivity.RESULT_DISMISS_ALL) {
            finish();
            if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
                overridePendingTransition(0, 0);
            }
        }
    }

    View getFolderSelectRowForTesting() {
        return mFolderSelectRow;
    }

    PropertyModel getFolderSelectRowPropertyModelForTesting() {
        return mFolderSelectRowModel;
    }
}
