// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.UnguessableToken;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.overlay_window.PlaybackState;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * A base class for video overlay activities (e.g. Picture-in-Picture, Immersive Playback). This
 * class handles the common lifecycle and JNI logic.
 */
@NullMarked
public abstract class VideoOverlayActivity extends AsyncInitializationActivity {
    protected static final String NATIVE_TOKEN_KEY =
            "org.chromium.chrome.browser.media.VideoOverlayActivity.NativeToken";
    protected static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.VideoOverlayActivity.WebContents";

    private @Nullable UnguessableToken mNativeToken;
    private long mNativeOverlayWindowAndroid;

    private @Nullable Tab mInitiatorTab;
    private @Nullable InitiatorTabObserver mTabObserver;

    private @Nullable CompositorView mCompositorView;

    private class InitiatorTabObserver extends EmptyTabObserver {
        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            if (closing) {
                finishOverlay(/* closeByNative= */ false);
            }
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab.isClosing()) {
                finishOverlay(/* closeByNative= */ false);
            }
        }

        @Override
        public void onCrash(Tab tab) {
            finishOverlay(/* closeByNative= */ false);
        }
    }

    @Override
    protected void triggerLayoutInflation() {
        onInitialLayoutInflationComplete();
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        ProfileProvider profileProvider =
                new ProfileProvider() {
                    @Override
                    public Profile getOriginalProfile() {
                        return assumeNonNull(mInitiatorTab).getProfile().getOriginalProfile();
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        if (!assumeNonNull(mInitiatorTab).getProfile().isOffTheRecord()) {
                            assert !createIfNeeded;
                            return null;
                        }
                        return mInitiatorTab.getProfile();
                    }
                };
        supplier.set(profileProvider);
        return supplier;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                getIntentRequestTracker(),
                getInsetObserver(),
                /* occlusionTrackingAllowed= */ true);
    }

    @Override
    @Initializer
    protected void onPreCreate() {
        super.onPreCreate();

        final Intent intent = getIntent();
        UnguessableToken token = intent.getParcelableExtra(NATIVE_TOKEN_KEY);
        if (token == null) {
            finishOverlay(/* closeByNative= */ false);
            return;
        }

        mNativeToken = assumeNonNull(token);
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        mInitiatorTab = TabUtils.fromWebContents(intent.getParcelableExtra(WEB_CONTENTS_KEY));
        if (mInitiatorTab != null) {
            mTabObserver = new InitiatorTabObserver();
            mInitiatorTab.addObserver(mTabObserver);
        }
    }

    @Override
    @Initializer
    public void onStart() {
        super.onStart();

        if (isInitiatorTabDestroyed()) {
            finishOverlay(/* closeByNative= */ false);
            return;
        }
    }

    protected boolean isInitiatorTabDestroyed() {
        return mInitiatorTab == null || TabUtils.getActivity(mInitiatorTab) == null;
    }

    @CalledByNative
    public void close() {
        finishOverlay(/* closeByNative= */ true);
    }

    @CalledByNative
    protected void onWindowDestroyed() {
        if (mNativeOverlayWindowAndroid == 0) {
            return;
        }
        mNativeOverlayWindowAndroid = 0;
        finishOverlay(/* closeByNative= */ true);
    }

    @CalledByNative
    public abstract void setPlaybackState(@PlaybackState int playbackState);

    @CalledByNative
    public abstract void updateVideoSize(int width, int height);

    @CalledByNative
    public void updateVisibleActions(int[] actions) {}

    @CalledByNative
    public void setMicrophoneMuted(boolean muted) {}

    @CalledByNative
    public void setCameraState(boolean turnedOn) {}

    @CalledByNative
    public void setMediaPosition(long durationMs, long positionMs, double playbackRate) {}

    @CalledByNative
    public void setImmersiveVideoOptions(int stereoMode, int projectionType) {}

    protected void togglePlayPause(boolean toggleOn) {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().togglePlayPause(mNativeOverlayWindowAndroid, toggleOn);
        }
    }

    protected void nextTrack() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().nextTrack(mNativeOverlayWindowAndroid);
        }
    }

    protected void previousTrack() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().previousTrack(mNativeOverlayWindowAndroid);
        }
    }

    protected void nextSlide() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().nextSlide(mNativeOverlayWindowAndroid);
        }
    }

    protected void previousSlide() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().previousSlide(mNativeOverlayWindowAndroid);
        }
    }

    protected void toggleMicrophone(boolean toggleOn) {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().toggleMicrophone(mNativeOverlayWindowAndroid, toggleOn);
        }
    }

    protected void toggleCamera(boolean toggleOn) {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().toggleCamera(mNativeOverlayWindowAndroid, toggleOn);
        }
    }

    protected void hangUp() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().hangUp(mNativeOverlayWindowAndroid);
        }
    }

    protected void hide() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().hide(mNativeOverlayWindowAndroid);
        }
    }

    private void compositorViewCreated(CompositorView compositorView) {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get()
                    .compositorViewCreated(mNativeOverlayWindowAndroid, compositorView);
        }
    }

    protected void onViewSizeChanged(int width, int height) {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get()
                    .onViewSizeChanged(mNativeOverlayWindowAndroid, width, height);
        }
    }

    protected void onBackToTab() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().onBackToTab(mNativeOverlayWindowAndroid);
        }
    }

    protected void onDismissal() {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().onDismissal(mNativeOverlayWindowAndroid);
        }
    }

    protected void seekTo(long positionMs) {
        if (mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().seekTo(mNativeOverlayWindowAndroid, positionMs);
        }
    }

    protected @Nullable CompositorView getCompositorView() {
        return mCompositorView;
    }

    protected void setCompositorView(CompositorView compositorView) {
        if (mCompositorView == null && compositorView != null) {
            mCompositorView = compositorView;
            compositorViewCreated(compositorView);
        }
    }

    public boolean isNativeHandleInitialized() {
        return mNativeOverlayWindowAndroid != 0;
    }

    protected UnguessableToken getNativeToken() {
        return assumeNonNull(mNativeToken);
    }

    protected void onActivityStart() {
        mNativeOverlayWindowAndroid =
                VideoOverlayActivityJni.get()
                        .onActivityStart(assumeNonNull(mNativeToken), this, getWindowAndroid());
        // If the compositor view was created earlier during finishNativeInitialization (part of
        // onCreate), notify native now that the native handle is ready.
        if (mCompositorView != null) {
            compositorViewCreated(mCompositorView);
        }
    }

    protected void finishOverlay(boolean closeByNative) {
        cleanup();

        if (!closeByNative && mNativeOverlayWindowAndroid != 0) {
            VideoOverlayActivityJni.get().destroyStartedByJava(mNativeOverlayWindowAndroid);
        }
        mNativeOverlayWindowAndroid = 0;

        if (mCompositorView != null) {
            mCompositorView.destroy();
            mCompositorView = null;
        }

        if (mInitiatorTab != null) {
            if (mTabObserver != null) {
                mInitiatorTab.removeObserver(mTabObserver);
            }
            mInitiatorTab = null;
        }
        mTabObserver = null;

        this.finish();
    }

    /** Subclasses should implement this to perform their own cleanup. */
    protected abstract void cleanup();

    @NativeMethods
    public interface Natives {
        long onActivityStart(
                UnguessableToken token, VideoOverlayActivity self, @Nullable WindowAndroid window);

        void destroyStartedByJava(long nativeOverlayWindowAndroid);

        void togglePlayPause(long nativeOverlayWindowAndroid, boolean toggleOn);

        void nextTrack(long nativeOverlayWindowAndroid);

        void previousTrack(long nativeOverlayWindowAndroid);

        void nextSlide(long nativeOverlayWindowAndroid);

        void previousSlide(long nativeOverlayWindowAndroid);

        void toggleMicrophone(long nativeOverlayWindowAndroid, boolean toggleOn);

        void toggleCamera(long nativeOverlayWindowAndroid, boolean toggleOn);

        void hangUp(long nativeOverlayWindowAndroid);

        void hide(long nativeOverlayWindowAndroid);

        void compositorViewCreated(long nativeOverlayWindowAndroid, CompositorView compositorView);

        void onViewSizeChanged(long nativeOverlayWindowAndroid, int width, int height);

        void onBackToTab(long nativeOverlayWindowAndroid);

        void onDismissal(long nativeOverlayWindowAndroid);

        void seekTo(long nativeOverlayWindowAndroid, long positionMs);
    }
}
