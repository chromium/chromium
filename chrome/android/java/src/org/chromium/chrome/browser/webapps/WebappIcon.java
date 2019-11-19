// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ShortcutHelper;

/** Represents bitmap icon. Lazily converts icon format. */
public class WebappIcon {
    private String mEncoded;
    private Bitmap mBitmap;
    private String mWebApkPackageName;
    private int mResourceId;

    public WebappIcon() {}

    public WebappIcon(String encoded) {
        mEncoded = encoded;
    }

    public WebappIcon(Bitmap bitmap) {
        mBitmap = bitmap;
    }

    public WebappIcon(String webApkPackageName, int resourceId) {
        mWebApkPackageName = webApkPackageName;
        mResourceId = resourceId;
    }

    public String encoded() {
        if (mEncoded == null) {
            mEncoded = ShortcutHelper.encodeBitmapAsString(bitmap());
        }
        return mEncoded;
    }

    public Bitmap bitmap() {
        if (mBitmap == null) {
            mBitmap = generateBitmap();
        }
        return mBitmap;
    }

    private Bitmap generateBitmap() {
        if (mEncoded != null) {
            return ShortcutHelper.decodeBitmapFromString(mEncoded);
        }
        if (mWebApkPackageName != null && mResourceId != 0) {
            try {
                PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
                Resources res = pm.getResourcesForApplication(mWebApkPackageName);
                BitmapDrawable bitmapDrawable =
                        (BitmapDrawable) ApiCompatibilityUtils.getDrawable(res, mResourceId);
                return bitmapDrawable != null ? bitmapDrawable.getBitmap() : null;
            } catch (Exception e) {
            }
        }
        return null;
    }
}
