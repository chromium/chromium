package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.ScreenUtils;

import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

public class ClipboardInterceptDialog extends OverDragBottomDialogFragment<ClipboardInterceptDialog>
        implements View.OnClickListener {

    public static final String KEY_TEXT = "key_text";

    private String mText;

    public static ClipboardInterceptDialog newInstance(String text) {
        Bundle args = new Bundle();
        args.putString(KEY_TEXT, text);
        ClipboardInterceptDialog fragment = new ClipboardInterceptDialog();
        fragment.setArguments(args);
        return fragment;
    }

    public ClipboardInterceptDialog() {
        setMarginTop(ScreenUtils.getStatusBarHeight());
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_clipboard_intercept;
    }

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        lightStatusBar();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                mText = getArguments().getString(KEY_TEXT);
            }
        } else {
            mText = savedInstanceState.getString(KEY_TEXT);
        }
        if (mText == null) {
            popThis();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(KEY_TEXT, mText);
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        ImageButton btnCopy = findViewById(R.id.btn_copy);
        btnCopy.setOnClickListener(this);

        ImageButton btnShare = findViewById(R.id.btn_share);
        btnShare.setOnClickListener(this);

        TextView tvContent = findViewById(R.id.tv_content);
        tvContent.setText(mText);

        TextView cancelBtn = findViewById(R.id.tv_cancel);
        TextView okBtn = findViewById(R.id.tv_ok);
        okBtn.setText("拦截");
        cancelBtn.setText(R.string.text_copy);
        SkinEngine.setTextColor(cancelBtn, R.attr.textColorMajor);
        SkinEngine.setTextColor(okBtn, R.attr.colorPrimary);

        okBtn.setOnClickListener(this);
        cancelBtn.setOnClickListener(this);

    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.btn_copy || id == R.id.tv_cancel) {
            if (Clipboard.getInstance().setTextFromUser(mText)) {
                ZToast.success("已复制到剪贴板！");
            } else {
                ZToast.error(R.string.copy_to_clipboard_failure_message);
            }
            dismiss();
        } else if (id == R.id.btn_share) {
            ShareParams.Builder builder = new ShareParams.Builder(_mActivity, mText, mText)
                    .setShareDirectly(false)
                    .setSaveLastUsed(true);
            ShareHelper.share(builder.build());
        } else if (id == R.id.tv_ok) {
            dismiss();
        }
    }
}

