package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.settings.Keys;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.settings.website.SingleWebsiteFragment;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.toast.ZToast;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.Clipboard;

public class TabActionDialog extends AttachListDialogFragment<String>
        implements AttachListDialogFragment.OnSelectListener<String> {

    private ITab mTab;

    public static TabActionDialog newInstance(ITab tab, float x, float y) {
        TabActionDialog dialog = newInstance(tab.getId());
        dialog.setTouchPoint(x, y);
        return dialog;
    }

    public static TabActionDialog newInstance(int id) {
        Bundle args = new Bundle();
        args.putInt(Keys.KEY_ID, id);
        TabActionDialog fragment = new TabActionDialog();
        fragment.setArguments(args);
        return fragment;
    }

    public TabActionDialog() {
        setOnSelectListener(this);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int id = Tab.INVALID_PAGE_ID;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                id = getArguments().getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
            }
        } else {
            id = savedInstanceState.getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
        }
        mTab = TabListManager.getInstance().getTabById(id);
        if (mTab == null) {
            popThis();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mTab != null) {
            outState.putInt(Keys.KEY_ID, mTab.getId());
        }
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        addItem("新窗口中打开")
                .addItem("隐私窗口中打开")
                .addItem("移动至新窗口")
                .addItem("复制标题")
                .addItem("复制链接")
                .addItem("克隆标签")
                .addItem("悬浮模式")
                .addItem(mTab.getTabInfo().isLocked() ? "解锁标签" : "锁定标签")
                .addItem("历史栈");
//        if (tabInfo.getCurrentPage().isFrozen()) {
//            addItem("网页设置");
//        } else {
//            addItem("网页信息");
//        }
        addItem("网页设置");
        super.initView(view, savedInstanceState);
    }

    @Override
    public void onSelect(AttachListDialogFragment<String> fragment, int position, String text) {
        switch(position){
            case 0:
                LoadUrlEvent.post(mTab.getCurrentPageInfo(), true, false);
                break;
            case 1:
                LoadUrlEvent.post(mTab.getCurrentPageInfo(), true, true);
                break;
            case 2:
                boolean r = TabListManager.moveToNewTab(mTab.getCurrentPageInfo());
                if (r) {
                    ZToast.success("移动页面成功！");
                } else {
                    ZToast.error("移动页面失败！");
                }
                break;
            case 3:
                Clipboard.getInstance().setTextFromUser(mTab.getCurrentPageInfo().getTitle());
                ZToast.success("标题复制成功");
                break;
            case 4:
                Clipboard.getInstance().setTextFromUser(mTab.getCurrentPageInfo().getUrl());
                ZToast.success("链接复制成功");
                break;
            case 5:
                ITab cloneTab = TabListManager.getInstance().cloneTab(mTab);
                if (cloneTab == null) {
                    ZToast.error("克隆标签失败！");
                } else {
                    ZToast.success("TODO 克隆标签成功！");
                }
                break;
            case 6:
                ZToast.normal("TODO 悬浮模式");
//                GetActivityEvent.post(new Callback<ChromeActivity>() {
//                    @Override
//                    public void onResult(ChromeActivity result) {
//                        new FloatingTab(result, mTab).show();
//                    }
//                });
                break;
            case 7:
                ZToast.normal("锁定标签");
                mTab.getTabInfo().setLocked(!mTab.getTabInfo().isLocked());
                break;
            case 8:
                HistoryStackDialogFragment.newInstance(mTab.getId()).show(context);
                break;
            case 9:
                start(SingleWebsiteFragment.newInstance(mTab.getCurrentPageInfo()));
                break;
            default:
                break;
        }
        fragment.dismiss();
    }

}

