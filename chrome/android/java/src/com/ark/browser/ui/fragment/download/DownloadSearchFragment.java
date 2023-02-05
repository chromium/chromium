package com.ark.browser.ui.fragment.download;

import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.ThreadPool;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.MultiRecycler;
import com.zpj.recyclerview.manager.MultiLayoutManager;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.FileUtils;
import com.zpj.widget.toolbar.ZSearchBar;

import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.offline_items_collection.ContentId;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class DownloadSearchFragment extends BaseSwipeBackFragment {

    private final List<DownloadItem> mAllDownloadItems = new ArrayList<>();

    private final DownloadMultiData downloadMultiData = new DownloadMultiData();

    private MultiRecycler mRecycler;

    private ZSearchBar searchBar;


    private EasyRecycler<DownloadChildFragment.FilterItem> mFilterRecycler;
    protected int mFilterIndex = 0;

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_download_search;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        searchBar = findViewById(R.id.search_bar);

        searchBar.setOnLeftButtonClickListener(v -> pop());

        searchBar.addTextWatcher(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                searchDownloadItem(s.toString());
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });

        mRecycler = new MultiRecycler(findViewById(R.id.recycler_view))
                .addItem(downloadMultiData)
                .setLayoutManager(new MultiLayoutManager())
                .build();
        mRecycler.showEmpty();


        ImageView ivExpand = findViewById(R.id.iv_expand);
        ivExpand.setOnClickListener(v -> {
            List<String> titles = new ArrayList<>();
            for (DownloadChildFragment.FilterItem item : mFilterRecycler.getItems()) {
                titles.add(item.name);
            }
            int oldIndex = mFilterIndex;
            new DownloadFilterDialog()
                    .addItems(titles)
                    .setSelectedItem(oldIndex)
                    .setOnItemClickListener((view12, title, position) -> {
                        mFilterIndex = position;
                        mFilterRecycler.notifyItemChanged(oldIndex);
                        mFilterRecycler.notifyItemChanged(mFilterIndex);
                        mFilterRecycler.scrollToPosition(mFilterIndex);
                        filterDownloadItems(mFilterIndex);
                    })
                    .setOnDismissListener(dialog -> {
                        ivExpand.animate()
                                .rotation(0)
                                .setDuration(250)
                                .start();
                        mFilterRecycler.getRecyclerView().animate().alpha(1f).setDuration(250).start();
                    })
                    .show(v);

            ivExpand.animate()
                    .rotation(90)
                    .setDuration(250)
                    .start();
            mFilterRecycler.getRecyclerView().animate().alpha(0f).setDuration(250).start();

        });

        mFilterRecycler = new EasyRecycler<>(findViewById(R.id.rv_filter));
        mFilterRecycler.setItemRes(R.layout.item_download_filter)
                .setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false))
                .setItems(getFilterItems())
                .onBindViewHolder((holder, list, position, payloads) -> {
                    DownloadChildFragment.FilterItem item = list.get(position);
                    if (position == mFilterIndex) {
                        holder.setText(R.id.tv_title, item.name + " | " + item.count);
                        holder.setTextColor(R.id.tv_title, Color.WHITE);
                        holder.setBackgroundResource(R.id.tv_title, R.drawable.grey_shape_select);
                    } else {
                        holder.setText(R.id.tv_title, item.name);
                        holder.setTextColor(R.id.tv_title, SkinEngine.getColor(context, R.attr.textColorNormal));
                        holder.setBackgroundResource(R.id.tv_title, R.drawable.grey_shape2);
                    }
                })
                .setLoadMoreEnabled(false)
                .onItemClick((holder, v, item) -> {
                    if (mFilterIndex == holder.getRealPosition()) {
                        return;
                    }
                    mFilterIndex = holder.getRealPosition();
                    mFilterRecycler.notifyDataSetChanged();
                    filterDownloadItems(mFilterIndex);
                })
                .build();


        post(() -> showSoftInput(searchBar.getEditor()));
    }

    @Override
    public void onDestroyView() {
        hideSoftInput();
        super.onDestroyView();
    }

    private DownloadManagerService.DownloadObserver mLastDownloadHistoryAdapter;

    private void searchDownloadItem(String keyword) {
        mRecycler.showLoading();
        DownloadManagerService downloadManagerService = DownloadManagerService.getDownloadManagerService();

        mLastDownloadHistoryAdapter = new DownloadManagerService.DownloadObserver() {

            @Override
            public void onAllDownloadsRetrieved(List<DownloadItem> list, ProfileKey profileKey) {
                downloadManagerService.removeDownloadObserver(this);
                if (mLastDownloadHistoryAdapter != this) {
                    return;
                }


                ThreadPool.execute(() -> {
                    List<DownloadItem> items = new ArrayList<>(list);
                    if (!TextUtils.isEmpty(keyword)) {
                        Iterator<DownloadItem> it = items.iterator();
                        while (it.hasNext()) {
                            DownloadInfo item = it.next().getDownloadInfo();
                            if (!item.getFileName().toLowerCase().contains(keyword)
                                    && !item.getFilePath().toLowerCase().contains(keyword)
                                    && !item.getUrl().getSpec().toLowerCase().contains(keyword)
                                    && !item.getOriginalUrl().toLowerCase().contains(keyword)) {
                                it.remove();
                            }
                        }
                    }
                    ThreadPool.runOnUIThread(() -> {
                        mAllDownloadItems.clear();
                        mAllDownloadItems.addAll(items);
                        filterDownloadItems(mFilterIndex);
                    });
                });
            }

            @Override
            public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {

            }

            @Override
            public void onDownloadItemFinished(DownloadItem item) {

            }

            @Override
            public void onDownloadItemRemoved(String guid) {

            }

            @Override
            public void onDownloadItemUpdated(DownloadItem item) {

            }

            @Override
            public void onDownloadItemCreated(DownloadItem item) {

            }
        };
        downloadManagerService.addDownloadObserver(mLastDownloadHistoryAdapter);
        downloadManagerService.getAllDownloads(null);

    }

    private void filterDownloadItems(int index) {
        DownloadChildFragment.FilterItem filterItem = mFilterRecycler.getItemAt(index);
        ThreadPool.execute(() -> {
            List<DownloadItem> filterItems = filterItem.filter(mAllDownloadItems);
            mRecycler.post(() -> {
                mRecycler.showContent();
                downloadMultiData.setData(filterItems);
                downloadMultiData.notifyDataSetChange();

                mFilterRecycler.getItemAt(index).count = filterItems.size();
                mFilterRecycler.post(() -> mFilterRecycler.notifyItemChanged(index));
            });
        });
    }

    private List<DownloadChildFragment.FilterItem> getFilterItems() {
        List<DownloadChildFragment.FilterItem> items = new ArrayList<>();
        items.add(new DownloadFragment.AllTypeFilterItem());
        items.add(new DownloadFragment.DownloadingTypeFilterItem());
        items.add(new DownloadFragment.FinishedTypeFilterItem());
        items.add(new DownloadFragment.RemovedTypeFilterItem());
        items.add(new DownloadFragment.FileTypeFilterItem("视频", FileUtils.FileType.VIDEO));
        items.add(new DownloadFragment.FileTypeFilterItem("图片", FileUtils.FileType.IMAGE));
        items.add(new DownloadFragment.FileTypeFilterItem("文本", FileUtils.FileType.TXT));
        items.add(new DownloadFragment.FileTypeFilterItem("压缩包", FileUtils.FileType.ARCHIVE));
        items.add(new DownloadFragment.FileTypeFilterItem("安装包", FileUtils.FileType.APK));
        items.add(new DownloadFragment.FileTypeFilterItem("其它", FileUtils.FileType.UNKNOWN));
        return items;
    }

}

