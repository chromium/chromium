// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.DaydreamApi;
import com.google.vr.ndk.base.GvrUiLayout;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ApplicationLifetime;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.RejectedExecutionException;

/**
 * Manages interactions with the VR Shell.
 */
@JNINamespace("vr")
public class VrShellDelegate
        implements View.OnSystemUiVisibilityChangeListener, ScreenOrientationDelegate {
    private static final String TAG = "VrShellDelegate";

    // Pseudo-random number to avoid request id collisions. Result codes must fit in lower 16 bits
    // when used with startActivityForResult...
    /* package */ static final int EXIT_VR_RESULT = 7212;
    private static final int GVR_KEYBOARD_UPDATE_RESULT = 7214;

    @IntDef({EnterVRResult.NOT_NECESSARY, EnterVRResult.CANCELLED, EnterVRResult.REQUESTED,
            EnterVRResult.SUCCEEDED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnterVRResult {
        int NOT_NECESSARY = 0;
        int CANCELLED = 1;
        int REQUESTED = 2;
        int SUCCEEDED = 3;
    }

    private static final String VR_ENTRY_RESULT_ACTION =
            "org.chromium.chrome.browser.vr.VrEntryResult";

    private static final long REENTER_VR_TIMEOUT_MS = 1000;

    private static final String FEEDBACK_REPORT_TYPE = "USER_INITIATED_FEEDBACK_REPORT_VR";

    private static final String GVR_KEYBOARD_PACKAGE_ID = "com.google.android.vr.inputmethod";
    private static final String GVR_KEYBOARD_MARKET_URI =
            "market://details?id=" + GVR_KEYBOARD_PACKAGE_ID;

    // This value is intentionally probably overkill. This is the time we need to wait from when
    // Chrome is resumed, to when Chrome actually renders a black frame, so that we can cancel the
    // stay_hidden animation and not see a white monoscopic frame in-headset. 150ms is definitely
    // too short, 250ms is sometimes too short for debug builds. 500ms should hopefully be safe even
    // under fairly exceptional conditions, and won't delay entering VR a noticeable amount given
    // how slow it already is.
    private static final int WINDOW_FADE_ANIMATION_DURATION_MS = 500;

    /** ID for SavedInstanceState Bundle for whether Chrome was in VR when killed. */
    private static final String IN_VR = "in_vr";

    private static VrShellDelegate sInstance;
    private static VrBroadcastReceiver sVrBroadcastReceiver;
    private static VrLifecycleObserver sVrLifecycleObserver;
    private static VrDaydreamApi sVrDaydreamApi;
    private static Set<Activity> sVrModeEnabledActivitys = new HashSet<>();
    private static boolean sRegisteredDaydreamHook;
    private static boolean sRegisteredVrAssetsComponent;
    private static boolean sTestVrShellDelegateOnStartup;
    private static ObservableSupplierImpl<Boolean> sVrModeEnabledSupplier;

    private ChromeActivity mActivity;

    private int mCachedGvrKeyboardPackageVersion;

    // How often to prompt the user to enter VR feedback.
    private int mFeedbackFrequency;

    private VrShell mVrShell;
    private Boolean mIsDaydreamCurrentViewer;
    private boolean mProbablyInDon;
    private boolean mInVr;
    private boolean mNeedsAnimationCancel;
    private boolean mCancellingEntryAnimation;

    // Whether or not the VR Device ON flow succeeded. If this is true it means the user has a VR
    // headset on, but we haven't switched into VR mode yet.
    // See further documentation here: https://developers.google.com/vr/daydream/guides/vr-entry
    private boolean mDonSucceeded;
    private boolean mShowingDaydreamDoff;
    private boolean mShowingExitVrPrompt;
    private boolean mDoffOptional;
    // Listener to be called once we exited VR due to to an unsupported mode, e.g. the user clicked
    // the URL bar security icon.
    private OnExitVrRequestListener mOnExitVrRequestListener;
    private Runnable mPendingExitVrRequest;
    private Boolean mShowVrServicesUpdatePrompt;
    private boolean mShowingDoffForGvrUpdate;
    private boolean mExitedDueToUnsupportedMode;
    private boolean mPaused;
    private boolean mVisible;
    private boolean mRestoreSystemUiVisibility;
    private Integer mRestoreOrientation;
    private boolean mRequestedWebVr;
    private boolean mStartedFromVrIntent;

    private boolean mInternalIntentUsedToStartVr;

    // Set to true if performed VR browsing at least once. That is, this was not simply a WebVr
    // presentation experience.
    private boolean mVrBrowserUsed;

    private int mExpectedDensityChange;

    // Gets run when the user exits VR mode by clicking the 'x' button or system UI back button.
    private Runnable mCloseButtonListener;

    // Gets run when the user exits VR mode by clicking the Gear button.
    private Runnable mSettingsButtonListener;

    @VisibleForTesting
    protected boolean mTestWorkaroundDontCancelVrEntryOnResume;

    private long mNativeVrShellDelegate;

    /* package */ static final class VrUnsupportedException extends RuntimeException {}

    private static final class VrLifecycleObserver
            implements ApplicationStatus.ActivityStateListener {
        @Override
        public void onActivityStateChange(Activity activity, int newState) {
            switch (newState) {
                case ActivityState.DESTROYED:
                    if (sVrBroadcastReceiver != null
                            && sVrBroadcastReceiver.targetActivity().get() == activity) {
                        sVrBroadcastReceiver.unregister();
                        sVrBroadcastReceiver = null;
                    }
                    sVrModeEnabledActivitys.remove(activity);
                    break;
                default:
                    break;
            }
            if (sInstance != null) sInstance.onActivityStateChange(activity, newState);
        }
    }

    private static final class VrBroadcastReceiver extends BroadcastReceiver {
        private final WeakReference<ChromeActivity> mTargetActivity;

        public VrBroadcastReceiver(ChromeActivity activity) {
            ensureLifecycleObserverInitialized();
            mTargetActivity = new WeakReference<ChromeActivity>(activity);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (!IntentUtils.isTrustedIntentFromSelf(intent)) return;
            ChromeActivity activity = mTargetActivity.get();
            if (activity == null) return;
            getInstance(activity);
            assert sInstance != null;
            if (sInstance == null) return;
            sInstance.onBroadcastReceived();

            // Note that even though we are definitely entering VR here, we don't want to set
            // the window mode yet, as setting the window mode while we're in the background can
            // racily lead to that window mode change essentially being ignored, with future
            // attempts to set the same window mode also being ignored.

            sInstance.mDonSucceeded = true;
            sInstance.mProbablyInDon = false;
            setVrModeEnabled(sInstance.mActivity, true);
            if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "VrBroadcastReceiver onReceive");

            // We add a black overlay view so that we can show black while the VR UI is loading.
            if (!sInstance.mInVr) {
                VrModuleProvider.getDelegate().addBlackOverlayViewForActivity(sInstance.mActivity);
            }

            if (sInstance.mPaused) {
                if (activity instanceof ChromeTabbedActivity) {
                    // We can special case singleInstance activities like CTA to avoid having to use
                    // moveTaskToFront. Using moveTaskToFront prevents us from disabling window
                    // animations, and causes the system UI to show up during the preview window and
                    // window animations.
                    Intent launchIntent = new Intent(activity, activity.getClass());
                    launchIntent = VrModuleProvider.getIntentDelegate().setupVrIntent(launchIntent);
                    sInstance.mInternalIntentUsedToStartVr = true;
                    sInstance.setExpectingIntent(true);
                    getVrDaydreamApi().launchInVr(
                            PendingIntent.getActivity(activity, 0, launchIntent,
                                    PendingIntent.FLAG_UPDATE_CURRENT
                                            | IntentUtils.getPendingIntentMutabilityFlag(false)));
                } else {
                    // We start the Activity with a custom animation that keeps it hidden while
                    // starting up to avoid Android showing stale 2D screenshots when the user is in
                    // their VR headset. The animation lasts up to 10 seconds, but is cancelled when
                    // we're resumed as at that time we'll be showing the black overlay added above.
                    int animation = !sInstance.mInVr && VrDelegate.USE_HIDE_ANIMATION
                            ? R.anim.stay_hidden
                            : 0;
                    sInstance.mNeedsAnimationCancel = animation != 0;
                    Bundle options =
                            ActivityOptions.makeCustomAnimation(activity, animation, 0).toBundle();
                    ((ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE))
                            .moveTaskToFront(activity.getTaskId(), 0, options);
                }
            } else {
                // If a WebVR app calls requestPresent in response to the displayactivate event
                // after the DON flow completes, the DON flow is skipped, meaning our app won't be
                // paused when daydream fires our BroadcastReceiver, so onResume won't be called.
                sInstance.handleDonFlowSuccess();
            }
        }

        /**
         * Unregisters this {@link BroadcastReceiver} from the activity it's registered to.
         */
        public void unregister() {
            ChromeActivity activity = mTargetActivity.get();
            if (activity == null) return;
            try {
                activity.unregisterReceiver(VrBroadcastReceiver.this);
            } catch (IllegalArgumentException e) {
                // Ignore this. This means our receiver was already unregistered somehow.
            }
        }

        WeakReference<ChromeActivity> targetActivity() {
            return mTargetActivity;
        }
    }

    /**
     * Immediately exits VR. If the user is in headset, they will see monoscopic UI while in the
     * headset, so use with caution.
     */
    public static void forceExitVrImmediately() {
        if (sInstance == null) return;
        sInstance.shutdownVr(true, true);
    }

    /**
     * See {@link Activity#onActivityResult}.
     */
    public static boolean onActivityResultWithNative(int requestCode, int resultCode) {
        // Handles the result of the exit VR flow (DOFF).
        if (requestCode == EXIT_VR_RESULT) {
            if (sInstance != null) sInstance.onExitVrResult(resultCode == Activity.RESULT_OK);
            return true;
        }
        // Handles the result of requesting to update GVR Keyboard.
        if (requestCode == GVR_KEYBOARD_UPDATE_RESULT) {
            if (sInstance != null) sInstance.onGvrKeyboardMaybeUpdated();
            return true;
        }
        return false;
    }

    /**
     * Called when the native library is first available.
     */
    public static void onNativeLibraryAvailable() {
        VrModule.ensureNativeLoaded();
        VrModuleProvider.registerJni();
        VrShellDelegateJni.get().onLibraryAvailable();
    }

    /**
     * Whether or not we are currently in VR.
     */
    public static boolean isInVr() {
        if (sInstance == null) return false;
        return sInstance.mInVr;
    }

    /**
     * @return Whether 2D intents can safely be launched without showing non-VR UI to users in VR
     *         headsets.
     */
    public static boolean canLaunch2DIntents() {
        if (!isInVr()) return true;
        return sInstance.canLaunch2DIntentsInternal();
    }

    /**
     * See {@link ChromeActivity#handleBackPressed}
     * Only handles the back press while in VR.
     */
    public static boolean onBackPressed() {
        if (sInstance == null) return false;
        return sInstance.onBackPressedInternal();
    }

    /**
     * Enters VR on the current tab if possible.
     *
     * @return Whether VR entry succeeded (or is in progress).
     */
    public static boolean enterVrIfNecessary() {
        boolean created_delegate = sInstance == null;
        VrShellDelegate instance = getInstance();
        if (instance == null) return false;
        int result = instance.enterVrInternal();
        if (result == EnterVRResult.CANCELLED && created_delegate) instance.destroy();
        return result != EnterVRResult.CANCELLED;
    }

    /**
     * If VR Shell is enabled, and the activity is supported, register with the Daydream
     * platform that this app would like to be launched in VR when the device enters VR.
     */
    public static void maybeRegisterVrEntryHook(final Activity activity) {
        // Daydream is not supported on pre-N devices.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return;
        if (sInstance != null) return; // Will be handled in onResume.
        if (!VrModuleProvider.getDelegate().activitySupportsVrBrowsing(activity)
                && sRegisteredVrAssetsComponent) {
            return;
        }

        // Short-circuit the asnyc task if we've already queried support level previously. Creating
        // the async task takes ~1ms on my Android Go device.
        Integer vrSupportLevel = VrCoreInstallUtils.getCachedVrSupportLevel();
        if (vrSupportLevel != null && vrSupportLevel != VrSupportLevel.VR_DAYDREAM) return;

        try {
            // Reading VR support level and version can be slow, so do it asynchronously.
            new AsyncTask<Integer>() {
                @Override
                protected Integer doInBackground() {
                    return VrCoreInstallUtils.getVrSupportLevel();
                }

                @Override
                protected void onPostExecute(Integer vrSupportLevel) {
                    if (vrSupportLevel != VrSupportLevel.VR_DAYDREAM) return;

                    if (!sRegisteredVrAssetsComponent) {
                        registerVrAssetsComponentIfDaydreamUser(isDaydreamCurrentViewer());
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } catch (RejectedExecutionException ex) {
            // This isn't critical work, so it's okay to fail silently. If the user does try to
            // enter VR the asset component may not be available, and headset insertion will go to
            // Daydream rather than Chrome.
        }
    }

    /**
     * When the app is pausing we need to unregister with the Daydream platform to prevent this app
     * from being launched from the background when the device enters VR.
     */
    public static void maybeUnregisterVrEntryHook() {
    }

    public static void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        if (isInMultiWindowMode && isInVr()) {
            sInstance.shutdownVr(true /* disableVrMode */, true /* stayingInChrome */);
        }
    }

    public static void requestToExitVrForSearchEnginePromoDialog(
            OnExitVrRequestListener listener, Activity activity) {
        // When call site requests to exit VR, depend on the timing, Chrome may not in VR yet
        // (Chrome only enter VR after onNewIntentWithNative is called in the cold start case).
        // While not in VR, calling requestToExitVr would immediately notify listener that exit VR
        // succeed (without showing DOFF screen). If call site decide to show 2D UI when exit VR
        // succeeded, it leads to case that 2D UI is showing on top of VR when Chrome eventually
        // enters VR. To prevent this from happening, we set mPendingExitVrRequest which should be
        // executed at runPendingExitVrTask. runPendingExitVrTask is called after it is safe to
        // request exit VR.
        if (isInVr()) {
            sInstance.requestToExitVrInternal(
                    listener, UiUnsupportedMode.SEARCH_ENGINE_PROMO, false);
        } else {
            // Making sure that we response to this request as it is very important that search
            // engine promo dialog isn't ignored due to VR.
            assert VrModuleProvider.getIntentDelegate().isVrIntent(activity.getIntent());
            VrShellDelegate instance = getInstance();
            if (instance == null) {
                listener.onDenied();
                return;
            }
            sInstance.mPendingExitVrRequest = () -> {
                VrShellDelegate.requestToExitVr(listener, UiUnsupportedMode.SEARCH_ENGINE_PROMO);
            };
        }
    }

    public static void requestToExitVr(OnExitVrRequestListener listener) {
        requestToExitVr(listener, UiUnsupportedMode.GENERIC_UNSUPPORTED_FEATURE);
    }

    public static void requestToExitVr(
            OnExitVrRequestListener listener, @UiUnsupportedMode int reason) {
        // If we're not in VR, just say that we've successfully exited VR.
        if (sInstance == null || !sInstance.mInVr) {
            listener.onSucceeded();
            return;
        }
        sInstance.requestToExitVrInternal(listener, reason, !supports2dInVr());
    }

    public static void requestToExitVrAndRunOnSuccess(Runnable onSuccess) {
        requestToExitVrAndRunOnSuccess(onSuccess, UiUnsupportedMode.GENERIC_UNSUPPORTED_FEATURE);
    }

    public static void requestToExitVrAndRunOnSuccess(
            Runnable onSuccess, @UiUnsupportedMode int reason) {
        requestToExitVr(new OnExitVrRequestListener() {
            @Override
            public void onSucceeded() {
                onSuccess.run();
            }

            @Override
            public void onDenied() {}
        }, reason);
    }

    /**
     * Called when the {@link Activity} becomes visible.
     */
    public static void onActivityShown(Activity activity) {
        if (sInstance != null && sInstance.mActivity == activity) sInstance.onActivityShown();
    }

    /**
     * Called when the {@link Activity} is hidden.
     */
    public static void onActivityHidden(Activity activity) {
        if (sInstance != null && sInstance.mActivity == activity) sInstance.onActivityHidden();
    }

    /**
     * @return Whether VrShellDelegate handled the density change. If the density change is
     * unhandled, the Activity should be recreated in order to handle the change.
     */
    public static boolean onDensityChanged(int oldDpi, int newDpi) {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "onDensityChanged [%d]->[%d] ", oldDpi, newDpi);
        if (sInstance == null) return false;
        // If density changed while in VR, we expect a second density change to restore the density
        // to what it previously was when we exit VR. We shouldn't have to recreate the activity as
        // all non-VR UI is still using the old density.
        if (sInstance.mExpectedDensityChange != 0) {
            assert !sInstance.mInVr && !sInstance.mDonSucceeded;
            int expectedDensity = sInstance.mExpectedDensityChange;
            sInstance.mExpectedDensityChange = 0;
            return (newDpi == expectedDensity);
        }
        if (sInstance.mInVr || sInstance.mDonSucceeded) {
            sInstance.mExpectedDensityChange = oldDpi;
            return true;
        }
        return false;
    }

    /**
     * @param topContentOffset The top content offset (usually applied by the omnibox).
     */
    public static void rawTopContentOffsetChanged(float topContentOffset) {
        assert isInVr();
        sInstance.mVrShell.rawTopContentOffsetChanged(topContentOffset);
    }

    /**
     * This is called every time ChromeActivity gets a new intent.
     */
    public static void onNewIntentWithNative(Activity activity, Intent intent) {
        if (activity.isFinishing()) return;
        if (!VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(activity, intent)) return;

        VrShellDelegate instance = getInstance((ChromeActivity) activity);
        if (instance == null) return;
        instance.onNewVrIntent();
    }

    /**
     * This is called when ChromeTabbedActivity gets a new intent before native is initialized.
     */
    public static void maybeHandleVrIntentPreNative(Activity activity, Intent intent) {
        boolean launchingIntoVr =
                VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(activity, intent);

        if (!launchingIntoVr) {
            // We trust that if an intent is targeted for 2D, that Chrome should switch to 2D
            // regardless of whether the user is in headset.
            if (VrShellDelegate.isInVr()) VrShellDelegate.forceExitVrImmediately();
            return;
        }

        if (VrModuleProvider.getDelegate().bootsToVr() && launchingIntoVr) {
            if (VrModuleProvider.getDelegate().relaunchOnMainDisplayIfNecessary(activity, intent)) {
                return;
            }
        }

        if (sInstance != null && !sInstance.mInternalIntentUsedToStartVr) {
            sInstance.swapHostActivity((ChromeActivity) activity, false /* disableVrMode */);
            // If the user has launched Chrome from the launcher, rather than resuming from the
            // dashboard, we don't want to launch into presentation.
            sInstance.exitWebVRAndClearState();
        }

        if (sInstance != null) sInstance.setExpectingIntent(false);

        if (VrDelegate.DEBUG_LOGS) {
            Log.i(TAG, "maybeHandleVrIntentPreNative: preparing for transition");
        }

        // We add a black overlay view so that we can show black while the VR UI is loading.
        // Note that this alone isn't sufficient to prevent 2D UI from showing when
        // auto-presenting WebVR. See comment about the custom animation in {@link
        // getVrIntentOptions}.
        // TODO(crbug.com/775574): This hack doesn't really work to hide the 2D UI on Samsung
        // devices since Chrome gets paused and we prematurely remove the overlay.
        if (sInstance == null || !sInstance.mInVr) {
            VrModuleProvider.getDelegate().addBlackOverlayViewForActivity(activity);
        }

        // Enable VR mode and hide system UI. We do this here so we don't get kicked out of
        // VR mode and to prevent seeing a flash of system UI.
        setVrModeEnabled(activity, true);
        VrModuleProvider.getDelegate().setSystemUiVisibilityForVr(activity);
    }

    /**
     * Asynchronously enable VR mode.
     */
    public static void setVrModeEnabled(Activity activity, boolean enabled) {
        ensureLifecycleObserverInitialized();
        if (sVrModeEnabledSupplier == null) sVrModeEnabledSupplier = new ObservableSupplierImpl<>();
        if (enabled) {
            if (sVrModeEnabledActivitys.contains(activity)) return;
            AndroidCompat.setVrModeEnabled(activity, true);
            sVrModeEnabledActivitys.add(activity);
            sVrModeEnabledSupplier.set(true);
        } else {
            if (!sVrModeEnabledActivitys.contains(activity)) return;
            AndroidCompat.setVrModeEnabled(activity, false);
            sVrModeEnabledActivitys.remove(activity);
            sVrModeEnabledSupplier.set(false);
        }
    }

    /**
     * Performs pre-inflation VR-related startup.
     */
    public static void doPreInflationStartup(Activity activity, Bundle savedInstanceState) {
        // We need to explicitly enable VR mode here so that the system doesn't kick us out of VR,
        // or drop us into the 2D-in-VR rendering mode, while we prepare for VR rendering.
        if (VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(
                    activity, activity.getIntent())) {
            setVrModeEnabled(activity, true);
        } else if (savedInstanceState != null && savedInstanceState.getBoolean(IN_VR, false)) {
            // When Chrome is restored from a SavedInstanceState with VR mode still on we need to
            // Explicitly turn VR mode off even though we can't really know for sure whether or not
            // it's currently on.
            AndroidCompat.setVrModeEnabled(activity, false);
            sVrModeEnabledActivitys.remove(activity);
        }
    }

    /**
     * See {@link Activity#onSaveInstanceState(Bundle)}
     */
    public static void onSaveInstanceState(Bundle outState) {
        if (isInVr()) outState.putBoolean(IN_VR, true);
    }

    public static void initAfterModuleInstall() {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        onNativeLibraryAvailable();
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof ChromeActivity
                && ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED) {
            maybeRegisterVrEntryHook((ChromeActivity) activity);
        }
    }

    /**
     * @return A Daydream Api instance, for interacting with Daydream platform features.
     */
    public static VrDaydreamApi getVrDaydreamApi() {
        if (sVrDaydreamApi == null) sVrDaydreamApi = new VrDaydreamApi();
        return sVrDaydreamApi;
    }

    public static boolean isDaydreamCurrentViewer() {
        if (sInstance != null) return sInstance.isDaydreamCurrentViewerInternal();
        return getVrDaydreamApi().isDaydreamCurrentViewer();
    }

    public static boolean supports2dInVr() {
        Context context = ContextUtils.getApplicationContext();
        return VrCoreInstallUtils.isDaydreamReadyDevice() && DaydreamApi.supports2dInVr(context);
    }

    protected static void enableTestVrShellDelegateOnStartupForTesting() {
        sTestVrShellDelegateOnStartup = true;
    }

    /* package */ static boolean isVrModeEnabled(Activity activity) {
        return sVrModeEnabledActivitys.contains(activity);
    }

    /* package */ static boolean expectedDensityChange() {
        return sInstance != null && sInstance.mExpectedDensityChange != 0;
    }

    private static boolean activitySupportsPresentation(Activity activity) {
        return activity instanceof ChromeTabbedActivity || activity instanceof CustomTabActivity
                || activity instanceof WebappActivity;
    }

    private static boolean activitySupportsExitFeedback(Activity activity) {
        return activity instanceof ChromeTabbedActivity
                && ChromeFeatureList.isEnabled(ChromeFeatureList.VR_BROWSING_FEEDBACK);
    }

    private static void registerVrAssetsComponentIfDaydreamUser(boolean isDaydreamCurrentViewer) {
        assert !sRegisteredVrAssetsComponent;
        if (isDaydreamCurrentViewer) {
            VrShellDelegateJni.get().registerVrAssetsComponent();
            sRegisteredVrAssetsComponent = true;
        }
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.VR_SHOULD_REGISTER_ASSETS_COMPONENT_ON_STARTUP,
                isDaydreamCurrentViewer);
    }

    // We need a custom Intent for entering VR in order to support VR in Custom Tabs. Custom Tabs
    // are not a singleInstance activity, so they cannot be resumed through Activity PendingIntents,
    // which is the typical way Daydream resumes your Activity. Instead, we use a broadcast intent
    // and then use the broadcast to bring ourselves back to the foreground.
    /* package */ static PendingIntent getEnterVrPendingIntent(Activity activity) {
        if (sVrBroadcastReceiver != null) sVrBroadcastReceiver.unregister();
        IntentFilter filter = new IntentFilter(VR_ENTRY_RESULT_ACTION);
        VrBroadcastReceiver receiver = new VrBroadcastReceiver((ChromeActivity) activity);
        // If we set sVrBroadcastReceiver then use it in registerReceiver, findBugs considers this
        // a thread-safety issue since it thinks the receiver isn't fully initialized before being
        // exposed to other threads. This isn't actually an issue in this case, but we need to set
        // sVrBroadcastReceiver after we're done using it here to fix the compile error.
        ContextUtils.registerNonExportedBroadcastReceiver(activity, receiver, filter);
        sVrBroadcastReceiver = receiver;
        Intent vrIntent = new Intent(VR_ENTRY_RESULT_ACTION);
        vrIntent.setPackage(activity.getPackageName());
        IntentUtils.addTrustedIntentExtras(vrIntent);
        return PendingIntent.getBroadcast(activity, 0, vrIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    private static boolean isVrBrowsingSupported(ChromeActivity activity) {
        return false;
    }

    /**
     * @return Whether or not VR Browsing is currently enabled for the given Activity.
     */
    /* package */ static boolean isVrBrowsingEnabled(ChromeActivity activity, int vrSupportLevel) {
        return isVrBrowsingSupported(activity) && vrSupportLevel == VrSupportLevel.VR_DAYDREAM;
    }

    /* package */ static boolean isInVrSession() {
        Context context = ContextUtils.getApplicationContext();
        // The call to isInVrSession crashes when called on a non-Daydream ready device, so we add
        // the device check (b/77268533).
        try {
            return VrCoreInstallUtils.isDaydreamReadyDevice() && DaydreamApi.isInVrSession(context);
        } catch (Exception ex) {
            Log.e(TAG, "Unable to check if in VR session", ex);
            return false;
        }
    }

    private static void startFeedback(Tab tab) {
        // TODO(ymalik): This call will connect to the Google Services api which can be slow. Can we
        // connect to it beforehand when we know that we'll be prompting for feedback?
        HelpAndFeedbackLauncherImpl.getInstance().showFeedback(TabUtils.getActivity(tab),
                Profile.fromWebContents(tab.getWebContents()), tab.getUrl().getSpec(),
                ContextUtils.getApplicationContext().getPackageName() + "." + FEEDBACK_REPORT_TYPE);
    }

    private static void promptForFeedback(final Tab tab) {
        if (tab == null) return;
        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {}

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                if (isPrimary) {
                    startFeedback(tab);
                } else {
                    VrFeedbackStatus.setFeedbackOptOut(true);
                }
                return false;
            }

            @Override
            public boolean onInfoBarLinkClicked() {
                return false;
            }
        };

        SimpleConfirmInfoBarBuilder.create(tab.getWebContents(), listener,
                InfoBarIdentifier.VR_FEEDBACK_INFOBAR_ANDROID, tab.getContext(),
                org.chromium.chrome.vr.R.drawable.vr_services,
                ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_shell_feedback_infobar_description),
                ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_shell_feedback_infobar_feedback_button),
                tab.getContext().getString(R.string.no_thanks), null /* linkText */,
                true /* autoExpire  */);
    }

    private static void ensureLifecycleObserverInitialized() {
        if (sVrLifecycleObserver != null) return;
        sVrLifecycleObserver = new VrLifecycleObserver();
        ApplicationStatus.registerStateListenerForAllActivities(sVrLifecycleObserver);
    }

    @CalledByNative
    private static VrShellDelegate getInstance() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) return null;
        return getInstance((ChromeActivity) activity);
    }

    @SuppressWarnings("unchecked")
    private static VrShellDelegate getInstance(ChromeActivity activity) {
        if (!LibraryLoader.getInstance().isInitialized()) return null;
        if (activity == null || !activitySupportsPresentation(activity)) return null;
        if (sInstance != null) return sInstance;
        ThreadUtils.assertOnUiThread();
        if (sTestVrShellDelegateOnStartup) {
            try {
                // This should only ever be run during tests on standalone devices. Normally, we
                // create a TestVrShellDelegate during pre-test setup after Chrome has started.
                // However, since Chrome is started in VR on standalones, creating a
                // TestVrShellDelegate after startup discards the existing VrShellDelegate instance
                // that's in use, which is bad. So, in those cases, create a TestVrShellDelegate
                // instead of the production version.
                Class clazz = Class.forName("org.chromium.chrome.browser.vr.TestVrShellDelegate");
                Method method = clazz.getMethod("createTestVrShellDelegate", ChromeActivity.class);
                method.invoke(null, activity);
            } catch (Exception e) {
                assert false;
            }
        } else {
            sInstance = new VrShellDelegate(activity);
        }
        return sInstance;
    }

    protected VrShellDelegate(ChromeActivity activity) {
        mActivity = activity;
        // If an activity isn't resumed at the point, it must have been paused.
        mPaused = ApplicationStatus.getStateForActivity(activity) != ActivityState.RESUMED;
        mVisible = activity.hasWindowFocus();
        mNativeVrShellDelegate = VrShellDelegateJni.get().init(VrShellDelegate.this);
        mFeedbackFrequency = VrFeedbackStatus.getFeedbackFrequency();
        ensureLifecycleObserverInitialized();
        if (!mPaused) onResume();

        sInstance = this;
    }

    public void onActivityStateChange(Activity activity, int newState) {
        switch (newState) {
            case ActivityState.DESTROYED:
                if (activity == mActivity) destroy();
                break;
            case ActivityState.PAUSED:
                if (activity == mActivity) onPause();
                // Other activities should only pause while we're paused due to Android lifecycle.
                assert mPaused;
                break;
            case ActivityState.STOPPED:
                if (activity == mActivity) onStop();
                break;
            case ActivityState.STARTED:
                if (activity == mActivity) onStart();
                break;
            case ActivityState.RESUMED:
                if (!activitySupportsPresentation(activity)) return;
                if (!(activity instanceof ChromeActivity)) return;
                swapHostActivity((ChromeActivity) activity, true /* disableVrMode */);
                onResume();
                break;
            default:
                break;
        }
    }

    // Called when an activity that supports VR is resumed, and attaches VrShellDelegate to that
    // activity.
    private void swapHostActivity(ChromeActivity activity, boolean disableVrMode) {
        assert mActivity != null;
        if (mActivity == activity) return;
        if (mInVr) shutdownVr(disableVrMode, false /* stayingInChrome */);
        mActivity = activity;
    }

    private int getGvrKeyboardPackageVersion() {
        return PackageUtils.getPackageVersion(GVR_KEYBOARD_PACKAGE_ID);
    }

    protected boolean isVrBrowsingEnabled() {
        return isVrBrowsingEnabled(mActivity, VrCoreInstallUtils.getVrSupportLevel());
    }

    private void onGvrKeyboardMaybeUpdated() {
        if (mCachedGvrKeyboardPackageVersion == getGvrKeyboardPackageVersion()) return;
        ApplicationLifetime.terminate(true);
    }

    private void maybeSetPresentResult(boolean result) {
        if (mNativeVrShellDelegate == 0 || !mRequestedWebVr) return;
        VrShellDelegateJni.get().setPresentResult(
                mNativeVrShellDelegate, VrShellDelegate.this, result);
        mRequestedWebVr = false;
    }

    /**
     * Handle a successful VR DON flow, entering VR in the process unless we're unable to.
     * @return False if VR entry failed.
     */
    private boolean enterVrAfterDon() {
        if (mNativeVrShellDelegate == 0) return false;
        if (!canEnterVr()) return false;

        enterVr();

        // The user has successfully completed a DON flow.
        RecordUserAction.record("VR.DON");

        return true;
    }

    private void enterVr() {
        // We should only enter VR when we're the resumed Activity or our changes to things like
        // system UI flags might get lost.
        assert !mPaused;
        assert mNativeVrShellDelegate != 0;
        if (mInVr) return;
        mInVr = true;
        setVrModeEnabled(mActivity, true);

        setWindowModeForVr();

        // We assume that we triggered the DON flow already for Daydream viewers. If that changes,
        // we need to make sure not to report success/fail to WebXR until after the DON flow runs.
        assert mDonSucceeded || !isDaydreamCurrentViewerInternal();

        mDonSucceeded = false;
        if (!createVrShell()) {
            cancelPendingVrEntry();
            mInVr = false;
            getVrDaydreamApi().launchVrHomescreen();
            return;
        }
        mExitedDueToUnsupportedMode = false;

        addVrViews();
        // Make sure that assets component is registered when creating native VR shell.
        if (!sRegisteredVrAssetsComponent) {
            registerVrAssetsComponentIfDaydreamUser(isDaydreamCurrentViewer());
        }
        mVrShell.initializeNative(mRequestedWebVr, VrModuleProvider.getDelegate().bootsToVr());
        mVrShell.setWebVrModeEnabled(mRequestedWebVr);

        // We're entering VR, but not in WebVr mode.
        mVrBrowserUsed = !mRequestedWebVr;

        // resume needs to be called on GvrLayout after initialization to make sure DON flow works
        // properly.
        if (mVisible) mVrShell.resume();
        mVrShell.getContainer().setOnSystemUiVisibilityChangeListener(this);

        maybeSetPresentResult(true);

        VrModuleProvider.onEnterVr();
    }

    private void onVrIntentUnsupported() {
        // If entering VR is unsupported for some reason, clean up what we did in
        // maybeHandleVrIntentPreNative.
        assert !mInVr;
        mStartedFromVrIntent = false;
        cancelPendingVrEntry();

        // Some Samsung devices change the screen density after exiting VR mode which causes
        // us to restart Chrome with the VR intent that originally started it. We don't want to
        // enable VR mode when the user opens Chrome again in 2D mode, so we remove VR specific
        // extras.
        VrModuleProvider.getIntentDelegate().removeVrExtras(mActivity.getIntent());

        // We may still be showing the STAY_HIDDEN animation, so cancel it if necessary.
        cancelStartupAnimationIfNeeded();
    }

    private void onNewVrIntent() {
        // We set the the system UI in maybeHandleVrIntentPreNative, so make sure we restore it when
        // we exit VR, or cancel VR entry.
        mRestoreSystemUiVisibility = true;

        // Nothing to do if we were launched by an internal intent.
        if (mInternalIntentUsedToStartVr) {
            mInternalIntentUsedToStartVr = false;

            // TODO(mthiesse): This shouldn't be necessary. This is another instance of b/65681875,
            // where the intent is received after we're resumed.
            if (mInVr) return;

            // This is extremely unlikely in practice. Some code must have called shutdownVR() while
            // we were entering VR through NFC insertion.
            if (!mDonSucceeded) cancelPendingVrEntry();
            return;
        }

        if (VrDelegate.USE_HIDE_ANIMATION) mNeedsAnimationCancel = true;

        if (!isVrBrowsingSupported(mActivity)) {
            onVrIntentUnsupported();
            return;
        }

        mStartedFromVrIntent = true;
        // Setting DON succeeded will cause us to enter VR when resuming.
        mDonSucceeded = true;

        if (!mPaused) {
            // Note that canceling the animation below is what causes us to enter VR mode. We start
            // an intermediate activity to cancel the animation which causes onPause and onResume to
            // be called and we enter VR mode in onResume (because we set the mEnterVrOnStartup bit
            // above). If Chrome is already running, onResume which will be called after
            // VrShellDelegate#onNewIntentWithNative which will cancel the animation and enter VR
            // after that.
            if (!cancelStartupAnimationIfNeeded()) {
                // If we didn't cancel the startup animation, we won't be getting another onResume
                // call, so enter VR here.
                handleDonFlowSuccess();
                runPendingExitVrTask();
            }
        }
    }

    private void runPendingExitVrTask() {
        if (mPendingExitVrRequest == null) return;
        new Handler().post(mPendingExitVrRequest);
        mPendingExitVrRequest = null;
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        if (mInVr && !isWindowModeCorrectForVr()) {
            setWindowModeForVr();
        }
    }

    @Override
    public boolean canUnlockOrientation(Activity activity, int defaultOrientation) {
        if (mActivity == activity && mRestoreOrientation != null) {
            mRestoreOrientation = defaultOrientation;
            return false;
        }
        return true;
    }

    @Override
    public boolean canLockOrientation() {
        return false;
    }

    public boolean hasRecordAudioPermission() {
        return mActivity.getWindowAndroid().hasPermission(android.Manifest.permission.RECORD_AUDIO);
    }

    public boolean canRequestRecordAudioPermission() {
        return mActivity.getWindowAndroid().canRequestPermission(
                android.Manifest.permission.RECORD_AUDIO);
    }

    public static ObservableSupplier<Boolean> getVrModeEnabledSupplier() {
        if (sVrModeEnabledSupplier == null) sVrModeEnabledSupplier = new ObservableSupplierImpl<>();
        return sVrModeEnabledSupplier;
    }

    private boolean isWindowModeCorrectForVr() {
        int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
        int orientation = mActivity.getResources().getConfiguration().orientation;
        // Mask the flags to only those that we care about.
        return (flags & VrDelegate.VR_SYSTEM_UI_FLAGS) == VrDelegate.VR_SYSTEM_UI_FLAGS
                && orientation == Configuration.ORIENTATION_LANDSCAPE;
    }

    private void setWindowModeForVr() {
        // Decouple the compositor size from the view size, or we'll get an unnecessary resize due
        // to the orientation change when entering VR, then another resize once VR has settled on
        // the content size.
        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                mActivity.getCompositorViewHolderSupplier();
        if (compositorViewHolderSupplier.hasValue()) {
            compositorViewHolderSupplier.get().onEnterVr();
        }
        ScreenOrientationProvider.getInstance().setOrientationDelegate(this);

        // Hide system UI.
        VrModuleProvider.getDelegate().setSystemUiVisibilityForVr(mActivity);

        // Set correct orientation.
        if (mRestoreOrientation == null) {
            mRestoreOrientation = mActivity.getRequestedOrientation();
        }

        mRestoreSystemUiVisibility = true;

        mActivity.getWindow().getAttributes().rotationAnimation =
                WindowManager.LayoutParams.ROTATION_ANIMATION_JUMPCUT;
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    private void restoreWindowMode() {
        ScreenOrientationProvider.getInstance().setOrientationDelegate(null);
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Restore orientation.
        if (mRestoreOrientation != null) mActivity.setRequestedOrientation(mRestoreOrientation);
        mRestoreOrientation = null;

        // Restore system UI visibility.
        if (mRestoreSystemUiVisibility) {
            int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
            mActivity.getWindow().getDecorView().setSystemUiVisibility(
                    flags & ~VrDelegate.VR_SYSTEM_UI_FLAGS);
        }
        mRestoreSystemUiVisibility = false;
        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                mActivity.getCompositorViewHolderSupplier();
        if (compositorViewHolderSupplier.hasValue()) {
            compositorViewHolderSupplier.get().onExitVr();
        }

        mActivity.getWindow().getAttributes().rotationAnimation =
                WindowManager.LayoutParams.ROTATION_ANIMATION_ROTATE;
    }

    /* package */ boolean canEnterVr() {
        if (VrCoreInstallUtils.vrSupportNeedsUpdate()) return false;

        // If VR browsing is not enabled and this is not a WebXR request, then return false.
        if (!isVrBrowsingEnabled() && !mRequestedWebVr) return false;
        return true;
    }

    @CalledByNative
    private void presentRequested() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "WebVR page requested presentation");
        mRequestedWebVr = true;
        if (VrModuleProvider.getDelegate().bootsToVr() && !mInVr) {
            maybeSetPresentResult(false);
            return;
        }
        switch (enterVrInternal()) {
            case EnterVRResult.NOT_NECESSARY:
                mVrShell.setWebVrModeEnabled(true);
                maybeSetPresentResult(true);
                break;
            case EnterVRResult.CANCELLED:
                maybeSetPresentResult(false);
                break;
            case EnterVRResult.REQUESTED:
                break;
            case EnterVRResult.SUCCEEDED:
                maybeSetPresentResult(true);
                break;
            default:
                Log.e(TAG, "Unexpected enum.");
        }
    }

    /**
     * Enters VR Shell if necessary, displaying browser UI and tab contents in VR.
     */
    @EnterVRResult
    private int enterVrInternal() {
        if (mPaused) return EnterVRResult.CANCELLED;
        if (mInVr) return EnterVRResult.NOT_NECESSARY;
        if (!canEnterVr()) return EnterVRResult.CANCELLED;

        if (VrCoreInstallUtils.getVrSupportLevel() == VrSupportLevel.VR_DAYDREAM
                && isDaydreamCurrentViewerInternal()) {
            // TODO(mthiesse): This is a workaround for b/66486878 (see also crbug.com/767594).
            // We have to trigger the DON flow before setting VR mode enabled to prevent the DON
            // flow from failing on the S8/S8+.
            // Due to b/66493165, we also can't create our VR UI before the density has changed,
            // so we can't trigger the DON flow by resuming the GvrLayout. This basically means that
            // calling launchInVr on ourself is the only viable option for getting into VR on the
            // S8/S8+.
            // This also fixes the issue tracked in crbug.com/767944, so this should not be removed
            // until the root cause of that has been found and fixed.
            getVrDaydreamApi().launchInVr(getEnterVrPendingIntent(mActivity));
            mProbablyInDon = true;
        } else {
            enterVr();
        }
        return EnterVRResult.REQUESTED;
    }

    private void requestToExitVrInternal(OnExitVrRequestListener listener,
            @UiUnsupportedMode int reason, boolean showExitPromptBeforeDoff) {
        assert listener != null;
        if (VrModuleProvider.getDelegate().bootsToVr()) {
            setVrModeEnabled(mActivity, false);
            listener.onSucceeded();
            return;
        }

        // If we are currently processing another request, deny the request.
        if (mOnExitVrRequestListener != null) {
            listener.onDenied();
            return;
        }
        mOnExitVrRequestListener = listener;
        mShowingExitVrPrompt = showExitPromptBeforeDoff;
        mVrShell.requestToExitVr(reason, showExitPromptBeforeDoff);
    }

    private void exitWebVRAndClearState() {
        exitWebVRPresent();
        mRequestedWebVr = false;
    }

    @CalledByNative
    /* package */ void exitWebVRPresent() {
        if (!mInVr) return;

        // If we have previously used the VRBrowser this session, go back to it.
        // If not, and we're on Daydream go back to Daydream home.
        // Otherwise, exit VR.
        if (mVrBrowserUsed) {
            mVrShell.setWebVrModeEnabled(false);
        } else if (isDaydreamCurrentViewerInternal()) {
            getVrDaydreamApi().launchVrHomescreen();
        } else {
            shutdownVr(true /* disableVrMode */, true /* stayingInChrome */);
        }
    }

    private boolean cancelStartupAnimationIfNeeded() {
        if (!mNeedsAnimationCancel) return false;
        if (VrDelegate.DEBUG_LOGS) Log.e(TAG, "canceling startup animation");
        mCancellingEntryAnimation = true;
        Bundle options = ActivityOptions.makeCustomAnimation(mActivity, 0, 0).toBundle();
        Intent intent = VrModuleProvider.getIntentDelegate().setupVrIntent(
                new Intent(mActivity, VrCancelAnimationActivity.class));
        // We don't want this to run in a new task stack, or we may end up resuming the wrong
        // Activity when the VrCancelAnimationActivity finishes.
        intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_NEW_TASK);
        mActivity.startActivity(intent, options);
        mNeedsAnimationCancel = false;
        return true;
    }

    @VisibleForTesting
    protected void onResume() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "onResume");
        if (mNeedsAnimationCancel) {
            // At least on some devices, like the Samsung S8+, a Window animation is run after our
            // Activity is shown that fades between a stale screenshot from before pausing to the
            // currently rendered content. It's impossible to cancel window animations, and in order
            // to modify the animation we would need to set up the desired animations before
            // calling setContentView, which we can't do because it would affect non-VR usage.
            // To work around this, we keep the stay_hidden animation active until the window
            // animation of the stale screenshot finishes and our black overlay is shown. We then
            // cancel the stay_hidden animation, revealing our black overlay, which we then replace
            // with VR UI.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return;
            // Just in case any platforms/users modify the window animation scale, we'll multiply
            // our wait time by that scale value.
            float scale = Settings.Global.getFloat(
                    mActivity.getContentResolver(), Settings.Global.WINDOW_ANIMATION_SCALE, 1.0f);
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    cancelStartupAnimationIfNeeded();
                }
            }, (long) (WINDOW_FADE_ANIMATION_DURATION_MS * scale));
            return;
        }

        mPaused = false;
        mCancellingEntryAnimation = false;

        // We call resume here to be symmetric with onPause in case we get paused/resumed without
        // being hidden/shown. However, we still don't want to resume if we're not visible to avoid
        // doing VR rendering that won't be seen.
        if (mInVr && mVisible) mVrShell.resume();

        // Shouldn't handle VR Intents pre-Daydream.
        assert (VrCoreInstallUtils.getVrSupportLevel() == VrSupportLevel.VR_DAYDREAM
                || !mStartedFromVrIntent);

        if (mNativeVrShellDelegate != 0) {
            VrShellDelegateJni.get().onResume(mNativeVrShellDelegate, VrShellDelegate.this);
        }

        // Perform slow initialization asynchronously.
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                if (!sRegisteredVrAssetsComponent) {
                    registerVrAssetsComponentIfDaydreamUser(isDaydreamCurrentViewerInternal());
                }
            }
        });

        if (mDonSucceeded) {
            handleDonFlowSuccess();
        } else {
            if (mProbablyInDon && !mTestWorkaroundDontCancelVrEntryOnResume) {
                // This means the user backed out of the DON flow, and we won't be entering VR.
                maybeSetPresentResult(false);

                shutdownVr(true, false);
            }
            // If we were resumed at the wrong density, we need to trigger activity recreation.
            if (!mInVr && mExpectedDensityChange != 0
                    && (mActivity.getResources().getConfiguration().densityDpi
                            != mExpectedDensityChange)) {
                mActivity.recreate();
            }
        }

        mProbablyInDon = false;
        mShowVrServicesUpdatePrompt = null;

        runPendingExitVrTask();
    }

    private void handleDonFlowSuccess() {
        setWindowModeForVr();
        if (mInVr) {
            maybeSetPresentResult(true);
            mDonSucceeded = false;
            return;
        }
        // If we fail to enter VR when we should have entered VR, return to the home screen.
        if (!enterVrAfterDon()) {
            cancelPendingVrEntry();
            getVrDaydreamApi().launchVrHomescreen();
        }
    }

    // Android lifecycle doesn't guarantee that this will be called after onResume (though it
    // will usually be), so make sure anything we do here can happen before or after
    // onResume.
    private void onActivityShown() {
        mVisible = true;

        // Only resume VrShell once we're visible so that we don't start rendering before being
        // visible and delaying startup.
        if (mInVr && !mPaused) mVrShell.resume();
    }

    private void onActivityHidden() {
        mVisible = false;
        // In case we're hidden before onPause is called, we pause here. Duplicate calls to pause
        // are safe.
        if (mInVr) mVrShell.pause();
    }

    private void onPause() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "onPause");
        mPaused = true;
        if (mCancellingEntryAnimation) return;
        if (VrCoreInstallUtils.getVrSupportLevel() <= VrSupportLevel.VR_NEEDS_UPDATE) return;

        if (mInVr) mVrShell.pause();
        if (mNativeVrShellDelegate != 0) {
            VrShellDelegateJni.get().onPause(mNativeVrShellDelegate, VrShellDelegate.this);
        }

        mIsDaydreamCurrentViewer = null;
    }

    private void onStart() {
        if (mDonSucceeded) setWindowModeForVr();

        // This handles the case where Chrome was paused in VR (ie the user navigated to DD home or
        // something), then exited VR and resumed Chrome in 2D. Chrome is still showing VR UI but
        // the user is no longer in a VR session.
        if (mInVr && !isInVrSession()) {
            shutdownVr(true, false);
        }

        // Note that we do not turn VR mode on here for two reasons.
        // 1. If we're in VR, it should already be on and won't get turned off until we explicitly
        // turn it off for this Activity.
        // 2. Turning VR mode on breaks popup showing code, which relies on VR mode sometimes being
        // off while in VR.
    }

    private void onStop() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "onStop");
        assert !mCancellingEntryAnimation;
    }

    private boolean onBackPressedInternal() {
        if (VrCoreInstallUtils.getVrSupportLevel() <= VrSupportLevel.VR_NEEDS_UPDATE) return false;
        cancelPendingVrEntry();
        if (!mInVr) return false;
        // Back button should be handled the same way as the close button.
        getVrCloseButtonListener().run();
        return true;
    }

    /**
     * @return Whether the user is currently seeing the DOFF screen.
     */
    /* package */ boolean showDoff(boolean optional) {
        assert !mShowingDaydreamDoff;
        if (!isDaydreamCurrentViewerInternal()) return false;

        if (supports2dInVr()) {
            setVrModeEnabled(mActivity, false);
            callOnExitVrRequestListener(true);
            return true;
        }

        try {
            if (getVrDaydreamApi().exitFromVr(mActivity, EXIT_VR_RESULT, new Intent())) {
                mShowingDaydreamDoff = true;
                mDoffOptional = optional;
                return true;
            }
        } catch (IllegalArgumentException | SecurityException e) {
            // DOFF calls can unpredictably throw exceptions if VrCore doesn't think Chrome is
            // the active component, for example.
        }
        if (!optional) getVrDaydreamApi().launchVrHomescreen();
        return false;
    }

    private void onExitVrResult(boolean success) {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "returned from DOFF, success: " + success);

        // We may have manually handled the exit early by swapping to another Chrome activity that
        // supports VR while in the DOFF activity. If that happens we want to exit early when the
        // real DOFF flow calls us back.
        if (!mShowingDaydreamDoff) return;

        // If Doff is not optional and user backed out, launch DD home. We can't re-trigger doff
        // here because we're not yet the active VR component and Daydream will throw a Security
        // Exception.
        if (!mDoffOptional && !success) getVrDaydreamApi().launchVrHomescreen();

        mShowingDaydreamDoff = false;

        if (mShowingDoffForGvrUpdate) mShowVrServicesUpdatePrompt = success;

        if (success) shutdownVr(true /* disableVrMode */, true /* stayingInChrome */);

        callOnExitVrRequestListener(success);
        mShowingDoffForGvrUpdate = false;
    }

    // Caches whether the current viewer is Daydream for performance.
    private boolean isDaydreamCurrentViewerInternal() {
        if (mIsDaydreamCurrentViewer == null) {
            mIsDaydreamCurrentViewer = getVrDaydreamApi().isDaydreamCurrentViewer();
        }
        return mIsDaydreamCurrentViewer;
    }

    private void cancelPendingVrEntry() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "cancelPendingVrEntry");
        VrModuleProvider.getDelegate().removeBlackOverlayView(mActivity, false /* animate */);
        mDonSucceeded = false;
        maybeSetPresentResult(false);
        if (!mShowingDaydreamDoff) {
            setVrModeEnabled(mActivity, false);
            restoreWindowMode();
        }
    }

    /**
     * Exits VR Shell, performing all necessary cleanup.
     */
    private void shutdownVr(boolean disableVrMode, boolean stayingInChrome) {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "shuttdown VR");
        cancelPendingVrEntry();

        if (!mInVr) return;
        if (mShowingDaydreamDoff) {
            onExitVrResult(true);
            return;
        }
        mInVr = false;

        // Some Samsung devices change the screen density after exiting VR mode which causes
        // us to restart Chrome with the VR intent that originally started it. We don't want to
        // enable VR mode again, so we remove VR specific extras.
        VrModuleProvider.getIntentDelegate().removeVrExtras(mActivity.getIntent());

        // The user has exited VR.
        RecordUserAction.record("VR.DOFF");

        if (disableVrMode) setVrModeEnabled(mActivity, false);

        // We get crashes on Android K related to surfaces if we manipulate the view hierarchy while
        // finishing.
        if (mActivity.isFinishing()) {
            if (mVrShell != null) mVrShell.destroyWindowAndroid();
            return;
        }

        restoreWindowMode();
        mVrShell.pause();
        removeVrViews();
        destroyVrShell();

        promptForFeedbackIfNeeded(stayingInChrome);

        // User exited VR (via something like the system back button) while looking at the exit VR
        // prompt.
        if (mShowingExitVrPrompt) callOnExitVrRequestListener(true);

        VrModuleProvider.onExitVr();
    }

    private void callOnExitVrRequestListener(boolean success) {
        if (mOnExitVrRequestListener != null) {
            if (success) {
                mOnExitVrRequestListener.onSucceeded();
            } else {
                mOnExitVrRequestListener.onDenied();
            }
        }
        mOnExitVrRequestListener = null;
    }

    /* package */ void onExitVrRequestResult(boolean shouldExit) {
        assert mOnExitVrRequestListener != null;
        mShowingExitVrPrompt = false;
        if (shouldExit) {
            mExitedDueToUnsupportedMode = true;
            if (!showDoff(true /* optional */)) callOnExitVrRequestListener(false);
        } else {
            callOnExitVrRequestListener(false);
        }
    }

    /**
     * Returns the callback for the user-triggered close button to exit VR mode.
     */
    /* package */ Runnable getVrCloseButtonListener() {
        if (mCloseButtonListener != null) return mCloseButtonListener;
        mCloseButtonListener = new Runnable() {
            @Override
            public void run() {
                shutdownVr(true /* disableVrMode */, true /* stayingInChrome */);
            }
        };
        return mCloseButtonListener;
    }

    /**
     * Returns the callback for the user-triggered close button to exit VR mode.
     */
    /* package */ Runnable getVrSettingsButtonListener() {
        if (mSettingsButtonListener != null) return mSettingsButtonListener;
        mSettingsButtonListener = new Runnable() {
            @Override
            public void run() {
                shutdownVr(true /* disableVrMode */, false /* stayingInChrome */);

                // Launch Daydream settings.
                GvrUiLayout.launchOrInstallGvrApp(mActivity);
            }
        };
        return mSettingsButtonListener;
    }

    /**
     * Prompts the user to enter feedback for their VR Browsing experience.
     */
    private void promptForFeedbackIfNeeded(boolean stayingInChrome) {
        // We only prompt for feedback if:
        // 1) The user hasn't explicitly opted-out of it in the past
        // 2) The user has performed VR browsing
        // 3) The user is exiting VR and going back into 2D Chrome
        // 4) We're not exiting to complete an unsupported VR action in 2D (e.g. viewing PageInfo)
        // 5) Every n'th visit (where n = mFeedbackFrequency)

        if (!activitySupportsExitFeedback(mActivity)) return;
        if (!stayingInChrome) return;
        if (VrFeedbackStatus.getFeedbackOptOut()) return;
        if (!mVrBrowserUsed) return;
        if (mExitedDueToUnsupportedMode) return;

        int exitCount = VrFeedbackStatus.getUserExitedAndEntered2DCount();
        VrFeedbackStatus.setUserExitedAndEntered2DCount((exitCount + 1) % mFeedbackFrequency);

        if (exitCount > 0) return;

        promptForFeedback(mActivity.getActivityTab());
    }

    /* package */ void promptForKeyboardUpdate() {
        mCachedGvrKeyboardPackageVersion = getGvrKeyboardPackageVersion();
        mActivity.startActivityForResult(
                new Intent(Intent.ACTION_VIEW, Uri.parse(GVR_KEYBOARD_MARKET_URI)),
                GVR_KEYBOARD_UPDATE_RESULT);
    }

    @VisibleForTesting
    protected boolean canLaunch2DIntentsInternal() {
        return supports2dInVr() && !sVrModeEnabledActivitys.contains(sInstance.mActivity);
    }

    @VisibleForTesting
    protected boolean createVrShell() {
        assert mVrShell == null;
        if (!mActivity.getCompositorViewHolderSupplier().hasValue()) return false;
        TabModelSelector tabModelSelector = mActivity.getTabModelSelector();
        if (tabModelSelector == null) return false;
        try {
            mVrShell = new VrShell(mActivity, this, tabModelSelector, mActivity.getToolbarManager(),
                    mActivity.getModalDialogManagerSupplier(),
                    mActivity.getCompositorViewHolderSupplier(), mActivity.getActivityTabProvider(),
                    mActivity.getBrowserControlsManager(), /* tabCreatorManager= */ mActivity,
                    mActivity.getWindowAndroid(), mActivity::isActivityFinishingOrDestroyed,
                    mActivity.getFullscreenManager(), mActivity::backShouldCloseTab,
                    mActivity::isInOverviewMode, /* menuOrKeyboardActionController= */ mActivity);
        } catch (VrUnsupportedException e) {
            return false;
        } finally {
        }
        return true;
    }

    private void addVrViews() {
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        decor.addView(mVrShell.getContainer(), params);
        // If the overlay exists, make sure to hide the GvrLayout behind it.
        View overlay = mActivity.getWindow().findViewById(R.id.vr_overlay_view);
        if (overlay != null) overlay.bringToFront();
        mActivity.onEnterVr();
    }

    @VisibleForTesting
    protected boolean isBlackOverlayVisible() {
        View overlay = mActivity.getWindow().findViewById(R.id.vr_overlay_view);
        return overlay != null;
    }

    private void removeVrViews() {
        mActivity.onExitVr();
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        decor.removeView(mVrShell.getContainer());
    }

    /**
     * Clean up VrShell, and associated native objects.
     */
    private void destroyVrShell() {
        if (mVrShell != null) {
            mVrShell.getContainer().setOnSystemUiVisibilityChangeListener(null);
            mVrShell.teardown();
            mVrShell = null;
        }
    }

    /**
     * @param api The VrDaydreamApi object this delegate will use instead of the default one
     */
    @VisibleForTesting
    protected void overrideDaydreamApi(VrDaydreamApi api) {
        sVrDaydreamApi = api;
    }

    /**
     * @return The VrShell for the VrShellDelegate instance
     */
    @VisibleForTesting
    protected VrShell getVrShell() {
        return mVrShell;
    }

    /**
     * @param frequency Sets how often to show the feedback prompt.
     */
    @VisibleForTesting
    protected void setFeedbackFrequency(int frequency) {
        mFeedbackFrequency = frequency;
    }

    @VisibleForTesting
    protected boolean isVrEntryComplete() {
        return mInVr && !mProbablyInDon && getVrShell().hasUiFinishedLoading();
    }

    @VisibleForTesting
    protected boolean isShowingDoff() {
        return mShowingDaydreamDoff;
    }

    @VisibleForTesting
    protected void onBroadcastReceived() {}

    @VisibleForTesting
    protected void setExpectingIntent(boolean expectingIntent) {}

    /**
     * @return Pointer to the native VrShellDelegate object.
     */
    @CalledByNative
    private long getNativePointer() {
        return mNativeVrShellDelegate;
    }

    private void destroy() {
        if (sInstance == null) return;
        shutdownVr(false /* disableVrMode */, false /* stayingInChrome */);
        if (mNativeVrShellDelegate != 0) {
            VrShellDelegateJni.get().destroy(mNativeVrShellDelegate, VrShellDelegate.this);
        }
        mNativeVrShellDelegate = 0;
        sInstance = null;
    }

    @NativeMethods
    interface Natives {
        long init(VrShellDelegate caller);
        void onLibraryAvailable();
        void setPresentResult(long nativeVrShellDelegate, VrShellDelegate caller, boolean result);
        void onPause(long nativeVrShellDelegate, VrShellDelegate caller);
        void onResume(long nativeVrShellDelegate, VrShellDelegate caller);
        void destroy(long nativeVrShellDelegate, VrShellDelegate caller);
        void registerVrAssetsComponent();
    }
}
