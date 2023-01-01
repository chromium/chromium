package com.ark.browser.ui.fragment.settings.website;

import android.view.View;

import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;

import org.chromium.chrome.R;
import org.chromium.components.content_settings.ContentSettingsType;

public class MidiFragment extends BaseWebsiteListFragment {

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_MIDI;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return website.getMidiPermission();
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//        website.setMidiPermission(contentSetting);
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("MIDI权限");

        addItem(getString(R.string.text_mic_permission),
                new OnCommonItemClickListener() {
                    @Override
                    public void onItemClick(CommonSettingItem item) {

                    }
                });
    }

    @Override
    protected int getContentSettingsType() {
        return ContentSettingsType.MIDI;
    }

}
