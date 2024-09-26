// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Bundle;
import android.util.Rational;
import android.util.Size;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.overlay_window.PlaybackState;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.HashSet;

/**
 * A picture in picture activity which get created when requesting
 * PiP from web API. The activity will connect to web API through
 * OverlayWindowAndroid.
 */
public class PictureInPictureActivity extends AsyncInitializationActivity {
    // Used to filter media buttons' remote action intents.
    private static final String MEDIA_ACTION =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.MediaAction";
    // Used to determine which action button has been touched.
    private static final String CONTROL_TYPE =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.ControlType";
    // Used to determine the media controls state. (e.g. microphone on/off)
    private static final String CONTROL_STATE =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.ControlState";

    // Use for passing unique window id to each PictureInPictureActivity instance.
    private static final String NATIVE_POINTER_KEY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.NativePointer";
    // Used for passing webcontents to PictureInPictureActivity.
    private static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.PictureInPicture.WebContents";
    // If present, it indicates that the intent launches into PiP.
    public static final String LAUNCHED_KEY = "com.android.chrome.pictureinpicture.launched";
    // If present, these provide our aspect ratio hint.
    private static final String SOURCE_WIDTH_KEY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.source.width";
    private static final String SOURCE_HEIGHT_KEY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.source.height";

    private static final float MAX_ASPECT_RATIO = 2.39f;
    private static final float MIN_ASPECT_RATIO = 1 / 2.39f;

    private static long sPendingNativeOverlayWindowAndroid;

    private long mNativeOverlayWindowAndroid;

    private Tab mInitiatorTab;
    private InitiatorTabObserver mTabObserver;

    private CompositorView mCompositorView;

    // If present, this is the video's aspect ratio.
    private Rational mAspectRatio;

    // Maximum pip width, in pixels, to prevent resizes that are too big.
    private int mMaxWidth;

    private MediaSessionBroadcastReceiver mMediaSessionReceiver;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    MediaActionButtonsManager mMediaActionsButtonsManager;

    /** A helper class for managing media action buttons in PictureInPicture window. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    class MediaActionButtonsManager {
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mPreviousSlide;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mPreviousTrack;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mPlay;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mPause;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mReplay;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mNextTrack;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mNextSlide;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final RemoteAction mHangUp;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final ToggleRemoteAction mMicrophone;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        final ToggleRemoteAction mCamera;

        private @PlaybackState int mPlaybackState;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        static class ToggleRemoteAction {
            private final RemoteAction mActionOn;
            private final RemoteAction mActionOff;
            private boolean mState;

            private ToggleRemoteAction(RemoteAction actionOn, RemoteAction actionOff) {
                mActionOn = actionOn;
                mActionOff = actionOff;
                mState = false;
            }

            private void setState(boolean on) {
                mState = on;
            }

            @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
            RemoteAction getAction() {
                return mState ? mActionOn : mActionOff;
            }
        }

        /** A set of {@link MediaSessionAction}. */
        private HashSet<Integer> mVisibleActions;

