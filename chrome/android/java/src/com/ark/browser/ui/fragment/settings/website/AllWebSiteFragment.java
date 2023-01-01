package com.ark.browser.ui.fragment.settings.website;

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.FaviconUtil;
import com.ark.browser.utils.ThreadPool;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

public class AllWebSiteFragment extends BaseSwipeBackFragment
        implements IEasy.OnBindViewHolderListener<Website> {

    private EasyRecycler<Website> mRecycler;

    public static AllWebSiteFragment newInstance() {

        Bundle args = new Bundle();

        AllWebSiteFragment fragment = new AllWebSiteFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_site_all;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle(R.string.all_sites);
        RecyclerView recyclerView = view.findViewById(R.id.recycler_view);
        mRecycler = new EasyRecycler<>(recyclerView);
        mRecycler.setItemRes(R.layout.item_website)
                .onBindViewHolder(this)
                .build();
        mRecycler.showLoading();
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(Profile.getLastUsedRegularProfile(), false);
        fetcher.fetchAllPreferences(new WebsitePermissionsFetcher.WebsitePermissionsCallback() {
            @Override
            public void onWebsitePermissionsAvailable(Collection<Website> sites) {
                ThreadPool.execute(() -> {
                    Collections.sort(new ArrayList<>(sites), Website::compareByStorageTo);
                    postOnEnterAnimationEnd(() -> {
                        mRecycler.setItems(sites);
                        mRecycler.showContent();
                    });
                });
            }
        });
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<Website> list, int position, List<Object> payloads) {
        Website website = list.get(position);
        ImageView favicon = holder.getView(R.id.iv_favicon);
        holder.setText(R.id.tv_title, website.getTitle());
        TextView content = holder.getView(R.id.tv_desc);
        if (TextUtils.isEmpty(website.getSummary())) {
            content.setVisibility(View.GONE);
        } else {
            content.setText(getString(R.string.website_settings_embedded_in, website.getSummary()));
            content.setVisibility(View.VISIBLE);
        }
        FaviconUtil.with(getContext(), website.getTitle())
                .setCallback(new Callback<Drawable>() {
                    @Override
                    public void onResult(Drawable result) {
                        favicon.setImageDrawable(result);
                    }
                })
                .start();
        holder.setOnItemClickListener(v -> start(SingleWebsiteFragment.newInstance(website)));
    }
}
