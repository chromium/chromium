// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/**
 * Bridge into native serialization, deserialization, and management of {@link WebContentsState}.
 */
@NullMarked
public class WebContentsStateBridge {
    /**
     * Creates a WebContents from the buffer.
     *
     * @param webContentsState The webContentsState to modify.
     * @param profile The profile used for the tab.
     * @param isHidden Whether or not the tab initially starts hidden.
     * @return Pointer A WebContents object.
     */
    public static @Nullable WebContents restoreContentsFromByteBuffer(
            WebContentsState webContentsState, Profile profile, boolean isHidden) {
        return WebContentsStateBridge.restoreContentsFromByteBuffer(
                webContentsState, profile, isHidden, /* noRenderer= */ false);
    }

    /**
     * Creates a WebContents from the buffer.
     *
     * @param webContentsState The webContentsState to modify.
     * @param profile The profile used for the tab.
     * @param isHidden Whether or not the tab initially starts hidden.
     * @param noRenderer Explicitly request to create without a renderer. If false a renderer may or
     *     may not be created.
     * @return Pointer A WebContents object.
     */
    public static @Nullable WebContents restoreContentsFromByteBuffer(
            WebContentsState webContentsState,
            Profile profile,
            boolean isHidden,
            boolean noRenderer) {
        return WebContentsStateBridgeJni.get()
                .restoreContentsFromByteBuffer(
                        profile,
                        webContentsState.buffer(),
                        webContentsState.version(),
                        isHidden,
                        noRenderer);
    }

    /**
     * Deletes navigation entries from this buffer matching predicate.
     *
     * @param webContentsState The webContentsState to modify.
     * @param predicate Handle for a deletion predicate interpreted by native code. Only valid
     *     during this call frame.
     * @return WebContentsState A new state or null if nothing changed.
     */
    public static @Nullable WebContentsState deleteNavigationEntries(
            WebContentsState webContentsState, long predicate) {
        ByteBuffer newBuffer =
                WebContentsStateBridgeJni.get()
                        .deleteNavigationEntries(
                                webContentsState.buffer(), webContentsState.version(), predicate);
        return newWebContentsStateFromByteBuffer(newBuffer);
    }

    /**
     * Creates a WebContentsState for a tab that will be loaded lazily.
     *
     * @param profile The profile used for the tab.
     * @param title The title to display.
     * @param url URL that is pending.
     * @param referrerUrl URL for the referrer.
     * @param referrerPolicy Policy for the referrer.
     * @param initiatorOrigin Initiator of the navigation.
     * @return ByteBuffer that represents a state representing a single pending URL.
     */
    public static @Nullable ByteBuffer createSingleNavigationStateAsByteBuffer(
            Profile profile,
            @Nullable String title,
            String url,
            @Nullable String referrerUrl,
            int referrerPolicy,
            @Nullable Origin initiatorOrigin) {
        return WebContentsStateBridgeJni.get()
                .createSingleNavigationStateAsByteBuffer(
                        profile, title, url, referrerUrl, referrerPolicy, initiatorOrigin);
    }

    /**
     * Creates a WebContentsState for a tab that will be loaded lazily.
     *
     * @param profile The profile used for the tab.
     * @param webContentsState The webContentsState to modify.
     * @param title The title to display.
     * @param url URL that is pending.
     * @param referrerUrl URL for the referrer.
     * @param referrerPolicy Policy for the referrer.
     * @param initiatorOrigin Initiator of the navigation.
     * @return ByteBuffer that represents a state with the pending navigation attached.
     */
    public static @Nullable WebContentsState appendPendingNavigation(
            Profile profile,
            WebContentsState webContentsState,
            @Nullable String title,
            String url,
            @Nullable String referrerUrl,
            int referrerPolicy,
            @Nullable Origin initiatorOrigin) {
        ByteBuffer buffer =
                WebContentsStateBridgeJni.get()
                        .appendPendingNavigation(
                                profile,
                                webContentsState.buffer(),
                                webContentsState.version(),
                                title,
                                url,
                                referrerUrl,
                                referrerPolicy,
                                initiatorOrigin);
        return newWebContentsStateFromByteBuffer(buffer);
    }

    /**
     * Returns the WebContents' state as a ByteBuffer.
     *
     * @param webContents WebContents to pickle.
     * @return ByteBuffer containing the state of the WebContents.
     */
    public static @Nullable ByteBuffer getContentsStateAsByteBuffer(WebContents webContents) {
        return WebContentsStateBridgeJni.get().getContentsStateAsByteBuffer(webContents);
    }

    /** @return Title currently being displayed in the saved state's current entry. */
    public static @Nullable String getDisplayTitleFromState(WebContentsState contentsState) {
        return WebContentsStateBridgeJni.get()
                .getDisplayTitleFromByteBuffer(contentsState.buffer(), contentsState.version());
    }

    /** @return URL currently being displayed in the saved state's current entry. */
    public static @Nullable String getVirtualUrlFromState(WebContentsState contentsState) {
        return WebContentsStateBridgeJni.get()
                .getVirtualUrlFromByteBuffer(contentsState.buffer(), contentsState.version());
    }

    private static @Nullable WebContentsState newWebContentsStateFromByteBuffer(
            @Nullable ByteBuffer buffer) {
        if (buffer == null) return null;
        WebContentsState newState = new WebContentsState(buffer);
        newState.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        return newState;
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        @Nullable WebContents restoreContentsFromByteBuffer(
                @JniType("Profile*") Profile profile,
                ByteBuffer buffer,
                int savedStateVersion,
                boolean initiallyHidden,
                boolean noRenderer);

        @Nullable ByteBuffer getContentsStateAsByteBuffer(WebContents webcontents);

        @Nullable ByteBuffer deleteNavigationEntries(
                ByteBuffer state, int saveStateVersion, long predicate);

        @Nullable ByteBuffer createSingleNavigationStateAsByteBuffer(
                @JniType("Profile*") Profile profile,
                @Nullable String title,
                String url,
                @Nullable String referrerUrl,
                int referrerPolicy,
                @Nullable Origin initiatorOrigin);

        @Nullable ByteBuffer appendPendingNavigation(
                @JniType("Profile*") Profile profile,
                ByteBuffer buffer,
                int savedStateVersion,
                @Nullable String title,
                String url,
                @Nullable String referrerUrl,
                int referrerPolicy,
                @Nullable Origin initiatorOrigin);

        @Nullable String getDisplayTitleFromByteBuffer(ByteBuffer state, int savedStateVersion);

        @Nullable String getVirtualUrlFromByteBuffer(ByteBuffer state, int savedStateVersion);
    }
}
