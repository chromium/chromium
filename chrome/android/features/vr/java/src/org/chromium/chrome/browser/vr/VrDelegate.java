// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.page_info.VrHandler;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.reflect.Method;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Delegate to call into VR. */
public abstract class VrDelegate implements VrHandler, BackPressHandler {
    private static final String TAG = "VrDelegate";
    private static final String VR_BOOT_SYSTEM_PROPERTY = "ro.boot.vr";
    private static final String SAMSUNG_GALAXY_PREFIX = "SM-";
    private static final Set<String> SAMSUNG_GALAXY_8_MODELS =
            Collections.unmodifiableSet(CollectionUtil.newHashSet("G950", "N950", "G955", "G892"));
    private static final Set<String> SAMSUNG_GALAXY_8_ALT_MODELS = Collections.unmodifiableSet(
            CollectionUtil.newHashSet("SC-02J", "SCV36", "SC-03J", "SCV35", "SC-01K", "SCV37"));
    /* package */ static final boolean DEBUG_LOGS = false;
    /* package */ static final int VR_SYSTEM_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
    // Android N doesn't allow us to dynamically control the preview window based on headset mode,
    // so we used an animation to hide the preview window instead.
    /* package */ static final boolean USE_HIDE_ANIMATION =
            Build.VERSION.SDK_INT < Build.VERSION_CODES.O;

    private static Boolean sBootsToVr;

    public abstract void forceExitVrImmediately();
    public abstract boolean onActivityResultWithNative(int requestCode, int resultCode);
    public abstract void onNativeLibraryAvailable();
    @Override
    public abstract boolean isInVr();
    public abstract boolean canLaunch2DIntents();
    public abstract boolean onBackPressed();
    public abstract boolean enterVrIfNecessary();
    public abstract void maybeRegisterVrEntryHook(final Activity activity);
    public abstract void maybeUnregisterVrEntryHook();
    public abstract void onMultiWindowModeChanged(boolean isInMultiWindowMode);
    public abstract void requestToExitVrForSearchEnginePromoDialog(
            OnExitVrRequestListener listener, Activity activity);
    public abstract void requestToExitVr(OnExitVrRequestListener listener);
    public abstract void requestToExitVr(
            OnExitVrRequestListener listener, @UiUnsupportedMode int reason);
    public abstract void requestToExitVrAndRunOnSuccess(Runnable onSuccess);
    public abstract void requestToExitVrAndRunOnSuccess(
            Runnable onSuccess, @UiUnsupportedMode int reason);
    public abstract void onActivityShown(Activity activity);
    public abstract void onActivityHidden(Activity activity);
    public abstract boolean onDensityChanged(int oldDpi, int newDpi);
    public abstract void rawTopContentOffsetChanged(float topContentOffset);
    public abstract void onNewIntentWithNative(Activity activity, Intent intent);
    public abstract void maybeHandleVrIntentPreNative(Activity activity, Intent intent);

    public abstract void setVrModeEnabled(Activity activity, boolean enabled);
    public abstract void doPreInflationStartup(Activity activity, Bundle savedInstanceState);

    public boolean bootsToVr() {
        if (sBootsToVr == null) {
            // TODO(mthiesse): Replace this with a Daydream API call when supported.
            // Note that System.GetProperty is unable to read system ro properties, so we have to
            // resort to reflection as seen below. This method of reading system properties has been
            // available since API level 1.
            sBootsToVr = getIntSystemProperty(VR_BOOT_SYSTEM_PROPERTY, 0) == 1;
        }
        return sBootsToVr;
    }

    public abstract boolean isDaydreamReadyDevice();
    public abstract boolean isDaydreamCurrentViewer();

    public boolean willChangeDensityInVr(WindowAndroid window) {
        // Only N+ support launching in VR at all, other OS versions don't care about this.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return false;

        // If the screen density changed while in VR, we have to disable the VR browser as java UI
        // used or created by VR browsing will be broken.
        // TODO: make work.
        if (expectedDensityChange()) return true;
        if (!isDaydreamReadyDevice()) return false;

        Context context = window.getContext().get();
        if (context == null) return true;

        DisplayAndroid display = window.getDisplay();
        int widthPixels = display.getDisplayWidth();
        int heightPixels = display.getDisplayHeight();
        int densityDpi;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            densityDpi = context.getResources().getConfiguration().densityDpi;
        } else {
            DisplayMetrics metrics = new DisplayMetrics();
            DisplayAndroidManager.getDefaultDisplayForContext(context).getRealMetrics(metrics);
            densityDpi = metrics.densityDpi;
        }

        int currentDensityDpi = context.getResources().getConfiguration().densityDpi;
        if (currentDensityDpi != 0 && currentDensityDpi != densityDpi) {
            return true;
        }

        if (!deviceCanChangeResolutionForVr()) return false;

        List<Display.Mode> modes = display.getSupportedModes();
        // Devices with only one mode won't switch modes while in VR.
        if (modes.size() <= 1) return false;
        Display.Mode vr_mode = modes.get(0);
        for (int i = 1; i < modes.size(); ++i) {
            if (modes.get(i).getPhysicalWidth() > vr_mode.getPhysicalWidth()) {
                vr_mode = modes.get(i);
            }
        }

