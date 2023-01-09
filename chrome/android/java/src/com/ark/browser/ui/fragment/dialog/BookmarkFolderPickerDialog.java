package com.ark.browser.ui.fragment.dialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.core.bookmark.BookmarkBridge;
import com.ark.browser.core.bookmark.BookmarkModel;
import com.ark.browser.settings.Keys;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.SupportActivity;
import com.zpj.fragmentation.SupportFragment;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.fragmentation.dialog.impl.InputDialogFragment;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.MultiData;
import com.zpj.recyclerview.MultiRecycler;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;

import java.util.ArrayList;
import java.util.List;

public class BookmarkFolderPickerDialog extends OverDragBottomDialogFragment<BookmarkFolderPickerDialog> {

    private static final String TAG = "BookmarkFolderSelectFragment";

    private final BookmarkBridge.BookmarkModelObserver mBookmarkModelObserver = new BookmarkBridge.BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            updateFolderList();
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                                        boolean isDoingExtensiveChanges) {
            if (node.isFolder()) {
                updateFolderList();
            }
        }
    };

    private BookmarkModel mBookmarkModel;
    private BookmarkId mParentId;

    private MultiRecycler mRecycler;

    private Callback mCallback;

    public interface Callback {
        void onSelectFolder(String selectedFolder, BookmarkId folderId);

        void onDestroy();
    }

    public static void startFolderSelectFragment(Context context, Callback callback, BookmarkId currentFolderId) {
        Bundle bundle = new Bundle();
        bundle.putString(Keys.KEY_ID, currentFolderId.toString());
        BookmarkFolderPickerDialog fragment = new BookmarkFolderPickerDialog();
        fragment.setCallback(callback);
        fragment.setArguments(bundle);
        Activity activity = ContextUtils.getActivity(context);
        if (activity instanceof SupportActivity) {
            ((SupportActivity) activity).start(fragment);
        } else {
            Toast.makeText(context, "启动DialogFragment失败", Toast.LENGTH_SHORT).show();
        }
    }

