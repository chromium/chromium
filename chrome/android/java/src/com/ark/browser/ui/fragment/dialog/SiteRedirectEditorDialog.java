package com.ark.browser.ui.fragment.dialog;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.settings.Keys;
import com.ark.browser.ui.fragment.settings.website.WebSiteRedirectFragment;
import com.ark.browser.ui.widget.EmptyAlertEditText;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;

import org.chromium.chrome.R;
import org.chromium.components.url_formatter.UrlFormatter;

public class SiteRedirectEditorDialog extends FitWindowOverDragBottomDialog<SiteRedirectEditorDialog>
        implements View.OnClickListener {

    private EmptyAlertEditText mTitleEditText;
    private EmptyAlertEditText mUrlEditText;

    private String mTargetUrl;
    private String mRedirectUrl;

    private boolean isAddMode;

    private Callback mCallback;

    public interface Callback {

        void onCallback(String target, String url);

    }

    public static SiteRedirectEditorDialog newInstance(WebSiteRedirectFragment.SiteRedirectItem item) {
        SiteRedirectEditorDialog fragment = newInstance();
        Bundle args = new Bundle();
        args.putString(Keys.KEY_TITLE, item.getTargetUrl());
        args.putString(Keys.KEY_LINK, item.getRedirectUrl());
        fragment.setArguments(args);
        return fragment;
    }

    public static SiteRedirectEditorDialog newInstance() {
        return new SiteRedirectEditorDialog();
    }

    public SiteRedirectEditorDialog setCallback(Callback mCallback) {
        this.mCallback = mCallback;
        return this;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (mCallback == null) {
            popThis();
            return;
        }

        if (getArguments() != null) {
            mTargetUrl = getArguments().getString(Keys.KEY_TITLE);
            mRedirectUrl = getArguments().getString(Keys.KEY_LINK);
        }

        isAddMode = TextUtils.isEmpty(mTargetUrl) || TextUtils.isEmpty(mRedirectUrl);

    }

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        lightStatusBar();
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_homepage_item_editor;
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        mTitleEditText = findViewById(R.id.title_text);
        mTitleEditText.setOnTouchListener((v, event) -> {
            mTitleEditText.setOnTouchListener(null);
            mTitleEditText.setCursorVisible(true);
            return false;
        });

        mUrlEditText = findViewById(R.id.url_text);
        mUrlEditText.setOnTouchListener((v, event) -> {
            mUrlEditText.setOnTouchListener(null);
            mUrlEditText.setCursorVisible(true);
            return false;
        });


        mTitleEditText.setText(mTargetUrl);
        mUrlEditText.setText(mRedirectUrl);

        TextView cancelBtn = findViewById(R.id.tv_cancel);
        TextView okBtn = findViewById(R.id.tv_ok);
        SkinEngine.setTextColor(cancelBtn, R.attr.textColorMajor);
        SkinEngine.setTextColor(okBtn, R.attr.colorPrimary);
        okBtn.setOnClickListener(this);
        cancelBtn.setOnClickListener(v -> dismiss());
        if (isAddMode) {
            okBtn.setText(R.string.text_add);
        } else {
            okBtn.setText(R.string.text_save);
        }
    }

    @Override
    public void onClick(View v) {
        String title = mTitleEditText.getTrimmedText();
        String url = mUrlEditText.getTrimmedText();
        if (TextUtils.isEmpty(title)) {
            ZToast.warning("目标链接不能为空！");
            return;
        }
        if (TextUtils.isEmpty(url)) {
            ZToast.warning("重定向链接不能为空！");
            return;
        }
        if (mCallback != null) {
            mCallback.onCallback(UrlFormatter.fixupUrl(title).getHost(),
                    UrlFormatter.fixupUrl(url).getHost());
        }
        dismiss();
    }

    @Override
    public void onDestroyView() {
        hideSoftInput();
        super.onDestroyView();
    }

}

