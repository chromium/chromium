package com.ark.browser.ui.fragment.settings.website;

import android.view.View;

import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;

public class AdsFragment extends BaseWebsiteListFragment {

//    @Override
//    protected int getLayoutId() {
//        return R.layout.fragment_setting_site_ads;
//    }
//
//    @Override
//    protected int getListViewId() {
//        return R.id.recycler_view;
//    }

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_ADS;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return website.getAdsPermission();
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//        website.setAdsPermission(contentSetting);
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("广告");

        addSwitchItem(getString(R.string.text_ads),
                WebsitePreferenceBridge.isCategoryEnabled(Profile.getLastUsedRegularProfile(), getContentSettingsType()),
                new OnCheckableItemClickListener() {
                    @Override
                    public void onItemClick(CheckableSettingItem item) {
                        WebsitePreferenceBridge.setCategoryEnabled(Profile.getLastUsedRegularProfile(),
                                getContentSettingsType(), item.isChecked());
                    }
                });
    }

    @Override
    protected int getContentSettingsType() {
        return ContentSettingsType.ADS;
    }

//    @Override
//    protected void setContentSetting(Website website, int contentSetting) {
//        website.setContentSetting(Profile.getLastUsedRegularProfile(), getContentSettingsType(), contentSetting);
//    }

}
