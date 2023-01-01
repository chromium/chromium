package com.ark.browser.ui.fragment.settings.website;

import android.view.View;

import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;

public class SoundFragment extends BaseWebsiteListFragment {

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_SOUND;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return website.getSoundPermission();
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//        website.setSoundPermission(contentSetting);
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("声音权限");

        addSwitchItem(getString(org.chromium.chrome.R.string.sound_permission_title),
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
        return ContentSettingsType.SOUND;
    }
}
