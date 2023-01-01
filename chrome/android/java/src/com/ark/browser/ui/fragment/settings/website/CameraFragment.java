package com.ark.browser.ui.fragment.settings.website;

import android.view.View;

import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;

public class CameraFragment extends BaseWebsiteListFragment {

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_CAMERA;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return website.getCameraPermission();
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//        website.setCameraPermission(contentSetting);
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("相机权限");

        addSwitchItem(getString(R.string.text_camera_permission),
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
        return ContentSettingsType.MEDIASTREAM_CAMERA;
    }

}
