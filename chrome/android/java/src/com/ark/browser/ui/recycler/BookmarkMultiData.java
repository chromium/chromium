package com.ark.browser.ui.recycler;

import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.text.TextUtils;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import com.ark.browser.core.bookmark.BookmarkBridge;
import com.ark.browser.core.bookmark.BookmarkModel;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.ui.fragment.dialog.BookmarkFolderPickerDialog;
import com.ark.browser.ui.fragment.dialog.CollectionEditorDialog;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.KeywordUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.InputDialogFragment;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.statemanager.State;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class BookmarkMultiData extends BaseHeaderMultiData<BookmarkId> {

    private static final String TAG = "BookmarkMultiData";

    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());
    private final BookmarkBridge.BookmarkModelObserver observer = new BookmarkBridge.BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            ArkLogger.d(TAG, "bookmarkModelChanged");
            refresh();
        }
    };

    private RoundedIconGenerator mIconGenerator;
    private int mMinIconSize;
    private int mDisplayedIconSize;
    private int mCornerRadius;

    private BookmarkModel bookmarkModel;
    private LargeIconBridge mLargeIconBridge;

    private String keyword;

    public BookmarkMultiData(String keyword) {
        super("网页书签");
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    public BookmarkMultiData(String keyword, List<BookmarkId> list) {
        super("网页书签", list);
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    public void updateKeyword(String keyword) {
        this.keyword = keyword;
        if (bookmarkModel != null) {
            refresh();
        }
    }

    private void init() {
        bookmarkModel = new BookmarkModel();
        bookmarkModel.addObserver(observer);
        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min((activityManager.getMemoryClass() / 4) * 1024 * 1024, 10 * 1024 * 1024);
        mLargeIconBridge.createCache(maxSize);

        Resources resource = ContextUtils.getApplication().getResources();
        mCornerRadius = resource.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        mMinIconSize = (int) resource.getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = resource.getDimensionPixelSize(R.dimen.default_favicon_size);
        int textSize = resource.getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        int iconColor = ApiCompatibilityUtils.getColor(
                resource, R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
                mDisplayedIconSize / 2,
                iconColor, textSize);
    }

    @Override
    public int getChildViewType(int position) {
        return R.layout.item_collection;
    }

    @Override
    public int getChildLayoutId(int viewType) {
        return R.layout.item_collection;
    }

    @Override
    public void onBindChild(EasyViewHolder holder, List<BookmarkId> list, int position, List<Object> payloads) {
        BookmarkId item = list.get(position);
        BookmarkItem bookmarkItem = bookmarkModel.getBookmarkById(item);
        String url = bookmarkItem.getUrl().getSpec();
        holder.setText(R.id.tv_title, KeywordUtil.hightlight(Color.RED, bookmarkItem.getTitle(), keyword));
        holder.setText(R.id.tv_info, sdf.format(new Date()));

        if (bookmarkItem.isFolder()) {
            holder.setText(R.id.tv_desc, "");
            holder.setImageResource(R.id.iv_icon, R.drawable.icon_list_folder);
        } else {
            Uri uri = Uri.parse(url);
            holder.setText(R.id.tv_desc, KeywordUtil.hightlight(Color.RED, uri.getHost(), keyword));
            mLargeIconBridge.getLargeIconForUrl(bookmarkItem.getUrl(), mMinIconSize,
                    (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                        if (icon == null) {
                            holder.setImageBitmap(R.id.iv_icon, mIconGenerator.generateIconForUrl(bookmarkItem.getUrl()));
                        } else {
                            RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                                    holder.getContext().getResources(),
                                    Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
                            roundedIcon.setCornerRadius(mCornerRadius);
                            holder.setImageDrawable(R.id.iv_icon, roundedIcon);
                        }
                    });
        }
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, bookmarkItem, bookmarkItem.isFolder(), x, y);
                    return true;
                });
    }

    @Override
    public boolean loadData() {
        refresh();
        return false;
    }

    @Override
    public int getCount() {
        if (getState() == State.STATE_CONTENT && getChildCount() == 0) {
            return 0;
        }
        return super.getCount();
    }

    private void refresh() {
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            if (TextUtils.isEmpty(keyword)) {
                showEmpty();
                return;
            }
            List<BookmarkId> bookmarks = bookmarkModel.searchBookmarks(keyword, -1);
            getAdapter().post(() -> {
                mData.clear();
                mData.addAll(bookmarks);
                ArkLogger.d(TAG, "showContent size=" + mData.size() + " count=" + getCount());
                showContent();
            });
        });
    }

    private void showMenu(EasyViewHolder holder, BookmarkItem item, boolean isFolder, float x, float y) {
        Context context = holder.getContext();
        ZDialog.attach()
                .addItemsIf(isFolder, "重命名", "删除", "移动")
                .addItemsIf(!isFolder, "新窗口打开", "复制链接", "删除", "编辑", "移动", "添加到主页")
                .setOnSelectListener((fragment, position, text) -> {
                    if (isFolder) {
                        switch (position) {
                            case 0:
                                new InputDialogFragment()
                                        .setEditText(item.getTitle())
                                        .setHint("书签文件夹标题")
                                        .setEmptyable(false)
                                        .setAutoShowKeyboard(true)
                                        .setTitle("重命名文件夹")
                                        .setPositiveButton((fragment1, which) -> {
                                            String text1 = fragment1.getText();
                                            bookmarkModel.setBookmarkTitle(item.getId(), text1);
                                            holder.setText(R.id.tv_title, text1);
                                        })
                                        .show(context);
                                break;
                            case 1:
                                showDeleteDialog("你将删除文件夹：" + item.getTitle(), item.getId(), holder);
                                break;
                            case 2:
                                showFolderSelectFragment(context, item);
                                break;
                        }
                    } else {
                        switch (position) {
                            case 0:
                                LoadUrlEvent.post(item.getUrl().getSpec(), true);
                                break;
                            case 1:
                                Clipboard.getInstance().setTextFromUser(item.getUrl().getSpec());
                                break;
                            case 2:
                                showDeleteDialog("你将删除书签：" + item.getUrl(), item.getId(), holder);
                                break;
                            case 3:
                                CollectionEditorDialog.newInstance(item.getId()).show(context);
                                break;
                            case 4:
                                showFolderSelectFragment(context, item);
                                break;
                            case 5:
                                // TODO
//                                EventBus.postAddToHomepageEvent(item.getTitle(), item.getUrl());
                                break;
                        }
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(context);
    }

    private void showDeleteDialog(String content, BookmarkId item, EasyViewHolder holder) {
        ZDialog.alert()
                .setTitle("确定删除?")
                .setContent(content)
                .setPositiveButton((fragment, which) -> {
                    bookmarkModel.deleteBookmark(item);
                    mData.remove(item);
                    if (mData.isEmpty()) {
                        showContent();
                    } else {
                        notifyItemRemoved(holder.getAdapterPosition());
                    }
                })
                .show(holder.getContext());
    }

    private void showFolderSelectFragment(Context context, BookmarkItem item) {
        BookmarkId bookmarkId = item.getId();
        BookmarkFolderPickerDialog.startFolderSelectFragment(
                context,
                new BookmarkFolderPickerDialog.Callback() {
                    @Override
                    public void onSelectFolder(String selectedFolder, BookmarkId folderId) {
                        List<BookmarkId> items = new ArrayList<>();
                        items.add(bookmarkId);
                        bookmarkModel.moveBookmarks(items, folderId);
                        refresh();
                    }

                    @Override
                    public void onDestroy() {

                    }
                },
                item.getParentId()

        );
    }

}

