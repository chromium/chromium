// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.Context;
import android.webkit.WebView;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
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
    @interface Baseline {
        int DEFAULT = 0;
    }

    @Target(ElementType.TYPE_USE)
    @IntDef({
        ConfigField.BASELINE,
        ConfigField.JAVASCRIPT_INTERFACE,
        ConfigField.RESTRICT_JAVASCRIPT_INTERFACE,
        ConfigField.PROFILE_NAME,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ConfigField {
        int BASELINE = 0;
        int JAVASCRIPT_INTERFACE = 1;
        int RESTRICT_JAVASCRIPT_INTERFACE = 2;
        int PROFILE_NAME = 3;
    }

    class Config implements Consumer<BiConsumer<@ConfigField Integer, Object>> {
        public int baseline = Baseline.DEFAULT;
        public boolean restrictJavascriptInterface;
        public @Nullable String profileName;

        List<Object> mJavascriptInterfaceObjects = new ArrayList<>();
        Map<String, Boolean> mJavascriptInterfaceNames = new LinkedHashMap<>();
        List<List<String>> mJavascriptInterfaceOriginPatterns = new ArrayList<>();

        public void addJavascriptInterface(
                Object object, String name, List<String> originPatterns) {
            if (mJavascriptInterfaceNames.containsKey(name)) {
                throw new IllegalArgumentException(
                        "A duplicate JavaScript interface was provided for \"" + name + "\"");
            }
            mJavascriptInterfaceObjects.add(object);
            mJavascriptInterfaceNames.put(name, true);
            mJavascriptInterfaceOriginPatterns.add(originPatterns);
        }

        // This method handles reading all config in AndroidX to transfer over to Chromium.
        // It's job is to essentially "serialize" the fields into known keys.
        @Override
        public void accept(BiConsumer<@ConfigField Integer, Object> chromiumConfig) {
            chromiumConfig.accept(ConfigField.BASELINE, baseline);
            chromiumConfig.accept(
                    ConfigField.RESTRICT_JAVASCRIPT_INTERFACE, restrictJavascriptInterface);
            chromiumConfig.accept(
                    ConfigField.JAVASCRIPT_INTERFACE,
                    new Object[] {
                        mJavascriptInterfaceObjects,
                        // We only use a Set while adding origin patterns to efficiently validate
                        // on each origin being added. When we send this to Chromium, we can provide
                        // this as the list expected.
                        new ArrayList<String>(mJavascriptInterfaceNames.keySet()),
                        mJavascriptInterfaceOriginPatterns
                    });
            if (profileName != null) {
                chromiumConfig.accept(ConfigField.PROFILE_NAME, profileName);
            }
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
