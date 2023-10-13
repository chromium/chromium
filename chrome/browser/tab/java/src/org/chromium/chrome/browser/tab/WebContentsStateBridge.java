// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/**
 * Bridge into native serialization, deserialization, and management of {@link WebContentsState}.
 */
public class WebContentsStateBridge {
    /**
     * Creates a WebContents from the buffer.
     * @param isHidden Whether or not the tab initially starts hidden.
     * @return Pointer A WebContents object.
     */
    public static WebContents restoreContentsFromByteBuffer(
            WebContentsState webContentsState, boolean isHidden) {
        return WebContentsStateBridge.restoreContentsFromByteBuffer(
                webContentsState, isHidden, /*noRenderer=*/false);
    }

    /**
     * Creates a WebContents from the buffer.
     * @param isHidden Whether or not the tab initially starts hidden.
     * @param noRenderer Explicitly request to create without a renderer. If false a renderer may or
     *     may not be created.
     * @return Pointer A WebContents object.
     */
    public static WebContents restoreContentsFromByteBuffer(
            WebContentsState webContentsState, boolean isHidden, boolean noRenderer) {
        return WebContentsStateBridgeJni.get().restoreContentsFromByteBuffer(
                webContentsState.buffer(), webContentsState.version(), isHidden, noRenderer);
    }

    /**
     * Deletes navigation entries from this buffer matching predicate.
     * @param predicate Handle for a deletion predicate interpreted by native code.
    Only valid during this call frame.
     * @return WebContentsState A new state or null if nothing changed.
     */
    public static @Nullable WebContentsState deleteNavigationEntries(
            WebContentsState webContentsState, long predicate) {
        ByteBuffer newBuffer = WebContentsStateBridgeJni.get().deleteNavigationEntries(
                webContentsState.buffer(), webContentsState.version(), predicate);
        if (newBuffer == null) return null;
        WebContentsState newState = new WebContentsState(newBuffer);
        newState.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        return newState;
    }

    /**
     * Creates a WebContentsState for a tab that will be loaded lazily.
     * @param url URL that is pending.
     * @param referrerUrl URL for the referrer.
     * @param referrerPolicy Policy for the referrer.
     * @param initiatorOrigin Initiator of the navigation.
     * @param isIncognito Whether or not the state is meant to be incognito (e.g. encrypted).
     * @return ByteBuffer that represents a state representing a single pending URL.
     */
    public static ByteBuffer createSingleNavigationStateAsByteBuffer(String url, String referrerUrl,
            int referrerPolicy, @Nullable Origin initiatorOrigin, boolean isIncognito) {
        return WebContentsStateBridgeJni.get().createSingleNavigationStateAsByteBuffer(
                url, referrerUrl, referrerPolicy, initiatorOrigin, isIncognito);
    }

    /**
     * Returns the WebContents' state as a ByteBuffer.
     * @param webContents WebContents to pickle.
     * @return ByteBuffer containing the state of the WebContents.
     */
    public static ByteBuffer getContentsStateAsByteBuffer(WebContents webContents) {
        return WebContentsStateBridgeJni.get().getContentsStateAsByteBuffer(webContents);
    }

    /** @return Title currently being displayed in the saved state's current entry. */
    public static String getDisplayTitleFromState(WebContentsState contentsState) {
        return WebContentsStateBridgeJni.get().getDisplayTitleFromByteBuffer(
                contentsState.buffer(), contentsState.version());
    }

    /** @return URL currently being displayed in the saved state's current entry. */
    public static String getVirtualUrlFromState(WebContentsState contentsState) {
        return WebContentsStateBridgeJni.get().getVirtualUrlFromByteBuffer(
                contentsState.buffer(), contentsState.version());
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        WebContents restoreContentsFromByteBuffer(ByteBuffer buffer, int savedStateVersion,
                boolean initiallyHidden, boolean noRenderer);
        ByteBuffer getContentsStateAsByteBuffer(WebContents webcontents);
        ByteBuffer deleteNavigationEntries(ByteBuffer state, int saveStateVersion, long predicate);
        ByteBuffer createSingleNavigationStateAsByteBuffer(String url, String referrerUrl,
                int referrerPolicy, Origin initiatorOrigin, boolean isIncognito);
        String getDisplayTitleFromByteBuffer(ByteBuffer state, int savedStateVersion);
        String getVirtualUrlFromByteBuffer(ByteBuffer state, int savedStateVersion);
    }
}
