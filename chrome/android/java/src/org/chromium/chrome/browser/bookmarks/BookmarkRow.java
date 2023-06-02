// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.browser_ui.widget.selectable_list.CheckableSelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Common logic for bookmark and folder rows.
 */
public abstract class BookmarkRow
        extends CheckableSelectableItemView<BookmarkId> implements BookmarkUiObserver {
    // The start icon view which is shows the favicon and the checkmark.
    protected ImageView mStartIconView;
    // 3-dot menu which displays contextual actions.
    protected ListMenuButton mMoreButton;
    // Image shown in selection mode which starts drag/drop.
    protected ImageView mDragHandle;
    // Displays the title of the bookmark.
    protected TextView mTitleView;
    // Displays the url of the bookmark.
    protected TextView mDescriptionView;

    protected BookmarkDelegate mDelegate;
    protected BookmarkId mBookmarkId;

    /** Levels for the background. */
    private final int mDefaultLevel;
    private final int mSelectedLevel;
    private boolean mIsAttachedToWindow;
    private PopupMenuShownListener mPopupListener;
    private @Location int mLocation;
    private boolean mFromFilterView;

    @IntDef({Location.TOP, Location.MIDDLE, Location.BOTTOM, Location.SOLO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Location {
        int TOP = 0;
        int MIDDLE = 1;
        int BOTTOM = 2;
        int SOLO = 3;
    }

    /**
     * Factory constructor for building the view programmatically.
     * @param row The BookmarkRow to build.
     * @param context The calling context, usually the parent view.
     * @param isVisualRefreshEnabled Whether to show the visual or compact bookmark row.
     */
    protected static void buildView(
            BookmarkRow row, Context context, boolean isVisualRefreshEnabled) {
        row.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        LayoutInflater.from(context).inflate(isVisualRefreshEnabled
                        ? org.chromium.chrome.R.layout.bookmark_row_layout_visual
                        : org.chromium.chrome.R.layout.bookmark_row_layout,
                row);
        row.onFinishInflate();
    }

    /** Constructor for inflating from XML. */
    public BookmarkRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        mDefaultLevel = getResources().getInteger(R.integer.list_item_level_default);
        mSelectedLevel = getResources().getInteger(R.integer.list_item_level_selected);
    }

    // FrameLayout implementation.

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mStartIconView = findViewById(R.id.start_icon);

        mDragHandle = findViewById(R.id.drag_handle);

        mMoreButton = findViewById(R.id.more);
        mMoreButton.setDelegate(getListMenuButtonDelegate());
        mMoreButton.setVisibility(View.GONE);

        mTitleView = findViewById(R.id.title);
        mDescriptionView = findViewById(R.id.description);
    }

    /**
     * Sets the bookmark ID for this BookmarkRow and provides information about its location
     * within the list of bookmarks.
     *
     * @param bookmarkId The BookmarkId that this BookmarkRow now contains.
     * @param location   The location of this BookmarkRow.
     * @param fromFilterView The Bookmark is being displayed in a filter view, determines if the row
     *         is selectable.
     * @return The BookmarkItem corresponding to BookmarkId.
     */
    BookmarkItem setBookmarkId(
            BookmarkId bookmarkId, @Location int location, boolean fromFilterView) {
        mLocation = location;
        mBookmarkId = bookmarkId;
        mFromFilterView = fromFilterView;
        BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmarkId);
        mMoreButton.dismiss();
        SelectableListUtils.setContentDescriptionContext(getContext(), mMoreButton,
                bookmarkItem.getTitle(), SelectableListUtils.ContentDescriptionSource.MENU_BUTTON);

        setChecked(isItemSelected());
        updateVisualState();

        super.setItem(bookmarkId);
        return bookmarkItem;
    }

    private void updateVisualState() {
        // This check is needed because it is possible for updateVisualState to be called between
        // onDelegateInitialized (SelectionDelegate triggers a redraw) and setBookmarkId. View is
        // not currently bound, so we can skip this for now. updateVisualState will run inside of
        // setBookmarkId.
        if (mBookmarkId == null) {
            return;
        }
        BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
        // This check is needed because updateVisualState is called when the item has been deleted
        // in the model but not in the adapter. If we hit this if-block, the
        // item is about to be deleted, and we don't need to do anything.
        if (bookmarkItem == null) {
            return;
        }
        // TODO(jhimawan): Look into using cleanup(). Perhaps unhook the selection state observer?

        // If the visibility of the drag handle or more icon is not set later, it will be gone.
        mDragHandle.setVisibility(GONE);
        mMoreButton.setVisibility(GONE);

        if (mDelegate.getDragStateDelegate().getDragActive()) {
            mDragHandle.setVisibility(
                    bookmarkItem.isReorderable() && !mFromFilterView ? VISIBLE : GONE);
            mDragHandle.setEnabled(isItemSelected());
        } else {
            mMoreButton.setVisibility(bookmarkItem.isEditable() ? VISIBLE : GONE);
            mMoreButton.setClickable(!isSelectionModeActive());
            mMoreButton.setEnabled(mMoreButton.isClickable());
            mMoreButton.setImportantForAccessibility(mMoreButton.isClickable()
                            ? IMPORTANT_FOR_ACCESSIBILITY_YES
                            : IMPORTANT_FOR_ACCESSIBILITY_NO);
        }
    }

    /**
     * Sets the delegate to use to handle UI actions related to this view.
     *
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     */
    public void onDelegateInitialized(BookmarkDelegate delegate) {
        super.setSelectionDelegate(delegate.getSelectionDelegate());
        mDelegate = delegate;
        if (mIsAttachedToWindow) initialize();
    }

    private void initialize() {
        mDelegate.addUiObserver(this);
        mPopupListener = () -> mDelegate.onBookmarkItemMenuOpened();
        mMoreButton.addPopupListener(mPopupListener);
    }

    private void cleanup() {
        mMoreButton.dismiss();
        mMoreButton.removePopupListener(mPopupListener);
        if (mDelegate != null) mDelegate.removeUiObserver(this);
    }

    private ModelList getItems() {
        // Rebuild listItems, cause mLocation may be changed anytime.
        boolean canReorder = false;
        boolean canMove = false;
        BookmarkItem bookmarkItem = null;
        if (mDelegate != null && mDelegate.getModel() != null) {
            bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
            if (bookmarkItem != null) {
                // Reading list items can sometimes be movable (for type swapping purposes), but for
                // UI purposes they shouldn't be movable.
                canMove = BookmarkUtils.isMovable(bookmarkItem);
                canReorder = bookmarkItem.isReorderable() && !mFromFilterView;
            }
        }
        ModelList listItems = new ModelList();
        if (mBookmarkId.getType() == BookmarkType.READING_LIST) {
            if (bookmarkItem != null) {
                listItems.add(buildMenuListItem(bookmarkItem.isRead()
                                ? R.string.reading_list_mark_as_unread
                                : R.string.reading_list_mark_as_read,
                        0, 0));
            }
            listItems.add(buildMenuListItem(R.string.bookmark_item_select, 0, 0));
            listItems.add(buildMenuListItem(R.string.bookmark_item_delete, 0, 0));
            listItems.add(buildMenuListItem(R.string.bookmark_item_edit, 0, 0));
            listItems.add(buildMenuListItem(R.string.bookmark_item_move, 0, 0));
        } else {
            listItems.add(buildMenuListItem(R.string.bookmark_item_select, 0, 0));
            listItems.add(buildMenuListItem(R.string.bookmark_item_edit, 0, 0));
            listItems.add(buildMenuListItem(R.string.bookmark_item_move, 0, 0, canMove));
            listItems.add(buildMenuListItem(R.string.bookmark_item_delete, 0, 0));
        }

        if (mDelegate.getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
            listItems.add(buildMenuListItem(R.string.bookmark_show_in_folder, 0, 0));
        } else if (mDelegate.getCurrentUiMode() == BookmarkUiMode.FOLDER
                && mLocation != Location.SOLO && canReorder) {
            // Only add move up / move down buttons if there is more than 1 item
            if (mLocation != Location.TOP) {
                listItems.add(buildMenuListItem(R.string.menu_item_move_up, 0, 0));
            }
            if (mLocation != Location.BOTTOM) {
                listItems.add(buildMenuListItem(R.string.menu_item_move_down, 0, 0));
            }
        }

        return listItems;
    }

    private ListMenu getListMenu() {
        ModelList listItems = getItems();
        ListMenu.Delegate delegate = item -> {
            int textId = item.get(ListMenuItemProperties.TITLE_ID);
            if (textId == R.string.bookmark_item_select) {
                setChecked(mDelegate.getSelectionDelegate().toggleSelectionForItem(mBookmarkId));
                RecordUserAction.record("Android.BookmarkPage.SelectFromMenu");
                if (mBookmarkId.getType() == BookmarkType.READING_LIST) {
                    RecordUserAction.record("Android.BookmarkPage.ReadingList.SelectFromMenu");
                }
            } else if (textId == R.string.bookmark_item_edit) {
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
                if (bookmarkItem.isFolder()) {
                    BookmarkAddEditFolderActivity.startEditFolderActivity(
                            getContext(), bookmarkItem.getId());
                } else {
                    BookmarkUtils.startEditActivity(getContext(), bookmarkItem.getId());
                }
            } else if (textId == R.string.reading_list_mark_as_read) {
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
                mDelegate.getModel().setReadStatusForReadingList(
                        bookmarkItem.getUrl(), /*read=*/true);
                RecordUserAction.record("Android.BookmarkPage.ReadingList.MarkAsRead");
            } else if (textId == R.string.reading_list_mark_as_unread) {
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
                mDelegate.getModel().setReadStatusForReadingList(
                        bookmarkItem.getUrl(), /*read=*/false);
                RecordUserAction.record("Android.BookmarkPage.ReadingList.MarkAsUnread");
            } else if (textId == R.string.bookmark_item_move) {
                BookmarkFolderSelectActivity.startFolderSelectActivity(getContext(), mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveToFolder");
            } else if (textId == R.string.bookmark_item_delete) {
                if (mDelegate != null && mDelegate.getModel() != null) {
                    mDelegate.getModel().deleteBookmarks(mBookmarkId);
                    RecordUserAction.record("Android.BookmarkPage.RemoveItem");
                    if (mBookmarkId.getType() == BookmarkType.READING_LIST) {
                        RecordUserAction.record("Android.BookmarkPage.ReadingList.RemoveItem");
                    }
                }
            } else if (textId == R.string.bookmark_show_in_folder) {
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
                mDelegate.openFolder(bookmarkItem.getParentId());
                mDelegate.highlightBookmark(mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerShowInFolder");
            } else if (textId == R.string.menu_item_move_up) {
                mDelegate.moveUpOne(mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveUp");
            } else if (textId == R.string.menu_item_move_down) {
                mDelegate.moveDownOne(mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveDown");
            };
        };
        return new BasicListMenu(getContext(), listItems, delegate);
    }

    // FrameLayout implementation.

    private ListMenuButtonDelegate getListMenuButtonDelegate() {
        return this::getListMenu;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mIsAttachedToWindow = true;
        if (mDelegate != null) {
            initialize();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mIsAttachedToWindow = false;
        cleanup();
    }

    // CheckableSelectableItemView implementation.

    @Override
    protected ImageView getIconView() {
        return mStartIconView;
    }

    @Override
    protected @Nullable ColorStateList getDefaultIconTint() {
        return null;
    }

    @Override
    protected int getSelectedLevel() {
        return mSelectedLevel;
    }

    @Override
    protected int getDefaultLevel() {
        return mDefaultLevel;
    }

    // BookmarkUiObserver implementation.

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        super.onSelectionStateChange(selectedBookmarks);
        updateVisualState();
    }

    @Override
    public void onDestroy() {
        cleanup();
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {}

    public boolean isItemSelected() {
        return mDelegate.getSelectionDelegate().isItemSelected(mBookmarkId);
    }

    void setDragHandleOnTouchListener(View.OnTouchListener l) {
        mDragHandle.setOnTouchListener(l);
    }

    public String getTitle() {
        return String.valueOf(mTitleView.getText());
    }

    private boolean isDragActive() {
        return mDelegate.getDragStateDelegate().getDragActive();
    }

    @Override
    public boolean onLongClick(View view) {
        // Override is needed in order to support long-press-to-drag on already-selected items.
        if (isDragActive() && isItemSelected()) return true;
        RecordUserAction.record("MobileBookmarkManagerLongPressToggleSelect");
        return super.onLongClick(view);
    }

    @Override
    public void onClick(View view) {
        // Override is needed in order to allow items to be selected / deselected with a click.
        // Since we override #onLongClick(), we cannot rely on the base class for this behavior.
        if (isDragActive()) {
            toggleSelectionForItem(getItem());
            RecordUserAction.record("MobileBookmarkManagerTapToggleSelect");
        } else {
            super.onClick(view);
        }
    }

    @VisibleForTesting
    public View getDragHandleViewForTesting() {
        return mDragHandle;
    }
}
