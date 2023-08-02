package com.ark.browser.ui.fragment.dialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.graphics.Rect;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.settings.Keys;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.core.GroupTab;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.utils.ArkLogger;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.fragmentation.dialog.impl.InputDialogFragment;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.MultiData;
import com.zpj.recyclerview.MultiRecycler;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

public class GroupTabPickerDialog extends OverDragBottomDialogFragment<GroupTabPickerDialog> {

    private static final String TAG = "BookmarkFolderSelectFragment";

    private MultiRecycler mRecycler;

    private ITab mTab;

    public static GroupTabPickerDialog newInstance(int tabId) {
        Bundle args = new Bundle();
        args.putInt(Keys.KEY_ID, tabId);
        GroupTabPickerDialog fragment = new GroupTabPickerDialog();
        fragment.setArguments(args);
        return fragment;
    }

    public GroupTabPickerDialog() {
        setMaxHeight(MATCH_PARENT);
        setMarginTop(ScreenUtils.getStatusBarHeight() * 2);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int tabId = Tab.INVALID_PAGE_ID;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                tabId = getArguments().getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
            }
        } else {
            tabId = savedInstanceState.getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
        }
        mTab = TabGroupManager.findTabById(tabId);
        if (mTab == null) {
            popThis();
        }
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
                        if (tag instanceof ITabGroup) {
                            ITabGroup group = (ITabGroup) tag;
                            int index = 0;
                            ITabGroup p = group.getParentTab();
                            while (p != null) {
                                index++;
                                p = p.getParentTab();
                            }
                            outRect.set(index * ScreenUtils.dp2pxInt(24), 0, 0, 0);
                        } else {
                            super.getItemOffsets(outRect, view, parent, state);
                        }
                    }
                })
                .build();
        mRecycler.showLoading();

        updateFolderList();
    }

    private void updateFolderList() {
        ITabGroup rootGroup = mTab.getRootGroupTab();
        TreeMultiData root = new TreeMultiData(null, rootGroup);
        getBookmarkFolderTree(root, 0);
        mRecycler.setItems(root);
        postOnEnterAnimationEnd(() -> mRecycler.showContent());
    }

    private void getBookmarkFolderTree(TreeMultiData parent, int index) {
        ITabGroup group = parent.getTabGroup();
        for (ITab child : group.getTabList()) {
            if (child instanceof ITabGroup) {
                if (mTab instanceof ITabGroup && mTab.getId() == child.getId()) {
                    continue;
                }
                TreeMultiData childNode = new TreeMultiData(parent, (ITabGroup) child);
                childNode.setIndex(index + 1);
                parent.getData().add(childNode);
                getBookmarkFolderTree(childNode, index + 1);
            }
        }
    }

    private class TreeMultiData extends MultiData<TreeMultiData> {

        private final TreeMultiData parent;
        private final ITabGroup tabGroup;

        private int i;

        public TreeMultiData(TreeMultiData parent, ITabGroup tabGroup) {
            this(parent, new ArrayList<>(), tabGroup);
        }

        public TreeMultiData(TreeMultiData parent, List<TreeMultiData> data, ITabGroup tabGroup) {
            super(data);
            this.parent = parent;
            this.tabGroup = tabGroup;
        }

        public void setIndex(int i) {
            this.i = i;
        }

        public int getIndex() {
            return i;
        }

        public ITabGroup getTabGroup() {
            return tabGroup;
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
            holder.getItemView().setTag(tabGroup);
            TextView textView = holder.getView(R.id.tv_title);
            textView.setText(tabGroup.getTitle());
            textView.setTextColor(mTab.getParentTab() == tabGroup ?
                    context.getResources().getColor(R.color.colorPrimary)
                    : SkinEngine.getColor(context, R.attr.textColorMajor));
            ClickHelper.with(holder.getItemView())
                    .setOnClickListener(new ClickHelper.OnClickListener() {
                        @Override
                        public void onClick(View v, float x, float y) {
                            GroupTabPickerDialog.TreeMultiData.this.onClick(v, x, y);
                        }
                    })
                    .setOnLongClickListener(new ClickHelper.OnLongClickListener() {
                        @Override
                        public boolean onLongClick(View v, float x, float y) {
                            return GroupTabPickerDialog.TreeMultiData.this.onLongClick(v, position, x, y);
                        }
                    });
        }

        public void onClick(View v, float x, float y) {
            ArkLogger.e(this, "moveToNewGroup group id=" + tabGroup.getId());
            if (tabGroup.getId() == ITab.INVALID_TAB_INDEX) {
                ITabGroup newGroup = new GroupTab(tabGroup.getParentTab());
                if (tabGroup.moveToNewGroup(newGroup)) {
                    if (newGroup.moveToNewGroup(mTab)) {
                        ArkLogger.e(this, "moveToNewGroup move tab success!");
                    } else {
                        ArkLogger.e(this, "moveToNewGroup move tab failed! info=" + mTab.getTabInfo());
                    }
                } else {
                    ArkLogger.e(this, "moveToNewGroup create new group failed! info=" + mTab.getTabInfo());
                }
            } else {
                if (tabGroup.moveToNewGroup(mTab)) {
                    ArkLogger.e(this, "moveToNewGroup move tab success!");
                } else {
                    ArkLogger.e(this, "moveToNewGroup move tab failed! info=" + mTab.getTabInfo());
                }
            }
            pop();
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
                                            String text = ((InputDialogFragment) fragment).getText();
                                            TabInfo tabInfo = TabInfo.create(ITab.INVALID_TAB_INDEX,
                                                    tabGroup.getId(), true);
                                            tabInfo.setTitle(text);
                                            ITabGroup newGroup = new GroupTab(tabGroup, tabInfo);
                                            getData().add(0, new TreeMultiData(parent, newGroup));
                                            mRecycler.notifyItemInserted(position + 1);
                                        })
                                        .show(context);
                                break;
                            case 1:
                                ZDialog.input()
                                        .setAutoShowKeyboard(true)
                                        .setHint("请输入文件夹名")
                                        .setEditText(tabGroup.getTitle())
                                        .setEmptyable(false)
                                        .setTitle("重命名")
                                        .setPositiveButton((fragment, which) -> {
                                            String text = ((InputDialogFragment) fragment).getText();
                                            tabGroup.getTabInfo().setTitle(text);
                                            TextView textView = v.findViewById(R.id.tv_title);
                                            textView.setText(text);
                                        })
                                        .show(context);
                                break;
                            case 2:
                                ZDialog.alert()
                                        .setTitle("确定删除?")
                                        .setContent("你将删除文件夹：" + tabGroup.getTitle())
                                        .setPositiveButton((fragment, which) -> {
                                            parent.getData().remove(TreeMultiData.this);

                                            mRecycler.getAdapter().notifyItemRangeRemoved(position, getCount());

                                            tabGroup.getParentTab().closeTab(tabGroup);
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

