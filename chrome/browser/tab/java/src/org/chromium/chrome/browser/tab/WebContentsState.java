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

/** Contains the state for a {@link WebContents}. */
@NullMarked
public class WebContentsState {
    /** Version number used to denote an invalid buffer. */
    public static final int INVALID_BUFFER_VERSION = -1;

    /**
     * Version number of the format used to save the {@link WebContents} navigation history, as
     * returned by {@code TabStateJni.get().getContentsStateAsByteBuffer()}.
     *
     * <pre>
     *   Version labels:
     *     0 - Chrome m18 (Deprecated)
     *     1 - Chrome m25 (Deprecated)
     *     2 - Chrome m26+
     * </pre>
     */
    public static final int CONTENTS_STATE_CURRENT_VERSION = 2;

    private static @Nullable WebContentsState sEmptyWebContentsState;

    /** A packed (pickle) representation of the navigation entries for a {@link WebContents}. */
    private static class PackedData {
        /**
         * mBuffer should not be modified once it is set. Also, it is required to be a "direct"
         * buffer which is allocated outside the JVM heap, so that it can be accessed via the JNI
         * direct buffer methods, which means it has to be allocated with
         * ByteBuffer.allocateDirect() or similar.
         */
        private final ByteBuffer mBuffer;

        private final int mVersion;

        /**
         * @param buffer The buffer for the WebContentsState.
         * @param version The version of the WebContentsState.
         */
        public PackedData(ByteBuffer buffer, int version) {
            assert buffer.isDirect();
            mBuffer = buffer;
            mVersion = version;
        }

        /** Returns the buffer for the WebContentsState. */
        public ByteBuffer buffer() {
            return mBuffer;
        }

        /** Returns the version of the WebContentsState. */
        public int version() {
            return mVersion;
        }
    }

    // TODO(crbug.com/447345580): Allow this to swap with an unpacked representation.
    private final PackedData mPackedData;

    private @Nullable String mFallbackUrlForRestorationFailure;

    /**
     * Returns the {@link WebContents}' state as a {@link WebContentsState}.
     *
     * @param webContents {@link WebContents} to pickle.
     * @return {@link WebContentsState} containing the state of the {@link WebContents} or null if
     *     something went wrong.
     */
    public static @Nullable WebContentsState getWebContentsStateFromWebContents(
            WebContents webContents) {
        ByteBuffer buffer = WebContentsStateJni.get().getContentsStateAsByteBuffer(webContents);
        return newWebContentsStateFromByteBuffer(buffer);
    }

    /**
     * Creates a {@link WebContentsState} for a tab that will be loaded lazily.
     *
     * @param profile The profile used for the tab.
     * @param title The title to display.
     * @param url URL that is pending.
     * @param referrerUrl URL for the referrer.
     * @param referrerPolicy Policy for the referrer.
     * @param initiatorOrigin Initiator of the navigation.
     * @return ByteBuffer that represents a state representing a single pending URL.
     */
    public static @Nullable WebContentsState createSingleNavigationWebContentsState(
            Profile profile,
            @Nullable String title,
            String url,
            @Nullable String referrerUrl,
            int referrerPolicy,
            @Nullable Origin initiatorOrigin) {
        ByteBuffer buffer =
                WebContentsStateJni.get()
                        .createSingleNavigationStateAsByteBuffer(
                                profile, title, url, referrerUrl, referrerPolicy, initiatorOrigin);
        return newWebContentsStateFromByteBuffer(buffer);
    }

    /** Returns a singleton empty {@link WebContentsState}. */
    public static WebContentsState getTempWebContentsState() {
        if (sEmptyWebContentsState == null) {
            sEmptyWebContentsState =
                    new WebContentsState(ByteBuffer.allocateDirect(0), INVALID_BUFFER_VERSION);
        }
        return sEmptyWebContentsState;
    }

    /**
     * @param buffer The buffer to use for the {@link WebContentsState}.
     * @param version The version of the {@link WebContentsState}.
     */
    public WebContentsState(ByteBuffer buffer, int version) {
        mPackedData = new PackedData(buffer, version);
    }

