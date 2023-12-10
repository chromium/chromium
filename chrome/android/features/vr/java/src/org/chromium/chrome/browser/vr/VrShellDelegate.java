// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
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

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.webapps.WebappActivity;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Set;

/** Manages interactions with the VR Shell. */
@JNINamespace("vr")
public class VrShellDelegate implements View.OnSystemUiVisibilityChangeListener {
    private static final String TAG = "VrShellDelegate";

    @IntDef({
        EnterVRResult.NOT_NECESSARY,
        EnterVRResult.CANCELLED,
        EnterVRResult.REQUESTED,
        EnterVRResult.SUCCEEDED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnterVRResult {
        int NOT_NECESSARY = 0;
        int CANCELLED = 1;
        int REQUESTED = 2;
        int SUCCEEDED = 3;
    }

    private static VrShellDelegate sInstance;
    private static VrLifecycleObserver sVrLifecycleObserver;
    private static VrDaydreamApi sVrDaydreamApi;
    private static Set<Activity> sVrModeEnabledActivitys = new HashSet<>();
    private static boolean sRegisteredDaydreamHook;
    private static boolean sTestVrShellDelegateOnStartup;
    private static ObservableSupplierImpl<Boolean> sVrModeEnabledSupplier;

    private ChromeActivity mActivity;

    private VrShell mVrShell;
    private Boolean mIsDaydreamCurrentViewer;
    private boolean mInVr;

    private boolean mPaused;
    private boolean mVisible;
    private boolean mRestoreSystemUiVisibility;
    private Integer mRestoreOrientation;
    private boolean mRequestedWebVr;

    // Gets run when the user exits VR mode by clicking the 'x' button or system UI back button.
    private Runnable mCloseButtonListener;

    // Gets run when the user exits VR mode by clicking the Gear button.
    private Runnable mSettingsButtonListener;

    @VisibleForTesting protected boolean mTestWorkaroundDontCancelVrEntryOnResume;

    private long mNativeVrShellDelegate;

    /* package */ static final class VrUnsupportedException extends RuntimeException {}

    private static final class VrLifecycleObserver
            implements ApplicationStatus.ActivityStateListener {
        @Override
        public void onActivityStateChange(Activity activity, int newState) {
            switch (newState) {
                case ActivityState.DESTROYED:
                    sVrModeEnabledActivitys.remove(activity);
                    break;
                default:
                    break;
            }
            if (sInstance != null) sInstance.onActivityStateChange(activity, newState);
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

    /** Called when the native library is first available. */
    public static void onNativeLibraryAvailable() {
        VrModule.ensureNativeLoaded();
        VrModuleProvider.registerJni();
        VrShellDelegateJni.get().onLibraryAvailable();
    }

    /** Whether or not we are currently in VR. */
    public static boolean isInVr() {
        if (sInstance == null) return false;
        return sInstance.mInVr;
    }

    /**
     * See {@link ChromeActivity#handleBackPressed}
     * Only handles the back press while in VR.
     */
    public static boolean onBackPressed() {
        if (sInstance == null) return false;
        return sInstance.onBackPressedInternal();
    }

    public static void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        if (isInMultiWindowMode && isInVr()) {
            sInstance.shutdownVr(/* disableVrMode= */ true, /* stayingInChrome= */ true);
        }
    }

    /** Called when the {@link Activity} becomes visible. */
    public static void onActivityShown(Activity activity) {
        if (sInstance != null && sInstance.mActivity == activity) sInstance.onActivityShown();
    }

    /** Called when the {@link Activity} is hidden. */
    public static void onActivityHidden(Activity activity) {
        if (sInstance != null && sInstance.mActivity == activity) sInstance.onActivityHidden();
    }

    /** Asynchronously enable VR mode. */
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

    public static void initAfterModuleInstall() {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        onNativeLibraryAvailable();
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

    protected static void enableTestVrShellDelegateOnStartupForTesting() {
        sTestVrShellDelegateOnStartup = true;
    }

    /* package */ static boolean isVrModeEnabled(Activity activity) {
        return sVrModeEnabledActivitys.contains(activity);
    }

    private static boolean activitySupportsPresentation(Activity activity) {
        return activity instanceof ChromeTabbedActivity
                || activity instanceof WebappActivity
                || (activity instanceof CustomTabActivity
                        && !getVrDaydreamApi().isDaydreamCurrentViewer());
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
                break;
            case ActivityState.STARTED:
                if (activity == mActivity) onStart();
                break;
            case ActivityState.RESUMED:
                if (!activitySupportsPresentation(activity)) return;
                if (!(activity instanceof ChromeActivity)) return;
                swapHostActivity((ChromeActivity) activity, /* disableVrMode= */ true);
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
        if (mInVr) shutdownVr(disableVrMode, /* stayingInChrome= */ false);
        mActivity = activity;
    }

    private void maybeSetPresentResult(boolean result) {
        if (mNativeVrShellDelegate == 0 || !mRequestedWebVr) return;
        VrShellDelegateJni.get()
                .setPresentResult(mNativeVrShellDelegate, VrShellDelegate.this, result);
        mRequestedWebVr = false;
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

        if (!createVrShell()) {
            cancelPendingVrEntry();
            mInVr = false;
            getVrDaydreamApi().launchVrHomescreen();
            return;
        }

        addVrViews();
        mVrShell.initializeNative();

        // resume needs to be called on GvrLayout after initialization to make sure DON flow works
        // properly.
        if (mVisible) mVrShell.resume();
        mVrShell.getContainer().setOnSystemUiVisibilityChangeListener(this);

        maybeSetPresentResult(true);
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        if (mInVr && !isWindowModeCorrectForVr()) {
            setWindowModeForVr();
        }
    }

    public boolean hasRecordAudioPermission() {
        return mActivity.getWindowAndroid().hasPermission(android.Manifest.permission.RECORD_AUDIO);
    }

    public boolean canRequestRecordAudioPermission() {
        return mActivity
                .getWindowAndroid()
                .canRequestPermission(android.Manifest.permission.RECORD_AUDIO);
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
        // Hide system UI.
        VrModuleProvider.getDelegate().setSystemUiVisibilityForVr(mActivity);

        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                mActivity.getCompositorViewHolderSupplier();
        if (compositorViewHolderSupplier.hasValue()) {
            compositorViewHolderSupplier.get().getCompositorView().setOverlayVrMode(true);
        }

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
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Restore orientation.
        if (mRestoreOrientation != null) mActivity.setRequestedOrientation(mRestoreOrientation);
        mRestoreOrientation = null;

        // Restore system UI visibility.
        if (mRestoreSystemUiVisibility) {
            int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
            mActivity
                    .getWindow()
                    .getDecorView()
                    .setSystemUiVisibility(flags & ~VrDelegate.VR_SYSTEM_UI_FLAGS);
        }
        mRestoreSystemUiVisibility = false;
        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                mActivity.getCompositorViewHolderSupplier();
        if (compositorViewHolderSupplier.hasValue()) {
            compositorViewHolderSupplier.get().getCompositorView().setOverlayVrMode(false);
        }
        mActivity.getWindow().getAttributes().rotationAnimation =
                WindowManager.LayoutParams.ROTATION_ANIMATION_ROTATE;
    }

    /* package */ boolean canEnterVr() {
        if (VrCoreInstallUtils.vrSupportNeedsUpdate()) return false;

        // If this is not a WebXR request, then return false.
        if (!mRequestedWebVr) return false;
        return true;
    }

    @CalledByNative
    private void presentRequested() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "WebVR page requested presentation");
        mRequestedWebVr = true;
        switch (enterVrInternal()) {
            case EnterVRResult.NOT_NECESSARY:
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

    /** Enters VR Shell if necessary, displaying browser UI and tab contents in VR. */
    private @EnterVRResult int enterVrInternal() {
        if (mPaused) return EnterVRResult.CANCELLED;
        if (mInVr) return EnterVRResult.NOT_NECESSARY;
        if (!canEnterVr()) return EnterVRResult.CANCELLED;

        enterVr();
        return EnterVRResult.REQUESTED;
    }

    @CalledByNative
    /* package */ void exitWebVRPresent() {
        if (!mInVr) return;

        shutdownVr(/* disableVrMode= */ true, /* stayingInChrome= */ true);
    }

    @VisibleForTesting
    protected void onResume() {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "onResume");

        mPaused = false;

        // We call resume here to be symmetric with onPause in case we get paused/resumed without
        // being hidden/shown. However, we still don't want to resume if we're not visible to avoid
        // doing VR rendering that won't be seen.
        if (mInVr && mVisible) mVrShell.resume();

        if (mNativeVrShellDelegate != 0) {
            VrShellDelegateJni.get().onResume(mNativeVrShellDelegate, VrShellDelegate.this);
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
        if (VrCoreInstallUtils.getVrSupportLevel() <= VrSupportLevel.VR_NEEDS_UPDATE) return;

        if (mInVr) mVrShell.pause();
        if (mNativeVrShellDelegate != 0) {
            VrShellDelegateJni.get().onPause(mNativeVrShellDelegate, VrShellDelegate.this);
        }

        mIsDaydreamCurrentViewer = null;
    }

    private void onStart() {
        // This handles the case where Chrome was paused in VR (ie the user navigated to DD home or
        // something), then exited VR and resumed Chrome in 2D. Chrome is still showing VR UI but
        // the user is no longer in a VR session.
        if (mInVr && !isInVrSession()) {
            shutdownVr(true, false);
        }

        // Note that we do not turn VR mode on here because if we're in VR, it should already be on
        // and won't get turned off until we explicitly turn it off for this Activity.
    }

    private boolean onBackPressedInternal() {
        if (VrCoreInstallUtils.getVrSupportLevel() <= VrSupportLevel.VR_NEEDS_UPDATE) return false;
        cancelPendingVrEntry();
        if (!mInVr) return false;
        // Back button should be handled the same way as the close button.
        getVrCloseButtonListener().run();
        return true;
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
        maybeSetPresentResult(false);
        setVrModeEnabled(mActivity, false);
        restoreWindowMode();
    }

    /** Exits VR Shell, performing all necessary cleanup. */
    /* package */ void shutdownVr(boolean disableVrMode, boolean stayingInChrome) {
        if (VrDelegate.DEBUG_LOGS) Log.i(TAG, "shuttdown VR");
        cancelPendingVrEntry();

        if (!mInVr) return;
        mInVr = false;

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
    }

    /** Returns the callback for the user-triggered close button to exit VR mode. */
    /* package */ Runnable getVrCloseButtonListener() {
        if (mCloseButtonListener != null) return mCloseButtonListener;
        mCloseButtonListener =
                new Runnable() {
                    @Override
                    public void run() {
                        shutdownVr(/* disableVrMode= */ true, /* stayingInChrome= */ true);
                    }
                };
        return mCloseButtonListener;
    }

    /** Returns the callback for the user-triggered close button to exit VR mode. */
    /* package */ Runnable getVrSettingsButtonListener() {
        if (mSettingsButtonListener != null) return mSettingsButtonListener;
        mSettingsButtonListener =
                new Runnable() {
                    @Override
                    public void run() {
                        shutdownVr(/* disableVrMode= */ true, /* stayingInChrome= */ false);

                        // Launch Daydream settings.
                        GvrUiLayout.launchOrInstallGvrApp(mActivity);
                    }
                };
        return mSettingsButtonListener;
    }

    @VisibleForTesting
    protected boolean createVrShell() {
        assert mVrShell == null;
        if (!mActivity.getCompositorViewHolderSupplier().hasValue()) return false;
        TabModelSelector tabModelSelector = mActivity.getTabModelSelector();
        if (tabModelSelector == null) return false;
        try {
            mVrShell =
                    new VrShell(
                            mActivity,
                            this,
                            tabModelSelector,
                            /* tabCreatorManager= */ mActivity,
                            mActivity.getWindowAndroid(),
                            mActivity.getActivityTab());
        } catch (VrUnsupportedException e) {
            return false;
        } finally {
        }
        return true;
    }

    private void addVrViews() {
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        decor.addView(mVrShell.getContainer(), params);
        mActivity.onEnterVr();
    }

    private void removeVrViews() {
        mActivity.onExitVr();
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        decor.removeView(mVrShell.getContainer());
    }

    /** Clean up VrShell, and associated native objects. */
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

    @VisibleForTesting
    protected boolean isVrEntryComplete() {
        return mInVr;
    }

    /**
     * @return Pointer to the native VrShellDelegate object.
     */
    @CalledByNative
    private long getNativePointer() {
        return mNativeVrShellDelegate;
    }

    private void destroy() {
        if (sInstance == null) return;
        shutdownVr(/* disableVrMode= */ false, /* stayingInChrome= */ false);
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
    }
}
