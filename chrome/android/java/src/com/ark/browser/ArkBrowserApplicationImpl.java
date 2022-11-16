package com.ark.browser;

import android.content.res.TypedArray;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import com.ark.browser.ui.widget.CheckLayout;
import com.ark.browser.ui.widget.DrawableTintTextView;
import com.ark.browser.ui.widget.ShadowLayout;
import com.zpj.progressbar.ZProgressBar;
import com.zpj.skin.SkinEngine;
import com.zpj.skin.applicator.SkinViewApplicator;
import com.zpj.statemanager.CustomizedViewHolder;
import com.zpj.statemanager.StateManager;
import com.zpj.utils.ScreenUtils;
import com.zpj.widget.setting.SimpleSettingItem;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplicationImpl;

public class ArkBrowserApplicationImpl extends ChromeApplicationImpl {

    @Override
    public void onCreate() {
        super.onCreate();

        SkinEngine.registerSkinApplicator(CheckLayout.class, new CheckLayoutApplicator());
        SkinEngine.registerSkinApplicator(SimpleSettingItem.class, new SimpleSettingItemApplicator());
        SkinEngine.registerSkinApplicator(SwitchSettingItem.class, new SettingItemApplicator());
        SkinEngine.registerSkinApplicator(DrawableTintTextView.class, new DrawableTintTextViewApplicator());
        SkinEngine.registerSkinApplicator(ShadowLayout.class, new ShadowLayoutApplicator());
        SkinEngine.registerSkinApplicator(View.class, new BackgroundLayoutApplicator());

        StateManager.config()
                .setLoadingViewHolder(new CustomizedViewHolder() {
                    @Override
                    public void onViewCreated(View view) {
                        ZProgressBar progressBar = new ZProgressBar(context);
                        progressBar.setProgressBarColor(context.getResources().getColor(R.color.colorPrimary));
                        progressBar.setBorderColor(Color.TRANSPARENT);
                        progressBar.setProgressBarRadius(ScreenUtils.dp2px(18));
                        float width = ScreenUtils.dp2px(2.5f);
                        progressBar.setProgressBarWidth(width);
                        progressBar.setBorderWidth(width);

                        int size = ScreenUtils.dp2pxInt(56);
                        ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(size, size);
                        progressBar.setLayoutParams(params);
                        this.container.addView(progressBar);
                        addTextViewWithPadding(R.string._text_loading, Color.GRAY);
                    }
                });
    }

    public static class CheckLayoutApplicator extends SkinViewApplicator {
        public CheckLayoutApplicator() {
            super();
            addAttributeApplicator("title_text_color", new IAttributeApplicator<CheckLayout>() {
                @Override
                public void onApply(CheckLayout view, TypedArray typedArray, int typedArrayIndex) {
                    view.setTitleTextColor(typedArray.getColor(typedArrayIndex, view.getContext().getResources().getColor(R.color.color_text_major)));
                }
            });
            addAttributeApplicator("content_text_color", new IAttributeApplicator<CheckLayout>() {
                @Override
                public void onApply(CheckLayout view, TypedArray typedArray, int typedArrayIndex) {
                    view.setContentTextColor(typedArray.getColor(typedArrayIndex, view.getContext().getResources().getColor(R.color.color_text_major)));
                }
            });
        }
    }

    public static class SimpleSettingItemApplicator extends SkinViewApplicator {
        public SimpleSettingItemApplicator() {
            super();
            addAttributeApplicator("z_setting_titleTextColor", new IAttributeApplicator<SimpleSettingItem>() {
                @Override
                public void onApply(SimpleSettingItem view, TypedArray typedArray, int typedArrayIndex) {
                    view.setTextColor(typedArray.getColorStateList(typedArrayIndex));
                }
            });
        }
    }

    public static class SettingItemApplicator extends SkinViewApplicator {
        public SettingItemApplicator() {
            super();
            addAttributeApplicator("z_setting_titleTextColor", new IAttributeApplicator<SwitchSettingItem>() {
                @Override
                public void onApply(SwitchSettingItem view, TypedArray typedArray, int typedArrayIndex) {
                    view.setTitleTextColor(typedArray.getColor(typedArrayIndex, view.getContext().getResources().getColor(R.color.color_text_major)));
                }
            });
        }
    }

    public static class DrawableTintTextViewApplicator extends SkinViewApplicator {
        public DrawableTintTextViewApplicator() {
            super();
            addAttributeApplicator("drawable_tint_color", new IAttributeApplicator<DrawableTintTextView>() {
                @Override
                public void onApply(DrawableTintTextView view, TypedArray typedArray, int typedArrayIndex) {
                    view.setTint(typedArray.getColor(typedArrayIndex, view.getContext().getResources().getColor(R.color.color_text_major)));
                }
            });
        }
    }

    public static class ShadowLayoutApplicator extends SkinViewApplicator {
        public ShadowLayoutApplicator() {
            super();

            addAttributeApplicator("hl_shadowBackColor", new IAttributeApplicator<ShadowLayout>() {
                @Override
                public void onApply(ShadowLayout view, TypedArray typedArray, int typedArrayIndex) {
                    int color = typedArray.getColor(typedArrayIndex, Color.WHITE);
                    view.setBackgroundColor(color);
                }
            });

        }
    }

    public static class BackgroundLayoutApplicator extends SkinViewApplicator {
        public BackgroundLayoutApplicator() {
            super();

            addAttributeApplicator("backgroundTint", new IAttributeApplicator<View>() {
                @Override
                public void onApply(View view, TypedArray typedArray, int typedArrayIndex) {
                    view.setBackgroundTintList(typedArray.getColorStateList(typedArrayIndex));
                }
            });

        }
    }

}
