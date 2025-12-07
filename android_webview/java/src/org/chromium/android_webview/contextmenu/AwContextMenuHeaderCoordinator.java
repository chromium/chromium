// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.R;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Coordinator for creating the context menu header */
@NullMarked
public class AwContextMenuHeaderCoordinator {
    private final PropertyModel mModel;
    private @Nullable static Pair<String, Drawable> sCachedFavicon;
    private final Context mContext;

    public AwContextMenuHeaderCoordinator(ContextMenuParams params, Context context) {
        mModel = buildModel(params.getUnfilteredLinkUrl().getSpec());
        mContext = context;
    }

    private PropertyModel buildModel(String title) {
        PropertyModel model =
                new PropertyModel.Builder(AwContextMenuHeaderProperties.ALL_KEYS)
                        .with(AwContextMenuHeaderProperties.TITLE, title)
                        .build();
        return model;
    }

    @VisibleForTesting
    public PropertyModel getModel() {
        return mModel;
    }

    public String getTitle() {
        return mModel.get(AwContextMenuHeaderProperties.TITLE);
    }

    public static void setCachedFaviconForTesting(Pair<String, Drawable> favicon) {
        sCachedFavicon = favicon;
    }

    public @Nullable static Pair<String, Drawable> getCachedFaviconForTesting() {
        return sCachedFavicon;
    }

    /**
     * Sets the header icon for the context menu.
     *
     * <p>This method performs checks because {@link AwContents#getFavicon()} doesn't always return
     * the correct favicon for the current URL. If the user navigates from URL A -> URL B, long
     * presses on a link, and the favicon for URL B hasn't been downloaded yet, the favicon for URL
     * A will be returned. In that case or the case where we don't have a favicon for the current
     * URL, we want to use a fallback icon.
     *
     * @param currentUrl The URL of the page for which the favicon is being set.
     * @param candidateFavicon The favicon bitmap to set.
     */
    public void setHeaderIcon(GURL currentUrl, Bitmap candidateFavicon) {
        String host = currentUrl.getHost();

        // This favicon is probably stale if it's not for the current host.
        if (sCachedFavicon != null) {
            boolean sameHost = sCachedFavicon.first != null && sCachedFavicon.first.equals(host);
            boolean sameBitmap =
                    ((BitmapDrawable) sCachedFavicon.second).getBitmap().equals(candidateFavicon);

            if (!sameHost && sameBitmap) {
                // Stale favicon, do not update
                setFallbackIcon();
                return;
            }
        }

        if (candidateFavicon == null) {
            setFallbackIcon();
            return;
        }

        Drawable newDrawable = new BitmapDrawable(mContext.getResources(), candidateFavicon);

        sCachedFavicon = Pair.create(host, newDrawable);
        mModel.set(AwContextMenuHeaderProperties.HEADER_ICON, newDrawable);
    }

    private void setFallbackIcon() {
        Drawable fallback = ContextCompat.getDrawable(mContext, R.drawable.ic_globe_24dp);
        if (fallback != null) {
            fallback = DrawableCompat.wrap(fallback.mutate());
            DrawableCompat.setTint(
                    fallback,
                    ContextCompat.getColor(mContext, R.color.default_icon_color_baseline));
            mModel.set(AwContextMenuHeaderProperties.HEADER_ICON, fallback);
        }
    }
}
