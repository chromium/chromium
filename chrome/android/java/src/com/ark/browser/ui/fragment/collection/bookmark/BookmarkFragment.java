package com.ark.browser.ui.fragment.collection.bookmark;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import com.ark.browser.core.bookmark.BookmarkBridge;
import com.ark.browser.core.bookmark.BookmarkModel;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.ui.fragment.collection.CollectionChildFragment;
import com.ark.browser.ui.fragment.dialog.BookmarkFolderPickerDialog;
import com.ark.browser.ui.fragment.dialog.CollectionEditorDialog;
import com.ark.browser.ui.widget.DrawableTintTextView;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.InputDialogFragment;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.SelectableRecycler;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.Clipboard;
import org.chromium.url.GURL;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Locale;

public class BookmarkFragment extends CollectionChildFragment
        implements View.OnClickListener,
        IEasy.OnBindViewHolderListener<BookmarkItem>,
        BookmarkModel.BookmarkDeleteObserver {

    private static final String TAG = "BookmarkFragment";
    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    private SelectableRecycler<BookmarkItem> mRecycler;

    private HorizontalScrollView scrollView;
    private LinearLayout scrollContainer;
    private LinearLayout defaultBottomBar;
    private LinearLayout editBottomBar;
    private TextView cloudBtn;
    private TextView newFolderBtn;
    private TextView editBtn;
    private TextView selectAllBtn;
    private TextView moveBtn;
    private TextView deleteBtn;
    private TextView completeBtn;

    private RoundedIconGenerator mIconGenerator;
    private int mMinIconSize;
    private int mDisplayedIconSize;
    private int mCornerRadius;

    private BookmarkModel bookmarkModel;
    private LargeIconBridge mLargeIconBridge;

    private final BookmarkBridge.BookmarkModelObserver observer = new BookmarkBridge.BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            Log.d(TAG, "bookmarkModelChanged");
            refresh();
        }
    };

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        bookmarkModel = new BookmarkModel();
        bookmarkModel.addDeleteObserver(this);
        bookmarkModel.addObserver(observer);
        bookmarkModel.loadEmptyPartnerBookmarkShimForTesting();

        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile().getOriginalProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min((activityManager.getMemoryClass() / 4) * 1024 * 1024, 10 * 1024 * 1024);
        mLargeIconBridge.createCache(maxSize);

        mCornerRadius = getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        mMinIconSize = (int) getResources().getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        int textSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        int iconColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
                mDisplayedIconSize / 2,
                iconColor, textSize);
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_collections_bookmark;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        scrollView = findViewById(R.id.scroll_view);
        scrollContainer = findViewById(R.id.scroll_container);


        mRecycler = new SelectableRecycler<BookmarkItem>(mRecyclerView)
                .addItemDecoration(new ShadowItemDecoration())
                .setOnSelectChangeListener(new IEasy.OnSelectChangeListener<BookmarkItem>() {
                    @Override
                    public void onSelectModeChange(boolean selectMode) {

                    }

                    @Override
                    public void onSelectChange(List<BookmarkItem> list, int position, boolean isChecked) {
                        setEditBottomBarState(mRecycler.getSelectedCount());
                    }

                    @Override
                    public void onSelectAll() {
                        selectAllBtn.setText("全不选");
                    }

                    @Override
                    public void onUnSelectAll() {
                        selectAllBtn.setText("全选");
                    }

                    @Override
                    public void onSelectOverMax(int maxSelectCount) {

                    }
                })
                .setItemRes(R.layout.item_collection)
                .onBindViewHolder(this)
                .build();
        mRecycler.showLoading();

        bookmarkModel.finishLoadingBookmarkModel(() -> {
            postOnLazyInit(() -> addScrollChild(bookmarkModel.getDefaultFolder(), "我的书签"));
        });
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        bookmarkModel.removeObserver(observer);
        bookmarkModel.destroy();
        bookmarkModel = null;
    }

    @Override
    public boolean onBackPressedSupport() {
        if (defaultBottomBar.getVisibility() == View.GONE) {
            defaultBottomBar.setVisibility(View.VISIBLE);
            editBottomBar.setVisibility(View.GONE);
            mRecycler.exitSelectMode();
            mRecycler.notifyDataSetChanged();
            setEditBottomBarState(0);
            return true;
        }
        if (scrollContainer.getChildCount() > 1) {
            scrollContainer.removeViewAt(scrollContainer.getChildCount() - 1);
            resetScrollChildColor();
            DrawableTintTextView view = (DrawableTintTextView) scrollContainer.getChildAt(scrollContainer.getChildCount() - 1);
            Drawable drawable = getResources().getDrawable(R.drawable.ic_enter_bak);
            view.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, drawable, null);
            view.setTint(ApiCompatibilityUtils.getColor(getResources(), R.color.colorPrimary));
            view.getPaint().setFakeBoldText(true);
            refresh();
            return true;
        }
        return super.onBackPressedSupport();
    }

    private void resetScrollChildColor() {
        int grey = ApiCompatibilityUtils.getColor(getResources(), R.color.google_grey_500);
        for (int i = 0; i < scrollContainer.getChildCount(); i++) {
            DrawableTintTextView textView = (DrawableTintTextView) scrollContainer.getChildAt(i);
            SkinEngine.applyViewAttr(textView, "textColor", R.attr.textColorNormal);
            Drawable drawable = getResources().getDrawable(R.drawable.ic_enter_bak);
            textView.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, drawable, null);
            textView.getPaint().setFakeBoldText(false);
            textView.setTint(grey);
        }
    }

    private void addScrollChild(BookmarkId bookmarkId, String content) {
        resetScrollChildColor();
        DrawableTintTextView view = new DrawableTintTextView(getContext());
        int primaryColor = ApiCompatibilityUtils.getColor(getResources(), R.color.colorPrimary);
        view.setOnClickListener(v -> {
            if (mRecycler.isSelectMode()) {
                return;
            }
            int count = scrollContainer.getChildCount();
            int index = scrollContainer.indexOfChild(view);
            if (count <= index + 1) {
                return;
            }
            for (int i = count - 1; i >= index + 1; i--) {
                scrollContainer.removeViewAt(i);
            }
            resetScrollChildColor();
            Drawable drawable = getResources().getDrawable(R.drawable.ic_enter_bak);
            view.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, drawable, null);
            view.getPaint().setFakeBoldText(true);
            view.setTint(primaryColor);
            post(() -> scrollView.fullScroll(HorizontalScrollView.FOCUS_RIGHT));
            refresh();
        });
        view.setGravity(Gravity.CENTER_VERTICAL);
        view.setTag(bookmarkId);
        view.setText(content);
        view.getPaint().setFakeBoldText(true);
        Drawable drawable = getResources().getDrawable(R.drawable.ic_enter_bak);
        view.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, drawable, null);
        scrollContainer.addView(view);
        view.setTint(primaryColor);
        post(() -> scrollView.fullScroll(HorizontalScrollView.FOCUS_RIGHT));
        refresh();
    }

    private BookmarkId getCurrentFolder() {
        TextView view = (TextView) scrollContainer.getChildAt(scrollContainer.getChildCount() - 1);
        return (BookmarkId) view.getTag();
    }

    private void showFolderSelectFragment(List<BookmarkId> items) {
        BookmarkFolderPickerDialog.startFolderSelectFragment(
                _mActivity,
                new BookmarkFolderPickerDialog.Callback() {
                    @Override
                    public void onSelectFolder(String selectedFolder, BookmarkId folderId) {
                        bookmarkModel.moveBookmarks(items, folderId);
                        refresh();
                    }

                    @Override
                    public void onDestroy() {

                    }
                },
                items.get(0)
        );
    }

    @Override
    public void onClick(View v) {
        if (v == cloudBtn) {
            ZToast.normal("云同步");
        } else if (v == newFolderBtn) {
            ZDialog.input()
                    .setHint("请输入文件夹名")
                    .setEditText("")
                    .setEmptyable(false)
                    .setAutoShowKeyboard(true)
                    .setTitle("新建文件夹")
                    .setPositiveButton((fragment, which) -> {
                        String text = fragment.getText();
                        bookmarkModel.addFolder(getCurrentFolder(), 0, text);
                        refresh();
                    })
                    .show(context);
        } else if (v == editBtn) {
            if (mRecycler.isEmpty()) {
                return;
            }
            defaultBottomBar.setVisibility(View.GONE);
            editBottomBar.setVisibility(View.VISIBLE);
            SkinEngine.applyViewAttr(selectAllBtn, "textColor", mRecycler.isEmpty() ? R.attr.textColorMinor : R.attr.textColorMajor);
//            selectAllBtn.setTextColor(ApiCompatibilityUtils.getColor(getResources(), bookmarkIdList.isEmpty() ? R.color.google_grey_400 : R.color.google_black_400));
            setEditBottomBarState(mRecycler.getSelectedCount());
            mRecycler.enterSelectMode();
            selectAllBtn.setText("全选");
        } else if (v == selectAllBtn) {
            String text = selectAllBtn.getText().toString();
            if (text.equals("全选")) {
                selectAllBtn.setText("全不选");
                mRecycler.selectAll();
                setEditBottomBarState(mRecycler.getItemCount());
            } else {
                selectAllBtn.setText("全选");
                mRecycler.unSelectAll();
                setEditBottomBarState(0);
            }
        } else if (v == moveBtn) {
            List<BookmarkId> ids = new ArrayList<>();
            for (BookmarkItem item : mRecycler.getSelectedItem()) {
                ids.add(item.getId());
            }
            showFolderSelectFragment(ids);
        } else if (v == deleteBtn) {
            ZDialog.alert()
                    .setTitle("确定删除？")
                    .setContent("你将删除" + mRecycler.getSelectedCount() + "个书签")
                    .setPositiveButton((fragment, which) -> {
                        BookmarkId[] bookmarkIds = new BookmarkId[mRecycler.getSelectedCount()];
                        int i = 0;
                        for (BookmarkItem item : mRecycler.getSelectedItem()) {
                            bookmarkIds[i] = item.getId();
                            i++;
                        }
                        Log.d(TAG, "length=" + bookmarkIds.length);
                        bookmarkModel.deleteBookmarks(bookmarkIds);
                    })
                    .show(context);
        } else if (v == completeBtn) {
            defaultBottomBar.setVisibility(View.VISIBLE);
            editBottomBar.setVisibility(View.GONE);
            mRecycler.exitSelectMode();
        }
    }

    public void setEditBottomBarState(int selectCount) {
        if (selectCount != 0) {
            SkinEngine.applyViewAttr(moveBtn, "textColor", R.attr.textColorMajor);
            deleteBtn.setTextColor(ApiCompatibilityUtils.getColor(getResources(), R.color.light_red));
            moveBtn.setClickable(true);
            deleteBtn.setClickable(true);
            deleteBtn.setText(getResources().getString(R.string.text_delete_with_count, selectCount));
        } else {
            SkinEngine.applyViewAttr(moveBtn, "textColor", R.attr.textColorMinor);
            SkinEngine.applyViewAttr(deleteBtn, "textColor", R.attr.textColorMinor);
            moveBtn.setClickable(false);
            deleteBtn.setClickable(false);
            deleteBtn.setText(getResources().getString(R.string.text_delete));
        }
    }

    private void refresh() {
        mRecycler.clearSelectedPosition();
        bookmarkModel.getBookmarksForFolder(getCurrentFolder(), new BookmarkBridge.BookmarksCallback() {
            @Override
            public void onBookmarksAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList) {
                ThreadPool.execute(() -> {
                    Collections.sort(bookmarksList, (item1, item2) -> {
                        if (item1.isFolder() && !item2.isFolder()) {
                            return -1;
                        } else if (item2.isFolder() && !item1.isFolder()) {
                            return 1;
                        }
                        return Long.compare(item2.getId().getId(), item1.getId().getId());
                    });
                    ListIterator<BookmarkItem> it = bookmarksList.listIterator();
                    while (it.hasNext()) {
                        BookmarkItem item = it.next();
                        if (item.isFolder() && TextUtils.isEmpty(item.getTitle())) {
                            it.remove();
                        }
                        if (!item.isFolder()) {
                            break;
                        }
                    }

                    postOnEnterAnimationEnd(() -> {
                        if (editBtn != null) {
                            editBtn.setClickable(!mRecycler.isEmpty());
                            SkinEngine.setTextColor(editBtn, mRecycler.isEmpty() ? R.attr.textColorMinor : R.attr.textColorMajor);
                        }
                        mRecycler.setItems(bookmarksList);
                        mRecycler.notifyDataSetChanged();
                    });
                });
            }

            @Override
            public void onBookmarksFolderHierarchyAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList) {

            }
        });

    }

    @Override
    public void onDeleteBookmarks(String[] titles, boolean isUndoable) {
        Log.d(TAG, "onDeleteBookmarks");
        refresh();
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<BookmarkItem> list, int position, List<Object> payloads) {
        BookmarkItem item = list.get(position);
        GURL url = item.getUrl();
        holder.setText(R.id.tv_title, item.getTitle());
        holder.setText(R.id.tv_info, sdf.format(new Date()));
        if (item.isFolder()) {
            holder.setText(R.id.tv_desc, "");
            holder.setImageResource(R.id.iv_icon, R.drawable.icon_list_folder);
        } else {
            holder.setText(R.id.tv_desc, url.getHost());
            mLargeIconBridge.getLargeIconForUrl(url, mMinIconSize, (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                if (icon == null) {
                    holder.setImageDrawable(R.id.iv_icon, new BitmapDrawable(getResources(), mIconGenerator.generateIconForUrl(item.getUrl())));
                } else {
                    RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                            getResources(),
                            Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
                    roundedIcon.setCornerRadius(mCornerRadius);
                    holder.setImageDrawable(R.id.iv_icon, roundedIcon);
                }
            });
        }

//        ImageView ivArrow = holder.getImageView(R.id.iv_arrow);


//        if (bookmarkItem.isFolder() && !recyclerLayout.isSelectMode()) {
//            ivArrow.setVisibility(View.VISIBLE);
//        } else {
//            ivArrow.setVisibility(View.GONE);
//        }

        holder.setOnItemClickListener(v -> {
            if (item.isFolder()) {
                addScrollChild(item.getId(), item.getTitle());
            } else {
                LoadUrlEvent.post(item.getUrl().getSpec());
            }
        });
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, item.isFolder(), x, y);
                    return true;
                });
    }

    private void showMenu(EasyViewHolder holder, BookmarkItem item, boolean isFolder, float x, float y) {
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
                                showDeleteDialog("你将删除文件夹：" + item.getTitle(), item, holder);
                                break;
                            case 2:
                                List<BookmarkId> bookmarkIdList = new ArrayList<>();
                                bookmarkIdList.add(item.getId());
                                showFolderSelectFragment(bookmarkIdList);
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
                                showDeleteDialog("你将删除书签：" + item.getUrl(), item, holder);
                                break;
                            case 3:
                                CollectionEditorDialog.newInstance(item.getId()).show(context);
                                break;
                            case 4:
                                List<BookmarkId> bookmarkIdList = new ArrayList<>();
                                bookmarkIdList.add(item.getId());
                                showFolderSelectFragment(bookmarkIdList);
                                break;
                            case 5:
                                ZToast.warning("TODO 添加到主页");
                                break;
                        }
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(context);
    }

    private void showDeleteDialog(String content, BookmarkItem item, EasyViewHolder holder) {
        ZDialog.alert()
                .setTitle("确定删除?")
                .setContent(content)
                .setPositiveButton((fragment, which) -> {
                    bookmarkModel.deleteBookmark(item.getId());
                    mRecycler.removeItem(item);
                    mRecycler.notifyItemRemoved(holder.getAdapterPosition());
                })
                .show(context);
    }

    @Override
    public View onCreateBottomBar(Context context) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_collection_bottom_bar, null, false);
        defaultBottomBar = view.findViewById(R.id.bottom_bar_default);
        editBottomBar = view.findViewById(R.id.bottom_bar_edit);
        cloudBtn = view.findViewById(R.id.btn_cloud);
        cloudBtn.setOnClickListener(this);
        newFolderBtn = view.findViewById(R.id.btn_new_folder);
        newFolderBtn.setOnClickListener(this);
        editBtn = view.findViewById(R.id.btn_edit);
        editBtn.setClickable(!mRecycler.isEmpty());
        SkinEngine.setTextColor(editBtn, mRecycler.isEmpty() ? R.attr.textColorMinor : R.attr.textColorMajor);
        editBtn.setOnClickListener(this);
        selectAllBtn = view.findViewById(R.id.btn_select_all);
        selectAllBtn.setOnClickListener(this);
        moveBtn = view.findViewById(R.id.btn_move);
        moveBtn.setOnClickListener(this);
        deleteBtn = view.findViewById(R.id.btn_delete);
        deleteBtn.setOnClickListener(this);
        completeBtn = view.findViewById(R.id.btn_complete);
        completeBtn.setOnClickListener(this);
        return view;
    }

    @Override
    public void onShowMenu(View view) {
        ZDialog.attach()
                .addItems("导入书签", "导出全部书签", "清空")
                .setOnSelectListener((fragment, position, text) -> {
                    ZToast.normal(text);
                    switch (position) {
                        case 0:
                            break;
                        case 1:
                            ThreadPool.execute(new Runnable() {
                                @Override
                                public void run() {
                                    saveBookmarks();
                                }
                            });
                            break;
                        case 2:
                            ZDialog.alert()
                                    .setTitle("确定清空书签？")
                                    .setContent("您将清空所有的书签！！！")
                                    .show(context);
                            break;
                    }
                    fragment.dismiss();
                })
                .setAttachView(view)
                .show(context);
    }

    private void saveBookmarks() {
        List<BookmarkItem> bookmarkItems = bookmarkModel.getBookmarksForFolder(bookmarkModel.getDefaultFolder());

        File saveFolder = new File(Environment.getExternalStorageDirectory(), "/QXBrowser/bookmarks/");
        if (!saveFolder.exists()) {
            saveFolder.mkdirs();
        }
        try (FileOutputStream fos = new FileOutputStream(new File(saveFolder, "千寻浏览器_书签备份_" + System.currentTimeMillis() + ".html"));
             BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(fos))
        ) {
            writer.write("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<!-- This is an automatically generated file.\n     It will be read and overwritten.\n     DO NOT EDIT! -->\n<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n<TITLE>Bookmarks</TITLE>\n<H1>Bookmarks</H1>\n<DL><p>");
            writer.newLine();

            for (BookmarkItem item : bookmarkItems) {
                if (item.isFolder()) {
                    writeBookmarks(writer, item);
                } else {
                    writer.write("<DT><A HREF=\"" + item.getUrl() + "\">" + item.getTitle() + "</A>");
                    writer.newLine();
                }
            }

            writer.write("</DL><p>");
            writer.newLine();
            ZToast.success("书签备份成功!");
        } catch (IOException e) {
            e.printStackTrace();
            ZToast.error("书签备份失败!" + e.getMessage());
        }

    }

    private void writeBookmarks(BufferedWriter writer, BookmarkItem folderItem) throws IOException {
        List<BookmarkItem> bookmarkItems = bookmarkModel.getBookmarksForFolder(folderItem.getId());

        Collections.sort(bookmarkItems, (o1, o2) -> {
            if (o1.isFolder() && !o2.isFolder()) {
                return -1;
            } else if (o2.isFolder() && !o1.isFolder()) {
                return 1;
            }
            return Long.compare(o2.getId().getId(), o1.getId().getId());
        });

        writer.write("<DT><H3 DATA=\"FOLDER\">" + folderItem.getTitle() + "</H3>");
        writer.newLine();
        writer.write("<DL><p>");
        writer.newLine();

        for (BookmarkItem item : bookmarkItems) {
            if (item.isFolder()) {
                writeBookmarks(writer, item);
            } else {
                writer.write("<DT><A HREF=\"" + item.getUrl() + "\">" + item.getTitle() + "</A>");
                writer.newLine();
            }
        }

        writer.write("</DL><p>");
        writer.newLine();
    }

    private void restoreBookmarks(String filePath) {
        try (FileInputStream fis = new FileInputStream(new File(filePath));
             BufferedReader reader = new BufferedReader(new InputStreamReader(fis))
        ) {
            while (true) {
                String line = reader.readLine();
                if (line == null) {
                    break;
                }

                String trim = line.trim();

                if ((trim.startsWith("<dt><h3") && trim.endsWith("</h3>"))
                        || (trim.startsWith("<DT><H3") && trim.endsWith("</H3>"))) {
                    // folder
                } else if (trim.startsWith("<dt><a") || trim.startsWith("<DT><A")) {
                    // item
                }
            }



        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}

