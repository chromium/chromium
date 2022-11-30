// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;

import org.chromium.chrome.R;

/** Intent-specific delegate to call into VR. */
public abstract class VrIntentDelegate {
    public static final String DAYDREAM_CATEGORY = "com.google.intent.category.DAYDREAM";

    /**
     * @return Whether or not the given intent is a VR-specific intent.
     */
    public boolean isVrIntent(Intent intent) {
        // For simplicity, we only return true here if VR is enabled on the platform and this intent
        // is not fired from a recent apps page. The latter is there so that we don't enter VR mode
        // when we're being resumed from the recent apps in 2D mode.
        // Note that Daydream removes the Daydream category for deep-links (for no real reason). In
        // addition to the category, DAYDREAM_VR_EXTRA tells us that this intent is coming directly
        // from VR.
        return intent != null && intent.hasCategory(DAYDREAM_CATEGORY)
                && !((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0);
    }

    /**
     * @param activity The Activity to check.
     * @param intent The intent the Activity was launched with.
     * @return Whether this Activity is launching into VR.
     */
    public boolean isLaunchingIntoVr(Activity activity, Intent intent) {
        return VrModuleProvider.getDelegate().isDaydreamReadyDevice() && isVrIntent(intent)
                && VrModuleProvider.getDelegate().activitySupportsVrBrowsing(activity);
    }

    /**
     * @return Options that a VR-specific Chrome activity should be launched with.
     */
    public Bundle getVrIntentOptions(Context context) {
        // These options are used to start the Activity with a custom animation to keep it hidden
        // for a few hundred milliseconds - enough time for us to draw the first black view.
        // The animation is sufficient to hide the 2D screenshot but not to the 2D UI while the
        // WebVR page is being loaded because the animation is somehow cancelled when we try to
        // enter VR (I don't know what's canceling it). To hide the 2D UI, we resort to the black
        // overlay view added in {@link startWithVrIntentPreNative}.
        int animation = VrDelegate.USE_HIDE_ANIMATION ? R.anim.stay_hidden : 0;
        ActivityOptions options = ActivityOptions.makeCustomAnimation(context, animation, 0);
        if (VrModuleProvider.getDelegate().bootsToVr()) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                assert false;
            } else {
                options.setLaunchDisplayId(Display.DEFAULT_DISPLAY);
            }
        }
        return options.toBundle();
    }

    /**
     * This function returns an intent that will launch a VR activity that will prompt the
     * user to take off their headset and forward the freIntent to the standard
     * 2D FRE activity.
     *
     * @param freIntent       The intent that will be used to start the first run in 2D mode.
     * @return The intermediate VR activity intent.
     */
    public abstract Intent setupVrFreIntent(Context context, Intent freIntent);

    /**
     * Removes VR specific extras from the given intent to make it a non-VR intent.
     */
    public abstract void removeVrExtras(Intent intent);

    /**
     * Adds the necessary VR flags to an intent.
     * @param intent The intent to add VR flags to.
     * @return the intent with VR flags set.
     */
    public abstract Intent setupVrIntent(Intent intent);
}
