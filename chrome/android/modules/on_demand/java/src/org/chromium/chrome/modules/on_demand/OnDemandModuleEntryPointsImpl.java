// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.on_demand;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.browser.pdf.PdfEntryPoint;
import org.chromium.chrome.browser.pdf.PdfEntryPointImpl;

/** Entry points implementation that lives inside the apk split. */
@NullMarked
@UsedByReflection("SplitChromeApplication.java")
public class OnDemandModuleEntryPointsImpl implements OnDemandModuleEntryPoints {
    private final PdfEntryPoint mPdfEntryPoint = new PdfEntryPointImpl();

    @Override
    public @Nullable Object getInternalEntryPoints() {
        return ServiceLoaderUtil.maybeCreate(OnDemandModuleInternalEntryPoints.class);
    }

    @Override
    public PdfEntryPoint getPdfEntryPoint() {
        return mPdfEntryPoint;
    }
}
