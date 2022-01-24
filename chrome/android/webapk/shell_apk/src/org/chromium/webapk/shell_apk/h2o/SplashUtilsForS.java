// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import static org.chromium.webapk.shell_apk.h2o.SplashUtils.createScaledBitmapAndCanvas;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Insets;
import android.graphics.Paint;
import android.graphics.drawable.Icon;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.widget.ImageView;

import org.chromium.webapk.shell_apk.R;
import org.chromium.webapk.shell_apk.WebApkUtils;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/** Contains splash screen related utility methods that require Android S APIs. */
class SplashUtilsForS {
    private SplashUtilsForS() {}

    /** A listener for the Android S+ splash screen. */
    public interface SplashscreenShownListener {
        /** Called once the splash screen has been shown and a screenshot has been created. */
        void onSplashScreenShown(View view, Bitmap bitmap);
    }

    /**
     * A copy of SplashScreen.OnExitAnimationListener for us to use since we're not building with
     * the Android S SDK.
     */
    private interface OnExitAnimationListener {
        void onSplashScreenExit(View splashView, View iconView);
    }

    /** Registers a listener for the Android S splash screen. */
    public static boolean listenForSplashScreen(
            Activity activity, Window window, SplashscreenShownListener listener) {
        try {
            setOnExitAnimationListener(activity, (splashView, iconView) -> {
                WindowInsets insets = window.getDecorView().getRootWindowInsets();
                Insets systemBarInsets = insets.getInsets(WindowInsets.Type.systemBars());

                int backgroundColor = WebApkUtils.getColor(
                        activity.getResources(), R.color.background_color_non_empty);
                Bitmap bitmap = screenshotSplashScreenView(splashView, iconView, systemBarInsets,
                        backgroundColor, SplashContentProvider.MAX_TRANSFER_SIZE_BYTES);

                listener.onSplashScreenShown(splashView, bitmap);
            });
        } catch (ClassNotFoundException | NoSuchMethodException | InvocationTargetException
                | IllegalAccessException e) {
            return false;
        }

        return true;
    }

    private static void setOnExitAnimationListener(Activity activity,
            OnExitAnimationListener listener) throws ClassNotFoundException, NoSuchMethodException,
                                                     InvocationTargetException,
                                                     IllegalAccessException {
        // The Java reflection that follows maps to:
        // activity.getSplashScreen().setOnExitAnimationListener(view -> {
        //     listener.onSplashScreenExit(view, view.getIconView());
        // });

        Class<?> onExitAnimationListenerInterface =
                Class.forName("android.window.SplashScreen$OnExitAnimationListener");
        Object instance = Proxy.newProxyInstance(activity.getClassLoader(),
                new Class<?>[] {onExitAnimationListenerInterface}, (proxy, method, args) -> {
                    if (!method.getName().equals("onSplashScreenExit")) return null;

                    try {
                        View splashView = (View) args[0];

                        Method getIconViewMethod = splashView.getClass().getMethod("getIconView");

                        View iconView = (View) getIconViewMethod.invoke(splashView);
                        listener.onSplashScreenExit(splashView, iconView);
                    } catch (NoSuchMethodException | InvocationTargetException
                            | IllegalAccessException e) {
                        // There's no easy error recovery option here, but also to reach here the
                        // reflection calls below must have succeeded, but the ones above must have
                        // failed, so this is unlikely to happen.
                    }
                    return null;
                });

        Method getSplashScreenMethod = activity.getClass().getMethod("getSplashScreen");
        Object splashScreen = getSplashScreenMethod.invoke(activity);
        Method setOnExitAnimationListenerMethod = splashScreen.getClass().getMethod(
                "setOnExitAnimationListener", onExitAnimationListenerInterface);
        setOnExitAnimationListenerMethod.invoke(splashScreen, instance);
    }

    /**
     * Returns a screenshot of the passed in Android S SplashScreenView and renders a Bitmap
     * suitable for Chrome to display for a seamless WebAPK shell to Chrome transition.
     *
     * Android S splash screens take the entire window (including the area normally covered by the
     * status and navigation bars). Chrome displays the provided Bitmap in the normal Android
     * Activity bounds, so to make sure that the screen doesn't change between the shell and Chrome,
     * we provide a screenshot without the areas normally covered by the status and navigation bar.
     */
    private static Bitmap screenshotSplashScreenView(
            View splashView, View iconView, Insets insets, int backgroundColor, int maxSizeBytes) {
        int width = splashView.getWidth() - insets.right - insets.left;
        int height = splashView.getHeight() - insets.top - insets.bottom;

        SplashUtils.BitmapAndCanvas pair = createScaledBitmapAndCanvas(width, height, maxSizeBytes);

        // Instead of taking a screenshot of the splash view and cutting it down to size, we found
        // it easier to create a canvas of the correct size, fill it with the background color and
        // then just draw the app icon in the appropriate place.

        Paint paint = new Paint();
        paint.setColor(backgroundColor);
        pair.canvas.drawRect(0, 0, width, height, paint);

        int[] loc = new int[2];
        iconView.getLocationOnScreen(loc);
        pair.canvas.translate(loc[0] - insets.left, loc[1] - insets.top);

        iconView.draw(pair.canvas);
        return pair.bitmap;
    }

    /**
     * Creates a View with a splash screen.
     *
     * Unlike {@link SplashUtils#createSplashView}, this splash screen will be an approximation of
     * the splash screen generated by Android S+ (the main difference being that there is no app
     * title).
     *
     * This is only used when the browser has been killed by Android for memory reasons, but the
     * shell APK is still alive. When this happens we want to show a splash screen, but no longer
     * have access to the one that was used to launch the shell APK (since we don't want to keep the
     * ~12MB (see MAX_TRANSFER_SIZE_BYTES) image around for longer than needed). We create an
     * approximation of the Android S splash screen instead (which is good enough since the user
     * will never see them side by side).
     */
    static View createSplashView(Context context) {
        View view = LayoutInflater.from(context).inflate(R.layout.splash_screen_view, null);
        if (WebApkUtils.isSplashIconAdaptive(context)) {
            Bitmap icon = WebApkUtils.decodeBitmapFromDrawable(
                    context.getResources(), R.drawable.splash_icon);
            ImageView imageView = (ImageView) view.findViewById(R.id.splashscreen_icon_view);
            imageView.setImageIcon(Icon.createWithAdaptiveBitmap(icon));
        }
        return view;
    }
}
