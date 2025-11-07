// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import android.graphics.Matrix;

import org.chromium.build.annotations.NullMarked;

/** A container for holding the portrait and landscape transformation matrices. */
@NullMarked
public class BackgroundImageInfo {
    public final Matrix portraitMatrix;
    public final Matrix landscapeMatrix;

    public BackgroundImageInfo(Matrix portrait, Matrix landscape) {
        this.portraitMatrix = portrait;
        this.landscapeMatrix = landscape;
    }
}
