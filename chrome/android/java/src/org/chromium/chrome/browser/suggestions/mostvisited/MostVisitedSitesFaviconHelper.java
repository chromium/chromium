// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;

import androidx.core.util.AtomicFile;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;
import java.util.Set;

/**
 * This class provides methods to fetch/save most visited sites favicon info to devices.
 */
public class MostVisitedSitesFaviconHelper {
    private static final String TAG = "TopSitesFavicon";

    private final int mMinIconSize;
    private final int mDesiredIconSize;
    private final LargeIconBridge mLargeIconBridge;
    private final RoundedIconGenerator mIconGenerator;

    public MostVisitedSitesFaviconHelper(Context context, LargeIconBridge largeIconBridge) {
        mLargeIconBridge = largeIconBridge;
        Resources resources = context.getResources();

        mDesiredIconSize = resources.getDimensionPixelSize(R.dimen.tile_view_icon_size);
        int minIconSize = resources.getDimensionPixelSize(R.dimen.tile_view_icon_min_size);
        // On ldpi devices, mDesiredIconSize could be even smaller than the global limit.
        mMinIconSize = Math.min(mDesiredIconSize, minIconSize);

        int iconColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_favicon_background_color);
        int iconTextSize = resources.getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator = new RoundedIconGenerator(
                mDesiredIconSize, mDesiredIconSize, mDesiredIconSize / 2, iconColor, iconTextSize);
    }

    /**
     * Save the favicon to the disk.
     * @param topSitesInfo SiteSuggestions data updated.
     * @param urlsToUpdate The set of urls which need to fetch and save the favicon.
     * @param callback The callback function after skipping the existing favicon or saving favicon.
     */
    public void saveFaviconsToFile(
            List<SiteSuggestion> topSitesInfo, Set<GURL> urlsToUpdate, Runnable callback) {
        for (SiteSuggestion siteData : topSitesInfo) {
            GURL url = siteData.url;
            if (!urlsToUpdate.contains(url)) {
                if (callback != null) {
                    callback.run();
                }
                continue;
            }
            LargeIconBridge.LargeIconCallback iconCallback =
                    (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                saveFaviconToFile(String.valueOf(siteData.faviconId),
                        MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory(), url,
                        fallbackColor, icon, callback);
            };
            fetchIcon(siteData, iconCallback);
        }
    }

    /**
     * Fetch the favicon for a given site.
     * @param siteData SiteSuggestion data which needs to fetch and save the favicon.
     * @param iconCallback  The callback function after fetching the favicon.
     */
    // TODO(https://crbug.com/1067386): Change fetchIcon() to public and static, then reuse it in
    // other classes.
    private void fetchIcon(
            final SiteSuggestion siteData, final LargeIconBridge.LargeIconCallback iconCallback) {
        if (siteData.allowlistIconPath.isEmpty()) {
            mLargeIconBridge.getLargeIconForUrl(siteData.url, mMinIconSize, iconCallback);
            return;
        }

        AsyncTask<Bitmap> task = new AsyncTask<Bitmap>() {
            @Override
            protected Bitmap doInBackground() {
                Bitmap bitmap = BitmapFactory.decodeFile(siteData.allowlistIconPath);
                if (bitmap == null) {
                    Log.d(TAG, "Image decoding failed: %s.", siteData.allowlistIconPath);
                }
                return bitmap;
            }

            @Override
            protected void onPostExecute(Bitmap icon) {
                if (icon == null) {
                    mLargeIconBridge.getLargeIconForUrl(siteData.url, mMinIconSize, iconCallback);
                } else {
                    iconCallback.onLargeIconAvailable(icon, Color.BLACK, false, IconType.INVALID);
                }
            }
        };
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Save the favicon to the disk.
     * @param fileName The file name to save the favicon.
     * @param directory The directory to save the favicon.
     * @param url The url which the favicon corresponds to.
     * @param fallbackColor The color for generating a new icon when favicon is null from native.
     * @param icon The favicon fetched from native.
     * @param callback The callback function after saving each favicon.
     */
    private void saveFaviconToFile(String fileName, File directory, GURL url, int fallbackColor,
            Bitmap icon, Runnable callback) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                Bitmap newIcon = icon;
                // If icon is null, we need to generate a favicon.
                if (newIcon == null) {
                    Log.i(TAG,
                            "Favicon is null for " + url.getSpec()
                                    + ". Generating an icon for it.");
                    mIconGenerator.setBackgroundColor(fallbackColor);
                    newIcon = mIconGenerator.generateIconForUrl(url.getSpec());
                }
                // Save icon to file.
                File metadataFile = new File(directory, fileName);
                AtomicFile file = new AtomicFile(metadataFile);
                FileOutputStream stream;
                try {
                    stream = file.startWrite();
                    assert newIcon != null;
                    newIcon.compress(Bitmap.CompressFormat.PNG, 100, stream);
                    file.finishWrite(stream);
                    Log.i(TAG,
                            "Finished saving top sites favicons to file: "
                                    + metadataFile.getAbsolutePath());
                } catch (IOException e) {
                    Log.e(TAG, "Fail to write file: " + metadataFile.getAbsolutePath());
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                if (callback != null) {
                    callback.run();
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
