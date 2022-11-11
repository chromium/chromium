package com.ark.browser.ui.dialog;

import android.view.Display;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.core.bookmark.BookmarkBridge;
import com.ark.browser.core.bookmark.BookmarkModel;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.ArrayList;
import java.util.List;

public class BookmarkManagerDialog {

    private static class BookmarkListAdapter extends RecyclerView.Adapter<BookmarkListViewHolder> {

        private final List<BookmarkItem> mBookmarkItems = new ArrayList<>();

        protected BookmarkListAdapter() {
        }

        public List<BookmarkItem> getCurrentList() {
            return mBookmarkItems;
        }

        public void setItems(List<BookmarkItem> items) {
            mBookmarkItems.clear();
            for (BookmarkItem item : items) {
                if (item.getId().getType() == BookmarkType.NORMAL) {
                    mBookmarkItems.add(item);
                }
            }
            notifyDataSetChanged();
        }

        @NonNull
        @Override
        public BookmarkListViewHolder onCreateViewHolder(@NonNull ViewGroup viewGroup, int i) {
            ArkLogger.e(BookmarkListAdapter.class, "onCreateViewHolder i=" + i);
            View itemView = LayoutInflater.from(viewGroup.getContext()).inflate(R.layout.item_collection, viewGroup, false);
            return new BookmarkListViewHolder(itemView);
        }

        @Override
        public void onBindViewHolder(@NonNull BookmarkListViewHolder holder, int position, @NonNull List<Object> payloads) {
            onBindViewHolder(holder, position);
        }

        @Override
        public void onBindViewHolder(@NonNull BookmarkListViewHolder holder, int i) {
            BookmarkItem item = mBookmarkItems.get(i);

            holder.tvTitle.setText(item.getTitle());
            holder.tvDesc.setText(item.getUrl().getHost());

            if (item.isFolder()) {
                holder.ivIcon.setImageResource(R.drawable.ic_folder_blue_24dp);
            } else {
                holder.ivIcon.setImageResource(R.drawable.ic_drive_file_24dp);
            }

            holder.itemView.setOnClickListener(v -> {
                Toast.makeText(v.getContext().getApplicationContext(), "bookmarkId=" + item.getId(),
                        Toast.LENGTH_SHORT).show();
            });

        }

        @Override
        public int getItemCount() {
            return mBookmarkItems.size();
        }

    }

    private static class BookmarkListViewHolder extends RecyclerView.ViewHolder {

        private TextView tvTitle;
        private ImageView ivIcon;
        private TextView tvInfo;
        private TextView tvDesc;

        public BookmarkListViewHolder(View itemView) {
            super(itemView);
            tvTitle = itemView.findViewById(R.id.tv_title);
            ivIcon = itemView.findViewById(R.id.iv_icon);
            tvInfo = itemView.findViewById(R.id.tv_info);
            tvDesc = itemView.findViewById(R.id.tv_desc);
        }
    }

    public static void show(ArkBrowserActivity activity) {



        RecyclerView recyclerView = new RecyclerView(activity);
        recyclerView.setLayoutManager(new LinearLayoutManager(activity));

        BookmarkListAdapter adapter = new BookmarkListAdapter();
        recyclerView.setAdapter(adapter);

        BookmarkModel bookmarkModel = new BookmarkModel();
        bookmarkModel.addObserver(new BookmarkBridge.BookmarkModelObserver() {
            @Override
            public void bookmarkModelChanged() {
                List<BookmarkItem> items = bookmarkModel.getBookmarksForFolder(bookmarkModel.getDefaultFolder());
                adapter.setItems(items);
            }
        });


        AlertDialog selector = new AlertDialog.Builder(activity)
                .setTitle("Bookmark Manager")
                .setView(recyclerView)
                .setOnCancelListener(dialog -> bookmarkModel.destroy())
                .setOnDismissListener(dialog -> bookmarkModel.destroy())
                .create();
        selector.show();

        //设置弹窗在底部
        Window window = selector.getWindow();
        window.setGravity(Gravity.CENTER);

        WindowManager m = activity.getWindowManager();
        Display d = m.getDefaultDisplay(); //为获取屏幕宽、高
        WindowManager.LayoutParams p = selector.getWindow().getAttributes(); //获取对话框当前的参数值
        p.width = d.getWidth(); //宽度设置为屏幕
        p.height = d.getHeight();
        selector.getWindow().setAttributes(p); //设置生效

//        List<BookmarkItem> items = bookmarkModel.getBookmarksForFolder(bookmarkModel.getDefaultFolder());
//        adapter.getCurrentList().clear();
//        adapter.getCurrentList().addAll(items);
//        adapter.notifyDataSetChanged();





        bookmarkModel.finishLoadingBookmarkModel(new Runnable() {
            @Override
            public void run() {
                bookmarkModel.getBookmarksForFolder(bookmarkModel.getDefaultFolder(), new BookmarkBridge.BookmarksCallback() {
                    @Override
                    public void onBookmarksAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList) {
                        adapter.setItems(bookmarksList);
                    }

                    @Override
                    public void onBookmarksFolderHierarchyAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList) {

                    }
                });
            }
        });

        bookmarkModel.loadEmptyPartnerBookmarkShimForTesting();


    }

}
