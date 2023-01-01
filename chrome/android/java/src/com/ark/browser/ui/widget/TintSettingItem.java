package com.ark.browser.ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;

import com.zpj.widget.setting.CommonSettingItem;

public class TintSettingItem extends CommonSettingItem {

    private Object mKey;

    public TintSettingItem(Context context) {
        super(context);
    }

    public TintSettingItem(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public TintSettingItem(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void setLeftIconTint(int color) {
        mLeftIconTintColor = color;
        if (inflatedLeftIcon instanceof ImageView && isEnabled()) {
            ((ImageView) inflatedLeftIcon).setColorFilter(mLeftIconTintColor);
        }
    }

    public void setKey(Object key) {
        this.mKey = key;
    }

    public Object getKey() {
        return mKey;
    }
}

