// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
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

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;

/**
 * A picture in picture activity which get created when requesting
 * PiP from web API. The activity will connect to web API through
 * OverlayWindowAndroid.
 */
public class PictureInPictureActivity extends AsyncInitializationActivity {
    private static final String ACTION_PLAY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.Play";

    // If present, it indicates that the intent launches into PiP.
    public static final String LAUNCHED_KEY = "com.android.chrome.pictureinpicture.launched";
    // If present, these provide our aspect ratio hint.
    private static final String SOURCE_WIDTH_KEY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.source.width";
    private static final String SOURCE_HEIGHT_KEY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.source.height";

    private static final float MAX_ASPECT_RATIO = 2.39f;
    private static final float MIN_ASPECT_RATIO = 1 / 2.39f;

    private static long sNativeOverlayWindowAndroid;
    private static Tab sInitiatorTab;
    private static int sInitiatorTabTaskID;
    private static InitiatorTabObserver sTabObserver;

    // Used to verify Pre-T that the broadcast sender was Chrome. This extra can be removed when the
    // min supported version is Android T.
    private static final String EXTRA_RECEIVER_TOKEN = "receiver_token";

    private CompositorView mCompositorView;
    private MediaSessionObserver mMediaSessionObserver;
    private boolean mIsPlayPauseVisible;

    // If present, this is the video's aspect ratio.
    private Rational mAspectRatio;

    private BroadcastReceiver mMediaSessionReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (sNativeOverlayWindowAndroid == 0) return;

            if (intent.getAction() == null || !intent.getAction().equals(ACTION_PLAY)) return;
            if (!intent.hasExtra(EXTRA_RECEIVER_TOKEN)
                    || intent.getIntExtra(EXTRA_RECEIVER_TOKEN, 0) != this.hashCode()) {
                return;
            }

