package com.ark.browser.ui.fragment.dialog;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.core.bookmark.BookmarkModel;
import com.ark.browser.settings.Keys;
import com.ark.browser.ui.widget.EmptyAlertEditText;
import com.zpj.fragmentation.SupportHelper;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.PrefsHelper;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

public class CollectionEditorDialog extends OverDragBottomDialogFragment<CollectionEditorDialog>
        implements View.OnClickListener {

    private static final String KEY_MOVE_TO_BOOKMARK_FOLDER = "move_to_bookmark_folder";

    private final BookmarkModel mModel = new BookmarkModel();


    private BookmarkId mBookmarkId;
    private Tab mTab;

    private String originalTitle;
    private GURL originalUrl;
    private String title;
    private String url;
    private boolean addToBookmark = false;
    private boolean addToHomepage = false;
    private boolean addToLauncher = false;
    private BookmarkId currentFolderId;
    private BookmarkId moveToFolderId;

    public static CollectionEditorDialog newInstance(BookmarkId bookmarkId) {
        CollectionEditorDialog fragment = new CollectionEditorDialog();
        if (bookmarkId != null) {
            Bundle args = new Bundle();
            args.putLong(Keys.KEY_ID, bookmarkId.getId());
            args.putInt(Keys.KEY_TYPE, bookmarkId.getType());
            fragment.setArguments(args);
        }
        return fragment;
    }

    public static CollectionEditorDialog newInstance(Tab page) {
        CollectionEditorDialog fragment = new CollectionEditorDialog();
        fragment.mTab = page;
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int type = BookmarkType.NORMAL;
        long id = BookmarkId.INVALID_ID;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                id = getArguments().getLong(Keys.KEY_ID, BookmarkId.INVALID_ID);
                type = getArguments().getInt(Keys.KEY_TYPE, BookmarkType.NORMAL);
            }
        } else {
            id = savedInstanceState.getLong(Keys.KEY_ID, BookmarkId.INVALID_ID);
            type = savedInstanceState.getInt(Keys.KEY_TYPE, BookmarkType.NORMAL);
        }
        if (id != BookmarkId.INVALID_ID) {
            mBookmarkId = new BookmarkId(id, type);
        }
        
        if (mBookmarkId == null) {
            if (mTab == null) {
                popThis();
            } else {
                mBookmarkId = mModel.getUserBookmarkIdForTab(mTab);
            }
        }
        
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mBookmarkId != null) {
            outState.putLong(Keys.KEY_ID, mBookmarkId.getId());
            outState.putInt(Keys.KEY_TYPE, mBookmarkId.getType());
        }
    }

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        lightStatusBar();
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_collection_editor;
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        mModel.finishLoadingBookmarkModel(this::init);
        mModel.loadEmptyPartnerBookmarkShimForTesting();
    }

    private void init() {
        if (mBookmarkId == null) {
            mBookmarkId = mModel.getUserBookmarkIdForTab(mTab);
        }

        BookmarkItem bookmarkItem = mModel.getBookmarkById(mBookmarkId);
//        currentFolderId = bookmarkItem == null ? mModel.getDefaultFolder() : bookmarkItem.getParentId();


        if (bookmarkItem == null) {
            long id = PrefsHelper.with().getLong(KEY_MOVE_TO_BOOKMARK_FOLDER, BookmarkId.INVALID_ID);
            if (id == BookmarkId.INVALID_ID) {
                currentFolderId = mModel.getDefaultFolder();
            } else {
                BookmarkId bookmarkId = new BookmarkId(id, BookmarkType.NORMAL);
                if (mModel.doesBookmarkExist(bookmarkId)) {
                    currentFolderId = bookmarkId;
                } else {
                    currentFolderId = mModel.getDefaultFolder();
                }
            }
        } else {
            currentFolderId = bookmarkItem.getParentId();
        }

        if (bookmarkItem != null && mModel.doesBookmarkExist(mBookmarkId)) {
            addToBookmark = true;
            originalUrl = bookmarkItem.getUrl();
            originalTitle = bookmarkItem.getTitle();
        } else {
            originalUrl = mTab.getUrl();
            originalTitle = mTab.getTitle();
        }
        url = originalUrl.getSpec();
        title = originalTitle;

//        if (HomepageManager.getFavoriteByUrl(url) != null) {
//            addToHomepage = true;
//        }

        EmptyAlertEditText mTitleEditText = findViewById(R.id.title_text);
        mTitleEditText.setOnTouchListener((v, event) -> {
            mTitleEditText.setOnTouchListener(null);
            mTitleEditText.setCursorVisible(true);
            return false;
        });

        TextView mFolderTextView = findViewById(R.id.folder_text);
        EmptyAlertEditText mUrlEditText = findViewById(R.id.url_text);
        mUrlEditText.setOnTouchListener((v, event) -> {
            mUrlEditText.setOnTouchListener(null);
            mUrlEditText.setCursorVisible(true);
            return false;
        });

        mFolderTextView.setText(mModel.getBookmarkTitle(currentFolderId));
        mFolderTextView.setEnabled(bookmarkItem == null || bookmarkItem.isMovable());
        mTitleEditText.setEnabled(bookmarkItem == null || bookmarkItem.isEditable());
        mUrlEditText.setEnabled(bookmarkItem == null || bookmarkItem.isUrlEditable());


        mTitleEditText.setText(title);
        mUrlEditText.setText(url);
        mTitleEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (TextUtils.isEmpty(s)) {
                    title = "";
                } else {
                    title = s.toString();
                }
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });
        mUrlEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (TextUtils.isEmpty(s)) {
                    url = "";
                } else {
                    url = s.toString();
                }
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });

        mFolderTextView.setOnClickListener(v -> {
            BookmarkFolderPickerDialog.startFolderSelectFragment(
                    _mActivity,
                    new BookmarkFolderPickerDialog.Callback() {
                        @Override
                        public void onSelectFolder(String selectedFolder, BookmarkId folderId) {
                            moveToFolderId = folderId;
                            PrefsHelper.with().applyLong(KEY_MOVE_TO_BOOKMARK_FOLDER, folderId.getId());
                            mFolderTextView.setText(selectedFolder);
                        }

                        @Override
                        public void onDestroy() {

                        }
                    },
                    moveToFolderId == null ? currentFolderId : moveToFolderId
            );
        });

        TextView add_to_bookmark = findViewById(R.id.add_to_bookmark);
        TextView add_to_home = findViewById(R.id.add_to_home);
        TextView add_to_launcher = findViewById(R.id.add_to_launcher);

        ImageView ivSelected1 = findViewById(R.id.iv_selected_1);
        ImageView ivSelected2 = findViewById(R.id.iv_selected_2);
        ImageView ivSelected3 = findViewById(R.id.iv_selected_3);

        ivSelected1.setVisibility(addToBookmark ? View.VISIBLE : View.GONE);
        if (addToBookmark) {
            add_to_bookmark.setTextColor(getResources().getColor(R.color.colorPrimary));
//            add_to_bookmark.setImageResource(R.drawable.qianxun_add_to_bookmark_added);
        }

        ivSelected2.setVisibility(addToHomepage ? View.VISIBLE : View.GONE);
        if (addToHomepage) {
            add_to_home.setTextColor(getResources().getColor(R.color.colorPrimary));
//            add_to_home.setImageResource(R.drawable.qianxun_add_to_home_added);
        }

        add_to_bookmark.setOnClickListener(v -> {
            addToBookmark = !addToBookmark;
            add_to_bookmark.setTextColor(getResources().getColor(addToBookmark ? R.color.colorPrimary : R.color.color_text_major));
            ivSelected1.setVisibility(addToBookmark ? View.VISIBLE : View.GONE);
//            add_to_bookmark.setImageResource(addToBookmark ? R.drawable.qianxun_add_to_bookmark_added : R.drawable.qianxun_add_to_bookmark);
        });

        add_to_home.setOnClickListener(v -> {
            addToHomepage = !addToHomepage;
            add_to_home.setTextColor(getResources().getColor(addToHomepage ? R.color.colorPrimary : R.color.color_text_major));
            ivSelected2.setVisibility(addToHomepage ? View.VISIBLE : View.GONE);
//            add_to_home.setImageResource(addToHomepage ? R.drawable.qianxun_add_to_home_added : R.drawable.qianxun_add_to_home);
        });

        add_to_launcher.setOnClickListener(v -> {
            addToLauncher = !addToLauncher;
            add_to_launcher.setTextColor(getResources().getColor(addToLauncher ? R.color.colorPrimary : R.color.color_text_major));
            ivSelected3.setVisibility(addToLauncher ? View.VISIBLE : View.GONE);
        });

        TextView cancelBtn = findViewById(R.id.tv_cancel);
        TextView okBtn = findViewById(R.id.tv_ok);
        SkinEngine.setTextColor(cancelBtn, R.attr.textColorMajor);
        SkinEngine.setTextColor(okBtn, R.attr.colorPrimary);
        okBtn.setOnClickListener(this);
        cancelBtn.setOnClickListener(v -> dismiss());
    }

    @Override
    public void onClick(View v) {
        if (addToBookmark) {
            if (mBookmarkId == null || !mModel.doesBookmarkExist(mBookmarkId)) {
                if (mModel.isEditBookmarksEnabled()) {
                    if (moveToFolderId == null || !mModel.doesBookmarkExist(moveToFolderId)) {
                        moveToFolderId = currentFolderId;
                    }
                    mModel.addBookmark(moveToFolderId,
                            mModel.getChildCount(moveToFolderId), title, url);

//                    if (!mTab.isClosing() && mTab.isInitialized()) {
//                        BookmarkId newBookmarkId;
//                        if (id == BookmarkId.INVALID_ID) {
//                            if (moveToFolderId == null || !mModel.doesBookmarkExist(moveToFolderId)) {
//                                moveToFolderId = mModel.getDefaultFolder();
//                            }
//                            newBookmarkId = mModel.addBookmark(moveToFolderId,
//                                    mModel.getChildCount(moveToFolderId), title, url);
//                        } else {
//                            newBookmarkId = new BookmarkId(id, BookmarkType.NORMAL);
//                        }
////                        if (newBookmarkId != null && newBookmarkId.getId() != id) {
////                            OfflinePageUtils.saveBookmarkOffline(newBookmarkId, mTab);
////                        }
//                    }
                }
            } else {
                if (moveToFolderId != null && !currentFolderId.equals(moveToFolderId)) {
                    mModel.moveBookmark(mBookmarkId, moveToFolderId, 0);
                }
                if (!title.isEmpty()) {
                    mModel.setBookmarkTitle(mBookmarkId, title);
                }

                if (!url.isEmpty()
                        && mModel.getBookmarkById(mBookmarkId).isUrlEditable()) {
                    GURL fixedUrl = UrlFormatter.fixupUrl(url);
                    if (fixedUrl != null && !fixedUrl.equals(originalUrl)) {
                        mModel.setBookmarkUrl(mBookmarkId, fixedUrl);
                    }
                }
            }
        } else if (mModel.doesBookmarkExist(mBookmarkId)) {
            mModel.deleteBookmark(mBookmarkId);
            ZToast.normal("删除书签成功");
        }

//        boolean inTheHomepage = HomepageManager.getFavoriteByUrl(url) != null;
//        if (addToHomepage && !inTheHomepage) {
//            EventBus.postAddToHomepageEvent(title, url);
//        } else if (!addToHomepage && inTheHomepage) {
//            ChromeActivity.fromContext(context).getLauncherFragment()
//                    .getLauncherLayout().removeItem(url, true);
//        }

//        ChromeActivity.fromContext(context).getLauncherFragment()
//                .getLauncherManager().getBottomContainer().updateStarButton(mTab);

        dismiss();
    }

    @Override
    public void onDestroyView() {
        hideSoftInput();
        super.onDestroyView();
        mModel.destroy();
    }

}

