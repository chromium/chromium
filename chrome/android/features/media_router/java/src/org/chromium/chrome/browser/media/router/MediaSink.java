// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;

import androidx.annotation.Nullable;

import com.google.android.gms.cast.CastDevice;

/**
 * A common descriptor of a device that can present some URI.
 */
public class MediaSink {
    private static final String CAST_SINK_URN_PREFIX = "urn:x-org.chromium:media:sink:cast-";
    private final String mId;
    private final String mName;
    private final CastDevice mDevice;

    /**
     * Constructor.
     * @param id A unique identifier of the sink.
     * @param name A user friendly name of the sink.
     * @param device {@link CastDevice} corresponding to this sink.
     */
    public MediaSink(String id, String name, CastDevice device) {
        mId = id;
        mName = name;
        mDevice = device;
    }

    /**
     * @return The unique identifier of the sink.
     */
    public String getId() {
        return mId;
    }

    /**
     * @return The user friendly name of the sink.
     */
    public String getName() {
        return mName;
    }

    /**
     * @return The valid URN representing the sink.
     */
    public String getUrn() {
        return CAST_SINK_URN_PREFIX + getId();
    }

    public CastDevice getDevice() {
        return mDevice;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (o instanceof MediaSink) {
            MediaSink other = (MediaSink) o;
            return mId.equals(other.getId()) && mName.equals(other.getName());
        }
        return false;
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((mId == null) ? 0 : mId.hashCode());
        result = prime * result + ((mName == null) ? 0 : mName.hashCode());
        return result;
    }

    @Override
    public String toString() {
        return String.format("MediaSink: %s, %s", getId(), getName());
    }

    /**
     * @param route The route information provided by Android.
     * @return A new MediaSink instance corresponding to the specified {@link RouteInfo}.
     */
    public static MediaSink fromRoute(MediaRouter.RouteInfo route) {
        return new MediaSink(
                route.getId(), route.getName(), CastDevice.getFromBundle(route.getExtras()));
    }

    /**
     * @param sinkId The id of the sink to find among known media routes.
     * @param router The instance of {@link MediaRouter} to enumerate the routes with.
     * @return A {@link MediaSink} corresponding to the {@link RouteInfo} with the specified id if
     * found, null otherwise.
     */
    @Nullable
    public static MediaSink fromSinkId(String sinkId, MediaRouter router) {
        for (MediaRouter.RouteInfo route : router.getRoutes()) {
            MediaSink sink = MediaSink.fromRoute(route);
            if (sink.getId().equals(sinkId)) return sink;
        }
        return null;
    }
}
