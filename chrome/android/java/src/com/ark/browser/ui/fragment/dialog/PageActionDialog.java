package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabListManager;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.toast.ZToast;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.Clipboard;

public class PageActionDialog extends AttachListDialogFragment<String>
        implements AttachListDialogFragment.OnSelectListener<String> {

    private static final String KEY_ID = "key_id";

    private PageInfo mPageInfo;

    public static PageActionDialog newInstance(int pageId) {
        Bundle args = new Bundle();
        args.putInt(KEY_ID, pageId);
        PageActionDialog fragment = new PageActionDialog();
        fragment.setArguments(args);
        return fragment;
    }

    public PageActionDialog() {
        setOnSelectListener(this);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int pageId = Tab.INVALID_PAGE_ID;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                pageId = getArguments().getInt(KEY_ID, Tab.INVALID_PAGE_ID);
            }
        } else {
            pageId = savedInstanceState.getInt(KEY_ID, Tab.INVALID_PAGE_ID);
        }
        mPageInfo = TabListManager.getInstance().findPageInfoById(pageId);
        if (mPageInfo == null) {
            popThis();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mPageInfo != null) {
            outState.putInt(KEY_ID, mPageInfo.getId());
        }
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        addItem("新窗口中打开")
                .addItem("隐私窗口中打开")
                .addItem("悬浮窗口中打开")
                .addItem("移动至新窗口")
                .addItem("复制标题")
                .addItem("复制链接");
        addItem("网页设置");
        super.initView(view, savedInstanceState);
    }

    @Override
    public void onSelect(AttachListDialogFragment<String> fragment, int position, String text) {
        switch(position){
            case 0:
                LoadUrlEvent.post(mPageInfo, true, false);
                break;
            case 1:
                LoadUrlEvent.post(mPageInfo, true, true);
                break;
            case 2:
                ZToast.normal("TODO 悬浮打开");
                break;
            case 3:
                boolean r = TabListManager.moveToNewTab(mPageInfo);
                if (r) {
                    ZToast.success("移动页面成功！");
                } else {
                    ZToast.error("移动页面失败！");
                }
                break;
            case 4:
                Clipboard.getInstance().setTextFromUser(mPageInfo.getTitle());
                ZToast.success("标题复制成功");
                break;
            case 5:
                Clipboard.getInstance().setTextFromUser(mPageInfo.getUrl());
                ZToast.success("链接复制成功");
                break;
            case 6:
                // TODO
//                SingleWebsiteFragment.start(mPageInfo);
                break;
            default:
                break;
        }
        fragment.dismiss();
    }

}