        private MediaActionButtonsManager() {
            int requestCode = 0;
            mPreviousTrack =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.PREVIOUS_TRACK,
                            R.drawable.ic_skip_previous_white_24dp,
                            R.string.accessibility_previous_track,
                            /* controlState= */ null);
            mPreviousSlide =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.PREVIOUS_SLIDE,
                            R.drawable.ic_skip_previous_white_24dp,
                            R.string.accessibility_previous_slide,
                            /* controlState= */ null);
            mPlay =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.PLAY,
                            R.drawable.ic_play_arrow_white_24dp,
                            R.string.accessibility_play,
                            /* controlState= */ null);
            mPause =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.PAUSE,
                            R.drawable.ic_pause_white_24dp,
                            R.string.accessibility_pause,
                            /* controlState= */ null);
            mReplay =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.PLAY,
                            R.drawable.ic_replay_white_24dp,
                            R.string.accessibility_replay,
                            /* controlState= */ null);
            mNextTrack =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.NEXT_TRACK,
                            R.drawable.ic_skip_next_white_24dp,
                            R.string.accessibility_next_track,
                            /* controlState= */ null);
            mNextSlide =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.NEXT_SLIDE,
                            R.drawable.ic_skip_next_white_24dp,
                            R.string.accessibility_next_slide,
                            /* controlState= */ null);
            mHangUp =
                    createRemoteAction(
                            requestCode++,
                            MediaSessionAction.HANG_UP,
                            R.drawable.ic_call_end_white_24dp,
                            R.string.accessibility_hang_up,
                            /* controlState= */ null);
            mMicrophone =
                    new ToggleRemoteAction(
                            createRemoteAction(
                                    requestCode++,
                                    MediaSessionAction.TOGGLE_MICROPHONE,
                                    R.drawable.ic_mic_white_24dp,
                                    R.string.accessibility_mute_microphone,
                                    /* controlState= */ true),
                            createRemoteAction(
                                    requestCode++,
                                    MediaSessionAction.TOGGLE_MICROPHONE,
                                    R.drawable.ic_mic_off_white_24dp,
                                    R.string.accessibility_unmute_microphone,
                                    /* controlState= */ false));
            mCamera =
                    new ToggleRemoteAction(
                            createRemoteAction(
                                    requestCode++,
                                    MediaSessionAction.TOGGLE_CAMERA,
                                    R.drawable.ic_videocam_24dp,
                                    R.string.accessibility_turn_off_camera,
                                    /* controlState= */ true),
                            createRemoteAction(
                                    requestCode++,
                                    MediaSessionAction.TOGGLE_CAMERA,
                                    R.drawable.ic_videocam_off_white_24dp,
                                    R.string.accessibility_turn_on_camera,
                                    /* controlState= */ false));

            mPlaybackState = PlaybackState.END_OF_VIDEO;
            mVisibleActions = new HashSet<>();
        }

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        @SuppressLint("NewApi")
        ArrayList<RemoteAction> getActionsForPictureInPictureParams() {
            ArrayList<RemoteAction> actions = new ArrayList<>();

            final boolean shouldShowPreviousNextTrack =
                    mVisibleActions.contains(MediaSessionAction.PREVIOUS_TRACK)
                            || mVisibleActions.contains(MediaSessionAction.NEXT_TRACK);
            if (shouldShowPreviousNextTrack) {
                mPreviousTrack.setEnabled(
                        mVisibleActions.contains(MediaSessionAction.PREVIOUS_TRACK));
                actions.add(mPreviousTrack);
            }

            final boolean shouldShowPreviousNextSlide =
                    mVisibleActions.contains(MediaSessionAction.PREVIOUS_SLIDE)
                            || mVisibleActions.contains(MediaSessionAction.NEXT_SLIDE);
            if (shouldShowPreviousNextSlide) {
                mPreviousSlide.setEnabled(
                        mVisibleActions.contains(MediaSessionAction.PREVIOUS_SLIDE));
                actions.add(mPreviousSlide);
            }

            if (mVisibleActions.contains(MediaSessionAction.PLAY)) {
                switch (mPlaybackState) {
                    case PlaybackState.PLAYING:
                        actions.add(mPause);
                        break;
                    case PlaybackState.PAUSED:
                        actions.add(mPlay);
                        break;
                    case PlaybackState.END_OF_VIDEO:
                        actions.add(mReplay);
                        break;
                }
            }

            if (shouldShowPreviousNextTrack) {
                mNextTrack.setEnabled(mVisibleActions.contains(MediaSessionAction.NEXT_TRACK));
                actions.add(mNextTrack);
            }

            if (shouldShowPreviousNextSlide) {
                mNextSlide.setEnabled(mVisibleActions.contains(MediaSessionAction.NEXT_SLIDE));
                actions.add(mNextSlide);
            }

            if (mVisibleActions.contains(MediaSessionAction.TOGGLE_MICROPHONE)) {
                actions.add(mMicrophone.getAction());
            }

            if (mVisibleActions.contains(MediaSessionAction.TOGGLE_CAMERA)) {
                actions.add(mCamera.getAction());
            }

            if (mVisibleActions.contains(MediaSessionAction.HANG_UP)) {
                actions.add(mHangUp);
            }

            // Insert a disabled placeholder remote action with transparent icon if action list is
            // empty. This is a workaround of the issue that android picture-in-picture will
            // fallback to default MediaSession when action list given is empty.
            // TODO (jazzhsu): Remove this when android picture-in-picture can accept empty list and
            // not fallback to default MediaSession.
            if (actions.isEmpty()) {
                RemoteAction placeholderAction =
                        new RemoteAction(
                                Icon.createWithBitmap(
                                        Bitmap.createBitmap(
                                                new int[] {Color.TRANSPARENT},
                                                1,
                                                1,
                                                Bitmap.Config.ARGB_8888)),
                                "",
                                "",
                                PendingIntent.getBroadcast(
                                        getApplicationContext(),
                                        -1,
                                        new Intent(MEDIA_ACTION),
                                        PendingIntent.FLAG_IMMUTABLE));
                placeholderAction.setEnabled(false);
                actions.add(placeholderAction);
            }

            return actions;
        }

        /**
         * Update visible actions.
         *
         * @param visibleActions A set of available {@link MediaSessionAction}.
         */
        private void updateVisibleActions(HashSet<Integer> visibleActions) {
            mVisibleActions = visibleActions;
        }

        private void updatePlaybackState(@PlaybackState int playbackState) {
            mPlaybackState = playbackState;
        }

        private void setMicrophoneMuted(boolean muted) {
            mMicrophone.setState(!muted);
        }

        private void setCameraOn(boolean cameraOn) {
            mCamera.setState(cameraOn);
        }

        /**
         * Create a remote action for picture-in-picture window.
         *
         * @param requestCode unique id for pending intent.
         * @param action {@link MediaSessionAction} that the action button is corresponding to.
         * @param iconResourceId used for getting icon associated with the id.
         * @param titleResourceId used for getting accessibility title associated with the id.
         * @param controlState indicate the action's state. (e.g. microphone on/off) Null if not
         * applicable
         */
        @SuppressLint("NewApi")
        private RemoteAction createRemoteAction(
                int requestCode,
                int action,
                int iconResourceId,
                int titleResourceId,
                Boolean controlState) {
            Intent intent = new Intent(MEDIA_ACTION);
            intent.setPackage(getApplicationContext().getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            intent.putExtra(CONTROL_TYPE, action);
            intent.putExtra(NATIVE_POINTER_KEY, mNativeOverlayWindowAndroid);
            if (controlState != null) {
                intent.putExtra(CONTROL_STATE, controlState);
            }

            PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(
                            getApplicationContext(),
                            requestCode,
                            intent,
                            PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

            return new RemoteAction(
                    Icon.createWithResource(getApplicationContext(), iconResourceId),
                    getApplicationContext().getResources().getText(titleResourceId),
                    "",
                    pendingIntent);
        }
    }

    private class MediaSessionBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!IntentUtils.isTrustedIntentFromSelf(intent)) return;

            long nativeOverlayWindowAndroid = intent.getLongExtra(NATIVE_POINTER_KEY, 0);
            if (nativeOverlayWindowAndroid != mNativeOverlayWindowAndroid
                    || mNativeOverlayWindowAndroid == 0) {
                return;
            }

            if (intent.getAction() == null || !intent.getAction().equals(MEDIA_ACTION)) return;

            Boolean controlState =
                    intent.hasExtra(CONTROL_STATE)
                            ? intent.getBooleanExtra(CONTROL_STATE, true)
                            : null;

            switch (intent.getIntExtra(CONTROL_TYPE, -1)) {
                case MediaSessionAction.PLAY:
                    PictureInPictureActivityJni.get()
                            .togglePlayPause(nativeOverlayWindowAndroid, /* toggleOn= */ true);
                    return;
                case MediaSessionAction.PAUSE:
                    PictureInPictureActivityJni.get()
                            .togglePlayPause(nativeOverlayWindowAndroid, /* toggleOn= */ false);
                    return;
                case MediaSessionAction.PREVIOUS_TRACK:
                    PictureInPictureActivityJni.get().previousTrack(nativeOverlayWindowAndroid);
                    return;
                case MediaSessionAction.NEXT_TRACK:
                    PictureInPictureActivityJni.get().nextTrack(nativeOverlayWindowAndroid);
                    return;
                case MediaSessionAction.PREVIOUS_SLIDE:
                    PictureInPictureActivityJni.get().previousSlide(nativeOverlayWindowAndroid);
                    return;
                case MediaSessionAction.NEXT_SLIDE:
                    PictureInPictureActivityJni.get().nextSlide(nativeOverlayWindowAndroid);
                    return;
                case MediaSessionAction.TOGGLE_MICROPHONE:
                    PictureInPictureActivityJni.get()
                            .toggleMicrophone(nativeOverlayWindowAndroid, !controlState);
                    return;
                case MediaSessionAction.TOGGLE_CAMERA:
                    PictureInPictureActivityJni.get()
                            .toggleCamera(nativeOverlayWindowAndroid, !controlState);
                    return;
                case MediaSessionAction.HANG_UP:
                    PictureInPictureActivityJni.get().hangUp(nativeOverlayWindowAndroid);
                    return;
                default:
                    return;
            }
        }
    }

    private class InitiatorTabObserver extends EmptyTabObserver {
        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            if (closing) {
                PictureInPictureActivity.this.onExitPictureInPicture(/* closeByNative= */ false);
            }
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab.isClosing()) {
                PictureInPictureActivity.this.onExitPictureInPicture(/* closeByNative= */ false);
            }
        }

        @Override
        public void onCrash(Tab tab) {
            PictureInPictureActivity.this.onExitPictureInPicture(/* closeByNative= */ false);
        }
    }

    /**
     * Interface to abstract makeLaunchIntoPip, which is only available in android T+.
     * Implementations of this API should expect to be called even on older version of Android, and
     * do nothing.  This allows tests to mock out the behavior.
     */
    interface LaunchIntoPipHelper {
        // Return a bundle to launch Picture in picture with `bounds` as the source rectangle.
        // May return null if the bundle could not be constructed.
        Bundle build(Context activityContext, Rect bounds);
    }

    // Default implementation that tries to `makeLaunchIntoPiP` via reflection.  Does nothing,
    // successfully, if this is not Android T or later.
    static LaunchIntoPipHelper sLaunchIntoPipHelper =
            new LaunchIntoPipHelper() {
                @Override
                public Bundle build(final Context activityContext, final Rect bounds) {
                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return null;

                    final Rational aspectRatio = new Rational(bounds.width(), bounds.height());
                    final PictureInPictureParams params =
                            new PictureInPictureParams.Builder()
                                    .setSourceRectHint(bounds)
                                    .setAspectRatio(aspectRatio)
                                    .build();
                    return ActivityOptions.makeLaunchIntoPip(params).toBundle();
                }
            };

    @Override
    protected void triggerLayoutInflation() {
        onInitialLayoutInflationComplete();
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        ProfileProvider profileProvider =
                new ProfileProvider() {
                    @NonNull
                    @Override
                    public Profile getOriginalProfile() {
                        return mInitiatorTab.getProfile().getOriginalProfile();
                    }

                    @Nullable
                    @Override
                    public Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        if (!mInitiatorTab.getProfile().isOffTheRecord()) {
                            throw new IllegalStateException(
                                    "Attempting to access invalid incognito profile from PiP");
                        }
                        return mInitiatorTab.getProfile();
                    }

                    @Override
                    public boolean hasOffTheRecordProfile() {
                        return mInitiatorTab.isIncognito();
                    }
                };
        supplier.set(profileProvider);
        return supplier;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        // Compute a somewhat arbitrary cut-off of 90% of the window's display width. The PiP
        // window can't be anywhere near this big, so the exact value doesn't matter. We'll ignore
        // resizes messages that are above it, since they're spurious.
        mMaxWidth = (int) (getWindowAndroid().getDisplay().getDisplayWidth() * 0.95);

        mCompositorView =
                CompositorViewFactory.create(
                        this, getWindowAndroid(), new ThinWebViewConstraints());
        addContentView(
                mCompositorView.getView(),
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mCompositorView
                .getView()
                .addOnLayoutChangeListener(
                        new OnLayoutChangeListener() {
                            @Override
                            public void onLayoutChange(
                                    View v,
                                    int left,
                                    int top,
                                    int right,
                                    int bottom,
                                    int oldLeft,
                                    int oldTop,
                                    int oldRight,
                                    int oldBottom) {
                                if (mNativeOverlayWindowAndroid == 0) return;
                                // We sometimes get an initial update of zero before getting
                                // something reasonable.
                                if (top == bottom || left == right) return;

                                // On close, sometimes we get a size update that's almost the entire
                                // display width.
                                // Pip window's can't be that big, so ignore it.
                                final int width = right - left;
                                if (width > mMaxWidth) return;

                                PictureInPictureActivityJni.get()
                                        .onViewSizeChanged(
                                                mNativeOverlayWindowAndroid, width, bottom - top);
                            }
                        });

        PictureInPictureActivityJni.get()
                .compositorViewCreated(mNativeOverlayWindowAndroid, mCompositorView);
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    @SuppressLint("NewAPI") // Picture-in-Picture API will not be enabled for oldver versions.
    public void onStart() {
        super.onStart();

        final Intent intent = getIntent();
        mNativeOverlayWindowAndroid = intent.getLongExtra(NATIVE_POINTER_KEY, 0);

        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        mInitiatorTab = TabUtils.fromWebContents(intent.getParcelableExtra(WEB_CONTENTS_KEY));

        // Finish the activity if OverlayWindowAndroid has already been destroyed
        // or InitiatorTab has been destroyed by user or crashed.
        if (mNativeOverlayWindowAndroid != sPendingNativeOverlayWindowAndroid
                || TabUtils.getActivity(mInitiatorTab) == null) {
            onExitPictureInPicture(/* closeByNative= */ false);
            return;
        }
        sPendingNativeOverlayWindowAndroid = 0;

        mTabObserver = new InitiatorTabObserver();
        mInitiatorTab.addObserver(mTabObserver);

        mMediaSessionReceiver = new MediaSessionBroadcastReceiver();
        ContextUtils.registerNonExportedBroadcastReceiver(
                this, mMediaSessionReceiver, new IntentFilter(MEDIA_ACTION));

        mMediaActionsButtonsManager = new MediaActionButtonsManager();

        PictureInPictureActivityJni.get()
                .onActivityStart(mNativeOverlayWindowAndroid, this, getWindowAndroid());

        // See if there are PiP hints in the extras.
        Size size =
                new Size(
                        intent.getIntExtra(SOURCE_WIDTH_KEY, 0),
                        intent.getIntExtra(SOURCE_HEIGHT_KEY, 0));
        if (size.getWidth() > 0 && size.getHeight() > 0) {
            clampAndStoreAspectRatio(size.getWidth(), size.getHeight());
        }

        // If the key is not present, then we need to launch into PiP now. Otherwise, the intent
        // did that for us.
        if (!getIntent().hasExtra(LAUNCHED_KEY)) {
            enterPictureInPictureMode(getPictureInPictureParams());
        }
    }

    @Override
    @RequiresApi(api = Build.VERSION_CODES.O)
    public void onPictureInPictureModeChanged(
            boolean isInPictureInPictureMode, Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        if (isInPictureInPictureMode) return;
        PictureInPictureActivityJni.get().onBackToTab(mNativeOverlayWindowAndroid);
        onExitPictureInPicture(/* closeByNative= */ false);
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this, /* listenToActivityState= */ true, getIntentRequestTracker());
    }

    @CalledByNative
    public void close() {
        onExitPictureInPicture(/* closeByNative= */ true);
    }

    private void onExitPictureInPicture(boolean closeByNative) {
        if (!closeByNative && mNativeOverlayWindowAndroid != 0) {
            PictureInPictureActivityJni.get().destroy(mNativeOverlayWindowAndroid);
        }

        if (mCompositorView != null) {
            mCompositorView.destroy();
            mCompositorView = null;
        }

        if (mMediaSessionReceiver != null) {
            unregisterReceiver(mMediaSessionReceiver);
            mMediaSessionReceiver = null;
        }

        if (mInitiatorTab != null) {
            mInitiatorTab.removeObserver(mTabObserver);
            mInitiatorTab = null;
        }
        mTabObserver = null;

        // If called by `closeByNative`, it means that the native side will be freed at some point
        // after this returns.  If `!closeByNative`, then we called destroyed on our native side (if
        // we have one).  Either way, we shouldn't refer to the native side after this.
        // See b/40063137 for details.
        mNativeOverlayWindowAndroid = 0;

        this.finish();
    }

    @SuppressLint("NewApi")
    private PictureInPictureParams getPictureInPictureParams() {
        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        builder.setActions(mMediaActionsButtonsManager.getActionsForPictureInPictureParams());
        builder.setAspectRatio(mAspectRatio);

        return builder.build();
    }

    @SuppressLint("NewApi")
    private void updatePictureInPictureParams() {
        setPictureInPictureParams(getPictureInPictureParams());
    }

    @CalledByNative
    @SuppressLint("NewApi")
    private void updateVideoSize(int width, int height) {
        clampAndStoreAspectRatio(width, height);
        updatePictureInPictureParams();
    }

    // Clamp the aspect ratio, and return the width assuming we allow the height.  If it's not
    // clamped, then it'll return the original width.  This is safe if `height` is zero.
    private static int clampAspectRatioAndRecomputeWidth(int width, int height) {
        if (height == 0) return width;

        final float aspectRatio =
                MathUtils.clamp(width / (float) height, MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);
        return (int) (height * aspectRatio);
    }

    private void clampAndStoreAspectRatio(int width, int height) {
        width = clampAspectRatioAndRecomputeWidth(width, height);
        mAspectRatio = new Rational(width, height);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    void setPlaybackState(@PlaybackState int playbackState) {
        mMediaActionsButtonsManager.updatePlaybackState(playbackState);
        updatePictureInPictureParams();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    void setMicrophoneMuted(boolean muted) {
        mMediaActionsButtonsManager.setMicrophoneMuted(muted);
        updatePictureInPictureParams();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    void setCameraState(boolean turnedOn) {
        mMediaActionsButtonsManager.setCameraOn(turnedOn);
        updatePictureInPictureParams();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    void updateVisibleActions(int[] actions) {
        HashSet<Integer> visibleActions = new HashSet<>();
        for (int action : actions) visibleActions.add(action);
        mMediaActionsButtonsManager.updateVisibleActions(visibleActions);
        updatePictureInPictureParams();
    }

    private long getNativeOverlayWindowAndroid() {
        return mNativeOverlayWindowAndroid;
    }

    private void resetNativeOverlayWindowAndroid() {
        mNativeOverlayWindowAndroid = 0;
    }

    @VisibleForTesting
    /* package */ Rational getAspectRatio() {
        return mAspectRatio;
    }

    @CalledByNative
    public static void createActivity(
            long nativeOverlayWindowAndroid,
            Object initiatorTab,
            int sourceX,
            int sourceY,
            int sourceWidth,
            int sourceHeight) {
        // Dissociate OverlayWindowAndroid if there is one already.
        if (sPendingNativeOverlayWindowAndroid != 0) {
            PictureInPictureActivityJni.get().destroy(sPendingNativeOverlayWindowAndroid);
        }

        sPendingNativeOverlayWindowAndroid = nativeOverlayWindowAndroid;

        Context activityContext = null;
        final WindowAndroid window = ((Tab) initiatorTab).getWindowAndroid();
        if (window != null) {
            activityContext = window.getActivity().get();
        }
        Context context =
                (activityContext != null) ? activityContext : ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, PictureInPictureActivity.class);
        intent.putExtra(WEB_CONTENTS_KEY, ((Tab) initiatorTab).getWebContents());

        intent.putExtra(NATIVE_POINTER_KEY, nativeOverlayWindowAndroid);

        Bundle optionsBundle = null;
        // Clamp the aspect ratio, which is okay even if they're unspecified.  We do this first in
        // case the width clamps to 0.  In that case, it's ignored as if it weren't given.
        sourceWidth = clampAspectRatioAndRecomputeWidth(sourceWidth, sourceHeight);

        if (sourceWidth > 0 && sourceHeight > 0) {
            // Auto-enter PiP if supported. This requires an Activity context.
            final Rect bounds =
                    new Rect(sourceX, sourceY, sourceX + sourceWidth, sourceY + sourceHeight);

            // Try to build the options bundle to launch into PiP.
            // Trivially out of bounds values indicate that they bounds are to be ignored.
            if (activityContext != null && bounds.left >= 0 && bounds.top >= 0) {
                optionsBundle = sLaunchIntoPipHelper.build(activityContext, bounds);
            }

            if (optionsBundle != null) {
                // That particular value doesn't matter, as long as the key is present.
                intent.putExtra(LAUNCHED_KEY, true);
            }

            // Add the aspect ratio parameters if we have them, so that we can enter pip with them
            // correctly immediately.
            intent.putExtra(SOURCE_WIDTH_KEY, sourceWidth);
            intent.putExtra(SOURCE_HEIGHT_KEY, sourceHeight);
        }

        context.startActivity(intent, optionsBundle);
    }

    @CalledByNative
    private static void onWindowDestroyed(long nativeOverlayWindowAndroid) {
        if (nativeOverlayWindowAndroid == sPendingNativeOverlayWindowAndroid) {
            sPendingNativeOverlayWindowAndroid = 0;
        }

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof PictureInPictureActivity)) continue;

            PictureInPictureActivity pipActivity = (PictureInPictureActivity) activity;
            if (nativeOverlayWindowAndroid == pipActivity.getNativeOverlayWindowAndroid()) {
                pipActivity.resetNativeOverlayWindowAndroid();
                pipActivity.onExitPictureInPicture(/* closeByNative= */ true);
            }
        }
    }

    // Allow tests to mock out our LaunchIntoPipHelper.  Returns the outgoing one.
    @VisibleForTesting
    /* package */ static LaunchIntoPipHelper setLaunchIntoPipHelper(LaunchIntoPipHelper helper) {
        LaunchIntoPipHelper original = sLaunchIntoPipHelper;
        sLaunchIntoPipHelper = helper;
        return original;
    }

    /* package */ View getViewForTesting() {
        return mCompositorView.getView();
    }

    @NativeMethods
    public interface Natives {
        void onActivityStart(
                long nativeOverlayWindowAndroid,
                PictureInPictureActivity self,
                WindowAndroid window);

        void destroy(long nativeOverlayWindowAndroid);

        void togglePlayPause(long nativeOverlayWindowAndroid, boolean toggleOn);

        void nextTrack(long nativeOverlayWindowAndroid);

        void previousTrack(long nativeOverlayWindowAndroid);

        void nextSlide(long nativeOverlayWindowAndroid);

        void previousSlide(long nativeOverlayWindowAndroid);

        void toggleMicrophone(long nativeOverlayWindowAndroid, boolean toggleOn);

        void toggleCamera(long nativeOverlayWindowAndroid, boolean toggleOn);

        void hangUp(long nativeOverlayWindowAndroid);

        void compositorViewCreated(long nativeOverlayWindowAndroid, CompositorView compositorView);

        void onViewSizeChanged(long nativeOverlayWindowAndroid, int width, int height);

        void onBackToTab(long nativeOverlayWindowAndroid);
    }
}
