package com.ark.browser.ui.fragment.settings.website;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.Manifest;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.text.format.Formatter;
import android.view.View;
import android.widget.LinearLayout;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;

import com.ark.browser.core.UserAgentManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.widget.TintSettingItem;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ColorPool;
import com.ark.browser.utils.FaviconUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.toolbar.ZToolBar;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.site_settings.ChosenObjectInfo;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.StorageInfo;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Objects;

public class SingleWebsiteFragment extends BaseSwipeBackFragment
        implements OnCommonItemClickListener,
        WebsitePermissionsFetcher.WebsitePermissionsCallback {

    public static final String EXTRA_SITE = "org.chromium.chrome.preferences.site";
    public static final String EXTRA_SITE_ADDRESS = "org.chromium.chrome.preferences.site_address";

    private Website website;

    TintSettingItem siteTitleItem;

    TintSettingItem userAgentItem;

    TintSettingItem siteUsageItem;

    TintSettingItem cookiesPermissionItem;

    TintSettingItem locationAccessItem;

    TintSettingItem cameraPermissionItem;

    TintSettingItem microphonePermissionItem;

    TintSettingItem pushNotificationsItem;

    TintSettingItem javascriptPermissionItem;

    TintSettingItem popupPermissionItem;

    TintSettingItem adsPermissionItem;

    TintSettingItem backgroundSyncPermissionItem;

    TintSettingItem protectedMediaIdentifierPermissionItem;

    TintSettingItem autoplayPermissionItem;

    TintSettingItem soundPermissionItem;

    TintSettingItem midiSysexPermissionItem;

//    LSettingItem usbPermissionItem;

    ActivityResultLauncher<String> mLauncher;

    private class SingleWebsitePermissionsPopulator
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        private final WebsiteAddress mSiteAddress;

        public SingleWebsitePermissionsPopulator(WebsiteAddress siteAddress) {
            mSiteAddress = siteAddress;
        }

        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;
            // TODO(mvanouwerkerk): Avoid modifying the outer class from this inner class.
            Log.d("SingleAdblockPreferences", "onWebsitePermissionsAvailable:" + sites.size());
            website = mergePermissionInfoForTopLevelOrigin(mSiteAddress, sites);

            displaySitePermissions();
        }
    }

    public static SingleWebsiteFragment newInstance(Website website) {
        Bundle args = new Bundle();
        SingleWebsiteFragment fragment = new SingleWebsiteFragment();
        args.putSerializable(EXTRA_SITE, website);
        fragment.setArguments(args);
        return fragment;
    }

    public static SingleWebsiteFragment newInstance(WebsiteAddress address) {
        Bundle args = new Bundle();
        SingleWebsiteFragment fragment = new SingleWebsiteFragment();
        args.putSerializable(EXTRA_SITE_ADDRESS, address);
        fragment.setArguments(args);
        return fragment;
    }

    public static SingleWebsiteFragment newInstance(Tab tab) {
        return newInstance(WebsiteAddress.create(tab.getOriginalUrl().getSpec()));
    }

    public static SingleWebsiteFragment newInstance(PageInfo pageInfo) {
        return newInstance(WebsiteAddress.create(pageInfo.getUrl()));
    }

    public static SingleWebsiteFragment newInstance(String url) {
        url = UrlFormatter.formatUrlForSecurityDisplay(url);
        return newInstance(WebsiteAddress.create(url));
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_site_single;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mLauncher = registerForActivityResult(new ActivityResultContracts.RequestPermission(),
                result -> {
                    if (result.equals(true)) {
                        //权限获取到之后的动作
                        displaySitePermissions();
                    } else {
                        //权限没有获取到的动作
                    }
                });
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        ZToolBar toolBar = findViewById(R.id.tool_bar);
        toolBar.setCenterText("权限管理");

        siteTitleItem = findViewById(R.id.item_site_title);
        siteTitleItem.setOnItemClickListener(this);

        userAgentItem = findViewById(R.id.item_user_agent);
        userAgentItem.setOnItemClickListener(this);

        siteUsageItem = findViewById(R.id.item_site_usage);
        siteUsageItem.setOnItemClickListener(this);

        cookiesPermissionItem = findViewById(R.id.item_cookies_permission);
        cookiesPermissionItem.setKey(ContentSettingsType.COOKIES);

        locationAccessItem = findViewById(R.id.item_location_access);
        locationAccessItem.setKey(ContentSettingsType.GEOLOCATION);

        cameraPermissionItem = findViewById(R.id.item_camera_permission);
        cameraPermissionItem.setKey(ContentSettingsType.MEDIASTREAM_CAMERA);

        microphonePermissionItem = findViewById(R.id.item_microphone_permission);
        microphonePermissionItem.setKey(ContentSettingsType.MEDIASTREAM_MIC);

        pushNotificationsItem = findViewById(R.id.item_push_notifications);
        pushNotificationsItem.setKey(ContentSettingsType.NOTIFICATIONS);

        javascriptPermissionItem = findViewById(R.id.item_javascript_permission);
        javascriptPermissionItem.setKey(ContentSettingsType.JAVASCRIPT);

        popupPermissionItem = findViewById(R.id.item_popup_permission);
        popupPermissionItem.setKey(ContentSettingsType.POPUPS);

        adsPermissionItem = findViewById(R.id.item_ads_permission);
        adsPermissionItem.setKey(ContentSettingsType.ADS);

        backgroundSyncPermissionItem = findViewById(R.id.item_background_sync_permission);
        backgroundSyncPermissionItem.setKey(ContentSettingsType.BACKGROUND_SYNC);

        protectedMediaIdentifierPermissionItem = findViewById(R.id.item_protected_media_identifier_permission);
        protectedMediaIdentifierPermissionItem.setKey(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER);

        autoplayPermissionItem = findViewById(R.id.item_autoplay_permission);
        autoplayPermissionItem.setKey(ContentSettingsType.AUTOPLAY);

        soundPermissionItem = findViewById(R.id.item_sound_permission);
        soundPermissionItem.setKey(ContentSettingsType.SOUND);

        midiSysexPermissionItem = findViewById(R.id.item_midi_sysex_permission);
        midiSysexPermissionItem.setKey(ContentSettingsType.MIDI_SYSEX);

//        usbPermissionItem = view.getView(R.id.item_usb_permission);
//        usbPermissionItem.setOnItemClickListener(this);


        LinearLayout llContainerPermission = findViewById(R.id.ll_container_permision);
        for (int i = 0; i < llContainerPermission.getChildCount(); i++) {
            View child = llContainerPermission.getChildAt(i);
            if (child instanceof TintSettingItem) {
                ((TintSettingItem) child).setOnItemClickListener(this);
                ((TintSettingItem) child).setLeftIconTint(ColorPool.getColor(i));
            }
        }


        ButtonCompat resetBtn = findViewById(R.id.btn_reset_site);
        resetBtn.setOnClickListener(v -> showResetDialog());

        if (getArguments() != null) {
            Object extraSite = getArguments().getSerializable(EXTRA_SITE);
            Object extraSiteAddress = getArguments().getSerializable(EXTRA_SITE_ADDRESS);
            if (extraSite != null && extraSiteAddress == null) {
                website = (Website) extraSite;
                displaySitePermissions();
            } else if (extraSiteAddress != null && extraSite == null) {
                WebsitePermissionsFetcher fetcher;
                WebsiteAddress websiteAddress = (WebsiteAddress) extraSiteAddress;
                fetcher = new WebsitePermissionsFetcher(Profile.getLastUsedRegularProfile(), false);
                fetcher.fetchAllPreferences(new SingleWebsitePermissionsPopulator((WebsiteAddress) extraSiteAddress));
            } else {
                throw new RuntimeException("Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.");
            }
        } else {
            throw new RuntimeException("Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.");
        }
    }

    @Override
    public void onItemClick(CommonSettingItem item) {
        int id = item.getId();
        if (id == R.id.item_site_title) {
        } else if (id == R.id.item_user_agent) {
            showUserAgentSelector();
        } else if (id == R.id.item_site_usage) {
            showClearDataDialog();
        } else if (id == R.id.item_cookies_permission) {
            showSelectDialog(cookiesPermissionItem);
        } else if (id == R.id.item_location_access) {
            showSelectDialog(locationAccessItem);
        } else if (id == R.id.item_camera_permission) {
            if ((boolean) cameraPermissionItem.getTag()) {
                showSelectDialog(cameraPermissionItem);
            } else {
                mLauncher.launch(Manifest.permission.CAMERA);
            }
        } else if (id == R.id.item_microphone_permission) {
            if ((boolean) microphonePermissionItem.getTag()) {
                showSelectDialog(microphonePermissionItem);
            } else {
                mLauncher.launch(android.Manifest.permission.RECORD_AUDIO);
            }
        } else if (id == R.id.item_push_notifications) {
            showSelectDialog(pushNotificationsItem);
        } else if (id == R.id.item_javascript_permission) {
            showSelectDialog(javascriptPermissionItem);
        } else if (id == R.id.item_popup_permission) {
            showSelectDialog(popupPermissionItem);
        } else if (id == R.id.item_ads_permission) {
            showSelectDialog(adsPermissionItem);
        } else if (id == R.id.item_background_sync_permission) {
            showSelectDialog(backgroundSyncPermissionItem);
        } else if (id == R.id.item_protected_media_identifier_permission) {
            showSelectDialog(protectedMediaIdentifierPermissionItem);
        } else if (id == R.id.item_autoplay_permission) {
            showSelectDialog(autoplayPermissionItem);
        } else if (id == R.id.item_sound_permission) {
            showSelectDialog(soundPermissionItem);
        } else if (id == R.id.item_midi_sysex_permission) {
            showSelectDialog(midiSysexPermissionItem);
        }
    }

    @Override
    public void onWebsitePermissionsAvailable(Collection<Website> sites) {

    }

    private void displaySitePermissions() {
        siteTitleItem.setTitleText(website.getTitle());
        FaviconUtil.with(getContext(), website.getTitle())
                .setCallback(result -> siteTitleItem.setLeftIcon(result))
                .start();


        UserAgentManager.UserAgent userAgent = UserAgentManager.getUserAgentByUrl(website.getAddress().getHost());

        userAgentItem.setInfoText(userAgent.getName()); // UserAgentUtil.getCurrentUserAgent().getUserAgentName()
        siteUsageItem.setInfoText(getString(R.string.origin_settings_storage_usage_brief,
                Formatter.formatShortFileSize(getContext(), website.getTotalUsage())));

        boolean hasCameraPermission = permissionOnInAndroid(android.Manifest.permission.CAMERA);
        if (hasCameraPermission) {
            setPermissionText(cameraPermissionItem);
        } else {
            cameraPermissionItem.setInfoText("浏览器未授予相机权限");
        }
        cameraPermissionItem.setLeftIcon(getResources().getDrawable(hasCameraPermission
                ? R.drawable.ic_videocam : R.drawable.exclamation_triangle));
        cameraPermissionItem.setTag(hasCameraPermission);

        boolean hasRecordPermission = permissionOnInAndroid(android.Manifest.permission.RECORD_AUDIO);
        if (hasRecordPermission) {
            setPermissionText(microphonePermissionItem);
        } else {
            microphonePermissionItem.setInfoText("浏览器未授予录音权限");
        }
        microphonePermissionItem.setLeftIcon(getResources().getDrawable(hasRecordPermission ?
                R.drawable.permission_mic : R.drawable.exclamation_triangle));
        microphonePermissionItem.setTag(hasRecordPermission);

        setPermissionText(cookiesPermissionItem);
        setPermissionText(locationAccessItem);
        setPermissionText(pushNotificationsItem);
        setPermissionText(javascriptPermissionItem);
        setPermissionText(popupPermissionItem);
        setPermissionText(adsPermissionItem);
        setPermissionText(backgroundSyncPermissionItem);
        setPermissionText(protectedMediaIdentifierPermissionItem);
        setPermissionText(autoplayPermissionItem);
        setPermissionText(soundPermissionItem);
        setPermissionText(midiSysexPermissionItem);
    }

    @ContentSettingValues
    private int getDefaultContentSetting(@ContentSettingsType int contentType) {
        boolean requiresTriStateSetting =
                WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);

        boolean checked = false;
