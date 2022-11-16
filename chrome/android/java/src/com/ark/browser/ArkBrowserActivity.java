package com.ark.browser;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.view.ViewCompat;

import com.ark.browser.core.utils.NavigationPredictorBridge;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.ui.fragment.ArkMainFragment;
import com.ark.browser.utils.ArkLogger;
import com.zpj.skin.SkinEngine;
import com.zpj.skin.SkinLayoutInflater;
import com.zpj.utils.StatusBarUtils;

import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.flags.ChromeSessionState;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

public class ArkBrowserActivity extends AsyncInitializationActivity {

    private static final String TAG = "BrowserActivity";

    private ArkMainFragment mFragment;

    private SkinLayoutInflater mLayoutInflater;

    @NonNull
    @Override
    public final LayoutInflater getLayoutInflater() {
        if (mLayoutInflater == null) {
            mLayoutInflater = new SkinLayoutInflater(this);
        }
        return mLayoutInflater;
    }

    @Override
    public final Object getSystemService(@NonNull String name) {
        if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
            return getLayoutInflater();
        }
        return super.getSystemService(name);
    }

    @Override
    protected void applyThemeOverlays() {
//        if (true) {
//            return;
//        }
//        super.applyThemeOverlays();

        SkinEngine.changeSkin(AppConfig.isNightMode() ? R.style.ArkNightTheme : R.style.ArkDayTheme);
        setTheme(SkinEngine.getSkin());
    }

    @Override
    protected void onPreCreate() {
        super.onPreCreate();

        translucentStatusBar(this, true);

        SkinEngine.changeSkin(AppConfig.isNightMode() ? R.style.ArkNightTheme : R.style.ArkDayTheme);

        getLayoutInflater();
        mLayoutInflater.applyCurrentSkin();
        // AppCompatActivity 需要设置
        AppCompatDelegate delegate = this.getDelegate();
        if (delegate instanceof LayoutInflater.Factory2) {
            mLayoutInflater.setFactory2((LayoutInflater.Factory2) delegate);
        }



        mFragment = findFragment(ArkMainFragment.class);
        if (mFragment == null) {
            mFragment = new ArkMainFragment();
        }
        getLifecycleDispatcher().register(mFragment);
    }

    @Override
    public void onPostCreate(@Nullable Bundle savedInstanceState, @Nullable PersistableBundle persistentState) {
        super.onPostCreate(savedInstanceState, persistentState);
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        ChromeSessionState.setIsInMultiWindowMode(
                MultiWindowUtils.getInstance().isInMultiWindowMode(this));

        ChromeSessionState.setDarkModeState(false, false);

        if (isWarmOnResume()) {
            NavigationPredictorBridge.onActivityWarmResumed();
        } else {
            NavigationPredictorBridge.onColdStart();
        }
    }

    @Override
    protected void performPreInflationStartup() {
        super.performPreInflationStartup();
        getWindow().setBackgroundDrawable(new ColorDrawable(getResources().getColor(R.color.light_background_color)));
    }

    @Override
    protected void triggerLayoutInflation() {
        try (TraceEvent te = TraceEvent.scoped("BrowserActivity.triggerLayoutInflation")) {
            SelectionPopupController.setShouldGetReadbackViewFromWindowAndroid();

            enableHardwareAcceleration();
//            setLowEndTheme();

            WarmupManager warmupManager = WarmupManager.getInstance();
            warmupManager.clearViewHierarchy();
            doLayoutInflation();
        }
    }

    @Override
    protected void onInitialLayoutInflationComplete() {

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();

        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        // Add a custom view right after the root view that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        InsetObserverView insetObserverView = InsetObserverView.create(this);
        rootView.addView(insetObserverView, 0);

        insetObserverView.addObserver(mFragment);

        super.onInitialLayoutInflationComplete();


        try (TraceEvent e = TraceEvent.scoped("BrowserActivity.startNativeInitialization")) {
            // This is on the critical path so don't delay.

            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::finishNativeInitialization);
        }
    }

    @Override
    public void initializeCompositor() {

    }

    @Override
    public void startNativeInitialization() {
//        super.startNativeInitialization();
    }

    @Override
    protected ArkWindowAndroid createWindowAndroid() {
        return mFragment.createWindowAndroid(this);
    }

    @Nullable
    @Override
    public ArkWindowAndroid getWindowAndroid() {
        return (ArkWindowAndroid) super.getWindowAndroid();
    }

    /**
     * This function implements the actual layout inflation, Subclassing Activities that override
     * this method without calling super need to call {@link #onInitialLayoutInflationComplete()}.
     */
    protected void doLayoutInflation() {
        ArkLogger.e(TAG, "doLayoutInflation");
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.doLayoutInflation")) {
            setContentView(R.layout.activity_browser);

            loadRootFragment(R.id.container_root, mFragment);

            onInitialLayoutInflationComplete();
        }
    }

//    private void setLowEndTheme() {
//        if (ActivityUtils.getThemeId() == R.style.Theme_Chromium_WithWindowAnimation_LowEnd) {
//            setTheme(R.style.Theme_Chromium_WithWindowAnimation_LowEnd);
//        }
//    }

    public Tab getActivityTab() {
        IPage page = TabListManager.getInstance().getCurrentPage();
        if (page != null) {
            return page.getNativePage();
        }
        return null;
    }


    @Override
    public final void onBackPressedSupport() {
        if (getSupportFragmentManager().getBackStackEntryCount() > 1) {
            pop();
        } else {
            getOnBackPressedDispatcher().onBackPressed();
        }
    }

    static void translucentStatusBar(Activity activity, boolean hideStatusBarBackground) {
        Window window = activity.getWindow();
        //添加Flag把状态栏设为可绘制模式
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        if (hideStatusBarBackground) {
            //如果为全透明模式，取消设置Window半透明的Flag
            window.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
            //设置状态栏为透明
            window.setStatusBarColor(Color.TRANSPARENT);
            //设置window的状态栏不可见
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        } else {
            //如果为半透明模式，添加设置Window半透明的Flag
            window.addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
            //设置系统状态栏处于可见状态
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
        }
        //view不根据系统窗口来调整自己的布局
        ViewGroup mContentView = (ViewGroup) window.findViewById(Window.ID_ANDROID_CONTENT);
        View mChildView = mContentView.getChildAt(0);
        if (mChildView != null) {
            ViewCompat.setFitsSystemWindows(mChildView, false);
            ViewCompat.requestApplyInsets(mChildView);
        }
    }

}
