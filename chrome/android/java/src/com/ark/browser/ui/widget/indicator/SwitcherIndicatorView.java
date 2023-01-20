package com.ark.browser.ui.widget.indicator;

import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.zpj.skin.SkinEngine;

import net.lucode.hackware.magicindicator.buildins.commonnavigator.titles.CommonPagerTitleView;

import org.chromium.chrome.R;

public class SwitcherIndicatorView extends CommonPagerTitleView {

    private final TextView tvTitle;
    private final TextView tvCount;

    private int mSelectedColor;
    protected int mNormalColor;

    private int mSubSelectedColor;
    protected int mSubNormalColor;

    public SwitcherIndicatorView(Context context) {
        super(context);

        mNormalColor = mSelectedColor = Color.WHITE;

        mSubNormalColor = SkinEngine.getColor(context, R.attr.textColorMinor);
//        mSubSelectedColor = SkinEngine.getColor(context, R.attr.textColorMajor);
        mSubSelectedColor = Color.WHITE;

        View view = LayoutInflater.from(context).inflate(R.layout.layout_switcher_indicator, null, false);
        LayoutParams params = new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        addView(view, params);

        tvTitle = view.findViewById(R.id.tv_title);
        tvCount = view.findViewById(R.id.tv_count);

    }

}
