// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Rect;

import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.WebContents;

/** A basic {@link WebContentsDelegateAndroid} that proxies methods into Tab. */
public abstract class TabWebContentsDelegateAndroid extends WebContentsDelegateAndroid {
    /**
     * Returns whether the page should resume accepting requests for the new window. This is
     * used when window creation is asynchronous and the navigations need to be delayed.
     */
    protected abstract boolean shouldResumeRequestsForCreatedWindow();

    /**
     * Creates a new tab with the already-created WebContents. The tab for the added
     * contents should be reparented correctly when this method returns.
     * @param sourceWebContents Source WebContents from which the new one is created.
     * @param webContents Newly created WebContents object.
     * @param disposition WindowOpenDisposition indicating how the tab should be created.
     * @param initialPosition Initial position of the content to be created.
     * @param userGesture {@code true} if opened by user gesture.
     * @return {@code true} if new tab was created successfully with a give WebContents.
     */
    protected abstract boolean addNewContents(
            WebContents sourceWebContents,
            WebContents webContents,
            int disposition,
            Rect initialPosition,
            boolean userGesture);

    /**
     * Sets the overlay mode.
     * Overlay mode means that we are currently using AndroidOverlays to display video, and
     * that the compositor's surface should support alpha and not be marked as opaque.
     */
    protected abstract void setOverlayMode(boolean useOverlayMode);

    /**
     * Provides info on web preferences for viewing downloaded media.
     * @return enabled Whether embedded media experience should be enabled.
     */
    protected boolean shouldEnableEmbeddedMediaExperience() {
        return false;
    }

    /**
     * @return web preferences for enabling Picture-in-Picture.
     */
    protected boolean isPictureInPictureEnabled() {
        return false;
    }

    /**
     * @return Night mode enabled/disabled for this Tab. To be used to propagate
     *         the preferred color scheme to the renderer.
     */
    protected boolean isNightModeEnabled() {
        return false;
    }

    /**
     * @return True if auto-darkening may be applied to web contents per Chrome browser settings.
     */
    protected boolean isForceDarkWebContentEnabled() {
        return false;
    }

    /**
     * Return true if app banners are to be permitted in this tab. May need to be overridden.
     * @return true if app banners are permitted, and false otherwise.
     */
    protected boolean canShowAppBanners() {
        return true;
    }

    /**
     * @return the WebAPK manifest scope. This gives frames within the scope increased privileges
     * such as autoplaying media unmuted.
     */
    protected String getManifestScope() {
        return null;
    }

    /**
     * Checks if the associated tab is currently presented in the context of custom tabs.
     * @return true if this is currently a custom tab.
     */
    protected boolean isCustomTab() {
        return false;
    }

    /**
     * Checks if the associated tab is running an activity for installed webapp (TWA only for now),
     * and whether the geolocation request should be delegated to the client app.
     * @return true if this is TWA and should delegate geolocation request.
     */
    protected boolean isInstalledWebappDelegateGeolocation() {
        return false;
    }

    /**
     * Checks if the associated tab uses modal context menu.
     * @return true if the current tab uses modal context menu.
     */
    protected boolean isModalContextMenu() {
        return true;
    }

    /**
     * @return true if the WebContents is a TWA.
     */
    public boolean isTrustedWebActivity(WebContents webContents) {
        return false;
    }
}
