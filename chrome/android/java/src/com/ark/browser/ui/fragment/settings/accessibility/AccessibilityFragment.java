package com.ark.browser.ui.fragment.settings.accessibility;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.util.TypedValue;
import android.view.View;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.chrome.R;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.text.NumberFormat;

public class AccessibilityFragment extends BaseSwipeBackFragment
        implements SeekBar.OnSeekBarChangeListener {

    private NumberFormat mFormat;
    private FontSizePrefs mFontSizePrefs;

    private SwitchSettingItem forceEnableZoomItem;

    private final FontSizePrefs.FontSizePrefsObserver mFontSizePrefsObserver = new FontSizePrefs.FontSizePrefsObserver() {
        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            ZToast.normal("onFontScaleFactorChanged");
            amount.setText(mFormat.format(userFontScaleFactor));
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {
            forceEnableZoomItem.setChecked(enabled);
        }
    };


    private TextView amount;
    private TextView mPreview;

    private float mMin, mMax, mStep;
    private float mValue;
    private boolean mTrackingTouch;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mMin = 0.5f;
        mMax = 2.0f;
        mStep = 0.05f;

        mFormat = NumberFormat.getPercentInstance();
        mFontSizePrefs = FontSizePrefs.getInstance(Profile.getLastUsedRegularProfile());
        mValue = mFontSizePrefs.getFontScaleFactor();
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_accessibility;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("无障碍");

        SeekBar seekBar = view.findViewById(R.id.seekbar);
        seekBar.setOnSeekBarChangeListener(this);
        seekBar.setMax(prefValueToSeekBarProgress(mMax));
        seekBar.setProgress(prefValueToSeekBarProgress(mValue));
        amount = view.findViewById(R.id.seekbar_amount);
        mPreview = view.findViewById(R.id.preview);


        updatePreview();

        forceEnableZoomItem = view.findViewById(R.id.item_force_enable_zoom);
        forceEnableZoomItem.setChecked(mFontSizePrefs.getForceEnableZoom());
        forceEnableZoomItem.setOnItemClickListener(item -> mFontSizePrefs.setForceEnableZoomFromUser(item.isChecked()));

        SwitchSettingItem readerForAccessibilityItem = view.findViewById(R.id.item_reader_for_accessibility);
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        readerForAccessibilityItem.setChecked(prefService.getBoolean(Pref.READER_FOR_ACCESSIBILITY));
        readerForAccessibilityItem.setOnItemClickListener(item -> prefService.setBoolean(
                Pref.READER_FOR_ACCESSIBILITY, item.isChecked()));


        findViewById(R.id.item_captions).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);
                // Open the activity in a new task because the back button on the caption
                // settings page navigates to the previous settings page instead of Chrome.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
            }
        });

    }

    @Override
    public void onStart() {
        super.onStart();
        mFontSizePrefs.addObserver(mFontSizePrefsObserver);
    }

    @Override
    public void onStop() {
        mFontSizePrefs.removeObserver(mFontSizePrefsObserver);
        super.onStop();
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            syncProgress(seekBar);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        mTrackingTouch = true;
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        mTrackingTouch = false;
    }

    private void syncProgress(SeekBar seekBar) {
        float value = seekBarProgressToPrefValue(seekBar.getProgress());
        if (value != mValue) {
            value = Math.min(mMax, Math.max(mMin, value));
            if (value != mValue) {
                mValue = value;
            }
            mFontSizePrefs.setUserFontScaleFactor(value);
            updatePreview();
            forceEnableZoomItem.setChecked(mFontSizePrefs.getForceEnableZoom());
        }
    }

    private float seekBarProgressToPrefValue(int seekBarProgress) {
        // SeekBar only supports integer steps, and always starts from 0.
        // So must convert floating point pref values to/from integers,
        // appropriately scaled for the SeekBar.
        return mMin + seekBarProgress * mStep;
    }

    private int prefValueToSeekBarProgress(float prefValue) {
        return Math.round((prefValue - mMin) / mStep);
    }

    private void updatePreview() {
        if (mPreview != null) {
            // Online body text tends to be around 13-16px. We ask the user to adjust the text scale
            // until 12px text is legible, that way all body text will be legible (and since font
            // boosting approximately preserves relative font size differences, other text will be
            // bigger/smaller as appropriate).
            final float smallestStandardWebPageFontSize = 12.0f; // CSS px
            mPreview.setTextSize(TypedValue.COMPLEX_UNIT_DIP,
                    smallestStandardWebPageFontSize * mFontSizePrefs.getFontScaleFactor());
        }
    }

}

