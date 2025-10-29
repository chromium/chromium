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
                    Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                    // Apply the insets paddings to the view.
                    v.setPadding(insets.left, insets.top, insets.right, insets.bottom);
                    return new WindowInsetsCompat.Builder(windowInsets)
                            .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.NONE)
                            .build();
                });
    }
}
