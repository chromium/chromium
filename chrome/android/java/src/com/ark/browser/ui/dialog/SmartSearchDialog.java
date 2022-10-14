package com.ark.browser.ui.dialog;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.PopupWindow;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatDialog;
import androidx.fragment.app.FragmentActivity;

import com.ark.browser.ui.widget.SmartSearchPanel;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.R;
import org.chromium.utils.ContextUtils;

public class SmartSearchDialog extends Dialog {

    private static final String TAG = "SmartSearchDialog";

    private SmartSearchPanel smartSearchPanel;
    private String keyword = "";

    private PopupWindow popupWindow;

    private final OnBackPressedCallback onBackPressedCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            dismiss();
        }
    };

    public SmartSearchDialog(@NonNull Context context) {
        super(context);
        ((FragmentActivity) ContextUtils.activityFromContext(context))
                .getOnBackPressedDispatcher()
                .addCallback(onBackPressedCallback);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.layout_smart_search);

        smartSearchPanel = findViewById(R.id.layout_smart_search);

        assert smartSearchPanel != null;
        smartSearchPanel.attachWindow(getWindow());
        smartSearchPanel.updateKeyword(keyword);
        smartSearchPanel.show();
    }

    @Override
    public void show() {
//        super.show();
//        Window window = getWindow();
//        if (window != null) {
//            WindowManager.LayoutParams params = window.getAttributes();
//            params.width = WindowManager.LayoutParams.MATCH_PARENT;
//            params.height = WindowManager.LayoutParams.MATCH_PARENT;
////            params.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
////            window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
//            //去除系统自带的margin
//            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
//            //背景全透明
//            window.setDimAmount(0f);
//            window.setAttributes(params);
////            window.getDecorView().clearFocus();
////            window.getDecorView().setFocusable(false);
////            window.getDecorView().setFocusableInTouchMode(false);
//        }
        onBackPressedCallback.setEnabled(true);


        Activity activity = ((Activity) ContextUtils.activityFromContext(getContext()));
        View parent = activity.getWindow().getDecorView();
        if (popupWindow == null) {
            View content = LayoutInflater.from(activity).inflate(
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
                        result = activity.dispatchTouchEvent(event);
                    }

//                    if (smartSearchPanel == null) {
//                        smartSearchPanel = popupWindow.getContentView().findViewById(R.id.layout_smart_search);
//                    }
//                    smartSearchPanel.dispatchTouchEvent(event);
//                    return true;
                    return true;
                }
            });
        }

        if (popupWindow.isShowing()) {
            return;
        }

        popupWindow.showAtLocation(parent, Gravity.CENTER, 0, 0);


        smartSearchPanel = popupWindow.getContentView().findViewById(R.id.layout_smart_search);

        assert smartSearchPanel != null;
        smartSearchPanel.attachWindow(getWindow());
        smartSearchPanel.updateKeyword(keyword);
        smartSearchPanel.show();


    }

    @Override
    public void dismiss() {
        onBackPressedCallback.setEnabled(false);
//        super.dismiss();
        if (smartSearchPanel != null) {
            smartSearchPanel.hide();
        }
        popupWindow.dismiss();
    }

//    @Override
//    public void hide() {
//        onBackPressedCallback.setEnabled(false);
//        super.hide();
//    }

//    @Override
//    public boolean dispatchTouchEvent(@NonNull MotionEvent ev) {
//        boolean result = super.dispatchTouchEvent(ev);
//        ArkLogger.e(TAG, "dispatchTouchEvent action=" + MotionEvent.actionToString(ev.getAction()) + " result=" + result + " context=" + getContext());
//        if (!result) {
//            MotionEvent e = MotionEvent.obtain(ev.getDownTime(), ev.getEventTime(), ev.getAction(),
//                    ev.getRawX(), ev.getRawY(), ev.getPressure(), ev.getSize(), ev.getMetaState(),
//                    ev.getXPrecision(), ev.getYPrecision(), ev.getDeviceId(), ev.getEdgeFlags());
//            return ContextUtils.activityFromContext(getContext()).dispatchTouchEvent(e);
//        }
//        return result;
//    }


    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (getWindow().superDispatchTouchEvent(event)) {
            return true;
        }
        return onTouchEvent(event);
    }

    public boolean onTouchEvent(@NonNull MotionEvent ev) {
        MotionEvent e = MotionEvent.obtain(ev.getDownTime(), ev.getEventTime(), ev.getAction(),
                ev.getRawX(), ev.getRawY(), ev.getMetaState());
        boolean result = ContextUtils.activityFromContext(getContext()).dispatchTouchEvent(e);
        ArkLogger.e(TAG, "onTouchEvent action=" + MotionEvent.actionToString(ev.getAction()) + " result=" + result);
        return result;
    }

    public void updateKeyword(String keyword) {
        if (smartSearchPanel != null) {
            smartSearchPanel.updateKeyword(keyword);
        } else {
            this.keyword = keyword;
        }
    }



}
