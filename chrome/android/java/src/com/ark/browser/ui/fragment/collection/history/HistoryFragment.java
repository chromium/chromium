package com.ark.browser.ui.fragment.collection.history;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.ui.fragment.collection.CollectionChildFragment;
import com.ark.browser.utils.FaviconUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.SelectableRecycler;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class HistoryFragment extends CollectionChildFragment
        implements HistoryProvider.BrowsingHistoryObserver,
        IEasy.OnBindViewHolderListener<HistoryItem>,
        View.OnClickListener {

    private static final String TAG = "HistoryFragment";
    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    private SelectableRecycler<HistoryItem> mRecycler;

    private TextView editBtn;
    private TextView selectAllBtn;
    private TextView deleteBtn;

//    private RoundedIconGenerator mIconGenerator;
//    private int mMinIconSize;
//    private int mDisplayedIconSize;
//    private int mCornerRadius;

    HistoryProvider historyProvider;
    private LargeIconBridge mLargeIconBridge;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        EventBus.getDefault().register(this);
        historyProvider = new BrowsingHistoryBridge(Profile.getLastUsedRegularProfile());
        historyProvider.setObserver(this);
        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile().getOriginalProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min((activityManager.getMemoryClass() / 4) * 1024 * 1024, 10 * 1024 * 1024);
        mLargeIconBridge.createCache(maxSize);

//        mCornerRadius = getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
//        mMinIconSize = (int) getResources().getDimension(R.dimen.default_favicon_min_size);
//        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
//        int textSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
//        int iconColor = ApiCompatibilityUtils.getColor(
//                getResources(), R.color.default_favicon_background_color);
//        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
//                mDisplayedIconSize / 2 ,
//                iconColor, textSize);

    }

    @Override
    public void onDestroy() {
        historyProvider.destroy();
        historyProvider = null;
        super.onDestroy();
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        mRecycler = new SelectableRecycler<HistoryItem>(mRecyclerView)
                .addItemDecoration(new ShadowItemDecoration())
                .setOnSelectChangeListener(new IEasy.OnSelectChangeListener<HistoryItem>() {
                    @Override
                    public void onSelectModeChange(boolean selectMode) {

                    }

                    @Override
                    public void onSelectChange(List<HistoryItem> list, int position, boolean isChecked) {
                        setBottomBarState(mRecycler.getSelectedCount());
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
                .onItemClick((holder, view1, data) -> {
                    LoadUrlEvent.post(data.getUrl().getSpec());
                })
                .build();
        mRecycler.showLoading();
//        historyProvider.queryHistory("");
    }

    @Override
    public void onLazyInitView(@Nullable Bundle savedInstanceState) {
        super.onLazyInitView(savedInstanceState);
        historyProvider.queryHistory("");
    }

//    @Subscribe
//    public void onRefreshEvent(RefreshEvent event) {
//        historyProvider.queryHistory("");
//    }

    @Override
    public boolean onBackPressedSupport() {
        if ("完成".equals(editBtn.getText().toString())) {
            editBtn.performClick();
            return true;
        }
        return super.onBackPressedSupport();
    }

//    @Override
//    protected void onRefreshEvent() {
//        historyProvider.queryHistory("");
//    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        postOnEnterAnimationEnd(new Runnable() {
            @Override
            public void run() {
                mRecycler.setItems(items);
                mRecycler.clearSelectedPosition();
                mRecycler.notifyDataSetChanged();
            }
        });
    }

    @Override
    public void onHistoryDeleted() {
        Log.d(TAG, "onHistoryDeleted");
        historyProvider.queryHistory("");
    }

    @Override
    public void onRemoveComplete() {
        historyProvider.queryHistory("");
    }

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {

    }

    @Override
    public void onClick(View v) {
        if (v == editBtn) {
            if (mRecycler.isEmpty()) {
                return;
            }
            String text = editBtn.getText().toString();
            if ("编辑".equals(text)) {
                setBottomBarState(mRecycler.getSelectedCount());
                editBtn.setText("完成");
                selectAllBtn.setVisibility(View.VISIBLE);
                deleteBtn.setVisibility(View.VISIBLE);
                mRecycler.enterSelectMode();
                selectAllBtn.setText("全选");
            } else {
                editBtn.setText("编辑");
                selectAllBtn.setVisibility(View.INVISIBLE);
                deleteBtn.setVisibility(View.INVISIBLE);
                mRecycler.exitSelectMode();
            }
        } else if (v == selectAllBtn) {
            String text = selectAllBtn.getText().toString();
            if (text.equals("全选")) {
                selectAllBtn.setText("全不选");
                mRecycler.selectAll();
            } else {
                selectAllBtn.setText("全选");
                mRecycler.unSelectAll();
            }
            setBottomBarState(mRecycler.getSelectedCount());
        } else if (v == deleteBtn) {
            ZDialog.alert()
                    .setTitle("确定删除？")
                    .setContent("你将删除" + mRecycler.getSelectedCount() + "个书签")
                    .setPositiveButton((fragment, which) -> {
                        for (HistoryItem item : mRecycler.getSelectedItem()) {
                            historyProvider.markItemForRemoval(item);
                        }
                        historyProvider.removeItems();
                        mRecycler.removeItems(mRecycler.getSelectedItem());
                        mRecycler.notifyDataSetChanged();
                    })
                    .show(context);
        }
    }

    public void setBottomBarState(int selectCount) {
        if (selectCount != 0) {
            deleteBtn.setTextColor(ApiCompatibilityUtils.getColor(getResources(), R.color.light_red));
            deleteBtn.setClickable(true);
            deleteBtn.setText(getResources().getString(R.string.text_delete_with_count, selectCount));
        } else {
            int color = ApiCompatibilityUtils.getColor(getResources(), R.color.google_grey_400);
            deleteBtn.setTextColor(color);
            deleteBtn.setClickable(false);
            deleteBtn.setText(getResources().getString(R.string.text_delete));
        }
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<HistoryItem> list, int position, List<Object> payloads) {
        HistoryItem item = list.get(position);
        holder.getView(R.id.layout_content).setVisibility(View.VISIBLE);
        String url = item.getUrl().getSpec();
        holder.getTextView(R.id.tv_title).setText(item.getTitle());
        holder.getTextView(R.id.tv_info).setText(sdf.format(new Date()));
        holder.getTextView(R.id.tv_desc).setText(url);

        FaviconUtil.with(getContext(), url)
                .setCallback(result -> holder.setImageDrawable(R.id.iv_icon, result))
                .start();
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, x, y);
                    return true;
                });
    }

    private void showMenu(EasyViewHolder holder, HistoryItem item, float x, float y) {
        new AttachListDialogFragment<String>()
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
                                    .show(context);
                            break;
                        case 3:
                            ZToast.warning("TODO 添加到主页");
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(context);
    }

    @Override
    public View onCreateBottomBar(Context context) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_collection_bottom_bar, null, false);
        selectAllBtn = view.findViewById(R.id.btn_select_all);
        deleteBtn = view.findViewById(R.id.btn_delete);
        editBtn = view.findViewById(R.id.btn_edit);
        selectAllBtn.setOnClickListener(this);
        deleteBtn.setOnClickListener(this);
        editBtn.setOnClickListener(this);
        return view;
    }

    @Override
    public void onShowMenu(View view) {
        ZDialog.attach()
                .addItems("清空")
                .setOnSelectListener((fragment, position, text) -> {
                    ZToast.normal(text);
                    switch (position) {
                        case 0:
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
}

