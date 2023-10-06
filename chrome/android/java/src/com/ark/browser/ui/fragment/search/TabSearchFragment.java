package com.ark.browser.ui.fragment.search;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.collection.ArraySet;

import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.core.GroupTab;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.dialog.GroupTabPickerDialog;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.MultiRecycler;
import com.zpj.recyclerview.SingleTypeMultiData;
import com.zpj.recyclerview.layouter.GridLayouter;
import com.zpj.recyclerview.manager.MultiLayoutManager;
import com.zpj.skin.SkinEngine;
import com.zpj.statemanager.State;
import com.zpj.widget.toolbar.ZSearchBar;

import org.chromium.base.Callback;
import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class TabSearchFragment extends BaseSwipeBackFragment {

    private final TabListMultiData tabMultiData = new TabListMultiData() {

        @Override
        public void onClick(EasyViewHolder holder, View view, ITab tab) {
            if (mIsSelectedMode) {
                if (mSelectedSet.contains(tab)) {
                    mSelectedSet.remove(tab);
                } else {
                    mSelectedSet.add(tab);
                }
                tabMultiData.notifyItemChanged(holder.getRealPosition());
            } else if (tab instanceof ITabGroup) {
                mGroup = (ITabGroup) tab;
                search(searchBar.getEditor().getText().toString());
            }
        }

        @Override
        public boolean onLongClick(EasyViewHolder holder, View view, ITab data) {
            if (mIsSelectedMode) {
                return false;
            }
            enterSelectMode();
            tabMultiData.mSelectedSet.add(data);
            tabMultiData.notifyItemChanged(holder.getRealPosition());
            return true;
        }
    };

    private MultiRecycler mRecycler;

    private ZSearchBar searchBar;
    private View mBottomBar;
    private TextView mTvDesc;

    protected int mFilterIndex = 0;

    protected ITabGroup mGroup = TabGroupManager.global().getTabGroup(false);

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_tab_search;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        mTvDesc = findViewById(R.id.tv_desc);
        mBottomBar = findViewById(R.id.bottom_bar);
        searchBar = findViewById(R.id.search_bar);

        searchBar.setOnLeftButtonClickListener(v -> pop());

        searchBar.addTextWatcher(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                search(s.toString());
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });

        findViewById(R.id.btn_move).setOnClickListener(v -> {
            if (tabMultiData.mSelectedSet.isEmpty()) {
                return;
            }
            GroupTabPickerDialog dialog = GroupTabPickerDialog.newInstance(new ArrayList<>(tabMultiData.mSelectedSet));
            dialog.setOnDismissListener(groupTabPickerDialog -> search());
            start(dialog);
        });

        findViewById(R.id.btn_group).setOnClickListener(v -> {
            if (tabMultiData.mSelectedSet.isEmpty()) {
                return;
            }
            ZDialog.alert()
                    .setTitle("标签页成组")
                    .setContent("确定将选中的" + tabMultiData.mSelectedSet.size() + "个标签页组成群组？")
                    .setPositiveButton((fragment, which) -> {
                        ITabGroup currentGroup = TabGroupManager.global().getTabGroup(false);
                        ITabGroup newGroup = new GroupTab(TabGroupManager.global(),
                                null, TabInfo.create(-1, true));
                        // add new group
                        currentGroup.moveToNewGroup(newGroup, false);
                        for (ITab tab : tabMultiData.mSelectedSet) {
                            newGroup.moveToNewGroup(tab, false);
                        }
                        search();
                    })
                    .show(context);
        });

        findViewById(R.id.btn_remove).setOnClickListener(v -> {
            if (tabMultiData.mSelectedSet.isEmpty()) {
                return;
            }
            ZDialog.alert()
                    .setTitle("删除标签页")
                    .setContent("确定删除选中的" + tabMultiData.mSelectedSet.size() + "个标签页？")
                    .setPositiveButton((fragment, which) -> {
                        for (ITab tab : tabMultiData.mSelectedSet) {
                            tab.getParentGroup().closeTab(tab);
                        }
                        search();
                    })
                    .show(context);
        });

        mRecycler = new MultiRecycler(findViewById(R.id.recycler_view))
                .addItem(tabMultiData)
                .setLayoutManager(new MultiLayoutManager())
                .build();
        mRecycler.showEmpty();

        post(() -> showSoftInput(searchBar.getEditor()));

        search(null);
    }

    @Override
    public boolean onBackPressedSupport() {
        if (tabMultiData.mIsSelectedMode) {
            exitSelectMode();
            return true;
        }
        if (mGroup.getParentGroup() != null) {
            mGroup = mGroup.getParentGroup();
            search(searchBar.getEditor().getText().toString());
            return true;
        }
        return super.onBackPressedSupport();
    }

    @Override
    public void onDestroyView() {
        hideSoftInput();
        super.onDestroyView();
    }

    private void enterSelectMode() {
        tabMultiData.mIsSelectedMode = true;
        tabMultiData.mSelectedSet.clear();
        mBottomBar.setVisibility(View.VISIBLE);
    }

    private void exitSelectMode() {
        tabMultiData.mIsSelectedMode = false;
        tabMultiData.mSelectedSet.clear();
        mBottomBar.setVisibility(View.GONE);
        mRecycler.notifyDataSetChanged();
    }

    private void search() {
        search(searchBar.getEditor().getText().toString());
    }

    private void search(String keyword) {
        mRecycler.showContent();
        tabMultiData.setData(searchTabs(keyword));

        if (TextUtils.isEmpty(mGroup.getTabInfo().getTitle())) {
            mTvDesc.setText("共" + tabMultiData.getCount() + "个标签页");
        } else {
            mTvDesc.setText(mGroup.getTitle() + "（" + tabMultiData.getCount() + "个标签页）");
        }
    }

    public List<ITab> searchTabs(String keyword) {
        List<ITab> tabList = new ArrayList<>();
        if (TextUtils.isEmpty(keyword)) {
            tabList.addAll(mGroup.getTabList());
        } else {
            keyword = keyword.toLowerCase();
            for (ITab tab : mGroup.getTabList()) {
                PageInfo info = tab.getCurrentPageInfo();
                if (info != null) {
                    if (info.getTitle().toLowerCase().contains(keyword)
                            || info.getUrl().toLowerCase().contains(keyword)) {
                        tabList.add(tab);
                    }
                }
            }
        }

        Collections.sort(tabList, new Comparator<ITab>() {

            @Override
            public int compare(ITab t0, ITab t1) {
                boolean isGroup0 = t0.getTabInfo().isGroup();
                boolean isGroup1 = t1.getTabInfo().isGroup();
                if (isGroup0 == isGroup1) {
                    return Long.compare(t1.getTabInfo().getCreateTime(), t0.getTabInfo().getCreateTime());
                }
                if (isGroup0) {
                    return -1;
                } else {
                    return 1;
                }

            }
        });
        return tabList;
    }

    private static class TabListMultiData extends SingleTypeMultiData<ITab> {

        protected final ArraySet<ITab> mSelectedSet = new ArraySet<>();
        protected boolean mIsSelectedMode;

        public TabListMultiData() {
            super(new GridLayouter(3));
        }

        public void setData(List<ITab> dataSet) {
            ThreadPool.runOnUIThread(new Runnable() {
                @Override
                public void run() {
                    mData.clear();
                    mData.addAll(dataSet);
                    if (mData.isEmpty()) {
                        setState(State.STATE_EMPTY);
                    } else {
                        setState(State.STATE_CONTENT);
                    }
                    notifyDataSetChange();
                }
            });
        }

        @Override
        public int getLayoutId() {
            return R.layout.item_tab_card_search;
        }

        @Override
        public void onBindViewHolder(EasyViewHolder holder, List<ITab> list, int position, List<Object> payloads) {
            ITab tab = list.get(position);
            TextView tvTitle = holder.getView(R.id.tv_title);
            tvTitle.setText(tab.getTitle());

            CardView cardView = holder.getView(R.id.card_view);
            FitWidthImageView ivThumbnail = holder.getView(R.id.iv_thumbnail);
            PageInfo pageInfo = tab.getCurrentPageInfo();
            if (!(tab instanceof ITabGroup) && pageInfo != null) {
                int theme = pageInfo.getThemeColor();
                cardView.setCardBackgroundColor(theme == 0 ? getDefaultThemeColor() : theme);
//                PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);
                Object tabIdTag = holder.getTag(R.id.key_tab_id);
                if (!(tabIdTag instanceof Integer) || (int) tabIdTag != tab.getId()) {
                    ivThumbnail.setImageBitmap(null);
                }
            } else {
                cardView.setCardBackgroundColor(getDefaultThemeColor());
            }

            if (mSelectedSet.contains(tab)) {
                tvTitle.setBackgroundColor(SkinEngine.getColor(holder.getContext(), R.attr.colorPrimary));
            } else {
                tvTitle.setBackgroundColor(SkinEngine.getColor(holder.getContext(), R.attr.backgroundColorAccent));
            }

            holder.setOnItemClickListener(v -> TabListMultiData.this.onClick(holder, v, tab));
            holder.setOnItemLongClickListener(view -> TabListMultiData.this.onLongClick(holder, view, tab));

            TabGroupManager.global().getTabContentManager()
                    .getTabThumbnailWithCallback(tab, null, new Callback<Bitmap>() {
                        @Override
                        public void onResult(Bitmap result) {
                            if (result == null && pageInfo != null) {
                                PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);
                            } else {
                                ivThumbnail.setImageBitmap(result);
                            }
                        }
                    }, false, false, false);

            holder.setTag(R.id.key_tab_id, tab.getId());
            holder.setTag(R.id.key_tab_position, position);
        }

        @Override
        public boolean onLongClick(EasyViewHolder holder, View view, ITab data) {
            return super.onLongClick(holder, view, data);
        }

        @Override
        public void onClick(EasyViewHolder holder, View view, ITab data) {
            super.onClick(holder, view, data);
        }

        @Override
        public boolean loadData() {
            return false;
        }

        public int getDefaultThemeColor() {
            return AppConfig.isNightMode() ? Color.BLACK : Color.WHITE;
        }
    }

}

