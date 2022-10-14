package com.ark.browser.ui.dialog;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.Gravity;
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

import org.chromium.chrome.R;
import org.chromium.utils.ContextUtils;

public class SmartSearchPopupWindow {

    private static final String TAG = "SmartSearchDialog";

    private final Context mContext;

    private String keyword = "";

    private final PopupWindow popupWindow;
    private final SmartSearchPanel smartSearchPanel;

    private final OnBackPressedCallback onBackPressedCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            dismiss();
        }
    };

    public SmartSearchPopupWindow(@NonNull Context context) {
        this.mContext = context;


        View content = LayoutInflater.from(context).inflate(
                R.layout.layout_smart_search, null, false);
        popupWindow = new PopupWindow(content, WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT, true);
        popupWindow.setElevation(1);
        popupWindow.setFocusable(true);
        popupWindow.setTouchable(true);
        popupWindow.setOutsideTouchable(false);
        popupWindow.setClippingEnabled(false);

        popupWindow.setTouchInterceptor(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                boolean result = popupWindow.getContentView().dispatchTouchEvent(event);
                ArkLogger.e(TAG, "onTouch action=" + MotionEvent.actionToString(event.getAction()) + " result=" + result);
                if (!result) {
                    Activity activity = ContextUtils.activityFromContext(mContext);
                    if (activity != null) {
                        activity.dispatchTouchEvent(event);
                    }
                }
                return true;
            }
        });


        smartSearchPanel = content.findViewById(R.id.layout_smart_search);

//        Activity activity = ((Activity) ContextUtils.activityFromContext(mContext));
//        smartSearchPanel.attachWindow(activity.getWindow());
        smartSearchPanel.updateKeyword(keyword);
    }

    public void setOnDismissListener(PopupWindow.OnDismissListener onDismissListener) {
        popupWindow.setOnDismissListener(onDismissListener);
    }

    public boolean isShowing() {
        return popupWindow.isShowing();
    }

    public void show() {
        if (popupWindow.isShowing()) {
            return;
        }

        FragmentActivity activity = (FragmentActivity) ContextUtils.activityFromContext(mContext);
        activity.getOnBackPressedDispatcher().addCallback(onBackPressedCallback);
        onBackPressedCallback.setEnabled(true);
        View parent = activity.getWindow().getDecorView();
        popupWindow.showAtLocation(parent, Gravity.CENTER, 0, 0);
        smartSearchPanel.show();

    }

    public void dismiss() {
        if (!popupWindow.isShowing()) {
            return;
        }

        ArkLogger.e(TAG, "dismiss");
        onBackPressedCallback.setEnabled(false);
        onBackPressedCallback.remove();
        popupWindow.dismiss();
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
