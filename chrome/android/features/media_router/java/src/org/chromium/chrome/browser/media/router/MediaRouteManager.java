// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import androidx.annotation.Nullable;

import java.util.List;

/**
 * A complimentary interface to {@link MediaRouteProvider}. Media route providers use the
 * implementation to communicate back to the {@link ChromeMediaRouter}.
 */
public interface MediaRouteManager {
    /**
     * Adds a {@link MediaRouteProvider} to be managed.
     * @param provider The provider to manage.
     */
    void addMediaRouteProvider(MediaRouteProvider provider);

    /**
     * Called when the sinks found by the media route provider for
     * the particular |sourceUrn| have changed.
     * @param sourceId The id of the source (presentation URL) that the sinks are received for.
     * @param provider The {@link MediaRouteProvider} that found the sinks.
     * @param sinks The list of {@link MediaSink}s
     */
    void onSinksReceived(String sourceId, MediaRouteProvider provider, List<MediaSink> sinks);

    /**
     * Called when the route was created successfully.
     * @param mediaRouteId the id of the created route.
     * @param mediaSinkId the id of the sink that the route was created for.
     * @param provider the provider that created and owns the route.
     * @param requestId the id of the route creation request.
     * @param wasLaunched whether the presentation on the other end of the route was launched or
     *                    just joined.
     */
    public void onRouteCreated(String mediaRouteId, String mediaSinkId, int requestId,
            MediaRouteProvider provider, boolean wasLaunched);

    /**
     * Called when the router failed to create or join a route.
     * @param errorText the error message to return to the page.
     * @param requestId the id of the route creation request.
     */
    public void onRouteRequestError(String errorText, int requestId);

    /**
     * Called when the route is terminated. This happens when the receiver app has stopped.
     *
     * @param mediaRouteId the id of the created route.
     */
    public void onRouteTerminated(String mediaRouteId);

    /**
     * Called when the route is closed with an optional error, for example, session launch failure.
     * This happens when a route leaves the session while not affecting the receiver app state.
     *
     * @param mediaRouteId the id of the created route.
     * @param error the error message. {@code null} indicates no error.
     */
    public void onRouteClosed(String mediaRouteId, @Nullable String error);

    /**
     * Called when a specified media route receives a message.
     * @param mediaRouteId The identifier of the media route.
     * @param message The message contents.
     */
    public void onMessage(String mediaRouteId, String message);
}
