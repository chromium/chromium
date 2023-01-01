package com.ark.browser.ui.fragment.settings.website;

import android.view.View;

import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;

import org.chromium.chrome.R;
import org.chromium.components.content_settings.ContentSettingsType;

public class UsbFragment extends BaseWebsiteListFragment {

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_DEVICE_LOCATION;
//    }
//
//    @Override
//    protected SiteSettingsCategory getSiteSettingsCategory() {
//        return null;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        website.getPermissionInfo(ContentSettingsType.USB_GUARD);
//        return website.getGeolocationPermission();
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("USB权限");

        addItem(getString(R.string.website_settings_usb),
                new OnCommonItemClickListener() {
                    @Override
                    public void onItemClick(CommonSettingItem item) {

                    }
                });
    }

    @Override
    protected int getContentSettingsType() {
        return ContentSettingsType.USB_GUARD;
    }
}
