// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.content_public.browser.WebContents;

/**
 * The class to handle Lens chip data and actions.
 */
public class LensChipDelegate implements ChipDelegate {
    private LensQueryParams mLensQueryParams;
    private LensController mLensController;
    private ContextMenuNativeDelegate mNativeDelegate;
    private Runnable mOnChipClickedCallback;
    private Runnable mOnChipShownCallback;

    public LensChipDelegate(String pageUrl, String titleOrAltText, String srcUrl, String pageTitle,
            boolean isIncognito, WebContents webContents, ContextMenuNativeDelegate nativeDelegate,
            Runnable onChipClickedCallback, Runnable onChipShownCallback) {
        mLensController = LensController.getInstance();
        if (!mLensController.isQueryEnabled()) {
            return;
        }
        mLensQueryParams = (new LensQueryParams.Builder())
                                   .withPageUrl(pageUrl)
                                   .withImageTitleOrAltText(titleOrAltText)
                                   .withSrcUrl(srcUrl)
                                   .withPageTitle(pageTitle)
                                   .withIsIncognito(isIncognito)
                                   .withWebContents(webContents)
                                   .build();
        mNativeDelegate = nativeDelegate;
        mOnChipClickedCallback = onChipClickedCallback;
        mOnChipShownCallback = onChipShownCallback;
    }

    @Override
    public void getChipRenderParams(Callback<ChipRenderParams> chipParamsCallback) {
        if (mLensQueryParams == null) {
            chipParamsCallback.onResult(null);
            return;
        }

        Callback<Uri> callback = (uri) -> {
            mLensQueryParams.setImageUri(uri);
            mLensController.getChipRenderParams(mLensQueryParams, (chipParams) -> {
                if (isValidChipRenderParams(chipParams)) {
                    // A new variable to avoid infinite loop inside the merged
                    // onClick callback.
                    Runnable originalOnClickCallback = chipParams.onClickCallback;
                    Runnable mergedOnClickCallback = () -> {
                        // The onClickCallback defined in LensController.
                        originalOnClickCallback.run();
                        // The onClickCallback defined when initialize the LensChipDelegate.
                        mOnChipClickedCallback.run();
                    };
                    chipParams.onClickCallback = mergedOnClickCallback;
                    chipParams.onShowCallback = mOnChipShownCallback;
                }
                chipParamsCallback.onResult(chipParams);
            });
        };

        // Must occur on UI thread.
        mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.ORIGINAL, callback);
    }

    @Override
    public void onMenuClosed() {
        if (mLensController.isQueryEnabled()) {
            mLensController.terminateClassification();
        }
    }

    @Override
    public boolean isValidChipRenderParams(ChipRenderParams chipRenderParams) {
        return chipRenderParams != null && chipRenderParams.titleResourceId != 0
                && chipRenderParams.onClickCallback != null && chipRenderParams.iconResourceId != 0;
    }
}
