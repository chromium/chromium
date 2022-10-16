package com.ark.browser.ui.dialog;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.PopupWindow;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;

import com.ark.browser.ui.widget.SmartSearchPanel;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.utils.ContextUtils;

public class SmartSearchPopupWindow extends PopupWindow {

    private static final String TAG = "SmartSearchPopupWindow";

    private final Context mContext;

    private String keyword = "";

    private final SmartSearchPanel smartSearchPanel;

    private final OnBackPressedCallback onBackPressedCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            ArkLogger.e(TAG, "handleOnBackPressed isShowing=" +
                    isShowing() + " isEnabled=" + isEnabled());
            dismiss();
        }
    };

    private View.OnTouchListener mTouchListener;

    @SuppressLint("ClickableViewAccessibility")
    public SmartSearchPopupWindow(@NonNull Context context) {
        this.mContext = context;


        View content = LayoutInflater.from(context).inflate(
                R.layout.layout_smart_search, null, false);

        setContentView(content);
        setWidth(WindowManager.LayoutParams.MATCH_PARENT);
        setHeight(WindowManager.LayoutParams.MATCH_PARENT);
        setElevation(1);
        setFocusable(true);
        setTouchable(true);
        setOutsideTouchable(false);
        setClippingEnabled(false);

        setTouchInterceptor((v, event) -> {
            boolean result = getContentView().dispatchTouchEvent(event);
            ArkLogger.e(TAG, "onTouch action=" + MotionEvent.actionToString(event.getAction()) + " result=" + result);
            if (!result) {
                if (mTouchListener != null) {
                    mTouchListener.onTouch(v, event);
                }
            }
            return true;
        });


        smartSearchPanel = content.findViewById(R.id.layout_smart_search);

//        Activity activity = ((Activity) ContextUtils.activityFromContext(mContext));
//        smartSearchPanel.attachWindow(activity.getWindow());
        smartSearchPanel.updateKeyword(keyword);
    }

    public void setOnPanelStateChangedListener(SmartSearchPanel.OnPanelStateChangedListener listener) {
        this.smartSearchPanel.setOnPanelStateChangedListener(listener);
    }

    public void setOnTouchListener(View.OnTouchListener listener) {
        this.mTouchListener = listener;
    }

    public void show() {
        if (isShowing()) {
            return;
        }

        FragmentActivity activity = (FragmentActivity) ContextUtils.activityFromContext(mContext);
        activity.getOnBackPressedDispatcher().addCallback(onBackPressedCallback);
        onBackPressedCallback.setEnabled(true);
        View parent = activity.getWindow().getDecorView();
        showAtLocation(parent, Gravity.CENTER, 0, 0);
        smartSearchPanel.show();
    }

    @Override
    public void dismiss() {
        if (!isShowing()) {
            return;
        }

        if (smartSearchPanel.onBackPressed()) {
            return;
        }

        ArkLogger.e(TAG, "dismiss");
        onBackPressedCallback.setEnabled(false);
        onBackPressedCallback.remove();
        super.dismiss();
        smartSearchPanel.hide();
    }

//    public void hide() {
//
//    }

    public void updateKeyword(String keyword) {
        if (smartSearchPanel != null) {
            smartSearchPanel.updateKeyword(keyword);
        } else {
            this.keyword = keyword;
        }
    }

}
