// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/** Contains the state for a {@link WebContents}. */
@NullMarked
public class WebContentsState implements Destroyable {
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
    private static class PackedData implements Destroyable {
        private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
        private int mRefCount = 1;

        /**
         * mBuffer should not be modified once it is set. Also, it is required to be a "direct"
         * buffer which is allocated outside the JVM heap, so that it can be accessed via the JNI
         * direct buffer methods, which means it has to be allocated with
         * ByteBuffer.allocateDirect() or similar.
         */
        private final ByteBuffer mBuffer;

        private final int mVersion;
        private final boolean mLastEntryWasPending;
        private long mNativeStringPointer;

        /**
         * @param buffer The buffer for the WebContentsState.
         * @param version The version of the WebContentsState.
         * @param nativeStringPointer The native string pointer for the buffer, may be 0 if the
         *     buffer is allocated in Java.
         * @param lastEntryWasPending Whether the last entry in the WebContentsState was for a
         *     pending load.
         */
        public PackedData(
                ByteBuffer buffer,
                int version,
                boolean lastEntryWasPending,
                long nativeStringPointer) {
            assert buffer.isDirect();
            mBuffer = buffer;
            mVersion = version;
            mLastEntryWasPending = lastEntryWasPending;
            mNativeStringPointer = nativeStringPointer;
            // There is no need to assert here as the buffer is allocated in Java.
            if (mNativeStringPointer == 0) {
                LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
            }
        }

        /** Returns the buffer for the WebContentsState. */
        public ByteBuffer buffer() {
            return mBuffer;
        }

        /** Returns the version of the WebContentsState. */
        public int version() {
            return mVersion;
        }

        /** Returns whether the last entry in the WebContentsState was a pending load. */
        public boolean lastEntryWasPending() {
            return mLastEntryWasPending;
        }

        /** Increments reference count for shared instances. */
        public void acquire() {
            ThreadUtils.assertOnUiThread();
            assert mRefCount > 0 : "Cannot acquire a destroyed PackedData";
            mRefCount++;
        }

        /** Destroys the native string pointer. */
        @Override
        public void destroy() {
            ThreadUtils.assertOnUiThread();
            assert mRefCount > 0 : "Underflow in PackedData ref count";
            mRefCount--;
            if (mRefCount == 0) {
                if (mNativeStringPointer != 0) {
                    WebContentsStateJni.get().freeStringPointer(mNativeStringPointer);
                    mNativeStringPointer = 0;
                    LifetimeAssert.destroy(mLifetimeAssert);
                }
            }
        }

        int getRefCountForTesting() {
            return mRefCount;
        }
    }

    /**
     * Metadata extracted from the serialized WebContentsState buffer. Contains essential
     * information needed to display the tab (title, URL, incognito status) without restoring the
     * full WebContents.
     */
    public static class WebContentsStateMetadata {
        /** The display title of the tab. */
        public final String title;

        /** The virtual URL of the tab. */
        public final String virtualUrl;

        /** Whether the tab is in incognito mode. */
        public final boolean isOffTheRecord;

        /**
         * Creates an instance of WebContentsStateMetadata.
         *
         * @param title The display title.
         * @param virtualUrl The virtual URL.
         * @param isOffTheRecord Whether it is incognito.
         */
        public WebContentsStateMetadata(String title, String virtualUrl, boolean isOffTheRecord) {
            this.title = title;
            this.virtualUrl = virtualUrl;
            this.isOffTheRecord = isOffTheRecord;
        }
    }

    /**
     * Factory method called by native code to create a WebContentsStateMetadata instance.
     *
     * @param title The display title.
     * @param virtualUrl The virtual URL.
     * @param isOffTheRecord Whether it is incognito.
     * @return A new WebContentsStateMetadata instance.
     */
    @CalledByNative
    public static WebContentsStateMetadata createMetadata(
            @JniType("std::u16string") String title,
            @JniType("std::string") String virtualUrl,
            boolean isOffTheRecord) {
        return new WebContentsStateMetadata(title, virtualUrl, isOffTheRecord);
    }

