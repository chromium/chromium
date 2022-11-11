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
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

public class HistoryManagerDialog {

    private static class HistoryListAdapter extends RecyclerView.Adapter<HistoryListViewHolder> {

        private final List<HistoryItem> mHistoryItems = new ArrayList<>();

        protected HistoryListAdapter() {
        }

        public List<HistoryItem> getCurrentList() {
            return mHistoryItems;
        }

        @NonNull
        @Override
        public HistoryListViewHolder onCreateViewHolder(@NonNull ViewGroup viewGroup, int i) {
            ArkLogger.e(HistoryListAdapter.class, "onCreateViewHolder i=" + i);
            View itemView = LayoutInflater.from(viewGroup.getContext()).inflate(R.layout.item_collection, viewGroup, false);
            return new HistoryListViewHolder(itemView);
        }

        @Override
        public void onBindViewHolder(@NonNull HistoryListViewHolder holder, int position, @NonNull List<Object> payloads) {
            onBindViewHolder(holder, position);
        }

        @Override
        public void onBindViewHolder(@NonNull HistoryListViewHolder holder, int i) {
            HistoryItem item = mHistoryItems.get(i);

            holder.tvTitle.setText(item.getTitle());
            holder.tvDesc.setText(item.getUrl().getHost());

            holder.itemView.setOnClickListener(v -> {
                Toast.makeText(v.getContext().getApplicationContext(), item.getUrl().toString(),
                        Toast.LENGTH_SHORT).show();
            });

        }

        @Override
        public int getItemCount() {
            return mHistoryItems.size();
        }

    }

    private static class HistoryListViewHolder extends RecyclerView.ViewHolder {

        private TextView tvTitle;
        private ImageView ivIcon;
        private TextView tvInfo;
        private TextView tvDesc;

        public HistoryListViewHolder(View itemView) {
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

        HistoryListAdapter adapter = new HistoryListAdapter();
        recyclerView.setAdapter(adapter);


        HistoryProvider historyProvider = new BrowsingHistoryBridge(Profile.getLastUsedRegularProfile());
        HistoryProvider.BrowsingHistoryObserver observer = new HistoryProvider.BrowsingHistoryObserver() {
            @Override
            public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
                adapter.getCurrentList().addAll(items);
                adapter.notifyDataSetChanged();
            }

            @Override
            public void onHistoryDeleted() {
                historyProvider.queryHistory("");
            }

            @Override
            public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {

            }
        };
        historyProvider.setObserver(observer);


        AlertDialog selector = new AlertDialog.Builder(activity)
                .setTitle("History Manager")
                .setView(recyclerView)
                .setOnCancelListener(dialog -> historyProvider.destroy())
                .setOnDismissListener(dialog -> historyProvider.destroy())
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

        historyProvider.queryHistory("");
    }

}
