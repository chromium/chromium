package com.ark.browser.ui.fragment.settings.website;

import android.view.View;

import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;

public class ProtectedContentFragment extends BaseWebsiteListFragment {

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_PROTECTED_MEDIA;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return website.getProtectedMediaIdentifierPermission();
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//        website.setProtectedMediaIdentifierPermission(contentSetting);
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("保护内容权限");

        addSwitchItem(getString(R.string.protected_content),
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
        return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
    }
}
