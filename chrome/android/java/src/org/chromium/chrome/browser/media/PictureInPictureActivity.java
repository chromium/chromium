// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Bundle;
import android.util.Rational;
import android.util.Size;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.media.R;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.content_public.browser.WebContents;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
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

    // Used to verify Pre-T that the broadcast sender was Chrome. This extra can be removed when the
    // min supported version is Android T.
    private static final String EXTRA_RECEIVER_TOKEN =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.ReceiverToken";

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

    private boolean mIsPlaying;

    // MediaSessionaction, RemoteAction pairs.
    private HashMap<Integer, RemoteAction> mRemoteActions = new HashMap<>();

    // If present, this is the video's aspect ratio.
    private Rational mAspectRatio;

    // Maximum pip width, in pixels, to prevent resizes that are too big.
    private int mMaxWidth;

    private MediaSessionBroadcastReceiver mMediaSessionReceiver;

    private class MediaSessionBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            long nativeOverlayWindowAndroid = intent.getLongExtra(NATIVE_POINTER_KEY, 0);
            if (nativeOverlayWindowAndroid != mNativeOverlayWindowAndroid
                    || mNativeOverlayWindowAndroid == 0) {
                return;
            }

            if (intent.getAction() == null || !intent.getAction().equals(MEDIA_ACTION)) return;
            if (!intent.hasExtra(EXTRA_RECEIVER_TOKEN)
                    || intent.getIntExtra(EXTRA_RECEIVER_TOKEN, 0) != this.hashCode()) {
                return;
            }

            switch (intent.getIntExtra(CONTROL_TYPE, -1)) {
                case MediaSessionAction.PLAY:
                case MediaSessionAction.PAUSE:
                    // TODO(crbug.com/1345956): Play/pause state might get out of sync.
                    PictureInPictureActivityJni.get().togglePlayPause(nativeOverlayWindowAndroid);
                    return;
                case MediaSessionAction.PREVIOUS_TRACK:
                    PictureInPictureActivityJni.get().previousTrack(nativeOverlayWindowAndroid);
                    return;
                case MediaSessionAction.NEXT_TRACK:
                    PictureInPictureActivityJni.get().nextTrack(nativeOverlayWindowAndroid);
                    return;
                default:
                    return;
            }
        }
    };

    private class InitiatorTabObserver extends EmptyTabObserver {
        @Override
        public void onDestroyed(Tab tab) {
            if (tab.isClosing()) PictureInPictureActivity.this.finish();
        }

        @Override
        public void onCrash(Tab tab) {
            PictureInPictureActivity.this.finish();
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
    static LaunchIntoPipHelper sLaunchIntoPipHelper = new LaunchIntoPipHelper() {
        @Override
        public Bundle build(final Context activityContext, final Rect bounds) {
            if (!BuildInfo.isAtLeastT()) return null;

            Bundle optionsBundle = null;
            final Rational aspectRatio = new Rational(bounds.width(), bounds.height());
            final PictureInPictureParams params = new PictureInPictureParams.Builder()
                                                          .setSourceRectHint(bounds)
                                                          .setAspectRatio(aspectRatio)
                                                          .build();
            // Use reflection to access ActivityOptions#makeLaunchIntoPip
            // TODO(crbug.com/1331593): Do not use reflection, with a new sdk.
            try {
                Method methodMakeEnterContentPip = ActivityOptions.class.getMethod(
                        "makeLaunchIntoPip", PictureInPictureParams.class);
                ActivityOptions opts = (ActivityOptions) methodMakeEnterContentPip.invoke(
                        ActivityOptions.class, params);
                optionsBundle = (opts != null) ? opts.toBundle() : null;
            } catch (NoSuchMethodException e) {
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                e.printStackTrace();
            }
            return optionsBundle;
        }
    };

    @Override
    protected void triggerLayoutInflation() {
        onInitialLayoutInflationComplete();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        // Compute a somewhat arbitrary cut-off of 90% of the window's display width. The PiP
        // window can't be anywhere near this big, so the exact value doesn't matter. We'll ignore
        // resizes messages that are above it, since they're spurious.
        mMaxWidth = (int) ((getWindowAndroid().getDisplay().getDisplayWidth()) * 0.9);

        mCompositorView = CompositorViewFactory.create(
                this, getWindowAndroid(), new ThinWebViewConstraints());
        addContentView(mCompositorView.getView(),
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mCompositorView.getView().addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (mNativeOverlayWindowAndroid == 0) return;
                // We sometimes get an initial update of zero before getting something reasonable.
                if (top == bottom || left == right) return;

                // On close, sometimes we get a size update that's almost the entire display width.
                // Pip window's can't be that big, so ignore it.
                final int width = right - left;
                if (width > mMaxWidth) return;

                PictureInPictureActivityJni.get().onViewSizeChanged(
                        mNativeOverlayWindowAndroid, width, bottom - top);
            }
        });

        PictureInPictureActivityJni.get().compositorViewCreated(
                mNativeOverlayWindowAndroid, mCompositorView);
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
                || !isInitiatorTabAlive()) {
            this.finish();
            return;
        }
        sPendingNativeOverlayWindowAndroid = 0;

        mTabObserver = new InitiatorTabObserver();
        mInitiatorTab.addObserver(mTabObserver);

        mMediaSessionReceiver = new MediaSessionBroadcastReceiver();
        ContextUtils.registerNonExportedBroadcastReceiver(
                this, mMediaSessionReceiver, new IntentFilter(MEDIA_ACTION));

        initializeRemoteActions();

        PictureInPictureActivityJni.get().onActivityStart(
                mNativeOverlayWindowAndroid, this, getWindowAndroid());

        // See if there are PiP hints in the extras.
        Size size = new Size(
                intent.getIntExtra(SOURCE_WIDTH_KEY, 0), intent.getIntExtra(SOURCE_HEIGHT_KEY, 0));
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
    public void onStop() {
        super.onStop();
        if (mCompositorView != null) mCompositorView.destroy();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mMediaSessionReceiver != null) {
            unregisterReceiver(mMediaSessionReceiver);
            mMediaSessionReceiver = null;
        }

        if (mInitiatorTab != null) {
            mInitiatorTab.removeObserver(mTabObserver);
            mInitiatorTab = null;
        }
        mTabObserver = null;
    }

    @Override
    @RequiresApi(api = Build.VERSION_CODES.O)
    public void onPictureInPictureModeChanged(
            boolean isInPictureInPictureMode, Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        if (isInPictureInPictureMode) return;
        PictureInPictureActivityJni.get().onBackToTab(mNativeOverlayWindowAndroid);
        this.finish();
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this, /* listenToActivityState= */ true, getIntentRequestTracker());
    }

    @SuppressLint("NewApi")
    private boolean isInitiatorTabAlive() {
        if (mInitiatorTab == null) return false;

        ActivityManager activityManager =
                (ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask appTask : activityManager.getAppTasks()) {
            if (appTask.getTaskInfo().id == TabUtils.getActivity(mInitiatorTab).getTaskId()) {
                return true;
            }
        }

        return false;
    }

    @CalledByNative
    public void close() {
        this.finish();
    }

    @SuppressLint("NewApi")
    private PictureInPictureParams getPictureInPictureParams() {
        ArrayList<RemoteAction> actions = new ArrayList<>();

        actions.add(mRemoteActions.get(MediaSessionAction.PREVIOUS_TRACK));
        actions.add(mRemoteActions.get(
                mIsPlaying ? MediaSessionAction.PAUSE : MediaSessionAction.PLAY));
        actions.add(mRemoteActions.get(MediaSessionAction.NEXT_TRACK));

        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        builder.setActions(actions);
        builder.setAspectRatio(mAspectRatio);

        return builder.build();
    }

    @SuppressLint("NewApi")
    private void initializeRemoteActions() {
        mRemoteActions.put(MediaSessionAction.PLAY, createRemoteAction(MediaSessionAction.PLAY));
        mRemoteActions.put(MediaSessionAction.PAUSE, createRemoteAction(MediaSessionAction.PAUSE));
        mRemoteActions.put(MediaSessionAction.PREVIOUS_TRACK,
                createRemoteAction(MediaSessionAction.PREVIOUS_TRACK));
        mRemoteActions.put(
                MediaSessionAction.NEXT_TRACK, createRemoteAction(MediaSessionAction.NEXT_TRACK));
    }

    /**
     * Create remote actions for picutre-in-picture window.
     *
     * @param action {@link MediaSessionAction} that the action button is corresponding to.
     */
    @SuppressLint("NewApi")
    private RemoteAction createRemoteAction(int action) {
        Intent intent = new Intent(MEDIA_ACTION);
        intent.putExtra(EXTRA_RECEIVER_TOKEN, mMediaSessionReceiver.hashCode());
        intent.putExtra(CONTROL_TYPE, action);
        intent.putExtra(NATIVE_POINTER_KEY, mNativeOverlayWindowAndroid);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(getApplicationContext(), action,
                intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

        RemoteAction remoteAction;
        switch (action) {
            case MediaSessionAction.PLAY:
                remoteAction = new RemoteAction(Icon.createWithResource(getApplicationContext(),
                                                        R.drawable.ic_play_arrow_white_36dp),
                        getApplicationContext().getResources().getText(R.string.accessibility_play),
                        "", pendingIntent);
                break;
            case MediaSessionAction.PAUSE:
                remoteAction = new RemoteAction(Icon.createWithResource(getApplicationContext(),
                                                        R.drawable.ic_pause_white_36dp),
                        getApplicationContext().getResources().getText(
                                R.string.accessibility_pause),
                        "", pendingIntent);
                break;
            case MediaSessionAction.NEXT_TRACK:
                remoteAction = new RemoteAction(Icon.createWithResource(getApplicationContext(),
                                                        R.drawable.ic_skip_next_white_36dp),
                        getApplicationContext().getResources().getText(
                                R.string.accessibility_next_track),
                        "", pendingIntent);
                break;
            case MediaSessionAction.PREVIOUS_TRACK:
                remoteAction = new RemoteAction(Icon.createWithResource(getApplicationContext(),
                                                        R.drawable.ic_skip_previous_white_36dp),
                        getApplicationContext().getResources().getText(
                                R.string.accessibility_previous_track),
                        "", pendingIntent);
                break;
            default:
                remoteAction = null;
        }

        remoteAction.setEnabled(false);
        return remoteAction;
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

    @CalledByNative
    @SuppressLint("NewAPI")
    private void setPlaybackState(boolean isPlaying) {
        mIsPlaying = isPlaying;
        updatePictureInPictureParams();
    }

    @CalledByNative
    @SuppressLint("NewAPI")
    private void updateVisibleActions(int[] actions) {
        HashSet<Integer> visibleActions = new HashSet<>();
        for (int action : actions) visibleActions.add(action);

        for (Integer actionKey : mRemoteActions.keySet()) {
            RemoteAction remoteAction = mRemoteActions.get(actionKey);
            if (remoteAction.isEnabled() == visibleActions.contains(actionKey)) continue;
            remoteAction.setEnabled(!remoteAction.isEnabled());
        }
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
    public static void createActivity(long nativeOverlayWindowAndroid, Object initiatorTab,
            int sourceX, int sourceY, int sourceWidth, int sourceHeight) {
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

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
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
                pipActivity.finish();
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

    @VisibleForTesting
    /* package */ View getViewForTesting() {
        return mCompositorView.getView();
    }

    @NativeMethods
    public interface Natives {
        void onActivityStart(long nativeOverlayWindowAndroid, PictureInPictureActivity self,
                WindowAndroid window);

        void destroy(long nativeOverlayWindowAndroid);

        void togglePlayPause(long nativeOverlayWindowAndroid);
        void nextTrack(long nativeOverlayWindowAndroid);
        void previousTrack(long nativeOverlayWindowAndroid);

        void compositorViewCreated(long nativeOverlayWindowAndroid, CompositorView compositorView);

        void onViewSizeChanged(long nativeOverlayWindowAndroid, int width, int height);
        void onBackToTab(long nativeOverlayWindowAndroid);
    }
}
