package com.ark.browser.ui.dialog;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SearchEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

public class TestWindow implements Window.Callback {

    private final Context mContext;
    private final WindowManager mWindowManager;
    private final Window mWindow;

    private boolean mCreated = false;
    private boolean mShowing = false;

    View mDecor;

    public TestWindow(Context context) {
        this.mContext = context;
        mWindowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);


        try {
            Class clazz = Class.forName("com.android.internal.policy.PhoneWindow");
            Constructor<?> constructor = clazz.getDeclaredConstructor(Context.class);
            final Window w = (Window) constructor.newInstance(context);
            mWindow = w;
            w.setCallback(this);
//        w.setOnWindowDismissedCallback(this);
//        w.setOnWindowSwipeDismissedCallback(() -> {
//            if (mCancelable) {
//                cancel();
//            }
//        });
            w.setWindowManager(mWindowManager, null, null);
            w.setGravity(Gravity.CENTER);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }


    }

    public <T extends View> T findViewById(int id) {
        return mWindow.findViewById(id);
    }

    public Window getWindow() {
        return mWindow;
    }

    public Context getContext() {
        return mContext;
    }

    public boolean isShowing() {
        return mShowing;
    }

    public void setContentView(int layoutResID) {
        mWindow.setContentView(layoutResID);
    }

    protected void onCreate(Bundle savedInstanceState) {
    }

    public void show() {


        if (!mCreated) {
            onCreate(null);
            mCreated = true;
        } else {
            // Fill the DecorView in on any configuration changes that
            // may have occured while it was removed from the WindowManager.
            final Configuration config = mContext.getResources().getConfiguration();
            mWindow.getDecorView().dispatchConfigurationChanged(config);
        }

        mDecor = mWindow.getDecorView();

        WindowManager.LayoutParams l = mWindow.getAttributes();
        boolean restoreSoftInputMode = false;
        if ((l.softInputMode
                & WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION) == 0) {
            l.softInputMode |=
                    WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION;
            restoreSoftInputMode = true;
        }

        mWindowManager.addView(mDecor, l);

        if (restoreSoftInputMode) {
            l.softInputMode &=
                    ~WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION;
        }
        mShowing = true;
    }

    public void dismiss() {
        if (mDecor == null || !mShowing) {
            return;
        }

        try {
            Field destroyedField = mWindow.getClass().getDeclaredField("mDestroyed");
            destroyedField.setAccessible(true);
            boolean isDestroyed = (boolean) destroyedField.get(mWindow);
            if (isDestroyed) {
                return;
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        try {
            mWindowManager.removeViewImmediate(mDecor);
        } finally {
            mDecor = null;
            mWindow.closeAllPanels();
            mShowing = false;
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return mWindow.superDispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchKeyShortcutEvent(KeyEvent event) {
        return mWindow.superDispatchKeyShortcutEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        return mWindow.superDispatchTouchEvent(event);
    }

    @Override
    public boolean dispatchTrackballEvent(MotionEvent event) {
        return mWindow.superDispatchTrackballEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        return mWindow.superDispatchGenericMotionEvent(event);
    }

    @Override
    public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent event) {
        event.setClassName(getClass().getName());
        event.setPackageName(mContext.getPackageName());

        ViewGroup.LayoutParams params = mWindow.getAttributes();
        boolean isFullScreen = (params.width == ViewGroup.LayoutParams.MATCH_PARENT) &&
                (params.height == ViewGroup.LayoutParams.MATCH_PARENT);
        event.setFullScreen(isFullScreen);

        return false;
    }

    @Nullable
    @Override
    public View onCreatePanelView(int featureId) {
        return null;
    }

    @Override
    public boolean onCreatePanelMenu(int featureId, @NonNull Menu menu) {
        return false;
    }

    @Override
    public boolean onPreparePanel(int featureId, @Nullable View view, @NonNull Menu menu) {
        return false;
    }

    @Override
    public boolean onMenuOpened(int featureId, @NonNull Menu menu) {
        return false;
    }

    @Override
    public boolean onMenuItemSelected(int featureId, @NonNull MenuItem item) {
        return false;
    }

    @Override
    public void onWindowAttributesChanged(WindowManager.LayoutParams params) {
        if (mDecor != null) {
            mWindowManager.updateViewLayout(mDecor, params);
        }
    }

    @Override
    public void onContentChanged() {

    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {

    }

    @Override
    public void onAttachedToWindow() {

    }

    @Override
    public void onDetachedFromWindow() {

    }

    @Override
    public void onPanelClosed(int featureId, @NonNull Menu menu) {

    }

    @Override
    public boolean onSearchRequested() {
        return false;
    }

    @Override
    public boolean onSearchRequested(SearchEvent searchEvent) {
        return false;
    }

    @Nullable
    @Override
    public ActionMode onWindowStartingActionMode(ActionMode.Callback callback) {
        return null;
    }

    @Nullable
    @Override
    public ActionMode onWindowStartingActionMode(ActionMode.Callback callback, int type) {
        return null;
    }

    @Override
    public void onActionModeStarted(ActionMode mode) {

    }

    @Override
    public void onActionModeFinished(ActionMode mode) {

    }
}