    // Cached metadata extracted from the buffer. Access is restricted to the UI thread.
    private @Nullable WebContentsStateMetadata mMetadata;
    private boolean mMetadataLoaded;
    private PackedData mPackedData;
    private boolean mIsDestroyed;

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
        return maybeCreateNewWebContentsState(buffer);
    }

    /**
     * Creates a {@link WebContentsState} for a tab that will be loaded lazily.
     *
     * @param profile The profile used for the tab.
     * @param title The title to display.
     * @param loadUrlParams The load url params to use.
     * @return ByteBuffer that represents a state representing a single pending URL.
     */
    public static @Nullable WebContentsState createSingleNavigationWebContentsState(
            Profile profile, @Nullable String title, LoadUrlParams loadUrlParams) {
        Referrer referrer = loadUrlParams.getReferrer();
        String url = loadUrlParams.getUrl();
        String referrerUrl = referrer != null ? referrer.getUrl() : null;
        // Policy will be ignored for null referrer url, 0 is just a placeholder.
        int referrerPolicy = referrer != null ? referrer.getPolicy() : 0;
        Origin initiatorOrigin = loadUrlParams.getInitiatorOrigin();

        ByteBuffer buffer =
                WebContentsStateJni.get()
                        .createSingleNavigationStateAsByteBuffer(
                                profile, title, url, referrerUrl, referrerPolicy, initiatorOrigin);
        return maybeCreateNewWebContentsState(buffer);
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
        mPackedData =
                new PackedData(
                        buffer,
                        version,
                        /* lastEntryWasPending= */ false,
                        /* nativeStringPointer= */ 0);
    }

    /**
     * @param buffer The buffer to use for the {@link WebContentsState}.
     * @param version The version of the {@link WebContentsState}.
     * @param nativeStringPointer The native string pointer for the buffer, may be 0 if the buffer
     *     is allocated in Java.
     */
    public WebContentsState(ByteBuffer buffer, int version, long nativeStringPointer) {
        mPackedData =
                new PackedData(
                        buffer, version, /* lastEntryWasPending= */ false, nativeStringPointer);
    }

    /**
     * Copy constructor that creates a new {@link WebContentsState} sharing the underlying {@link
     * PackedData} buffer with reference counting.
     *
     * @param other The existing WebContentsState to copy.
     */
    public WebContentsState(WebContentsState other) {
        ThreadUtils.assertOnUiThread();
        assert !other.mIsDestroyed : "Cannot copy a destroyed WebContentsState";
        mPackedData = other.mPackedData;
        mPackedData.acquire();
        mFallbackUrlForRestorationFailure = other.mFallbackUrlForRestorationFailure;
        if (other.mMetadataLoaded) {
            mMetadata = other.mMetadata;
            mMetadataLoaded = true;
        }
    }

    /** Destroys the {@link WebContentsState}. */
    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mPackedData.destroy();
    }

    /** Returns whether this {@link WebContentsState} has been destroyed. */
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    /** Returns the current reference count of the underlying packed data for testing. */
    public int getRefCountForTesting() {
        return mPackedData.getRefCountForTesting();
    }

    /** Returns the buffer for the {@link WebContentsState}. */
    public ByteBuffer buffer() {
        return mPackedData.buffer();
    }

    /** Returns the version of the {@link WebContentsState}. */
    public int version() {
        return mPackedData.version();
    }

    /**
     * Returns the metadata for the {@link WebContentsState}, or null if it fails to extract. The
     * metadata is extracted lazily on the first call and cached. Access is restricted to the UI
     * thread.
     *
     * @return The extracted metadata, or null.
     */
    public @Nullable WebContentsStateMetadata getMetadata() {
        if (!mMetadataLoaded && version() != INVALID_BUFFER_VERSION) {
            mMetadata = WebContentsStateJni.get().getMetadata(buffer(), version());
            mMetadataLoaded = true;
        }
        return mMetadata;
    }

    /** Returns the title currently being displayed in the saved state's current entry. */
    public @Nullable String getDisplayTitleFromState() {
        WebContentsStateMetadata metadata = getMetadata();
        return metadata != null ? metadata.title : null;
    }

    /** Returns the URL currently being displayed in the saved state's current entry. */
    public @Nullable String getVirtualUrlFromState() {
        WebContentsStateMetadata metadata = getMetadata();
        return metadata != null ? metadata.virtualUrl : null;
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
     * @return Whether any deletions happened.
     */
    public boolean deleteNavigationEntries(long predicate) {
        ByteBuffer newBuffer =
                WebContentsStateJni.get().deleteNavigationEntries(buffer(), version(), predicate);
        return maybeSwapPackedData(newBuffer, mPackedData.lastEntryWasPending());
    }

    /**
     * Appends a pending navigation to the {@link WebContentsState}.
     *
     * @param profile The profile used for the tab.
     * @param title The title to display.
     * @param loadUrlParams The load url params to use.
     * @param trackLastEntryWasPending Whether to track whether if the last entry was pending load.
     * @return Whether the operation was successful.
     */
    public boolean appendPendingNavigation(
            Profile profile,
            @Nullable String title,
            LoadUrlParams loadUrlParams,
            boolean trackLastEntryWasPending) {
        Referrer referrer = loadUrlParams.getReferrer();
        String url = loadUrlParams.getUrl();
        String referrerUrl = referrer != null ? referrer.getUrl() : null;
        // Policy will be ignored for null referrer url, 0 is just a placeholder.
        int referrerPolicy = referrer != null ? referrer.getPolicy() : 0;
        Origin initiatorOrigin = loadUrlParams.getInitiatorOrigin();

        ByteBuffer buffer =
                WebContentsStateJni.get()
                        .appendPendingNavigation(
                                profile,
                                buffer(),
                                version(),
                                mPackedData.lastEntryWasPending() && trackLastEntryWasPending,
                                title,
                                url,
                                referrerUrl,
                                referrerPolicy,
                                initiatorOrigin);
        return maybeSwapPackedData(buffer, /* lastEntryWasPending= */ trackLastEntryWasPending);
    }

    @VisibleForTesting
    boolean maybeSwapPackedData(@Nullable ByteBuffer buffer, boolean lastEntryWasPending) {
        if (buffer == null) return false;
        mPackedData.destroy();
        mPackedData =
                new PackedData(
                        buffer,
                        CONTENTS_STATE_CURRENT_VERSION,
                        lastEntryWasPending,
                        /* nativeStringPointer= */ 0);
        mMetadata = null;
        mMetadataLoaded = false;
        return true;
    }

    private static @Nullable WebContentsState maybeCreateNewWebContentsState(
            @Nullable ByteBuffer buffer) {
        if (buffer == null) return null;
        return new WebContentsState(buffer, CONTENTS_STATE_CURRENT_VERSION);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        @Nullable
        @JniType("content::WebContents*")
        WebContents restoreContentsFromByteBuffer(
                @JniType("Profile*") Profile profile,
                ByteBuffer buffer,
                int savedStateVersion,
                boolean initiallyHidden,
                boolean noRenderer);

        @Nullable ByteBuffer getContentsStateAsByteBuffer(
                @JniType("content::WebContents*") WebContents webcontents);

        @Nullable ByteBuffer deleteNavigationEntries(
                ByteBuffer state, int savedStateVersion, long predicate);

        @Nullable ByteBuffer createSingleNavigationStateAsByteBuffer(
                @JniType("Profile*") Profile profile,
                @JniType("std::optional<std::u16string>") @Nullable String title,
                @JniType("std::string") String url,
                @JniType("std::optional<std::string>") @Nullable String referrerUrl,
                int referrerPolicy,
                @JniType("std::optional<url::Origin>") @Nullable Origin initiatorOrigin);

        @Nullable ByteBuffer appendPendingNavigation(
                @JniType("Profile*") Profile profile,
                ByteBuffer buffer,
                int savedStateVersion,
                boolean clobberCurrentEntry,
                @JniType("std::optional<std::u16string>") @Nullable String title,
                @JniType("std::string") String url,
                @JniType("std::optional<std::string>") @Nullable String referrerUrl,
                int referrerPolicy,
                @JniType("std::optional<url::Origin>") @Nullable Origin initiatorOrigin);

        @Nullable WebContentsStateMetadata getMetadata(ByteBuffer buffer, int version);

        void freeStringPointer(long stringPointer);
    }
}
