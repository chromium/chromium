package com.ark.browser.ui.fragment.wallpaper;

import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import com.zpj.bus.ZBus;
import com.zpj.utils.Callback;
import com.zpj.utils.PrefsHelper;

public class WallpaperManager {

    private static final String PREFS_WALLPAPER = "configs_wallpaper";
    private static final String KEY_PATH = "wallpaper_path";

    public static void setWallpaperPath(String path) {
        PrefsHelper.with(PREFS_WALLPAPER).applyString(KEY_PATH, path);
        ZBus.post(KEY_PATH, path);
    }

    public static String getWallpaperPath() {
        return PrefsHelper.with(PREFS_WALLPAPER).getString(KEY_PATH);
    }

    public static void observer(LifecycleOwner owner, ZBus.SingleConsumer<String> consumer) {
        if (consumer == null) {
            return;
        }


        ZBus.with(owner)
                .observe(KEY_PATH, String.class)
                .doOnChange(consumer)
                .subscribe();

//        SharedPreferences.OnSharedPreferenceChangeListener listener = (sharedPreferences, s) -> {
//            if (KEY_PATH.equals(s)) {
//                String wallpaperPath = sharedPreferences.getString(s, null);
//                callback.onCallback(wallpaperPath);
//            }
//        };
//        owner.getLifecycle().addObserver(new DefaultLifecycleObserver() {
//            @Override
//            public void onDestroy(@NonNull LifecycleOwner owner) {
//                owner.getLifecycle().removeObserver(this);
//                PrefsHelper.with(PREFS_WALLPAPER).unregisterOnChangeListener(listener);
//            }
//        });
//
//        PrefsHelper.with(PREFS_WALLPAPER).registerOnChangeListener(listener);
    }

}
