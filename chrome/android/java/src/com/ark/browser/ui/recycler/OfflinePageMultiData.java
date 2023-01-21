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
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.ui.base.Clipboard;

import java.text.SimpleDateFormat;
import java.util.List;
import java.util.Locale;

public class OfflinePageMultiData extends BaseHeaderMultiData<OfflinePageItem>
        implements Callback<List<OfflinePageItem>> {

    private static final String TAG = "BookmarkMultiData";

    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    private OfflinePageBridge offlinePageBridge;
    private LargeIconBridge mLargeIconBridge;

    private String keyword;

    public OfflinePageMultiData(String keyword) {
        super("离线网页");
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    public OfflinePageMultiData(String keyword, List<OfflinePageItem> list) {
        super("离线网页", list);
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    private void init() {
        offlinePageBridge = OfflinePageBridge.getForProfile(Profile.getLastUsedRegularProfile());
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
    public void onBindChild(EasyViewHolder holder, List<OfflinePageItem> list, int position, List<Object> payloads) {
        OfflinePageItem item = list.get(position);
        holder.getView(R.id.layout_content).setVisibility(View.VISIBLE);
        String url = item.getUrl();
        holder.setText(R.id.tv_title, KeywordUtil.hightlight(Color.RED, item.getTitle(), keyword));
        holder.setText(R.id.tv_info, sdf.format(item.getCreationTimeMs()));
        holder.setText(R.id.tv_desc, KeywordUtil.hightlight(Color.RED, item.getUrl(), keyword));

        FaviconUtil.with(holder.getContext(), url)
                .setCallback(result -> holder.setImageDrawable(R.id.iv_icon, result))
                .start();

        holder.setOnItemClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                        url,
                        item.getOfflineId(),
                        LaunchLocation.DOWNLOAD_HOME,
                        (params) -> {
                            if (params == null) return;
                            LoadUrlEvent.post(params, true);
                        },
                        Profile.getLastUsedRegularProfile()
                );
            }
        });
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, x, y);
                    return true;
                });
    }

    @Override
    public boolean loadData() {
        offlinePageBridge.getAllPages(this);
        return false;
    }

    private void showMenu(EasyViewHolder holder, OfflinePageItem item, float x, float y) {
        ZDialog.attach()
                .addItems("新窗口打开", "复制链接", "删除", "添加到主页")
                .setOnSelectListener((fragment, position, text) -> {
                    switch (position) {
                        case 0:
                            LoadUrlEvent.post(item.getUrl(), true);
                            break;
                        case 1:
                            Clipboard.getInstance().setTextFromUser(item.getUrl());
                            break;
                        case 2:
                            ZDialog.alert()
                                    .setTitle("确定删除?")
                                    .setContent("你将删除历史记录：" + item.getTitle())
                                    .setPositiveButton((fragment1, which) -> {
                                        offlinePageBridge.deletePage(item.getClientId(), new Callback<Integer>() {
                                            @Override
                                            public void onResult(Integer result) {
                                                offlinePageBridge.getAllPages(OfflinePageMultiData.this);
                                            }
                                        });
                                    })
                                    .show(holder.getContext());
                            break;
                        case 3:
                            ZToast.normal(text);
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(holder.getContext());
    }

    @Override
    public void onResult(List<OfflinePageItem> result) {
        getAdapter().post(() -> {
            String key = keyword.toLowerCase();
            mData.clear();
            for (OfflinePageItem item : result) {
                if (item.getTitle().toLowerCase().contains(key)
                        || item.getUrl().toLowerCase().contains(key)) {
                    mData.add(item);
                }
            }
            Log.d(TAG, "showContent size=" + mData.size() + " count=" + getCount());
            showContent();
        });
    }
}

