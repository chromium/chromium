// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.net.Uri;

import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.DateFormat;
import java.util.Date;

/**
 * A class to keep track of the meta data associated with a an image in the photo picker.
 */
public class PickerBitmap implements Comparable<PickerBitmap> {
    // The possible types of tiles involved in the viewer.
    @IntDef({TileTypes.PICTURE, TileTypes.CAMERA, TileTypes.GALLERY, TileTypes.VIDEO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TileTypes {
        int PICTURE = 0;
        int CAMERA = 1;
        int GALLERY = 2;
        int VIDEO = 3;
    }

    // The URI of the bitmap to show.
    private Uri mUri;

    // When the bitmap was last modified on disk.
    private long mLastModified;

    // The type of tile involved.
    @TileTypes
    private int mType;

    /**
     * The PickerBitmap constructor.
     * @param uri The URI for the bitmap to show.
     * @param lastModified When the bitmap was last modified on disk.
     * @param type The type of tile involved.
     */
    public PickerBitmap(Uri uri, long lastModified, @TileTypes int type) {
        mUri = uri;
        mLastModified = lastModified;
        mType = type;
    }

    /**
     * Accessor for the URI.
     * @return The URI for this PickerBitmap object.
     */
    public Uri getUri() {
        return mUri;
    }

    /**
     * Accessor for the filename.
     * @return The filename (without the extension and path).
     */
    public String getFilenameWithoutExtension() {
        String filePath = mUri.getPath();
        int index = filePath.lastIndexOf("/");
        if (index == -1) return filePath;
        return filePath.substring(index + 1, filePath.length());
    }

    /**
     * Accessor for the last modified date.
     * @return The last modified date in string format.
     */
    public String getLastModifiedString() {
        return DateFormat.getDateTimeInstance().format(new Date(mLastModified));
    }

    /**
     * Accessor for the tile type.
     * @return The type of tile involved for this bitmap object.
     */
    @TileTypes
    public int type() {
        return mType;
    }

    /**
     * A comparison function for PickerBitmaps (results in a last-modified first sort).
     * @param other The PickerBitmap to compare it to.
     * @return 0, 1, or -1, depending on which is bigger.
     */
    @Override
    public int compareTo(PickerBitmap other) {
        return ApiCompatibilityUtils.compareLong(other.mLastModified, mLastModified);
    }
}
