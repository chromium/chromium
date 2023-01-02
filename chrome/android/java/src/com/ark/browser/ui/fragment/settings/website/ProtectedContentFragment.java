package com.ark.browser.ui.fragment.settings.website;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.RadioGroup;

import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;

public class ProtectedContentFragment extends BaseWebsiteListFragment {

    private RadioButtonWithDescription mAllowed;
    private RadioButtonWithDescription mAsk;
    private RadioButtonWithDescription mBlocked;
    private RadioGroup mRadioGroup;

    private @ContentSettingValues int mSetting = ContentSettingValues.DEFAULT;


    @Override
    protected void initView(View view) {
        setToolbarTitle("保护内容权限");


        mSetting = WebsitePreferenceBridge.getDefaultContentSetting(
                Profile.getLastUsedRegularProfile(), getContentSettingsType());
        int[] descriptionIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(getContentSettingsType());


        View item = LayoutInflater.from(context).inflate(
                R.layout.tri_state_site_settings_preference, null, false);
        addItem(item);

        mAllowed = item.findViewById(R.id.allowed);
        mAsk = item.findViewById(R.id.ask);
        mBlocked = item.findViewById(R.id.blocked);
        mRadioGroup = item.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int i) {
                if (mAllowed.isChecked()) {
                    mSetting = ContentSettingValues.ALLOW;
                } else if (mAsk.isChecked()) {
                    mSetting = ContentSettingValues.ASK;
                } else if (mBlocked.isChecked()) {
                    mSetting = ContentSettingValues.BLOCK;
                }
                WebsitePreferenceBridge.setDefaultContentSetting(
                        Profile.getLastUsedRegularProfile(), getContentSettingsType(), mSetting);
            }
        });


        if (descriptionIds != null) {
            mAllowed.setDescriptionText(getText(descriptionIds[0]));
            mAsk.setDescriptionText(getText(descriptionIds[1]));
            mBlocked.setDescriptionText(getText(descriptionIds[2]));
        }



        RadioButtonWithDescription radioButton = findRadioButton(mSetting);
        if (radioButton != null) radioButton.setChecked(true);
    }

    @Override
    protected int getContentSettingsType() {
        return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
    }

    /**
     * @param setting The setting to find RadioButton for.
     */
    private RadioButtonWithDescription findRadioButton(@ContentSettingValues int setting) {
        if (setting == ContentSettingValues.ALLOW) {
            return mAllowed;
        } else if (setting == ContentSettingValues.ASK) {
            return mAsk;
        } else if (setting == ContentSettingValues.BLOCK) {
            return mBlocked;
        } else {
            return null;
        }
    }

}
