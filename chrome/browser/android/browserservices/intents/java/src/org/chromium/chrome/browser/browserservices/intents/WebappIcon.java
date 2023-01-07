// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;

/** Represents bitmap icon. Lazily converts icon format. */
public class WebappIcon {
    private byte[] mUnsafeData;
    private String mEncoded;
    private Bitmap mBitmap;
    private String mWebApkPackageName;
    private int mResourceId;
    private boolean mIsTrusted;

    public WebappIcon() {}

    public WebappIcon(byte[] unsafeData) {
        mUnsafeData = unsafeData;
    }

    /**
     * @param encoded The encoded data of a bitmap.
     * @param isTrusted Whether the encoded data came from a trusted source. If false, the data
     *     won't be used to generate a bitmap.
     */
    public WebappIcon(String encoded, boolean isTrusted) {
        mEncoded = encoded;
        mIsTrusted = isTrusted;
    }

    public WebappIcon(Bitmap bitmap) {
        mBitmap = bitmap;
    }

    public WebappIcon(String webApkPackageName, int resourceId) {
        mWebApkPackageName = webApkPackageName;
        mResourceId = resourceId;
    }

    public byte[] data() {
        if (mUnsafeData != null) {
            return mUnsafeData;
        }
        return Base64.decode(encoded(), Base64.DEFAULT);
    }

    public String encoded() {
        if (mEncoded == null) {
            mEncoded = BitmapHelper.encodeBitmapAsString(bitmap());
        }
        return mEncoded;
    }

    public Bitmap bitmap() {
        if (mBitmap == null) {
            mBitmap = generateBitmap();
        }
        return mBitmap;
    }

    @VisibleForTesting
    public int resourceIdForTesting() {
        return mResourceId;
    }

    private Bitmap generateBitmap() {
        if (mEncoded != null && mIsTrusted) {
            return BitmapHelper.decodeBitmapFromString(mEncoded);
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
