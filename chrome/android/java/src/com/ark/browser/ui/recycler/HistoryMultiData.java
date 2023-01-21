package com.ark.browser.ui.recycler;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Color;
import android.view.View;

import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.utils.FaviconUtil;
import com.ark.browser.utils.KeywordUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.statemanager.State;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.Clipboard;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class HistoryMultiData extends BaseHeaderMultiData<HistoryItem>
        implements HistoryProvider.BrowsingHistoryObserver {

    private static final String TAG = "BookmarkMultiData";

    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    HistoryProvider historyProvider;
    private LargeIconBridge mLargeIconBridge;

    private String keyword;

    public HistoryMultiData(String keyword) {
        super("历史记录");
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    public HistoryMultiData(String keyword, List<HistoryItem> list) {
        super("历史记录", list);
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    private void init() {
        historyProvider = new BrowsingHistoryBridge(Profile.getLastUsedRegularProfile());
        historyProvider.setObserver(this);
        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile().getOriginalProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min((activityManager.getMemoryClass() / 4) * 1024 * 1024, 10 * 1024 * 1024);
        mLargeIconBridge.createCache(maxSize);
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
    public void onBindChild(EasyViewHolder holder, List<HistoryItem> list, int position, List<Object> payloads) {
        HistoryItem item = list.get(position);
        holder.getView(R.id.layout_content).setVisibility(View.VISIBLE);
        String url = item.getUrl().getSpec();
        holder.setText(R.id.tv_title, KeywordUtil.hightlight(Color.RED, item.getTitle(), keyword));
        holder.setText(R.id.tv_info, sdf.format(new Date()));
        holder.setText(R.id.tv_desc, KeywordUtil.hightlight(Color.RED, url, keyword));

        FaviconUtil.with(holder.getContext(), url)
                .setCallback(result -> holder.setImageDrawable(R.id.iv_icon, result))
                .start();
        holder.setOnItemClickListener(view -> LoadUrlEvent.post(item.getUrl().getSpec()));
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, x, y);
                    return true;
                });
    }

    @Override
    public boolean loadData() {
        historyProvider.queryHistory(keyword);
        return false;
    }

    private void showMenu(EasyViewHolder holder, HistoryItem item, float x, float y) {
        ZDialog.attach()
                .addItems("新窗口打开", "复制链接", "删除", "添加到主页")
                .setOnSelectListener((fragment, position, title) -> {
                    switch (position) {
                        case 0:
//                            BrowserEvent.post(item.getUrl(), true);
                            LoadUrlEvent.post(item.getUrl().getSpec(), true);
                            break;
                        case 1:
                            Clipboard.getInstance().setTextFromUser(item.getUrl().getSpec());
                            break;
                        case 2:
                            ZDialog.alert()
                                    .setTitle("确定删除?")
                                    .setContent("你将删除历史记录：" + item.getTitle())
                                    .setPositiveButton((fragment1, which) -> {
                                        historyProvider.markItemForRemoval(item);
                                        historyProvider.removeItems();
                                    })
                                    .show(holder.getContext());
                            break;
                        case 3:
//                            EventBus.postAddToHomepageEvent(item.getTitle(), item.getUrl());
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(holder.getContext());
    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {

        getAdapter().post(new Runnable() {
            @Override
            public void run() {
                mData.clear();
                mData.addAll(items);
                Log.d(TAG, "showContent size=" + mData.size() + " count=" + getCount());
                showContent();
            }
        });
    }

    @Override
    public void onHistoryDeleted() {
        historyProvider.queryHistory(keyword);
    }

    @Override
    public void onRemoveComplete() {
        onHistoryDeleted();
    }

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {

    }

    @Override
    public void onDestroy() {
        if (historyProvider != null) {
            historyProvider.destroy();
            historyProvider = null;
        }
    }
}

