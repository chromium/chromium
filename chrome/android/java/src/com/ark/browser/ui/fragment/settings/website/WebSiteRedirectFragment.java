package com.ark.browser.ui.fragment.settings.website;

import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.dialog.SiteRedirectEditorDialog;
import com.ark.browser.utils.FaviconUtil;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.statemanager.State;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.PrefsHelper;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class WebSiteRedirectFragment extends BaseSwipeBackFragment
        implements IEasy.OnBindViewHolderListener<WebSiteRedirectFragment.SiteRedirectItem> {

    private EasyRecycler<SiteRedirectItem> mRecycler;

    public static class SiteRedirectItem {

        private int id;

        private String targetUrl;

        private String redirectUrl;

        public int getId() {
            return id;
        }

        public void setId(int id) {
            this.id = id;
        }

        public String getTargetUrl() {
            return targetUrl;
        }

        public void setTargetUrl(String url) {
            this.targetUrl = url;
        }

        public String getRedirectUrl() {
            return redirectUrl;
        }

        public void setRedirectUrl(String redirectUrl) {
            this.redirectUrl = redirectUrl;
        }
    }

    public static WebSiteRedirectFragment newInstance() {

        Bundle args = new Bundle();

        WebSiteRedirectFragment fragment = new WebSiteRedirectFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_site_redirect;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("链接重定向");
        RecyclerView recyclerView = view.findViewById(R.id.recycler_view);
        mRecycler = new EasyRecycler<>(recyclerView);
        mRecycler.setItemRes(R.layout.item_website)
                .onBindViewHolder(this)
                .build();
        mRecycler.showLoading();


        ButtonCompat btnAddRedirect = findViewById(R.id.btn_add_redirect);
        btnAddRedirect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                SiteRedirectEditorDialog.newInstance()
                        .setCallback((target, url) -> {
                            SiteRedirectItem item = new SiteRedirectItem();
                            item.setTargetUrl(target);
                            item.setRedirectUrl(url);

                            PrefsHelper.with("site_redirect_manager").applyString(target, url);

                            mRecycler.addItem(item);
                            if (mRecycler.getState() != State.STATE_CONTENT) {
                                mRecycler.showContent();
                            } else {
                                mRecycler.notifyItemInserted(mRecycler.getItemCount() - 1);
                            }
                        })
                        .show(context);
            }
        });

        getAllData();
    }

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        darkStatusBar();
    }

    private void getAllData() {
        ThreadPool.execute(() -> {

            Map<String, ?> map = PrefsHelper.with("site_redirect_manager").getAll();

            if (map == null || map.isEmpty()) {
                ThreadPool.runOnUIThread(() -> mRecycler.showEmpty());
                return;
            }

            List<SiteRedirectItem> items = new ArrayList<>();
            for (Map.Entry<String, ?> entry : map.entrySet()) {
                SiteRedirectItem item = new SiteRedirectItem();
                item.setTargetUrl(entry.getKey());
                item.setRedirectUrl(String.valueOf(entry.getValue()));
                items.add(item);
            }

            postOnEnterAnimationEnd(() -> {
                mRecycler.setItems(items);
                mRecycler.showContent();
            });
        });
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<SiteRedirectItem> list, int position, List<Object> payloads) {
        SiteRedirectItem item = list.get(position);
        ImageView favicon = holder.getView(R.id.iv_favicon);
        holder.setText(R.id.tv_title, item.getTargetUrl());
        holder.setText(R.id.tv_desc, item.getRedirectUrl());
        FaviconUtil.with(getContext(), item.getTargetUrl())
                .setCallback(favicon::setImageDrawable)
                .start();

        ClickHelper.with(holder.getItemView())
                .setOnClickListener((v, x, y) -> showEditDialog(item, holder.getLayoutPosition()))
                .setOnLongClickListener((v, x, y) -> {
                    ZDialog.attach()
                            .addItems("编辑", "删除")
                            .setOnSelectListener((fragment, pos, text) -> {
                                fragment.dismiss();
                                if (pos == 0) {
                                    showEditDialog(item, holder.getLayoutPosition());
                                } else {
                                    ZDialog.alert()
                                            .setTitle("确定删除？")
                                            .setContent("你将删除链接重定向配置：" + item.getTargetUrl())
                                            .setPositiveButton((f, which) -> {
                                                mRecycler.removeItem(item);
                                                mRecycler.notifyItemRemoved(holder.getLayoutPosition());

                                                PrefsHelper.with("site_redirect_manager")
                                                        .applyRemove(item.getTargetUrl());

                                                if (mRecycler.isEmpty()) {
                                                    mRecycler.showEmpty();
                                                }
                                            })
                                            .show(context);
                                }
                            })
                            .setTouchPoint(x, y)
                            .show(context);
                    return true;
                });
    }

    private void showEditDialog(SiteRedirectItem item, int pos) {
        SiteRedirectEditorDialog.newInstance(item)
                .setCallback((target, url) -> {
                    item.setTargetUrl(target);
                    item.setRedirectUrl(url);

                    PrefsHelper.with("site_redirect_manager").applyString(target, url);

                    mRecycler.notifyItemChanged(pos);
                })
                .show(context);
    }

}