//        @ContentSettingValues
//        int setting = ContentSettingValues.DEFAULT;



        if (contentType == ContentSettingsType.GEOLOCATION) {
            checked = WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                    Profile.getLastUsedRegularProfile());
        } else if (requiresTriStateSetting) {
            return WebsitePreferenceBridge.getDefaultContentSetting(
                    Profile.getLastUsedRegularProfile(), contentType);
        } else {
            checked = WebsitePreferenceBridge.isCategoryEnabled(
                    Profile.getLastUsedRegularProfile(), contentType);
        }

        ContentSettingsResources.ResourceItem resourceItem = ContentSettingsResources.getResourceItem(contentType);
        if (checked) {
            return resourceItem.getDefaultEnabledValue();
        } else {
            return resourceItem.getDefaultDisabledValue();
        }
    }

    private void setPermissionText(TintSettingItem item) {

//        int contentType = (int) item.getKey();
//
//        boolean requiresTriStateSetting =
//                WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);
//
//        boolean checked = false;
//        @ContentSettingValues
//        int setting = ContentSettingValues.DEFAULT;
//
//        if (contentType == ContentSettingsType.GEOLOCATION) {
//            checked = WebsitePreferenceBridge.areAllLocationSettingsEnabled(
//                    Profile.getLastUsedRegularProfile());
//        } else if (requiresTriStateSetting) {
//            setting = WebsitePreferenceBridge.getDefaultContentSetting(
//                    Profile.getLastUsedRegularProfile(), contentType);
//        } else {
//            checked = WebsitePreferenceBridge.isCategoryEnabled(
//                    Profile.getLastUsedRegularProfile(), contentType);
//        }
//
////        item.setTitleText(getString(ContentSettingsResources.getTitle(contentType)));
//
//        if ((ContentSettingsType.MEDIASTREAM_CAMERA == contentType
//                || ContentSettingsType.MEDIASTREAM_MIC == contentType
//                || ContentSettingsType.NOTIFICATIONS == contentType
//                || ContentSettingsType.AR == contentType)) {
//            // Show 'disabled' message when permission is not granted in Android.
//            item.setInfoText(getString(ContentSettingsResources.getCategorySummary(contentType, false)));
//        } else if (ContentSettingsType.COOKIES == contentType && checked
//                && UserPrefs.get(Profile.getLastUsedRegularProfile()).getInteger(COOKIE_CONTROLS_MODE)
//                == CookieControlsMode.BLOCK_THIRD_PARTY) {
//            item.setInfoText(getString(ContentSettingsResources.getCookieAllowedExceptThirdPartySummary()));
//        } else if (ContentSettingsType.GEOLOCATION == contentType && checked
//                && WebsitePreferenceBridge.isLocationAllowedByPolicy(Profile.getLastUsedRegularProfile())) {
//            item.setInfoText(getString(ContentSettingsResources.getGeolocationAllowedSummary()));
//        } else if (ContentSettingsType.CLIPBOARD_READ_WRITE == contentType && !checked) {
//            item.setInfoText(getString(ContentSettingsResources.getClipboardBlockedListSummary()));
//        } else if (ContentSettingsType.ADS == contentType && !checked) {
//            item.setInfoText(getString(ContentSettingsResources.getAdsBlockedListSummary()));
//        } else if (ContentSettingsType.SOUND == contentType && !checked) {
//            item.setInfoText(getString(ContentSettingsResources.getSoundBlockedListSummary()));
//        } else if (ContentSettingsType.REQUEST_DESKTOP_SITE == contentType) {
//            item.setInfoText(getString(ContentSettingsResources.getDesktopSiteListSummary(checked)));
//        } else if (ContentSettingsType.AUTO_DARK_WEB_CONTENT == contentType) {
//            item.setInfoText(getString(ContentSettingsResources.getAutoDarkWebContentListSummary(checked)));
//        } else if (requiresTriStateSetting) {
//            item.setInfoText(getString(ContentSettingsResources.getCategorySummary(setting)));
//        } else {
//            item.setInfoText(getString(ContentSettingsResources.getCategorySummary(contentType, checked)));
//        }



        item.setInfoText(getPermissionString((int) item.getKey()));
    }

    private String getPermissionString(@ContentSettingsType int type) {
        int contentSetting = getContentSetting(type);
        return getString(ContentSettingsResources.getCategorySummary(contentSetting));
//        if (contentSetting == null) {
//            return "null";
//        }
//
//        boolean requiresTriStateSetting =
//                WebsitePreferenceBridge.requiresTriStateContentSetting((int) contentSetting);
//        ArkLogger.e(this, "getPermissionString contentSetting=" + contentSetting
//                + " requiresTriStateSetting=" + requiresTriStateSetting);
//        if (requiresTriStateSetting) {
//            return getString(ContentSettingsResources.getCategorySummary(contentSetting));
//        }
//
//
//        return getString(ContentSettingsResources.getSiteSummary(contentSetting));
    }

    private String getPermissionString(@ContentSettingValues Integer contentSetting) {
        return getString(ContentSettingsResources.getCategorySummary(contentSetting));
    }

    public void showUserAgentSelector() {
        String host = website.getAddress().getHost();
        String title = "选择浏览器标识:" + host;
        int index = UserAgentManager.getUserAgentIndexByUrl(host) + 1;
        List<UserAgentManager.UserAgent> userAgents = UserAgentManager.getUserAgentListWithDefault();
        new UserAgentSelectDialog()
                .setTitle(title)
                .setData(userAgents)
                .setSelected(index)
                .onSingleSelect((fragment, position, item) -> {
                    position -= 1;
                    UserAgentManager.setUserAgentByUrl(host, position);
                    userAgentItem.setInfoText(item.getName());
                })
                .show(context);
    }

    public static class UserAgentSelectDialog extends ZDialog.BottomSelectDialogFragmentImpl<UserAgentManager.UserAgent> {

        @Override
        protected int getImplLayoutId() {
            return R.layout.fragment_dialog_layout_center_impl_list;
        }

        public UserAgentSelectDialog() {
            onBindTitle((titleView, item, position) -> titleView.setText(item.getName()));
            onBindSubtitle((subtitleView, item, position) -> subtitleView.setText(item.getString()));
        }

        @Override
        protected void initView(View view, @Nullable Bundle savedInstanceState) {
            super.initView(view, savedInstanceState);
            findViewById(R.id.btn_close).setOnClickListener(v -> dismiss());
        }

    }

    @ContentSettingValues
    private int getContentSetting(@ContentSettingsType int contentType) {
        Integer contentSetting = website.getContentSetting(Profile.getLastUsedRegularProfile(), contentType);
        if (contentSetting == null) {
            contentSetting = getDefaultContentSetting(contentType);
        }
        return contentSetting;
    }

    private void showSelectDialog(TintSettingItem item) {
        List<Integer> list = new ArrayList<>(3);

        int contentType = (int) item.getKey();

        boolean requiresTriStateSetting =
                WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);
        if (requiresTriStateSetting) {
            list.add(ContentSettingValues.ASK);
            list.add(ContentSettingValues.ALLOW);
            list.add(ContentSettingValues.BLOCK);
        } else {
            ContentSettingsResources.ResourceItem resourceItem = ContentSettingsResources.getResourceItem(contentType);
            list.add(resourceItem.getDefaultEnabledValue());
            list.add(resourceItem.getDefaultDisabledValue());
        }

        Integer contentSetting = getContentSetting(contentType);
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
                    setPermission(item, setting);
                    item.setInfoText(getString(ContentSettingsResources.getSiteSummary(setting)));
                })
                .setShowButtons(true)
                .setData(list)
                .setTitle(item.getTitleText())
                .show(context);
    }

    private void showResetDialog() {
        ZDialog.alert()
                .setTitle(R.string.website_reset)
                .setContent(getString(R.string.website_reset_confirmation))
                .setPositiveButton((fragment, which) -> resetSite())
                .show(_mActivity);
    }

    private void showClearDataDialog() {
        ZDialog.alert()
                .setTitle(R.string.webstorage_clear_data_dialog_title)
                .setContent(getString(R.string.webstorage_clear_data_dialog_message))
                .setPositiveButton((fragment, which) -> clearData())
                .show(_mActivity);
    }

    private void setPermission(CommonSettingItem item, @ContentSettingValues int contentSetting) {
        Profile profile = Profile.getLastUsedRegularProfile();
        if (item == cookiesPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.COOKIES, contentSetting);
        } else if (item == locationAccessItem) {
            website.setContentSetting(profile, ContentSettingsType.GEOLOCATION, contentSetting);
        } else if (item == cameraPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.MEDIASTREAM_CAMERA, contentSetting);
        } else if (item == microphonePermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.MEDIASTREAM_MIC, contentSetting);
        } else if (item == pushNotificationsItem) {
            website.setContentSetting(profile, ContentSettingsType.NOTIFICATIONS, contentSetting);
        } else if (item == javascriptPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.JAVASCRIPT, contentSetting);
        } else if (item == popupPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.POPUPS, contentSetting);
        } else if (item == adsPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.ADS, contentSetting);
        } else if (item == backgroundSyncPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.BACKGROUND_SYNC, contentSetting);
        } else if (item == protectedMediaIdentifierPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER, contentSetting);
        } else if (item == autoplayPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.AUTOPLAY, contentSetting);
        } else if (item == soundPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.SOUND, contentSetting);
        } else if (item == midiSysexPermissionItem) {
            website.setContentSetting(profile, ContentSettingsType.MIDI_SYSEX, contentSetting);
        }
