// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.Context;
import android.webkit.WebView;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/**
 * Boundary interface to be used for configuring WebView via a builder.
 *
 * <p>Behind the scenes we will construct a WebView and then configure individual behaviors.
 *
 * <p>This _must_ be called on the UIThread given that this is constructing an Android View.
 */
@NullMarked
public interface WebViewBuilderBoundaryInterface {
    @Retention(RetentionPolicy.SOURCE)
    public @interface Baseline {
        public int DEFAULT = 0;
    }

    @Target(ElementType.TYPE_USE)
    @IntDef({
        ConfigField.BASELINE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ConfigField {
        int BASELINE = 0;
    }

    class Config implements Consumer<BiConsumer<@ConfigField Integer, Object>> {
        public int baseline = Baseline.DEFAULT;

        // This method handles reading all config in AndroidX to transfer over to Chromium.
        // It's job is to essentially "serialize" the fields into known keys.
        @Override
        public void accept(BiConsumer<@ConfigField Integer, Object> chromiumConfig) {
            chromiumConfig.accept(ConfigField.BASELINE, baseline);
        }
    }

    /**
     * This gets called when the configuration is done. At this point, a WebView will actually be
     * constructed and returned to the AndroidX layer.
     *
     * @throws IllegalArgumentException if a configuration was incorrect from an Android app
     */
    WebView build(
            Context context,
            /* Config= */ Consumer<BiConsumer<@ConfigField Integer, Object>> buildConfig);
}
