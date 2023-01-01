package com.ark.browser.ui.fragment.settings.website;

import android.text.format.Formatter;
import android.view.View;

import androidx.annotation.Nullable;

import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.Collection;

public class UsageFragment extends BaseWebsiteListFragment {

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_USE_STORAGE;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return null;
//    }
//
//    @Override
//    protected String getContentStr(Website website) {
//        return Formatter.formatShortFileSize(getContext(), website.getTotalUsage());
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//
//    }

    @Override
    protected String getContentStr(Website website) {
        return Formatter.formatShortFileSize(getContext(), website.getTotalUsage());
    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("存储占用");

        addItem(getString(R.string.website_settings_usage_category),
                new OnCommonItemClickListener() {
                    @Override
                    public void onItemClick(CommonSettingItem item) {

                    }
                });
    }

    @Override
    protected void loadPermissions() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(Profile.getLastUsedRegularProfile(), false);
        fetcher.fetchAllPreferences(new WebsitePermissionsFetcher.WebsitePermissionsCallback() {
            @Override
            public void onWebsitePermissionsAvailable(Collection<Website> sites) {
                postOnEnterAnimationEnd(() -> {
                    mRecycler.setItems(sites);
                    mRecycler.showContent();
                });
            }
        });
    }

    @Nullable
    @Override
    protected Integer getContentSetting(Website website) {
        return null;
    }

    @Override
    protected int getContentSettingsType() {
        return ContentSettingsType.DEFAULT;
    }

}
