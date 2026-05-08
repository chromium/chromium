// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import com.airbnb.lottie.LottieCompositionFactory;
import com.airbnb.lottie.LottieDrawable;
import com.airbnb.lottie.LottieProperty;
import com.airbnb.lottie.model.KeyPath;
import com.airbnb.lottie.value.LottieValueCallback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helper class for Glic UI. */
@NullMarked
public class GlicUiHelper {

    /**
     * Creates a layered drawable for the Glic button's working state. It combines a Lottie
     * animation (spinner) with a static spark icon on top. The Lottie animation is tinted
     * dynamically based on the applied color state list.
     *
     * @param context The Android context used to load resources.
     * @param sparkIcon The drawable for the spark icon to be placed on top.
     * @return A {@link Drawable} (specifically a LayerDrawable) combining the spinner and the spark
     *     icon.
     */
    public static Drawable createWorkingDrawable(Context context, Drawable sparkIcon) {
        LottieDrawable lottieDrawable = new LottieDrawable();
        LottieCompositionFactory.fromRawRes(context, R.raw.glic_spinner)
                .addListener(
                        composition -> {
                            lottieDrawable.setComposition(composition);
                            lottieDrawable.setRepeatCount(LottieDrawable.INFINITE);
                            lottieDrawable.playAnimation();
                        });

        LayerDrawable layerDrawable =
                new LayerDrawable(new Drawable[] {lottieDrawable, sparkIcon}) {
                    private @Nullable ColorStateList mTintList;

                    @Override
                    public void setTintList(@Nullable ColorStateList tint) {
                        super.setTintList(tint);
                        mTintList = tint;
                        updateLottieTint();
                    }

                    @Override
                    protected boolean onStateChange(int[] state) {
                        boolean changed = super.onStateChange(state);
                        if (updateLottieTint()) {
                            changed = true;
                        }
                        return changed;
                    }

                    @Override
                    public boolean isStateful() {
                        return (mTintList != null && mTintList.isStateful()) || super.isStateful();
                    }

                    private boolean updateLottieTint() {
                        if (mTintList != null) {
                            int color =
                                    mTintList.getColorForState(
                                            getState(), mTintList.getDefaultColor());
                            lottieDrawable.addValueCallback(
                                    new KeyPath("**"),
                                    LottieProperty.COLOR_FILTER,
                                    new LottieValueCallback<>(
                                            new PorterDuffColorFilter(
                                                    color, PorterDuff.Mode.SRC_IN)));
                            return true;
                        }
                        return false;
                    }
                };
        float density = context.getResources().getDisplayMetrics().density;
        // Adjust sizes of the spark and spinner.
        int sparkInset = Math.round(2 * density);
        int spinnerInset = Math.round(-10 * density);
        layerDrawable.setLayerInset(0, spinnerInset, spinnerInset, spinnerInset, spinnerInset);
        layerDrawable.setLayerInset(1, sparkInset, sparkInset, sparkInset, sparkInset);
        return layerDrawable;
    }
}
