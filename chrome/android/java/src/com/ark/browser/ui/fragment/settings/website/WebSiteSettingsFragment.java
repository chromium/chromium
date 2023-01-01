package com.ark.browser.ui.fragment.settings.website;

import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.widget.TintSettingItem;
import com.ark.browser.utils.ColorPool;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.chrome.R;

public class WebSiteSettingsFragment extends BaseSwipeBackFragment
        implements OnCommonItemClickListener {
    // The keys for each category shown on the Site Settings page.
    static final String ALL_SITES_KEY = "all_sites";
    static final String ADS_KEY = "ads";
    static final String AUTOPLAY_KEY = "autoplay";
    static final String BACKGROUND_SYNC_KEY = "background_sync";
    static final String CAMERA_KEY = "camera";
    static final String COOKIES_KEY = "cookies";
    static final String JAVASCRIPT_KEY = "javascript";
    static final String LOCATION_KEY = "device_location";
    static final String MEDIA_KEY = "media";
    static final String MICROPHONE_KEY = "microphone";
    static final String NOTIFICATIONS_KEY = "notifications";
    static final String POPUPS_KEY = "popups";
    static final String PROTECTED_CONTENT_KEY = "protected_content";
    static final String SOUND_KEY = "sound";
    static final String STORAGE_KEY = "use_storage";
    static final String TRANSLATE_KEY = "translate";
    static final String USB_KEY = "usb";

    // Whether the Protected Content menu is available for display.
    boolean mProtectedContentMenuAvailable;

    // Whether this class is handling showing the Media sub-menu (and not the main menu).
    boolean mMediaSubMenu;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mProtectedContentMenuAvailable = Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_site;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("网页管理");

        LinearLayout llContainer = findViewById(R.id.ll_container);
        for (int i = 0; i < llContainer.getChildCount(); i++) {
            View child = llContainer.getChildAt(i);
            if (child instanceof TintSettingItem) {
                ((TintSettingItem) child).setOnItemClickListener(this);
                ((TintSettingItem) child).setLeftIconTint(ColorPool.getColor(i));
            }
        }
    }

    private int keyToContentSettingsType(String key) {
        if (ADS_KEY.equals(key)) {
            return ContentSettingsType.ADS;
        } else if (AUTOPLAY_KEY.equals(key)) {
            return ContentSettingsType.AUTOPLAY;
        } else if (BACKGROUND_SYNC_KEY.equals(key)) {
            return ContentSettingsType.BACKGROUND_SYNC;
        } else if (CAMERA_KEY.equals(key)) {
            return ContentSettingsType.MEDIASTREAM_CAMERA;
        } else if (COOKIES_KEY.equals(key)) {
            return ContentSettingsType.COOKIES;
        } else if (JAVASCRIPT_KEY.equals(key)) {
            return ContentSettingsType.JAVASCRIPT;
        } else if (LOCATION_KEY.equals(key)) {
            return ContentSettingsType.GEOLOCATION;
        } else if (MICROPHONE_KEY.equals(key)) {
            return ContentSettingsType.MEDIASTREAM_MIC;
        } else if (NOTIFICATIONS_KEY.equals(key)) {
            return ContentSettingsType.NOTIFICATIONS;
        } else if (POPUPS_KEY.equals(key)) {
            return ContentSettingsType.POPUPS;
        } else if (PROTECTED_CONTENT_KEY.equals(key)) {
            return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
        } else if (SOUND_KEY.equals(key)) {
            return ContentSettingsType.SOUND;
        }
        return -1;
    }

    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    public void onItemClick(CommonSettingItem item) {
        int id = item.getId();
        if (id == R.id.item_all_sites) {
            start(new AllWebSiteFragment());
        } else if (id == R.id.item_site_redirect) {
            start(new WebSiteRedirectFragment());
        } else if (id == R.id.item_cookies) {
            start(new CookieFragment());
        } else if (id == R.id.item_location) {
            start(new LocationFragment());
        } else if (id == R.id.item_camera) {
            start(new CameraFragment());
        } else if (id == R.id.item_microphone) {
            start(new MicrophoneFragment());
        } else if (id == R.id.item_notification) {
            start(new NotificationFragment());
        } else if (id == R.id.item_javascript) {
            start(new JavaScriptFragment());
        } else if (id == R.id.item_popups) {
            start(new PopupFragment());
        } else if (id == R.id.item_ads) {
            start(new AdsFragment());
        } else if (id == R.id.item_background_sync) {
            start(new BackgroundSyncFragment());
        } else if (id == R.id.item_protected_content) {
            start(new ProtectedContentFragment());
        } else if (id == R.id.item_auto_play) {
            start(new AutoPlayFragment());
        } else if (id == R.id.item_sound) {
            start(new SoundFragment());
        } else if (id == R.id.item_storage) {
            start(new UsageFragment());
        } else if (id == R.id.item_usb) {
            start(new UsbFragment());
        } else if (id == R.id.item_midi) {
            start(new MidiFragment());
        }
    }
}

