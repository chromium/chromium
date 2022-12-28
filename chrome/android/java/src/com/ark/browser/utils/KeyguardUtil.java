package com.ark.browser.utils;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;

import com.zpj.toast.ZToast;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;

public final class KeyguardUtil {

    private static final int CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE = 2333;

    private final Activity mActivity;
    private CharSequence mTitle;
    private CharSequence mDescription;

    private KeyguardUtil(Activity activity) {
        mActivity = activity;
    }

    public static KeyguardUtil with(Activity activity) {
        return new KeyguardUtil(activity);
    }

    public KeyguardUtil setTitle(CharSequence mTitle) {
        this.mTitle = mTitle;
        return this;
    }

    public KeyguardUtil setDescription(CharSequence mDescription) {
        this.mDescription = mDescription;
        return this;
    }

    public void lockDevice() {
        KeyguardManager keyguardManager =
                (KeyguardManager) mActivity.getSystemService(Context.KEYGUARD_SERVICE);
        if (mDescription == null) {
            mDescription = mActivity.getString(R.string.lockscreen_description_export);
        }
        Intent intent =
                keyguardManager.createConfirmDeviceCredentialIntent(mTitle, mDescription);
        if (intent != null) {
            mActivity.startActivityForResult(intent, CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE);
            ZToast.error(mDescription.toString());
        } else {
            ZToast.error("该设备不支持查看密码！");
        }
    }

    public static boolean shouldHandleActivityResult(int requestCode, int resultCode) {
        if (requestCode == CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                ReauthenticationManager.recordLastReauth(
                        System.currentTimeMillis(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
            } else {
                ReauthenticationManager.resetLastReauth();
            }
            return true;
        }
        return false;
    }

}

