package com.ark.browser.utils;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;

/**
 * 当主题中用了<item name="android:windowTranslucentStatus">true</item>后，软键盘弹出就不会将输入框往上推了，
 * 该类可以解决这个问题。
 * http://stackoverflow.com/a/9108219/325479
 * https://github.com/mikepenz/MaterialDrawer/blob/aa9136fb4f5b3a80460fe5f47213985026d20c88/library/src/main/java/com/mikepenz/materialdrawer/util/KeyboardUtil.java
 */
public class WindowDisplayFrameObserver {


    private final View decorView;
    private final Rect r = new Rect();
    private final Callback<Rect> mCallback;
    private boolean isEnable = false;

    //a small helper to allow showing the editText focus
    private final ViewTreeObserver.OnGlobalLayoutListener onGlobalLayoutListener = new ViewTreeObserver.OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            //r will be populated with the coordinates of your view that area still visible.
            decorView.getWindowVisibleDisplayFrame(r);
            mCallback.onResult(r);
        }
    };

    public static WindowDisplayFrameObserver with(Window window, Callback<Rect> callback) {
        return new WindowDisplayFrameObserver(window, callback);
    }

    public WindowDisplayFrameObserver(Context context, Callback<Rect> callback) {
        this(ContextUtils.activityFromContext(context).getWindow(), callback);
    }

    public WindowDisplayFrameObserver(Window window, Callback<Rect> callback) {
        this.decorView = window.getDecorView();
        mCallback = callback;
    }

    public void enable() {
        if (!isEnable) {
            decorView.getViewTreeObserver().addOnGlobalLayoutListener(onGlobalLayoutListener);
            isEnable = true;
        }
    }

    public void disable() {
        if (isEnable) {
            decorView.getViewTreeObserver().removeOnGlobalLayoutListener(onGlobalLayoutListener);
            isEnable = false;
        }
    }

}