    /** Returns the buffer for the {@link WebContentsState}. */
    public ByteBuffer buffer() {
        return mPackedData.buffer();
    }

    /** Returns the version of the {@link WebContentsState}. */
    public int version() {
        return mPackedData.version();
    }

    /** Returns the title currently being displayed in the saved state's current entry. */
    public @Nullable String getDisplayTitleFromState() {
        return WebContentsStateJni.get().getDisplayTitleFromByteBuffer(buffer(), version());
    }

    /** Returns the URL currently being displayed in the saved state's current entry. */
    public @Nullable String getVirtualUrlFromState() {
        return WebContentsStateJni.get().getVirtualUrlFromByteBuffer(buffer(), version());
    }

    /** Get the URL to be loaded if restoring the serialized web content state fails. */
    public @Nullable String getFallbackUrlForRestorationFailure() {
        return mFallbackUrlForRestorationFailure;
    }

    /** Set the URL to be loaded if restoring the serialized web content state fails. */
    public void setFallbackUrlForRestorationFailure(String fallbackUrlForRestorationFailure) {
        mFallbackUrlForRestorationFailure = fallbackUrlForRestorationFailure;
    }

    /**
     * Creates a {@link WebContents from the buffer.
     *
     * @param webContentsState The webContentsState to modify.
     * @param profile The profile used for the tab.
     * @param isHidden Whether or not the tab initially starts hidden.
     * @return A {@link WebContents} object or null if something went wrong.
     */
    public @Nullable WebContents restoreWebContents(Profile profile, boolean isHidden) {
        return restoreWebContents(profile, isHidden, /* noRenderer= */ false);
    }

    /**
     * Creates a {@link WebContents} from the buffer.
     *
     * @param webContentsState The webContentsState to modify.
     * @param profile The profile used for the tab.
     * @param isHidden Whether or not the tab initially starts hidden.
     * @param noRenderer Explicitly request to create without a renderer. If false a renderer may or
     *     may not be created.
     * @return A {@link WebContents} object or null if something went wrong.
     */
    public @Nullable WebContents restoreWebContents(
            Profile profile, boolean isHidden, boolean noRenderer) {
        return WebContentsStateJni.get()
                .restoreContentsFromByteBuffer(profile, buffer(), version(), isHidden, noRenderer);
    }

    /**
     * Deletes navigation entries from this {@link WebContentsState} matching the predicate.
     *
     * @param predicate Handle for a deletion predicate interpreted by native code. Only valid
     *     during this call frame.
     * @return A new {@link WebContentsState} or null if nothing changed.
     */
    public @Nullable WebContentsState deleteNavigationEntries(long predicate) {
        ByteBuffer newBuffer =
                WebContentsStateJni.get().deleteNavigationEntries(buffer(), version(), predicate);
        return newWebContentsStateFromByteBuffer(newBuffer);
    }

    /**
     * Appends a pending navigation to the {@link WebContentsState}.
     *
     * @param profile The profile used for the tab.
     * @param title The title to display.
     * @param url URL that is pending.
     * @param referrerUrl URL for the referrer.
     * @param referrerPolicy Policy for the referrer.
     * @param initiatorOrigin Initiator of the navigation.
     * @return A new {@link WebContentsState} with the pending navigation attached.
     */
    public @Nullable WebContentsState appendPendingNavigation(
            Profile profile,
            @Nullable String title,
            String url,
            @Nullable String referrerUrl,
            int referrerPolicy,
            @Nullable Origin initiatorOrigin) {
        ByteBuffer buffer =
                WebContentsStateJni.get()
                        .appendPendingNavigation(
                                profile,
                                buffer(),
                                version(),
                                title,
                                url,
                                referrerUrl,
                                referrerPolicy,
                                initiatorOrigin);
        return newWebContentsStateFromByteBuffer(buffer);
    }

    private static @Nullable WebContentsState newWebContentsStateFromByteBuffer(
            @Nullable ByteBuffer buffer) {
        if (buffer == null) return null;
        return new WebContentsState(buffer, WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
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
