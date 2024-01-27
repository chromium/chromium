// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;
import android.util.Base64;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.io.IOException;

/** Represents bitmap icon. Lazily converts icon format. */
public class WebappIcon {
    public static final int ICON_WITH_URL_AND_HASH_SHELL_VERSION = 169;

    private static final String TAG = "WebappIcon";

    private byte[] mUnsafeData;
    private String mEncoded;
    private Bitmap mBitmap;
    private String mWebApkPackageName;
    private int mResourceId;
    private boolean mIsTrusted;

    private String mIconUrl;
    private String mIconHash;

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

    public WebappIcon(
            String webApkPackageName, int resourceId, Resources res, int shellApkVersion) {
        mWebApkPackageName = webApkPackageName;
        mResourceId = resourceId;

        if (shellApkVersion >= ICON_WITH_URL_AND_HASH_SHELL_VERSION) {
            XmlResourceParser parser = res.getXml(mResourceId);
            try {
                int eventType = parser.getEventType();
                while (eventType != XmlPullParser.END_DOCUMENT) {
                    if (eventType == XmlPullParser.START_TAG
                            && TextUtils.equals(parser.getName(), "bitmap")) {
                        mIconUrl = parser.getAttributeValue(null, "iconUrl");
                        mIconHash = parser.getAttributeValue(null, "iconHash");
                    }
                    eventType = parser.next();
                }

            } catch (XmlPullParserException | IOException e) {
                Log.e(TAG, "Failed to parse icon XML", e);
            }
        }
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

    public int resourceIdForTesting() {
        return mResourceId;
    }

    public String iconUrl() {
        return mIconUrl;
    }

    public String iconHash() {
        return mIconHash;
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