//        else if (item == usbPermissionItem) {
//
//        }
    }

    protected void resetSite() {

        if (getActivity() == null) return;

        website.resetPermissions(Profile.getLastUsedRegularProfile());
        clearData();
    }

    private void clearData() {
        website.clearData(Profile.getLastUsedRegularProfile(),
                this::displaySitePermissions);
    }

    private boolean permissionOnInAndroid(String permission) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return true;
        return PackageManager.PERMISSION_GRANTED == ApiCompatibilityUtils.checkPermission(
                getContext(), permission, Process.myPid(), Process.myUid());
    }


    private static Website mergePermissionInfoForTopLevelOrigin(
            WebsiteAddress address, Collection<Website> websites) {
        String origin = address.getOrigin();
        String host = Uri.parse(origin).getHost();
        Website merged = new Website(address, null);
        // This loop looks expensive, but the amount of data is likely to be relatively small
        // because most sites have very few permissions.
        for (Website other : websites) {
            if (merged.getContentSettingException(ContentSettingsType.ADS) == null
                    && other.getContentSettingException(ContentSettingsType.ADS) != null
                    && other.compareByAddressTo(merged) == 0) {
                merged.setContentSettingException(ContentSettingsType.ADS,
                        other.getContentSettingException(ContentSettingsType.ADS));
            }
            for (PermissionInfo info : other.getPermissionInfos()) {
                if (merged.getPermissionInfo(info.getContentSettingsType()) == null
                        && permissionInfoIsForTopLevelOrigin(info, origin)) {
                    merged.setPermissionInfo(info);
                }
            }
            if (merged.getLocalStorageInfo() == null && other.getLocalStorageInfo() != null
                    && origin.equals(other.getLocalStorageInfo().getOrigin())) {
                merged.setLocalStorageInfo(other.getLocalStorageInfo());
            }
            for (StorageInfo storageInfo : other.getStorageInfo()) {
                if (host.equals(storageInfo.getHost())) {
                    merged.addStorageInfo(storageInfo);
                }
            }
            for (ChosenObjectInfo objectInfo : other.getChosenObjectInfo()) {
                if (origin.equals(objectInfo.getOrigin())) {
                    merged.addChosenObjectInfo(objectInfo);
                }
            }
            if (host.equals(other.getAddress().getHost())) {
                for (ContentSettingException exception : other.getContentSettingExceptions()) {
                    int type = exception.getContentSettingType();
                    if (type == ContentSettingsType.ADS) {
                        continue;
                    }
                    if (merged.getContentSettingException(type) == null) {
                        merged.setContentSettingException(type, exception);
                    }
                }
            }
        }
        return merged;
    }

    private static boolean permissionInfoIsForTopLevelOrigin(PermissionInfo info, String origin) {
        // TODO(mvanouwerkerk): Find a more generic place for this method.
        return origin.equals(info.getOrigin())
                && (origin.equals(info.getEmbedderSafe()) || "*".equals(info.getEmbedderSafe()));
    }
}
