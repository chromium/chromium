package com.ark.browser.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import com.zpj.skin.SkinEngine;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * @author Z-P-J
 * @date 2019/5/17 16:14
 */
public class CheckLayout extends FrameLayout {

    public interface OnCheckedChangeListener{
        void onCheckedChanged(CheckLayout checkLayout, boolean isChecked);
    }

    private ImageView iconView;
    private TextView titleView;
    private TextView contentView;
    private ZCheckBox checkBox;

    private boolean checked;
    private String title;
    private int titleTextColor;
    private int titleTextSize;
    private String content;
    private int contentTextColor;
    private int contentTextSize;

    private OnCheckedChangeListener onCheckedChangeListener;

    public CheckLayout(Context context) {
        super(context);
        init(context, null);
    }

    public CheckLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public CheckLayout(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    private void init(Context context, AttributeSet attrs) {
        View view = LayoutInflater.from(context).inflate(R.layout.layout_check, this, true);
        iconView = view.findViewById(R.id.icon_view);
        titleView = view.findViewById(R.id.title_view);
        contentView = view.findViewById(R.id.content_view);
        checkBox = view.findViewById(R.id.check_box);
        final TypedArray typedArray = context.obtainStyledAttributes(attrs, R.styleable.CheckLayout);
//            checked = typedArray.getBoolean(R.styleable.CheckLayout_box_checked, false);
        title = typedArray.getString(R.styleable.CheckLayout_check_title);
        titleTextColor = typedArray.getColor(R.styleable.CheckLayout_title_text_color, ApiCompatibilityUtils.getColor(getResources(), R.color.google_black_400));
        titleTextSize = typedArray.getInt(R.styleable.CheckLayout_title_text_size, 16);
        content = typedArray.getString(R.styleable.CheckLayout_check_content);
        contentTextColor = typedArray.getColor(R.styleable.CheckLayout_content_text_color, ApiCompatibilityUtils.getColor(getResources(), R.color.google_grey_500));
        contentTextSize = typedArray.getInt(R.styleable.CheckLayout_content_text_size, 14);
        Drawable icon = typedArray.getDrawable(R.styleable.CheckLayout_check_icon);
        typedArray.recycle();
//            checkBox.setChecked(checked, true);
        titleView.setText(title);
        titleView.setTextColor(titleTextColor);
        titleView.setTextSize(titleTextSize);
        contentView.setText(content);
        contentView.setTextColor(contentTextColor);
        contentView.setTextSize(contentTextSize);
        if (TextUtils.isEmpty(content)) {
            contentView.setVisibility(GONE);
        }
        if (icon == null) {
            iconView.setVisibility(GONE);
        }
        checkBox.setCheckedColor(SkinEngine.getColor(context, android.R.attr.colorPrimary));
        checkBox.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                performClick();
            }
        });
        setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                checkBox.setChecked(!checked, true);
                checked = !checked;
                if (onCheckedChangeListener != null) {
                    onCheckedChangeListener.onCheckedChanged(CheckLayout.this, checked);
                }
            }
        });
    }

    public void setTitle(String title) {
        this.title = title;
        titleView.setText(title);
    }

    public void setTitleTextColor(int titleTextColor) {
        this.titleTextColor = titleTextColor;
        titleView.setTextColor(titleTextColor);
        iconView.setColorFilter(titleTextColor);
    }

    public void setTitleTextSize(int titleTextSize) {
        this.titleTextSize = titleTextSize;
        titleView.setTextSize(titleTextSize);
    }

    public void setContent(String content) {
        this.content = content;
        contentView.setText(content);
        contentView.setVisibility(TextUtils.isEmpty(content) ? GONE : VISIBLE);
    }

    public void setChecked(boolean checked) {
        this.checked = checked;
        checkBox.setChecked(checked, true);
    }

    public void setOnCheckedChangeListener(OnCheckedChangeListener onCheckedChangeListener) {
        this.onCheckedChangeListener = onCheckedChangeListener;
    }

    public void setContentTextSize(int textSize) {
        this.contentTextSize = textSize;
        contentView.setTextSize(textSize);
    }

    public void setContentTextColor(int textColor) {
        this.contentTextColor = textColor;
        contentView.setTextColor(textColor);
    }

    public void setIcon(@DrawableRes int resId) {
        iconView.setImageResource(resId);
        iconView.setVisibility(VISIBLE);
        iconView.setColorFilter(titleTextColor);
    }

    public boolean isChecked() {
        return checked;
    }
}
