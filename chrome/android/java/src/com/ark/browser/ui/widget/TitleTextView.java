package com.ark.browser.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;

import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;

/**
 * @author Z-P-J
 */
public class TitleTextView extends LinearLayout {

    private TextView mTvTitle;
    private TextView mTvText;

    public TitleTextView(Context context) {
        this(context, null);
    }

    public TitleTextView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TitleTextView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    private void init(Context context, AttributeSet attrs) {
        View view = LayoutInflater.from(context).inflate(R.layout.layout_title_text, this, true);
        if (attrs != null) {
            mTvTitle = view.findViewById(R.id.tv_title);
            mTvText = view.findViewById(R.id.tv_text);
            mTvText.setOnLongClickListener(new OnLongClickListener() {
                @Override
                public boolean onLongClick(View v) {
                    Clipboard.getInstance().setTextFromUser(mTvText.getText().toString());
                    ZToast.success("已复制到剪贴板！");
                    return true;
                }
            });
            final TypedArray typedArray = context.obtainStyledAttributes(attrs, R.styleable.DetailLayout);
            String leftText = typedArray.getString(R.styleable.DetailLayout_left_text);
            String rightText = typedArray.getString(R.styleable.DetailLayout_right_text);
            int rightTextMaxems = typedArray.getInt(R.styleable.DetailLayout_right_text_maxEms, 10);
            int rightTextMaxlines = typedArray.getInt(R.styleable.DetailLayout_right_text_maxLines, 3);
//            int right_text_lineSpacingExtra = typedArray.getInt(R.styleable.DetailLayout_right_text_lineSpacingExtra, 5);
            int leftTextColor = typedArray.getColor(R.styleable.DetailLayout_right_text_color, Color.BLACK);
            int rightTextColor = typedArray.getColor(R.styleable.DetailLayout_right_text_color, Color.BLACK);

            typedArray.recycle();
            SkinEngine.setTextColor(mTvTitle,  R.attr.textColorMajor);
            SkinEngine.setTextColor(mTvText, R.attr.textColorMajor);
            mTvTitle.setText(leftText + ":");
//            detailTitle.setTextColor(leftTextColor);
            mTvText.setText(rightText);
            mTvText.setMaxEms(rightTextMaxems);
            mTvText.setMaxLines(rightTextMaxlines);
//            detailContent.setTextColor(rightTextColor);
            mTvText.setEllipsize(TextUtils.TruncateAt.END);
        }
    }

    public void setTitle(String title) {
        mTvTitle.setText(title + ":");
    }

    public void setText(String text) {
        if (text == null) {
            text = "无";
        }
        mTvText.setText(text);
    }

}

