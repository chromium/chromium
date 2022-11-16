package com.ark.browser.ui.fragment.base;

import android.graphics.Color;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.settings.AppConfig;
import com.zpj.skin.SkinLayoutInflater;

public abstract class SkinFragment extends BaseFragment {

    protected void initStatusBar() {
        if (getTopFragment() == this && getTopChildFragment() == null && toolbar != null && toolbar.getVisibility() == View.VISIBLE) {
            if (AppConfig.isNightMode()) {
                lightStatusBar();
            } else {
                darkStatusBar();
            }
        }
    }

    @Override
    public void lightStatusBar() {
        super.lightStatusBar();
    }

    @Override
    public void darkStatusBar() {
        super.darkStatusBar();
    }

    private void setAndroidNativeLightStatusBar(boolean dark) {
        View decor = _mActivity.getWindow().getDecorView();
        if (dark) {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
        } else {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
        _mActivity.getWindow().setStatusBarColor(Color.TRANSPARENT);
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
//        blurBackground(view);
//        view.setBackgroundColor(SkinEngine.getColor(context, R.attr.backgroundColor));
//        SkinEngine.setBackground(view, R.attr.backgroundColor);
    }

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        initStatusBar();
    }

    @Override
    public void onDetach() {
        super.onDetach();
        LayoutInflater layoutInflater = getLayoutInflater();
        if(layoutInflater instanceof SkinLayoutInflater){
            SkinLayoutInflater skinLayoutInflater = (SkinLayoutInflater) layoutInflater;
            skinLayoutInflater.destory();
        }
    }

//    protected void blurBackground(View view) {
//        if (toolbar != null && toolbar.getVisibility() == View.VISIBLE) {
//            boolean isNight = AppConfig.isNightMode();
//            int color = isNight ? Color.BLACK : Color.WHITE;
//            float alpha = isNight ? 0.8f : 0.8f;
//            ZBlurry.with(_mActivity.findViewById(R.id.wallpaper))
////                .setObserveView(view)
//                    .antiAlias(true)
//                    .foregroundColor(ColorUtils.alphaColor(color, alpha))
//                    .scale(0.4f)
//                    .radius(8)
//                    .blur(new ZBlurry.Callback() {
//                        @Override
//                        public void down(Bitmap bitmap) {
//                            view.setBackground(new BitmapDrawable(bitmap));
//                        }
//                    });
//            toolbar.setBackground(new ColorDrawable(Color.TRANSPARENT), true);
//            toolbar.setLightStyle(isNight);
//        }
//    }

//    public static void start(BaseFragment fragment) {
//        ZBus.post(fragment);
//    }

}
