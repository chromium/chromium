// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.browser.trusted.TrustedWebActivityIntentBuilder.EXTRA_SPLASH_SCREEN_PARAMS;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.splashscreens.SplashScreenParamKey;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashDelegate;
import org.chromium.chrome.browser.customtabs.TranslucentCustomTabActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.util.ColorUtils;

import javax.inject.Inject;

/**
 * Orchestrates the flow of showing and removing splash screens for apps based on Trusted Web
 * Activities.
 *
 * The flow is as follows:
 * - TWA client app verifies conditions for showing splash screen. If the checks pass, it shows the
 * splash screen immediately.
 * - The client passes the URI to a file with the splash image to
 * {@link androidx.browser.customtabs.CustomTabsService}. The image is decoded and put into
 * {@link SplashImageHolder}.
 * - The client then launches a TWA, at which point the Bitmap is already available.
 * - ChromeLauncherActivity calls {@link #handleIntent}, which starts
 * {@link TranslucentCustomTabActivity} - a CustomTabActivity with translucent style. The
 * translucency is necessary in order to avoid a flash that might be seen when starting the activity
 * before the splash screen is attached.
 * - {@link TranslucentCustomTabActivity} creates an instance of {@link TwaSplashController} which
 * immediately displays the splash screen in an ImageView on top of the rest of view hierarchy.
 * - It also immediately removes the translucency. See comment in {@link SplashController} for more
 * details.
 * - It waits for the page to load, and removes the splash image once first paint (or a failure)
 * occurs.
 *
 * Lifecycle: this class is resolved only once when CustomTabActivity is launched, and is
 * gc-ed when it finishes its job.
 * If these lifecycle assumptions change, consider whether @ActivityScope needs to be added.
 */
public class TwaSplashController implements SplashDelegate {
    // TODO(pshmakov): move this to AndroidX.
    private static final String KEY_SHOWN_IN_CLIENT =
            "androidx.browser.trusted.KEY_SPLASH_SCREEN_SHOWN_IN_CLIENT";

    private final SplashController mSplashController;
    private final Activity mActivity;
    private final SplashImageHolder mSplashImageCache;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    @Inject
    public TwaSplashController(
            SplashController splashController,
            Activity activity,
            ActivityWindowAndroid activityWindowAndroid,
            SplashImageHolder splashImageCache,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mSplashController = splashController;
        mActivity = activity;
        mSplashImageCache = splashImageCache;
        mIntentDataProvider = intentDataProvider;

        long splashHideAnimationDurationMs =
                IntentUtils.safeGetInt(
                        getSplashScreenParamsFromIntent(),
                        SplashScreenParamKey.KEY_FADE_OUT_DURATION_MS,
                        0);
        mSplashController.setConfig(this, splashHideAnimationDurationMs);
    }

    @Override
    public View buildSplashView() {
        Bitmap bitmap = mSplashImageCache.takeImage(mIntentDataProvider.getSession());
        if (bitmap == null) {
            return null;
        }
        ImageView splashView = new ImageView(mActivity);
        splashView.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        splashView.setImageBitmap(bitmap);
        applyCustomizationsToSplashScreenView(splashView);
        return splashView;
    }

    @Override
    public void onSplashHidden(Tab tab, long startTimestamp, long endTimestamp) {}

    @Override
    public boolean shouldWaitForSubsequentPageLoadToHideSplash() {
        return false;
    }

    private void applyCustomizationsToSplashScreenView(ImageView imageView) {
        Bundle params = getSplashScreenParamsFromIntent();

        int backgroundColor =
                IntentUtils.safeGetInt(
                        params, SplashScreenParamKey.KEY_BACKGROUND_COLOR, Color.WHITE);
        imageView.setBackgroundColor(ColorUtils.getOpaqueColor(backgroundColor));

        int scaleTypeOrdinal =
                IntentUtils.safeGetInt(params, SplashScreenParamKey.KEY_SCALE_TYPE, -1);
        ImageView.ScaleType[] scaleTypes = ImageView.ScaleType.values();
        ImageView.ScaleType scaleType;
        if (scaleTypeOrdinal < 0 || scaleTypeOrdinal >= scaleTypes.length) {
            scaleType = ImageView.ScaleType.CENTER;
        } else {
            scaleType = scaleTypes[scaleTypeOrdinal];
        }
        imageView.setScaleType(scaleType);

        if (scaleType != ImageView.ScaleType.MATRIX) return;
        float[] matrixValues =
                IntentUtils.safeGetFloatArray(
                        params, SplashScreenParamKey.KEY_IMAGE_TRANSFORMATION_MATRIX);
        if (matrixValues == null || matrixValues.length != 9) return;
        Matrix matrix = new Matrix();
        matrix.setValues(matrixValues);
        imageView.setImageMatrix(matrix);
    }

    private Bundle getSplashScreenParamsFromIntent() {
        return mIntentDataProvider.getIntent().getBundleExtra(EXTRA_SPLASH_SCREEN_PARAMS);
    }

    /** Returns true if the intent corresponds to a TWA with a splash screen. */
    public static boolean intentIsForTwaWithSplashScreen(Intent intent) {
        boolean isTrustedWebActivity =
                IntentUtils.safeGetBooleanExtra(
                        intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
        boolean requestsSplashScreen =
                IntentUtils.safeGetParcelableExtra(intent, EXTRA_SPLASH_SCREEN_PARAMS) != null;
        return isTrustedWebActivity && requestsSplashScreen;
    }

    /**
     * Handles the intent if it should launch a TWA with splash screen.
     * @param activity Activity, from which to start the next one.
     * @param intent Incoming intent.
     * @return Whether the intent was handled.
     */
    public static boolean handleIntent(Activity activity, Intent intent) {
        if (!intentIsForTwaWithSplashScreen(intent)) return false;

        Bundle params =
                IntentUtils.safeGetBundleExtra(
                        intent, TrustedWebActivityIntentBuilder.EXTRA_SPLASH_SCREEN_PARAMS);
        boolean shownInClient = IntentUtils.safeGetBoolean(params, KEY_SHOWN_IN_CLIENT, true);
        // shownInClient is "true" by default for the following reasons:
        // - For compatibility with older clients which don't use this bundle key.
        // - Because getting "false" when it should be "true" leads to more severe visual glitches,
        // than vice versa.
        if (shownInClient) {
            // If splash screen was shown in client, we must launch a translucent activity to
            // ensure smooth transition.
            intent.setClassName(activity, TranslucentCustomTabActivity.class.getName());
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        activity.startActivity(intent);
        activity.overridePendingTransition(0, 0);
        return true;
    }
}