            PictureInPictureActivityJni.get().play(sNativeOverlayWindowAndroid);
        }
    };

    private static class InitiatorTabObserver extends EmptyTabObserver {
        private enum Status { OK, DESTROYED }
        private PictureInPictureActivity mActivity;
        private Status mStatus;

        InitiatorTabObserver() {
            mStatus = Status.OK;
        }

        public void setActivity(PictureInPictureActivity activity) {
            mActivity = activity;
        }

        public Status getStatus() {
            return mStatus;
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab.isClosing() || !isInitiatorTabAlive()) {
                mStatus = Status.DESTROYED;
                if (mActivity != null) mActivity.finish();
            }
        }

        @Override
        public void onCrash(Tab tab) {
            mStatus = Status.DESTROYED;
            if (mActivity != null) mActivity.finish();
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

        mCompositorView = CompositorViewFactory.create(
                this, getWindowAndroid(), new ThinWebViewConstraints());
        addContentView(mCompositorView.getView(),
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mCompositorView.getView().addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (sNativeOverlayWindowAndroid == 0) return;
                // We sometimes get an initial update of zero before getting something reasonable.
                if (top == bottom || left == right) return;

                PictureInPictureActivityJni.get().onViewSizeChanged(
                        sNativeOverlayWindowAndroid, right - left, bottom - top);
            }
        });

        PictureInPictureActivityJni.get().compositorViewCreated(
                sNativeOverlayWindowAndroid, mCompositorView);
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    @SuppressLint("NewAPI") // Picture-in-Picture API will not be enabled for oldver versions.
    public void onStart() {
        super.onStart();

        // Finish the activity if OverlayWindowAndroid has already been destroyed
        // or InitiatorTab has been destroyed by user or crashed.
        if (sNativeOverlayWindowAndroid == 0
                || sTabObserver.getStatus() == InitiatorTabObserver.Status.DESTROYED) {
            this.finish();
            return;
        }

        sTabObserver.setActivity(this);

        ContextUtils.registerNonExportedBroadcastReceiver(
                this, mMediaSessionReceiver, new IntentFilter(ACTION_PLAY));

        PictureInPictureActivityJni.get().onActivityStart(
                sNativeOverlayWindowAndroid, this, getWindowAndroid());

        // Add an observer to refresh the Picture-in-Picture params if the media
        // session state changes.
        MediaSession mediaSession = MediaSession.fromWebContents(sInitiatorTab.getWebContents());
        mMediaSessionObserver = new MediaSessionObserver(mediaSession) {
            @Override
            public void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
                updatePictureInPictureParams();
            }
        };

        // See if there are PiP hints in the extras.
        final Intent intent = getIntent();
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
        sNativeOverlayWindowAndroid = 0;
        sInitiatorTab.removeObserver(sTabObserver);
        sInitiatorTab = null;
        sTabObserver = null;

        if (mMediaSessionObserver != null) {
            mMediaSessionObserver.stopObserving();
            mMediaSessionObserver = null;
        }

        unregisterReceiver(mMediaSessionReceiver);
    }

    @Override
    @RequiresApi(api = Build.VERSION_CODES.O)
    public void onPictureInPictureModeChanged(
            boolean isInPictureInPictureMode, Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        if (!isInPictureInPictureMode) this.finish();
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this, /* listenToActivityState= */ true, getIntentRequestTracker());
    }

    @SuppressLint("NewApi")
    private static boolean isInitiatorTabAlive() {
        ActivityManager activityManager =
                (ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask appTask : activityManager.getAppTasks()) {
            if (appTask.getTaskInfo().id == sInitiatorTabTaskID) return true;
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

        // If the associated media session is not controllable then we should
        // place a play button in the Picture-in-Picture window that will
        // trigger playback.
        if (mMediaSessionObserver != null
                && !mMediaSessionObserver.getMediaSession().isControllable()
                && mIsPlayPauseVisible) {
            Intent intent = new Intent(ACTION_PLAY);
            intent.putExtra(EXTRA_RECEIVER_TOKEN, mMediaSessionReceiver.hashCode());
            PendingIntent pendingIntent = PendingIntent.getBroadcast(getApplicationContext(), 0,
                    intent, IntentUtils.getPendingIntentMutabilityFlag(false));

            actions.add(new RemoteAction(Icon.createWithResource(getApplicationContext(),
                                                 R.drawable.ic_play_arrow_white_36dp),
                    getApplicationContext().getResources().getText(R.string.accessibility_play), "",
                    pendingIntent));
        }

        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        builder.setActions(actions);
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

    @CalledByNative
    @SuppressLint("NewAPI")
    private void setPlayPauseButtonVisibility(boolean isVisible) {
        mIsPlayPauseVisible = isVisible;
        updatePictureInPictureParams();
    }

    @VisibleForTesting
    /* package */ Rational getAspectRatio() {
        return mAspectRatio;
    }

    @CalledByNative
    public static void createActivity(long nativeOverlayWindowAndroid, Object initiatorTab,
            int sourceX, int sourceY, int sourceWidth, int sourceHeight) {
        // Dissociate OverlayWindowAndroid if there is one already.
        if (sNativeOverlayWindowAndroid != 0) {
            PictureInPictureActivityJni.get().destroy(sNativeOverlayWindowAndroid);
        }

        sNativeOverlayWindowAndroid = nativeOverlayWindowAndroid;
        sInitiatorTab = (Tab) initiatorTab;
        sInitiatorTabTaskID = TabUtils.getActivity(sInitiatorTab).getTaskId();

        Context activityContext = null;
        final WindowAndroid window = sInitiatorTab.getWindowAndroid();
        if (window != null) {
            activityContext = window.getActivity().get();
        }
        Context context =
                (activityContext != null) ? activityContext : ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, PictureInPictureActivity.class);

        sTabObserver = new InitiatorTabObserver();
        sInitiatorTab.addObserver(sTabObserver);

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
        if (sNativeOverlayWindowAndroid != nativeOverlayWindowAndroid) return;

        sNativeOverlayWindowAndroid = 0;
    }

    // Allow tests to mock out our LaunchIntoPipHelper.  Returns the outgoing one.
    @VisibleForTesting
    /* package */ static LaunchIntoPipHelper setLaunchIntoPipHelper(LaunchIntoPipHelper helper) {
        LaunchIntoPipHelper original = sLaunchIntoPipHelper;
        sLaunchIntoPipHelper = helper;
        return original;
    }

    @NativeMethods
    public interface Natives {
        void onActivityStart(long nativeOverlayWindowAndroid, PictureInPictureActivity self,
                WindowAndroid window);

        void destroy(long nativeOverlayWindowAndroid);

        void play(long nativeOverlayWindowAndroid);

        void compositorViewCreated(long nativeOverlayWindowAndroid, CompositorView compositorView);

        void onViewSizeChanged(long nativeOverlayWindowAndroid, int width, int height);
    }
}
