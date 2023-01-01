package com.ark.browser.ui.fragment.settings.website;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RadioGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

public class CookieFragment extends BaseWebsiteListFragment {

    private RadioButtonWithDescription mAllowButton;
    private RadioButtonWithDescription mBlockThirdPartyIncognitoButton;
    private RadioButtonWithDescription mBlockThirdPartyButton;
    private RadioButtonWithDescription mBlockButton;
    private RadioGroup mRadioGroup;
    private TextViewWithCompoundDrawables mManagedView;

//    @Override
//    protected String getSiteSettingsCategoryStr() {
//        return SiteSettingsCategory.CATEGORY_COOKIES;
//    }
//
//    @Override
//    protected ContentSetting getContentSetting(Website website) {
//        return website.getCookiePermission();
//    }
//
//    @Override
//    protected void setContentSetting(Website website, ContentSetting contentSetting) {
//        website.setCookiePermission(contentSetting);
//    }

    @Override
    protected void initView(View view) {
        setToolbarTitle("COOKIE权限");


        View item = LayoutInflater.from(context).inflate(
                R.layout.four_state_cookie_settings_preference, null, false);
        addItem(item);


        mAllowButton = item.findViewById(R.id.allow);
        mBlockThirdPartyIncognitoButton =
                item.findViewById(R.id.block_third_party_incognito);
        mBlockThirdPartyButton =
                item.findViewById(R.id.block_third_party);
        mBlockButton = item.findViewById(R.id.block);
        mRadioGroup = item.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int checkedId) {
                FourStateCookieSettingsPreference.CookieSettingsState state = getState();
                setCookieSettingsPreference(state);
            }
        });

        mManagedView = item.findViewById(R.id.managed_view);
        Drawable[] drawables = mManagedView.getCompoundDrawablesRelative();
        Drawable managedIcon = ApiCompatibilityUtils.getDrawable(getContext().getResources(),
                ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        mManagedView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                managedIcon, drawables[1], drawables[2], drawables[3]);



        FourStateCookieSettingsPreference.Params params =
                new FourStateCookieSettingsPreference.Params();
        params.allowCookies = WebsitePreferenceBridge.isCategoryEnabled(
                Profile.getLastUsedRegularProfile(), ContentSettingsType.COOKIES);
        PrefService prefService =
                UserPrefs.get(Profile.getLastUsedRegularProfile());
        params.cookieControlsMode = prefService.getInteger(COOKIE_CONTROLS_MODE);
        params.cookiesContentSettingEnforced = !WebsitePreferenceBridge.isContentSettingUserModifiable(
                Profile.getLastUsedRegularProfile(), getContentSettingsType());
        params.cookieControlsModeEnforced = prefService.isManagedPreference(COOKIE_CONTROLS_MODE);
        params.isIncognitoModeEnabled = true;

        configureRadioButtons(params);

