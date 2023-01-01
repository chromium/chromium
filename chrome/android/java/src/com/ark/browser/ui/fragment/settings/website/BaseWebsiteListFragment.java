package com.ark.browser.ui.fragment.settings.website;

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.FaviconUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.skin.SkinEngine;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Objects;

public abstract class BaseWebsiteListFragment extends BaseSwipeBackFragment
        implements IEasy.OnBindViewHolderListener<Website> {

    protected EasyRecycler<Website> mRecycler;
    protected LinearLayout llContainer;

    @Override
    protected final int getLayoutId() {
        return R.layout.fragment_setting_site_base;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        llContainer = view.findViewById(R.id.ll_container);
        initRecyclerView(view);
        initView(view);
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<Website> list, int position, List<Object> payloads) {
        Website website = list.get(position);
        ImageView favicon = holder.getView(R.id.iv_favicon);
        TextView title = holder.getView(R.id.tv_title);
        TextView content = holder.getView(R.id.tv_desc);
        title.setText(website.getTitle());
        Integer contentSetting = getContentSetting(website);
        if (contentSetting == null) {
            if (getContentStr(website) == null) {
                content.setVisibility(View.GONE);
            } else {
                content.setVisibility(View.VISIBLE);
                content.setText(getContentStr(website));
            }
        } else {
            content.setVisibility(View.VISIBLE);
            content.setText(getPermissionString(contentSetting));
        }
        FaviconUtil.with(getContext(), website.getTitle())
                .setCallback(new Callback<Drawable>() {
                    @Override
                    public void onResult(Drawable result) {
                        favicon.setImageDrawable(result);
                    }
                })
                .start();
        holder.setOnItemClickListener(v -> {
            if (getContentSetting(list.get(position)) == null) {
                onItemViewClick(v, list.get(position));
                return;
            }
            showSelectDialog(content, list.get(position));
        });
    }

    protected void addItem(View view) {
        ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        llContainer.addView(view, llContainer.getChildCount() - 1, params);
    }

    protected CommonSettingItem addItem(String title, OnCommonItemClickListener listener) {
        CommonSettingItem item = new CommonSettingItem(context);
        item.setTitleText(title);
        item.setOnItemClickListener(listener);
        SkinEngine.applyViewAttr(item, "z_setting_titleTextColor", R.attr.textColorMajor);
        SkinEngine.applyViewAttr(item, "z_setting_rightTextColor", R.attr.textColorNormal);
        SkinEngine.applyViewAttr(item, "z_setting_infoTextColor", R.attr.textColorMinor);
        ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        llContainer.addView(item, llContainer.getChildCount() - 1, params);
        return item;
    }

    protected SwitchSettingItem addSwitchItem(String title, boolean checked, OnCheckableItemClickListener listener) {
        SwitchSettingItem item = new SwitchSettingItem(context);
        item.setChecked(checked);
        item.setTitleText(title);
        item.setOnItemClickListener(listener);
        SkinEngine.applyViewAttr(item, "z_setting_titleTextColor", R.attr.textColorMajor);
        SkinEngine.applyViewAttr(item, "z_setting_rightTextColor", R.attr.textColorNormal);
        SkinEngine.applyViewAttr(item, "z_setting_infoTextColor", R.attr.textColorMinor);
        ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        llContainer.addView(item, llContainer.getChildCount() - 1, params);
        return item;
    }

    private void initRecyclerView(View view) {
        mRecycler = new EasyRecycler<>(view.findViewById(R.id.recycler_view));
        mRecycler.setItemRes(R.layout.item_website)
                .setLayoutManager(new LinearLayoutManager(getContext()))
                .onBindViewHolder(this)
                .build();
        mRecycler.showLoading();
        loadPermissions();
    }

    protected void loadPermissions() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(Profile.getLastUsedRegularProfile(), false);
        fetcher.fetchPreferencesForCategory(getContentSettingsType(), new WebsitePermissionsFetcher.WebsitePermissionsCallback() {
            @Override
            public void onWebsitePermissionsAvailable(Collection<Website> sites) {
                postOnEnterAnimationEnd(() -> {
                    mRecycler.setItems(sites);
                    mRecycler.showContent();
                });
            }
        });
    }

    private String getPermissionString(@ContentSettingValues @Nullable Integer value) {
        return getString(ContentSettingsResources.getSiteSummary(value));
    }

    protected String getContentStr(Website website) {
        return null;
    }

    protected void showSelectDialog(TextView contentView, Website item) {

        List<Integer> list = new ArrayList<>(3);
//        String category = getSiteSettingsCategoryStr();
//        if (category.equals(SiteSettingsCategory.CATEGORY_DEVICE_LOCATION)
//                || category.equals(SiteSettingsCategory.CATEGORY_CAMERA)
//                || category.equals(SiteSettingsCategory.CATEGORY_MICROPHONE)
//                || category.equals(SiteSettingsCategory.CATEGORY_NOTIFICATIONS)
//                || category.equals(SiteSettingsCategory.CATEGORY_PROTECTED_MEDIA)
//                || category.equals(SiteSettingsCategory.CATEGORY_MIDI)) {
//            list.add(ContentSetting.ASK);
//        }
//        list.add(ContentSetting.ALLOW);
//        list.add(ContentSetting.BLOCK);

        list.add(ContentSettingValues.ASK);
        list.add(ContentSettingValues.ALLOW);
        list.add(ContentSettingValues.BLOCK);



        Integer contentSetting = getContentSetting(item);
        int selected = 0;
        for (int i = 0; i < list.size(); i++) {
            if (Objects.equals(list.get(i), contentSetting)) {
                selected = i;
                break;
            }
        }

        ZDialog.select(Integer.class)
                .onBindTitle((titleView, item1, position) -> titleView.setText(getPermissionString(item1)))
                .setSelected(selected)
                .onSingleSelect((dialog, position, setting) -> {
                    contentView.setText(getPermissionString(setting));
                    setContentSetting(item, setting);
                })
                .setShowButtons(true)
                .setData(list)
                .setTitle(item.getTitle())
                .show(context);

    }

    protected void onItemViewClick(View view, Website website) {
        start(SingleWebsiteFragment.newInstance(website));
    }

    protected abstract void initView(View view);

    @ContentSettingsType
    protected abstract int getContentSettingsType();

//    public @ContentSettingsType int getContentSettingsType() {
//        return SiteSettingsCategory.contentSettingsType(mCategory);
//    }

    @Nullable
    protected @ContentSettingValues Integer getContentSetting(Website website) {
        return website.getContentSetting(Profile.getLastUsedRegularProfile(), getContentSettingsType());
    }

    protected void setContentSetting(Website website, @ContentSettingValues int contentSetting) {
        website.setContentSetting(Profile.getLastUsedRegularProfile(), getContentSettingsType(), contentSetting);
    }

}