        // If we're currently in the mode supported by VR the density won't change.
        // We actually can't use display.getMode() to get the current mode as that just always
        // returns the same mode ignoring the override, so we just check that our current display
        // size is not equal to the vr mode size.
        if (vr_mode.getPhysicalWidth() != widthPixels
                && vr_mode.getPhysicalWidth() != heightPixels) {
            return true;
        }
        if (vr_mode.getPhysicalHeight() != widthPixels
                && vr_mode.getPhysicalHeight() != heightPixels) {
            return true;
        }
        return false;
    }

    public abstract void onSaveInstanceState(Bundle outState);

    @Override
    public void exitVrAndRun(Runnable r, @VrHandler.UiType int uiType) {
        assert (isInVr());
        switch (uiType) {
            case UiType.CERTIFICATE_INFO:
                requestToExitVrAndRunOnSuccess(r, UiUnsupportedMode.UNHANDLED_CERTIFICATE_INFO);
                return;
            case UiType.CONNECTION_SECURITY_INFO:
                requestToExitVrAndRunOnSuccess(
                        r, UiUnsupportedMode.UNHANDLED_CONNECTION_SECURITY_INFO);
                return;
            default:
                assert false : "Unrecognized uiType";
                return;
        }
    }

    public void setSystemUiVisibilityForVr(Activity activity) {
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        int flags = activity.getWindow().getDecorView().getSystemUiVisibility();
        activity.getWindow().getDecorView().setSystemUiVisibility(flags | VR_SYSTEM_UI_FLAGS);
    }

    public void addBlackOverlayViewForActivity(Activity activity) {
        View overlay = activity.getWindow().findViewById(R.id.vr_overlay_view);
        if (overlay != null) return;
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT);
        View v = new View(activity);
        v.setId(R.id.vr_overlay_view);
        v.setBackgroundColor(Color.BLACK);
        FrameLayout decor = (FrameLayout) activity.getWindow().getDecorView();
        decor.addView(v, params);
    }

    public void removeBlackOverlayView(Activity activity, boolean animate) {
        View overlay = activity.getWindow().findViewById(R.id.vr_overlay_view);
        if (overlay == null) return;
        FrameLayout decor = (FrameLayout) activity.getWindow().getDecorView();
        if (!animate) {
            decor.removeView(overlay);
        } else {
            overlay.animate()
                    .alpha(0)
                    .setDuration(activity.getResources().getInteger(
                            android.R.integer.config_mediumAnimTime))
                    .setListener(new AnimatorListener() {
                        @Override
                        public void onAnimationStart(Animator arg0) {}

                        @Override
                        public void onAnimationRepeat(Animator arg0) {}

                        @Override
                        public void onAnimationEnd(Animator arg0) {
                            decor.removeView(overlay);
                        }

                        @Override
                        public void onAnimationCancel(Animator arg0) {}
                    });
        }
    }

    public boolean activitySupportsVrBrowsing(Activity activity) {
        if (activity instanceof ChromeTabbedActivity) return true;
        return false;
    }

    /* package */ boolean relaunchOnMainDisplayIfNecessary(Activity activity, Intent intent) {
        boolean onMainDisplay = DisplayAndroid.getNonMultiDisplay(activity).getDisplayId()
                == Display.DEFAULT_DISPLAY;
        // TODO(mthiesse): There's a known race when switching displays on Android O/P that can
        // lead us to actually be on the main display, but our context still thinks it's on
        // the virtual display. This is intended to be fixed for Android Q+, but we can work
        // around the race by explicitly relaunching ourselves to the main display.
        if (!onMainDisplay) {
            Log.i(TAG, "Relaunching Chrome onto the main display.");
            activity.finish();
            activity.startActivity(intent,
                    ApiCompatibilityUtils.createLaunchDisplayIdActivityOptions(
                            Display.DEFAULT_DISPLAY));
            return true;
        }
        return false;
    }

    /* package */ abstract void initAfterModuleInstall();

    protected abstract boolean expectedDensityChange();

    private int getIntSystemProperty(String key, int defaultValue) {
        try {
            final Class<?> systemProperties = Class.forName("android.os.SystemProperties");
            final Method getInt = systemProperties.getMethod("getInt", String.class, int.class);
            return (Integer) getInt.invoke(null, key, defaultValue);
        } catch (Exception e) {
            Log.e("Exception while getting system property %s. Using default %s.", key,
                    defaultValue, e);
            return defaultValue;
        }
    }

    private boolean deviceCanChangeResolutionForVr() {
        // Samsung devices no longer change density when entering VR on O+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) return false;
        String model = android.os.Build.MODEL;
        if (SAMSUNG_GALAXY_8_ALT_MODELS.contains(model)) return true;

        // Only Samsung devices change resolution in VR.
        if (!model.startsWith(SAMSUNG_GALAXY_PREFIX)) return false;
        String modelNumber = model.substring(3, 7);
        // Only S8(+) and Note 8 models change resolution in VR.
        if (!SAMSUNG_GALAXY_8_MODELS.contains(modelNumber)) return false;
        return true;
    }
}