//        addSwitchItem(getString(R.string.cookies_title),
//                PrefServiceBridge.getInstance().isAcceptCookiesEnabled(),
//                item -> PrefServiceBridge.getInstance().setAllowCookiesEnabled(item.isChecked()));
//
//        addSwitchItem(getString(R.string.allow_third_party_cookies_title),
//                !PrefServiceBridge.getInstance().isBlockThirdPartyCookiesEnabled(),
//                item -> PrefServiceBridge.getInstance().setBlockThirdPartyCookiesEnabled(!item.isChecked()));
    }

    @Override
    protected int getContentSettingsType() {
        return ContentSettingsType.COOKIES;
    }

    private void configureRadioButtons(FourStateCookieSettingsPreference.Params params) {
        assert (mRadioGroup != null);
        mAllowButton.setEnabled(true);
        mBlockThirdPartyIncognitoButton.setEnabled(true);
        mBlockThirdPartyButton.setEnabled(true);
        mBlockButton.setEnabled(true);
        for (RadioButtonWithDescription button : getEnforcedButtons(params)) {
            button.setEnabled(false);
        }
        mManagedView.setVisibility(
                (params.cookiesContentSettingEnforced || params.cookieControlsModeEnforced)
                        ? View.VISIBLE
                        : View.GONE);

        RadioButtonWithDescription button = getButton(getActiveState(params));
        // Always want to enable the selected option.
        button.setEnabled(true);
        button.setChecked(true);
    }

    private RadioButtonWithDescription[] getEnforcedButtons(FourStateCookieSettingsPreference.Params params) {
        if (!params.cookiesContentSettingEnforced && !params.cookieControlsModeEnforced) {
            // Nothing is enforced.
            if (!params.isIncognitoModeEnabled) {
                return buttons(mBlockThirdPartyIncognitoButton);
            } else {
                return buttons();
            }
        }
        if (params.cookiesContentSettingEnforced && params.cookieControlsModeEnforced) {
            return buttons(mAllowButton, mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton,
                    mBlockButton);
        }
        if (params.cookiesContentSettingEnforced) {
            if (params.allowCookies) {
                if (!params.isIncognitoModeEnabled) {
                    return buttons(mBlockButton, mBlockThirdPartyIncognitoButton);
                } else {
                    return buttons(mBlockButton);
                }
            } else {
                return buttons(mAllowButton, mBlockThirdPartyIncognitoButton,
                        mBlockThirdPartyButton, mBlockButton);
            }
        }
        // cookieControlsModeEnforced must be true.
        if (params.cookieControlsMode == CookieControlsMode.BLOCK_THIRD_PARTY) {
            return buttons(mAllowButton, mBlockThirdPartyIncognitoButton);
        } else {
            return buttons(mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton);
        }
    }

    private RadioButtonWithDescription[] buttons(RadioButtonWithDescription... args) {
        return args;
    }

    private FourStateCookieSettingsPreference.CookieSettingsState getActiveState(FourStateCookieSettingsPreference.Params params) {
        // These conditions only check the preference combinations that deterministically decide
        // your cookie settings state. In the future we would refactor the backend preferences to
        // reflect the only possible states you can be in
        // (Allow/BlockThirdPartyIncognito/BlockThirdParty/Block), instead of using this
        // combination of multiple signals.
        if (!params.allowCookies) {
            return FourStateCookieSettingsPreference.CookieSettingsState.BLOCK;
        } else if (params.cookieControlsMode == CookieControlsMode.BLOCK_THIRD_PARTY) {
            return FourStateCookieSettingsPreference.CookieSettingsState.BLOCK_THIRD_PARTY;
        } else if (params.cookieControlsMode == CookieControlsMode.INCOGNITO_ONLY
                && params.isIncognitoModeEnabled) {
            return FourStateCookieSettingsPreference.CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
        } else {
            return FourStateCookieSettingsPreference.CookieSettingsState.ALLOW;
        }
    }

    private RadioButtonWithDescription getButton(FourStateCookieSettingsPreference.CookieSettingsState state) {
        switch (state) {
            case ALLOW:
                return mAllowButton;
            case BLOCK_THIRD_PARTY_INCOGNITO:
                return mBlockThirdPartyIncognitoButton;
            case BLOCK_THIRD_PARTY:
                return mBlockThirdPartyButton;
            case BLOCK:
                return mBlockButton;
            case UNINITIALIZED:
                assert false;
                return null;
        }
        assert false;
        return null;
    }

    public FourStateCookieSettingsPreference.CookieSettingsState getState() {
        if (mAllowButton.isChecked()) {
            return FourStateCookieSettingsPreference.CookieSettingsState.ALLOW;
        } else if (mBlockThirdPartyIncognitoButton.isChecked()) {
            return FourStateCookieSettingsPreference.CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
        } else if (mBlockThirdPartyButton.isChecked()) {
            return FourStateCookieSettingsPreference.CookieSettingsState.BLOCK_THIRD_PARTY;
        } else {
            return FourStateCookieSettingsPreference.CookieSettingsState.BLOCK;
        }
    }

    private void setCookieSettingsPreference(FourStateCookieSettingsPreference.CookieSettingsState state) {
        boolean allowCookies;
        @CookieControlsMode
        int mode;

        switch (state) {
            case ALLOW:
                allowCookies = true;
                mode = CookieControlsMode.OFF;
                break;
            case BLOCK_THIRD_PARTY_INCOGNITO:
                allowCookies = true;
                mode = CookieControlsMode.INCOGNITO_ONLY;
                break;
            case BLOCK_THIRD_PARTY:
                allowCookies = true;
                mode = CookieControlsMode.BLOCK_THIRD_PARTY;
                break;
            case BLOCK:
                allowCookies = false;
                mode = CookieControlsMode.BLOCK_THIRD_PARTY;
                break;
            default:
                return;
        }

        WebsitePreferenceBridge.setCategoryEnabled(Profile.getLastUsedRegularProfile(),
                ContentSettingsType.COOKIES, allowCookies);
        PrefService prefService =
                UserPrefs.get(Profile.getLastUsedRegularProfile());
        prefService.setInteger(COOKIE_CONTROLS_MODE, mode);
    }

}
