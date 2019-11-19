// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media.router.caf.remoting;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v7.media.MediaRouteSelector;
import android.util.Base64;

import androidx.annotation.Nullable;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.io.UnsupportedEncodingException;

/**
 * Abstracts parsing the Cast application id and other parameters from the source id.
 */
public class RemotingMediaSource implements MediaSource {
    private static final String TAG = "MediaRemoting";

    // Need to be in sync with third_party/WebKit/Source/modules/remoteplayback/RemotePlayback.cpp.
    // TODO(avayvod): Find a way to share the constants somehow.
    private static final String SOURCE_PREFIX = "remote-playback://";
    private static final String REMOTE_PLAYBACK_APP_ID_KEY =
            "org.chromium.content.browser.REMOTE_PLAYBACK_APP_ID";

    /**
     * The Cast application id.
     */
    private static String sApplicationId;

    /**
     * The original source URL that the {@link MediaSource} object was created from.
     */
    private final String mSourceId;

    /**
     * The URL to fling to the Cast device.
     */
    private final String mMediaUrl;

    /**
     * Initializes the media source from the source id.
     * @param sourceId a URL containing encoded info about the media element's source.
     * @return an initialized media source if the id is valid, null otherwise.
     */
    @Nullable
    public static RemotingMediaSource from(String sourceId) {
        assert sourceId != null;

        if (!sourceId.startsWith(SOURCE_PREFIX)) return null;

        String encodedContentUrl = sourceId.substring(SOURCE_PREFIX.length());

        String mediaUrl;
        try {
            mediaUrl = new String(Base64.decode(encodedContentUrl, Base64.URL_SAFE), "UTF-8");
        } catch (IllegalArgumentException | UnsupportedEncodingException e) {
            Log.e(TAG, "Couldn't parse the source id.", e);
            return null;
        }

        return new RemotingMediaSource(sourceId, mediaUrl);
    }

    /**
     * Returns a new {@link MediaRouteSelector} to use for Cast device filtering for this
     * particular media source or null if the application id is invalid.
     *
     * @return an initialized route selector or null.
     */
    @Override
    public MediaRouteSelector buildRouteSelector() {
        return new MediaRouteSelector.Builder()
                .addControlCategory(CastMediaControlIntent.categoryForCast(getApplicationId()))
                .build();
    }

    /**
     * Lazily loads a custom App ID from the AndroidManifest, which can be overriden
     * downstream. This app ID will never change, so we can store it in a static field.
     * If there is no custom app ID defined, or if there is an error retreiving the app ID,
     * we fallback to the default media receiver app ID.
     *
     * @return a custom app ID or the default media receiver app ID.
     */
    private static String applicationId() {
        if (sApplicationId == null) {
            String customAppId = null;

            try {
                Context context = ContextUtils.getApplicationContext();
                ApplicationInfo ai = context.getPackageManager().getApplicationInfo(
                        context.getPackageName(), PackageManager.GET_META_DATA);
                Bundle bundle = ai.metaData;
                customAppId = bundle.getString(REMOTE_PLAYBACK_APP_ID_KEY);
            } catch (Exception e) {
                // Should never happen, implies a corrupt AndroidManifest.
            }

            sApplicationId = (customAppId != null && !customAppId.isEmpty())
                    ? customAppId
                    : CastMediaControlIntent.DEFAULT_MEDIA_RECEIVER_APPLICATION_ID;
        }

        return sApplicationId;
    }

    /**
     * @return the Cast application id corresponding to the source. Can be overridden downstream.
     */
    @Override
    public String getApplicationId() {
        return applicationId();
    }

    /**
     * @return the id identifying the media source
     */
    @Override
    public String getSourceId() {
        return mSourceId;
    }

    /**
     * @return the media URL to fling to the Cast device.
     */
    public String getMediaUrl() {
        return mMediaUrl;
    }

    private RemotingMediaSource(String sourceId, String mediaUrl) {
        mSourceId = sourceId;
        mMediaUrl = mediaUrl;
    }
}