//    public static void startFolderSelectFragment(SupportFragment parentFragment, Callback callback) {
//        BookmarkFolderPickerDialog fragment = new BookmarkFolderPickerDialog();
//        fragment.setCallback(callback);
//        parentFragment.start(fragment);
//    }

    public BookmarkFolderPickerDialog() {
        setMaxHeight(MATCH_PARENT);
        setMarginTop(ScreenUtils.getStatusBarHeight() * 2);
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_bookmark_folder_select;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        mRecycler = MultiRecycler.with(findViewById(R.id.recycler_view))
                .addItemDecoration(new ShadowItemDecoration())
                .addItemDecoration(new RecyclerView.ItemDecoration() {
                    @Override
                    public void getItemOffsets(@NonNull Rect outRect, @NonNull View view, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
                        Object tag = view.getTag();
                        if (tag instanceof BookmarkId) {
                            BookmarkId id = (BookmarkId) tag;
                            int index = 0;
                            BookmarkId p = id.getParentFolder();
                            while (p != null) {
                                index++;
                                p = p.getParentFolder();
                            }
                            outRect.set(index * ScreenUtils.dp2pxInt(24), 0, 0, 0);
                        } else {
                            super.getItemOffsets(outRect, view, parent, state);
                        }
                    }
                })
                .build();
        mRecycler.showLoading();



        mBookmarkModel = new BookmarkModel();
        mBookmarkModel.finishLoadingBookmarkModel(() -> {
            Bundle bundle = getArguments();
            String parentIdStr = bundle == null ? null : bundle.getString(Keys.KEY_ID);
            if (TextUtils.isEmpty(parentIdStr)) {
                mParentId = null;
            } else {
                mParentId = BookmarkId.getBookmarkIdFromString(parentIdStr);
            }
            if (mParentId == null || !mBookmarkModel.doesBookmarkExist(mParentId)) {
                mParentId = mBookmarkModel.getDefaultFolder();
            }

            mBookmarkModel.addObserver(mBookmarkModelObserver);
            updateFolderList();
        });
        mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
    }

    public void setCallback(Callback mCallback) {
        this.mCallback = mCallback;
    }

    private void updateFolderList() {
//        BookmarkId bookmarkId = mBookmarkModel.getDefaultFolder();
//        TreeMultiData root = new TreeMultiData(null, bookmarkId);
//        mBookmarkModel.getBookmarksForFolder(bookmarkId, new BookmarkBridge.BookmarksCallback() {
//            @Override
//            public void onBookmarksAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList) {
//
//            }
//
//            @Override
//            public void onBookmarksFolderHierarchyAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList) {
//
//            }
//        });

        BookmarkId bookmarkId = mBookmarkModel.getDefaultFolder();
        TreeMultiData root = new TreeMultiData(null, bookmarkId);
        getBookmarkFolderTree(root, 0);
        mRecycler.setItems(root);
        postOnEnterAnimationEnd(() -> mRecycler.showContent());



//        ThreadPool.execute(() -> {
//            BookmarkId bookmarkId = mBookmarkModel.getDefaultFolder();
//            TreeMultiData root = new TreeMultiData(null, bookmarkId);
//            getBookmarkFolderTree(root, 0);
//            mRecycler.setItems(root);
//            postOnEnterAnimationEnd(() -> mRecycler.showContent());
//        });
    }

    private void getBookmarkFolderTree(TreeMultiData parent, int index) {
        BookmarkId id = parent.getBookmarkId();
        for (BookmarkItem child : mBookmarkModel.getBookmarksForFolder(id)) {
            if (child.isFolder() && !TextUtils.isEmpty(child.getTitle())) {
                if (!mBookmarkModel.isFolderVisible(child.getId())) continue;
                child.getId().setParentFolder(id);
                TreeMultiData childNode = new TreeMultiData(parent, child.getId());
                childNode.setIndex(index + 1);
                parent.getData().add(childNode);
                getBookmarkFolderTree(childNode, index + 1);
            }
        }
    }

    @Override
    public void onDestroy() {
        if (mCallback != null) {
            mCallback.onDestroy();
        }
        super.onDestroy();

        mBookmarkModel.removeObserver(mBookmarkModelObserver);
        mBookmarkModel.destroy();
        mBookmarkModel = null;
    }

    private class TreeMultiData extends MultiData<TreeMultiData> {

        private final TreeMultiData parent;
        private final BookmarkId bookmarkId;

        private int i;

        public TreeMultiData(TreeMultiData parent, BookmarkId bookmarkId) {
            this(parent, new ArrayList<>(), bookmarkId);
        }

        public TreeMultiData(TreeMultiData parent, List<TreeMultiData> mData, BookmarkId bookmarkId) {
            super(mData);
            this.parent = parent;
            this.bookmarkId = bookmarkId;
        }

        public void setIndex(int i) {
            this.i = i;
        }

        public int getIndex() {
            return i;
        }

        public BookmarkId getBookmarkId() {
            return bookmarkId;
        }

        @Override
        public int getCount() {
            int count = 1;
            for (TreeMultiData data : getData()) {
                count += data.getCount();
            }
            return count;
        }

        @Override
        public int getLayoutId(int viewType) {
            return R.layout.layout_icon_node;
        }

        @Override
        public boolean hasViewType(int viewType) {
            return true;
        }

        @Override
        public boolean loadData() {
            return false;
        }

        @Override
        public void onBindViewHolder(EasyViewHolder holder, List<TreeMultiData> list, int position, List<Object> payloads) {
            TreeMultiData data = getDataAt(position);
            if (data != null) {
                data.onBind(holder, position, payloads);
            }
        }

        private TreeMultiData getDataAt(int position) {
            if (position == 0) {
                return this;
            }
            int i = 1;
            for (TreeMultiData data : getData()) {
                int count = data.getCount();
                Log.d(TAG, "i=" + i + " position=" + position + " count=" + count);
                if (position >= i && position < i + count) {
                    return data.getDataAt(position - i);
                } else {
                    i += count;
                }
            }
            return null;
        }

        public void onBind(EasyViewHolder holder, int position, List<Object> payloads) {
            holder.getItemView().setTag(bookmarkId);
            TextView textView = holder.getView(R.id.tv_title);
            textView.setText(mBookmarkModel.getBookmarkById(bookmarkId).getTitle());
            textView.setTextColor(mParentId == bookmarkId ?
                    context.getResources().getColor(R.color.colorPrimary)
                    : SkinEngine.getColor(context, R.attr.textColorMajor));
            ClickHelper.with(holder.getItemView())
                    .setOnClickListener(new ClickHelper.OnClickListener() {
                        @Override
                        public void onClick(View v, float x, float y) {
                            BookmarkFolderPickerDialog.TreeMultiData.this.onClick(v, x, y);
                        }
                    })
                    .setOnLongClickListener(new ClickHelper.OnLongClickListener() {
                        @Override
                        public boolean onLongClick(View v, float x, float y) {
                            return BookmarkFolderPickerDialog.TreeMultiData.this.onLongClick(v, position, x, y);
                        }
                    });
        }

        public void onClick(View v, float x, float y) {
            if (mCallback != null) {
                mCallback.onSelectFolder(mBookmarkModel.getBookmarkById(bookmarkId).getTitle(), bookmarkId);
                pop();
            }
        }

        public boolean onLongClick(View v, int position, float x, float y) {
            ZDialog.attach()
                    .setItems("新建文件夹", "重命名")
                    .addItemIf(parent != null, "删除")
                    .setOnSelectListener((dialogFragment, pos, title) -> {
                        switch (pos) {
                            case 0:
                                ZDialog.input()
                                        .setAutoShowKeyboard(true)
                                        .setHint("请输入文件夹名")
                                        .setEmptyable(false)
                                        .setTitle("新建文件夹")
                                        .setPositiveButton((fragment, which) -> {
                                            BookmarkId id = mBookmarkModel.addFolder(bookmarkId, 0, ((InputDialogFragment) fragment).getText());
                                            id.setParentFolder(bookmarkId);
                                            getData().add(0, new TreeMultiData(parent, id));
                                            mRecycler.notifyItemInserted(position + 1);
                                        })
                                        .show(context);
                                break;
                            case 1:
                                ZDialog.input()
                                        .setAutoShowKeyboard(true)
                                        .setHint("请输入文件夹名")
                                        .setEditText(mBookmarkModel.getBookmarkTitle(bookmarkId))
                                        .setEmptyable(false)
                                        .setTitle("重命名")
                                        .setPositiveButton((fragment, which) -> {
                                            String text = ((InputDialogFragment) fragment).getText();
                                            mBookmarkModel.setBookmarkTitle(bookmarkId, text);
                                            TextView textView = v.findViewById(R.id.tv_title);
                                            textView.setText(text);
                                        })
                                        .show(context);
                                break;
                            case 2:
                                ZDialog.alert()
                                        .setTitle("确定删除?")
                                        .setContent("你将删除文件夹：" + mBookmarkModel.getBookmarkTitle(bookmarkId))
                                        .setPositiveButton((fragment, which) -> {
                                            parent.getData().remove(TreeMultiData.this);

                                            mRecycler.getAdapter().notifyItemRangeRemoved(position, getCount());

                                            mBookmarkModel.deleteBookmark(bookmarkId);
                                        })
                                        .show(context);
                                break;
                        }
                        dialogFragment.dismiss();
                    })
                    .setTouchPoint(x, y)
                    .show(context);
            return true;
        }

    }

}

