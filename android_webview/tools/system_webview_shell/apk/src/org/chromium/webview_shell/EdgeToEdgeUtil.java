// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

public final class EdgeToEdgeUtil {

    private EdgeToEdgeUtil() {} // Class is not meant to be instantiated.

    /**
     * Enable edge-to-edge rendering and add padding to the content view to avoid drawing under the
     * insets.
     */
    public static void setupEdgeToEdge(AppCompatActivity activity) {
        EdgeToEdge.enable(activity);
        ViewCompat.setOnApplyWindowInsetsListener(
                activity.findViewById(android.R.id.content),
                (v, windowInsets) -> {
                    int types =
                            WindowInsetsCompat.Type.systemBars()
                                    | WindowInsetsCompat.Type.displayCutout();
                    Insets insets = windowInsets.getInsets(types);
                    // Apply the insets paddings to the view.
                    v.setPadding(insets.left, insets.top, insets.right, insets.bottom);
                    return new WindowInsetsCompat.Builder(windowInsets)
                            .setInsets(types, Insets.NONE)
                            .build();
                });
    }

    /**
     * Enable edge-to-edge rendering and add an onApplyWindowInsetsListener that does nothing and
     * passes the insets down the view hierarchy (to be handled by the web contents).
     */
    public static void setupEdgeToEdgeFullscreen(AppCompatActivity activity) {
        EdgeToEdge.enable(activity);
        ViewCompat.setOnApplyWindowInsetsListener(
                activity.findViewById(android.R.id.content), (v, windowInsets) -> windowInsets);
    }
}
