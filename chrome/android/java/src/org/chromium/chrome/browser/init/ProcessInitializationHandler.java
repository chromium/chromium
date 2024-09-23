// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Process;
import android.text.format.DateUtils;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import androidx.annotation.CallSuper;
import androidx.annotation.WorkerThread;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.memory.MemoryPressureUma;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.ChainedTasks;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ChromeStrictMode;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.app.bluetooth.BluetoothNotificationService;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.app.usb.UsbNotificationService;
import org.chromium.chrome.browser.backup.ChromeBackupAgentImpl;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManager;
import org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetProvider;
import org.chromium.chrome.browser.contacts_picker.ChromePickerAdapter;
import org.chromium.chrome.browser.content_capture.ContentCaptureHistoryDeletionObserver;
import org.chromium.chrome.browser.crash.CrashUploadCountStore;
import org.chromium.chrome.browser.crash.LogcatExtractionRunnable;
import org.chromium.chrome.browser.crash.MinidumpUploadServiceImpl;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.OfflineContentAvailabilityStatusProvider;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.firstrun.TosDialogBehaviorSharedPrefInvalidator;
import org.chromium.chrome.browser.history.HistoryDeletionBridge;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.incognito.IncognitoTabLauncher;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.media.MediaCaptureNotificationServiceImpl;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.PackageMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.notifications.channels.ChannelsUpdater;
import org.chromium.chrome.browser.ntp.FeedPositionUtils;
import org.chromium.chrome.browser.offlinepages.measurements.OfflineMeasurementsBackgroundTask;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.photo_picker.DecoderService;
import org.chromium.chrome.browser.preferences.AllPreferenceKeyRegistries;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap.ProfileSelection;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.browser.tab.state.PersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.ui.cars.DrivingRestrictionsManager;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.chrome.browser.usb.UsbNotificationManager;
import org.chromium.chrome.browser.util.AfterStartupTaskUtils;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebApkUninstallTracker;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.browser_ui.contacts_picker.ContactsPickerDialog;
import org.chromium.components.browser_ui.photo_picker.DecoderServiceHost;
import org.chromium.components.browser_ui.photo_picker.PhotoPickerDelegateBase;
import org.chromium.components.browser_ui.photo_picker.PhotoPickerDialog;
import org.chromium.components.browser_ui.share.ClipboardImageFileProvider;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.content_capture.PlatformContentCaptureController;
import org.chromium.components.crash.anr.AnrCollector;
import org.chromium.components.crash.browser.ChildProcessCrashObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppDetailsDelegate;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.SpeechRecognition;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PhotoPicker;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.SelectFileDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Handles the initialization dependences of the browser process. This is meant to handle the
 * initialization that is not tied to any particular Activity, and the logic that should only be
 * triggered a single time for the lifetime of the browser process.
 */
public class ProcessInitializationHandler {
    private static final String TAG = "ProcessInitHandler";

    private static final String DEV_TOOLS_SERVER_SOCKET_PREFIX = "chrome";

    /** Prevents race conditions when deleting snapshot database. */
    private static final Object SNAPSHOT_DATABASE_LOCK = new Object();

    private static final String SNAPSHOT_DATABASE_NAME = "snapshots.db";

    private static ProcessInitializationHandler sInstance;

    private boolean mInitializedPreNative;
    private boolean mInitializedPreNativeLibraryLoad;
    private boolean mInitializedPostNative;
    private boolean mInitializedPostNativeFollowingActivityInit;
    private boolean mInitializedDeferredStartupTasks;
    private boolean mNetworkChangeNotifierInitializationComplete;
    private final Locale mInitialLocale = Locale.getDefault();

    private DevToolsServer mDevToolsServer;

    private final ProfileKeyedMap<Boolean> mStartupProfileTasksCompleted =
            new ProfileKeyedMap<>(
                    ProfileSelection.REDIRECTED_TO_ORIGINAL,
                    ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);

    /**
     * @return The ProcessInitializationHandler for use during the lifetime of the browser process.
     */
    public static ProcessInitializationHandler getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            ProcessInitializationHandler instance =
                    ServiceLoaderUtil.maybeCreate(ProcessInitializationHandler.class);
            if (instance == null) {
                instance = new ProcessInitializationHandler();
            }
            sInstance = instance;
        }
        return sInstance;
    }

    /**
     * Initializes the dependencies that are required and used before native library loading.
     *
     * <p>If a dependency should be initialized early but only is utilizes when the native library
     * has been loaded, then add that dependency in {@link
     * #handlePreNativeLibraryLoadInitialization()}.
     *
     * <p>Adding anything expensive to this must be avoided as it would delay the Chrome startup
     * path.
     *
     * <p>All entry points that do not rely on {@link ChromeBrowserInitializer} must call this on
     * startup.
     */
    public final void initializePreNative() {
        try (TraceEvent e =
                TraceEvent.scoped("ProcessInitializationHandler.initializePreNative()")) {
            ThreadUtils.checkUiThread();
            if (mInitializedPreNative) return;
            handlePreNativeInitialization();
            mInitializedPreNative = true;
        }
    }

    /** Performs the shared class initialization. */
    @CallSuper
    protected void handlePreNativeInitialization() {
        setProcessStateSummaryForAnrs(false);
    }

    /**
     * Initializes the dependencies that must occur before native library has been loaded.
     *
     * <p>Adding anything expensive to this must be avoided as it would delay the Chrome startup
     * path.
     *
     * <p>All entry points that do not rely on {@link ChromeBrowserInitializer} must call this on
     * startup.
     */
    public final void initializePreNativeLibraryLoad() {
        try (TraceEvent e =
                TraceEvent.scoped(
                        "ProcessInitializationHandler.initializePreNativeLibraryLoad()")) {
            ThreadUtils.checkUiThread();
            if (mInitializedPreNativeLibraryLoad) return;
            handlePreNativeLibraryLoadInitialization();
            mInitializedPreNativeLibraryLoad = true;
        }
    }

    /**
     * Performs the shared class initialization of dependencies that need to be initialized
     * immediately before native library loading and initialization.
     */
    @CallSuper
    protected void handlePreNativeLibraryLoadInitialization() {
        new Thread(SafeBrowsingApiBridge::ensureSafetyNetApiInitialized).start();
        new Thread(SafeBrowsingApiBridge::initSafeBrowsingApi).start();

        // Ensure critical files are available, so they aren't blocked on the file-system
        // behind long-running accesses in next phase.
        // Don't do any large file access here!
        ChromeStrictMode.configureStrictMode();
        ChromeWebApkHost.init();

        // In ENABLE_ASSERTS builds, initialize SharedPreferences key registry checking.
        if (BuildConfig.ENABLE_ASSERTS) {
            AllPreferenceKeyRegistries.initializeKnownRegistries();
        }
        // Time this call takes in background from test devices:
        // - Pixel 2: ~10 ms
        // - Nokia 1 (Android Go): 20-200 ms
        warmUpSharedPrefs();

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        ApplicationStatus.registerStateListenerForAllActivities(
                (activity, newState) -> {
                    if (newState == ActivityState.CREATED || newState == ActivityState.DESTROYED) {
                        // When the app locale is overridden a change in system locale will not
                        // effect Chrome's UI language. There is race condition where the initial
                        // locale may not equal the overridden default locale
                        // (https://crbug.com/1224756).
                        if (GlobalAppLocaleController.getInstance().isOverridden()) return;
                        // Android destroys Activities at some point after a locale change, but
                        // doesn't kill the process.  This can lead to a bug where Chrome is halfway
                        // RTL, where stale natively-loaded resources are not reloaded
                        // (http://crbug.com/552618).
                        if (!mInitialLocale.equals(Locale.getDefault())) {
                            Log.e(TAG, "Killing process because of locale change.");
                            Process.killProcess(Process.myPid());
                        }
                    }
                });
    }

    /**
     * Pre-load shared prefs to avoid being blocked on the disk access async task in the future.
     * Running in an AsyncTask as pre-loading itself may cause I/O.
     */
    private void warmUpSharedPrefs() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    DownloadManagerService.warmUpSharedPrefs();
                });
    }

    /**
     * Enqueues tasks that should be run before any Activity (or similar Android entry point) begins
     * their respective post native initialization.
     *
     * <p>There is no guarantee that these tasks are completed, so each task should individually
     * track their own corresponding completeness status and ensure subsequent calls only run the
     * required work.
     *
     * @param tasks The ordered list of startup tasks to be run.
     * @param minimalBrowserMode Whether this is being started in minimal mode.
     */
    public final void enqueuePostNativeTasksToRunBeforeActivityNativeInit(
            ChainedTasks tasks, boolean minimalBrowserMode) {
        ThreadUtils.checkUiThread();

        // If full browser process is not going to be launched, it is up to individual service to
        // launch its required components.
        if (!minimalBrowserMode && !mInitializedPostNative) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (mInitializedPostNative) return;
                        handlePostNativeInitialization();
                        mInitializedPostNative = true;
                    });
        }

        if (!mNetworkChangeNotifierInitializationComplete) {
            tasks.add(TaskTraits.UI_DEFAULT, this::initNetworkChangeNotifier);
        }
    }

    /**
     * Enqueues tasks that should be run after any Activity (or similar Android entry point)
     * completes their respective post native initialization.
     *
     * <p>There is no guarantee that these tasks are completed, so each task should individually
     * track their own corresponding completeness status and ensure subsequent calls only run the
     * required work.
     *
     * @param tasks The ordered list of startup tasks to be run.
     */
    public final void enqueuePostNativeTasksToRunAfterActivityNativeInit(ChainedTasks tasks) {
        if (!mInitializedPostNativeFollowingActivityInit) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (mInitializedPostNativeFollowingActivityInit) return;
                        handlePostNativeInitializationFollowingActivityInit();
                        mInitializedPostNativeFollowingActivityInit = true;
                    });
        }
    }

    /** Performs the post native initialization. */
    @CallSuper
    protected void handlePostNativeInitialization() {
        ChromeActivitySessionTracker.getInstance().initializeWithNative();
        ProfileManagerUtils.removeSessionCookiesForAllProfiles();
        AppBannerManager.setAppDetailsDelegate(
                ServiceLoaderUtil.maybeCreate(AppDetailsDelegate.class));
        ChromeLifetimeController.initialize();
        Clipboard.getInstance().setImageFileProvider(new ClipboardImageFileProvider());

        DecoderServiceHost.setIntentSupplier(
                () -> {
                    return new Intent(ContextUtils.getApplicationContext(), DecoderService.class);
                });

        SelectFileDialog.setPhotoPickerDelegate(
                new PhotoPickerDelegateBase() {
                    @Override
                    public PhotoPicker showPhotoPicker(
                            WindowAndroid windowAndroid,
                            PhotoPickerListener listener,
                            boolean allowMultiple,
                            List<String> mimeTypes) {
                        PhotoPickerDialog dialog =
                                new PhotoPickerDialog(
                                        windowAndroid,
                                        windowAndroid.getContext().get().getContentResolver(),
                                        listener,
                                        allowMultiple,
                                        mimeTypes);
                        dialog.getWindow().getAttributes().windowAnimations =
                                R.style.PickerDialogAnimation;
                        dialog.show();
                        return dialog;
                    }
                });

        ContactsPicker.setContactsPickerDelegate(
                (WebContents webContents,
                        ContactsPickerListener listener,
                        boolean allowMultiple,
                        boolean includeNames,
                        boolean includeEmails,
                        boolean includeTel,
                        boolean includeAddresses,
                        boolean includeIcons,
                        String formattedOrigin) -> {
                    WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
                    ContactsPickerDialog dialog =
                            new ContactsPickerDialog(
                                    windowAndroid,
                                    new ChromePickerAdapter(
                                            windowAndroid.getContext().get(),
                                            Profile.fromWebContents(webContents)),
                                    listener,
                                    allowMultiple,
                                    includeNames,
                                    includeEmails,
                                    includeTel,
                                    includeAddresses,
                                    includeIcons,
                                    formattedOrigin);
                    dialog.getWindow().getAttributes().windowAnimations =
                            R.style.PickerDialogAnimation;
                    dialog.show();
                    return dialog;
                });

        SearchActivityPreferencesManager.onNativeLibraryReady();
        SearchWidgetProvider.initialize();
        QuickActionSearchWidgetProvider.initialize();

        PrivacyPreferencesManagerImpl.getInstance().onNativeInitialized();
        setProcessStateSummaryForAnrs(true);

        List<Profile> profiles = ProfileManager.getLoadedProfiles();
        assert !profiles.isEmpty()
                : "At least one Profile should be loaded before post native init.";
        for (Profile profile : profiles) {
            handleProfileDependentPostNativeInitialization(profile);
        }
        ProfileManager.addObserver(
                new ProfileManager.Observer() {
                    @Override
                    public void onProfileAdded(Profile profile) {
                        if (profile.isOffTheRecord()) return;
                        handleProfileDependentPostNativeInitialization(profile);
                    }

                    @Override
                    public void onProfileDestroyed(Profile profile) {}
                });

        AccessibilityState.registerObservers();

        if (BuildInfo.getInstance().isAutomotive) {
            DrivingRestrictionsManager.initialize();
        }

        // Initialize UMA settings for survey component.
        SurveyClientFactory.initialize(PrivacyPreferencesManagerImpl.getInstance());

        AppHooks.get().registerPolicyProviders(CombinedPolicyProvider.get());
        SpeechRecognition.initialize();
    }

    /**
     * Handles additional post native initialization that should be run after the triggering
     * Activity (or other Android entry point) has completed their native init.
     */
    @CallSuper
    protected void handlePostNativeInitializationFollowingActivityInit() {
        FileProviderUtils.setFileProviderUtil(new FileProviderHelper());

        // When a child process crashes, search for the most recent minidump for the child's process
        // ID and attach a logcat to it. Then upload it to the crash server. Note that the logcat
        // extraction might fail. This is ok; in that case, the minidump will be found and uploaded
        // upon the next browser launch.
        ChildProcessCrashObserver.registerCrashCallback(
                new ChildProcessCrashObserver.ChildCrashedCallback() {
                    @Override
                    public void childCrashed(int pid) {
                        CrashFileManager crashFileManager =
                                new CrashFileManager(
                                        ContextUtils.getApplicationContext().getCacheDir());

                        File minidump = crashFileManager.getMinidumpSansLogcatForPid(pid);
                        if (minidump != null) {
                            AsyncTask.THREAD_POOL_EXECUTOR.execute(
                                    new LogcatExtractionRunnable(minidump));
                        } else {
                            Log.e(TAG, "Missing dump for child " + pid);
                        }
                    }
                });

        MemoryPressureUma.initializeForBrowser();
        UmaUtils.recordBackgroundRestrictions();

        // Needed for field trial metrics to be properly collected in minimal browser mode.
        ChromeCachedFlags.getInstance().cacheMinimalBrowserFlags();
    }

    public final void initNetworkChangeNotifier() {
        if (mNetworkChangeNotifierInitializationComplete) return;
        mNetworkChangeNotifierInitializationComplete = true;

        ThreadUtils.assertOnUiThread();
        TraceEvent.begin("NetworkChangeNotifier.init");
        // Enable auto-detection of network connectivity state changes.
        NetworkChangeNotifier.init();
        NetworkChangeNotifier.setAutoDetectConnectivityState(true);
        TraceEvent.end("NetworkChangeNotifier.init");
    }

    /**
     * Handle per-{@link Profile} post native initialization.
     *
     * <p>This will be called for each non-incognito {@link Profile} that is loaded and initialized.
     */
    @CallSuper
    protected void handleProfileDependentPostNativeInitialization(Profile profile) {
        FeedPositionUtils.cacheSegmentationResult(profile);

        HistoryDeletionBridge.getForProfile(profile)
                .addObserver(
                        new ContentCaptureHistoryDeletionObserver(
                                () -> PlatformContentCaptureController.getInstance()));
    }

    /**
     * We use the Android API to store key information which we can't afford to have wrong on our
     * ANR reports. So, we set the version number, and the main .so file's Build ID once native has
     * been loaded. Then, when we query Android for any ANRs that have happened, we can also pull
     * these key fields.
     *
     * <p>We are limited to 128 bytes in ProcessStateSummary, so we only store the most important
     * things that can change between the ANR happening and an upload (when the rest of the metadata
     * is gathered). Some fields we ignore because they won't change (eg. which channel or what the
     * .so filename is) and some we ignore because they aren't as critical (eg. experiments). In the
     * future, we could make this point to a file where we would write out all our crash keys, and
     * thus get full fidelity.
     */
    protected void setProcessStateSummaryForAnrs(boolean includeNative) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ActivityManager am =
                    (ActivityManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.ACTIVITY_SERVICE);
            String summary = VersionInfo.getProductVersion();
            if (includeNative) {
                summary += "," + AnrCollector.getSharedLibraryBuildId();
            }
            am.setProcessStateSummary(summary.getBytes(StandardCharsets.UTF_8));
        }
    }

    /**
     * Handle application level deferred startup tasks that can be lazily done after all
     * the necessary initialization has been completed. Should only be triggered once per browser
     * process lifetime. Any calls requiring network access should probably go here.
     *
     * Keep these tasks short and break up long tasks into multiple smaller tasks, as they run on
     * the UI thread and are blocking. Remember to follow RAIL guidelines, as much as possible, and
     * that most devices are quite slow, so leave enough buffer.
     */
    public final void initializeDeferredStartupTasks() {
        ThreadUtils.checkUiThread();
        if (mInitializedDeferredStartupTasks) return;
        mInitializedDeferredStartupTasks = true;

        DeferredStartupHandler deferredStartupHandler = DeferredStartupHandler.getInstance();
        List<Runnable> deferredTasks = new ArrayList<>();
        addPerApplicationStartupDeferredTasks(deferredTasks);
        deferredStartupHandler.addDeferredTasks(deferredTasks);
    }

    /**
     * Handle per-profile level deferred startup tasks that can be lazily done after all the
     * necessary initialization has been completed. Should only be triggered once per profile
     * lifetime. Any calls requiring network access should probably go here.
     *
     * @param profile The Profile associated with the startup tasks.
     * @see #initializeDeferredStartupTasks() for timing considerations.
     */
    public final void initializeProfileDependentDeferredStartupTasks(Profile profile) {
        ThreadUtils.checkUiThread();

        // Ignore the return value as ProfileKeyedMap is used to track that these actions are
        // handled at most once per Profile.
        mStartupProfileTasksCompleted.getForProfile(
                profile, this::handlePerProfileDeferredStartupTasksInitialization);
    }

    private boolean handlePerProfileDeferredStartupTasksInitialization(Profile profile) {
        DeferredStartupHandler deferredStartupHandler = DeferredStartupHandler.getInstance();
        List<Runnable> deferredTasks = new ArrayList<>();
        addPerProfileStartupDeferredTasks(profile, deferredTasks);
        deferredStartupHandler.addDeferredTasks(deferredTasks);

        return true; // Return a non-null value to ensure ProfileKeyedMap tracks this was completed.
    }

    /**
     * Adds all the deferred startup tasks that should be called exactly once for the lifetime of
     * the application.
     *
     * @param tasks The list where new tasks should be added.
     */
    @CallSuper
    protected void addPerApplicationStartupDeferredTasks(List<Runnable> tasks) {
        tasks.add(
                () -> {
                    initAsyncDiskTask();

                    DefaultBrowserInfo.initBrowserFetcher();

                    AfterStartupTaskUtils.setStartupComplete();

                    PartnerBrowserCustomizations.getInstance()
                            .setOnInitializeAsyncFinished(
                                    () -> {
                                        HomepageManager homepageManager =
                                                HomepageManager.getInstance();
                                        GURL homepageGurl = homepageManager.getHomepageGurl();
                                        LaunchMetrics.recordHomePageLaunchMetrics(
                                                homepageManager.isHomepageEnabled(),
                                                UrlUtilities.isNtpUrl(homepageGurl),
                                                homepageGurl);
                                    });

                    ShareImageFileUtils.clearSharedImages();

                    SelectFileDialog.clearCapturedCameraFiles();

                    if (ChannelsUpdater.getInstance().shouldUpdateChannels()) {
                        initChannelsAsync();
                    }
                });

        tasks.add(
                () -> {
                    // Clear notifications that existed when Chrome was last killed.
                    MediaCaptureNotificationServiceImpl.clearMediaNotifications();
                    BluetoothNotificationManager.clearBluetoothNotifications(
                            BluetoothNotificationService.class);
                    UsbNotificationManager.clearUsbNotifications(UsbNotificationService.class);

                    startBindingManagementIfNeeded();

                    recordKeyboardLocaleUma();
                });

        tasks.add(() -> LocaleManager.getInstance().recordStartupMetrics());

        tasks.add(() -> HomepageManager.getInstance().recordHomepageLocationTypeIfEnabled());

        // Record the saved restore state in a histogram
        tasks.add(ChromeBackupAgentImpl::recordRestoreHistogram);

        tasks.add(RevenueStats::getInstance);

        tasks.add(
                () -> {
                    mDevToolsServer = new DevToolsServer(DEV_TOOLS_SERVER_SOCKET_PREFIX);
                    mDevToolsServer.setRemoteDebuggingEnabled(
                            true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);
                });

        tasks.add(() -> BackgroundTaskSchedulerFactory.getScheduler().doMaintenance());

        tasks.add(MediaViewerUtils::updateMediaLauncherActivityEnabled);

        tasks.add(
                ChromeApplicationImpl.getComponent().resolveClearDataDialogResultRecorder()
                        ::makeDeferredRecordings);
        tasks.add(WebApkUninstallTracker::runDeferredTasks);

        tasks.add(OfflineContentAvailabilityStatusProvider::getInstance);
        tasks.add(() -> EnterpriseInfo.getInstance().logDeviceEnterpriseInfo());
        tasks.add(TosDialogBehaviorSharedPrefInvalidator::refreshSharedPreferenceIfTosSkipped);
        tasks.add(OfflineMeasurementsBackgroundTask::clearPersistedDataFromPrefs);
        tasks.add(
                () -> {
                    GlobalAppLocaleController.getInstance().maybeSetupLocaleManager();
                    GlobalAppLocaleController.getInstance().recordOverrideLanguageMetrics();
                });
        tasks.add(PersistedTabData::onDeferredStartup);

        // Asynchronously query system accessibility state so it is ready for clients.
        tasks.add(AccessibilityState::initializeOnStartup);
        tasks.add(TabPersistentStore::onDeferredStartup);
    }

    /**
     * Adds all the deferred startup tasks that should be called exactly once for the lifetime of a
     * profile.
     *
     * @param profile The profile triggering the startup tasks.
     * @param tasks The list where new tasks should be added.
     */
    @CallSuper
    protected void addPerProfileStartupDeferredTasks(Profile profile, List<Runnable> tasks) {
        // TODO(crbug.com/40254448): Determine how IncognitoTabLauncher should support multiple
        // concurrent profiles. Currently, the enabled state will change based on the Profile
        // initialization order.
        tasks.add(() -> IncognitoTabLauncher.updateComponentEnabledState(profile));

        // Initialize the SigninChecker.
        tasks.add(() -> SigninCheckerProvider.get(profile));

        tasks.add(
                () -> {
                    OptimizationGuideBridge optimizationGuideBridge =
                            OptimizationGuideBridgeFactory.getForProfile(profile);
                    if (optimizationGuideBridge != null) {
                        // OptimizationTypes which we give a guarantee will be registered when we
                        // pass the onDeferredStartup() signal to OptimizationGuide.
                        optimizationGuideBridge.registerOptimizationTypes(
                                Arrays.asList(HintsProto.OptimizationType.PRICE_TRACKING));
                        optimizationGuideBridge.onDeferredStartup();
                    }
                    // TODO(crbug.com/40236066) Move to PersistedTabData.onDeferredStartup
                    if (PriceTrackingFeatures.isPriceTrackingEligible(profile)) {
                        ShoppingPersistedTabData.onDeferredStartup();
                    }
                });
    }

    private void initChannelsAsync() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> ChannelsUpdater.getInstance().updateChannels());
    }

    private void initAsyncDiskTask() {
        new AsyncTask<Void>() {
            /**
             * The threshold after which it's no longer appropriate to try to attach logcat output
             * to a minidump file.
             * Note: This threshold of 12 hours was chosen fairly imprecisely, based on the
             * following intuition: On the one hand, Chrome can only access its own logcat output,
             * so the most recent lines should be relevant when available. On a typical device,
             * multiple hours of logcat output are available. On the other hand, it's important to
             * provide an escape hatch in case the logcat extraction code itself crashes, as
             * described in the doesCrashMinidumpNeedLogcat() documentation. Since this is a fairly
             * small and relatively frequently-executed piece of code, crashes are expected to be
             * unlikely; so it's okay for the escape hatch to be hard to use -- it's intended as an
             * extreme last resort.
             */
            private static final long LOGCAT_RELEVANCE_THRESHOLD_IN_HOURS = 12;

            @Override
            protected Void doInBackground() {
                try {
                    TraceEvent.begin("ChromeBrowserInitializer.onDeferredStartup.doInBackground");
                    initCrashReporting();

                    // Initialize the WebappRegistry if it's not already initialized. Must be in
                    // async task due to shared preferences disk access on N.
                    WebappRegistry.getInstance();

                    // Force a widget refresh in order to wake up any possible zombie widgets.
                    // This is needed to ensure the right behavior when the process is suddenly
                    // killed.
                    BookmarkWidgetProvider.refreshAllWidgets();

                    removeSnapshotDatabase();

                    // Warm up all web app shared prefs. This must be run after the WebappRegistry
                    // instance is initialized.
                    WebappRegistry.warmUpSharedPrefs();

                    PackageMetrics.recordPackageStats();
                    return null;
                } finally {
                    TraceEvent.end("ChromeBrowserInitializer.onDeferredStartup.doInBackground");
                }
            }

            @Override
            protected void onPostExecute(Void params) {
                // Must be run on the UI thread after the WebappRegistry has been completely warmed.
                WebappRegistry.getInstance().unregisterOldWebapps(System.currentTimeMillis());
            }

            /**
             * Initializes the crash reporting system. More specifically, enables the crash
             * reporting system if it is user-permitted, and initiates uploading of any pending
             * crash reports. Also updates some UMA metrics and performs cleanup in the local crash
             * minidump storage directory.
             */
            private void initCrashReporting() {
                // Crash reports can be uploaded as part of a background service even while the main
                // Chrome activity is not running, and hence regular metrics reporting is not
                // possible. Instead, metrics are temporarily written to prefs; export those prefs
                // to UMA metrics here.
                MinidumpUploadServiceImpl.storeBreakpadUploadStatsInUma(
                        CrashUploadCountStore.getInstance());

                // Likewise, this is a good time to process and clean up any pending or stale crash
                // reports left behind by previous runs.
                CrashFileManager crashFileManager =
                        new CrashFileManager(ContextUtils.getApplicationContext().getCacheDir());
                crashFileManager.cleanOutAllNonFreshMinidumpFiles();

                // ANR collection is only available on R+.
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    crashFileManager.collectAndWriteAnrs();
                }
                // Next, identify any minidumps that lack logcat output, and are too old to add
                // logcat output to. Mark these as ready for upload. If there is a fresh minidump
                // that still needs logcat output to be attached, stash it for now.
                File minidumpMissingLogcat = processMinidumpsSansLogcat(crashFileManager);

                // Now, upload all pending crash reports that are not still in need of logcat data.
                File[] minidumps =
                        crashFileManager.getMinidumpsReadyForUpload(
                                MinidumpUploadServiceImpl.MAX_TRIES_ALLOWED);
                if (minidumps.length > 0) {
                    Log.i(
                            TAG,
                            "Attempting to upload %d accumulated crash dumps.",
                            minidumps.length);
                    MinidumpUploadServiceImpl.scheduleUploadJob();
                }

                // Finally, if there is a minidump that still needs logcat output to be attached, do
                // so now. Note: It's important to do this strictly after calling
                // |crashFileManager.getMinidumpsReadyForUpload()|. Otherwise, there is a race
                // between appending the logcat and getting the results from that call, as the
                // minidump will be renamed to be a valid file for upload upon logcat extraction
                // success.
                if (minidumpMissingLogcat != null) {
                    // Note: When using the JobScheduler API to schedule uploads, this call might
                    // result in a duplicate request to schedule minidump uploads -- if the call
                    // succeeds, and there were also other pending minidumps found above. This is
                    // fine; the job scheduler is robust to such duplicate calls.
                    AsyncTask.THREAD_POOL_EXECUTOR.execute(
                            new LogcatExtractionRunnable(minidumpMissingLogcat));
                }
            }

            /**
             * Process all pending minidump files that lack logcat output. As a simplifying
             * assumption, assume that logcat output would only be relevant to the most recent
             * pending minidump, if there are multiple. As of Chrome version 58, about 50% of
             * startups that had *any* pending minidumps had at least one pending minidump without
             * any logcat output. About 5% had multiple minidumps without any logcat output.
             *
             * TODO(isherman): This is the simplest approach to resolving the complexity of
             * correctly attributing logcat output to the correct crash. However, it would be better
             * to attach logcat output to each minidump file that lacks it, if the relevant output
             * is still available. We can look at timestamps to correlate logcat lines with the
             * minidumps they correspond to.
             *
             * @return A single fresh minidump that should have logcat attached to it, or null if no
             *     such minidump exists.
             */
            private File processMinidumpsSansLogcat(CrashFileManager crashFileManager) {
                File[] minidumpsSansLogcat = crashFileManager.getMinidumpsSansLogcat();

                // If there are multiple minidumps present that are missing logcat output, only
                // append it to the most recent one. Upload the rest as-is.
                if (minidumpsSansLogcat.length > 1) {
                    for (int i = 1; i < minidumpsSansLogcat.length; ++i) {
                        CrashFileManager.trySetReadyForUpload(minidumpsSansLogcat[i]);
                    }
                }

                // Try to identify a single fresh minidump that should have logcat output appended
                // to it.
                if (minidumpsSansLogcat.length > 0) {
                    File mostRecentMinidumpSansLogcat = minidumpsSansLogcat[0];
                    if (doesCrashMinidumpNeedLogcat(mostRecentMinidumpSansLogcat)) {
                        return mostRecentMinidumpSansLogcat;
                    } else {
                        CrashFileManager.trySetReadyForUpload(mostRecentMinidumpSansLogcat);
                    }
                }
                return null;
            }

            /**
             * Returns whether or not it's appropriate to try to extract recent logcat output and
             * include that logcat output alongside the given {@param minidump} in a crash report.
             * Logcat output should only be extracted if (a) it hasn't already been extracted for
             * this minidump file, and (b) the minidump is fairly fresh. The freshness check is
             * important for two reasons: (1) First of all, it helps avoid including irrelevant
             * logcat output for a crash report. (2) Secondly, it provides an escape hatch that can
             * help circumvent a possible infinite crash loop, if the code responsible for
             * extracting and appending the logcat content is itself crashing. That is, the user can
             * wait 12 hours prior to relaunching Chrome, at which point this potential crash loop
             * would be circumvented.
             * @return Whether to try to include logcat output in the crash report corresponding to
             *     the given minidump.
             */
            private boolean doesCrashMinidumpNeedLogcat(File minidump) {
                if (!CrashFileManager.isMinidumpSansLogcat(minidump.getName())) return false;

                long ageInMillis = new Date().getTime() - minidump.lastModified();
                long ageInHours = ageInMillis / DateUtils.HOUR_IN_MILLIS;
                return ageInHours < LOGCAT_RELEVANCE_THRESHOLD_IN_HOURS;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Deletes the snapshot database which is no longer used because the feature has been removed
     * in Chrome M41.
     */
    @WorkerThread
    private void removeSnapshotDatabase() {
        synchronized (SNAPSHOT_DATABASE_LOCK) {
            SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
            if (!prefs.readBoolean(ChromePreferenceKeys.SNAPSHOT_DATABASE_REMOVED, false)) {
                ContextUtils.getApplicationContext().deleteDatabase(SNAPSHOT_DATABASE_NAME);
                prefs.writeBoolean(ChromePreferenceKeys.SNAPSHOT_DATABASE_REMOVED, true);
            }
        }
    }

    private void startBindingManagementIfNeeded() {
        // Moderate binding doesn't apply to low end devices.
        if (SysUtils.isLowEndDevice()) return;
        ChildProcessLauncherHelper.startBindingManagement(ContextUtils.getApplicationContext());
    }

    @SuppressWarnings("deprecation") // InputMethodSubtype.getLocale() deprecated in API 24
    private void recordKeyboardLocaleUma() {
        InputMethodManager imm =
                (InputMethodManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.INPUT_METHOD_SERVICE);
        List<InputMethodInfo> ims = imm.getEnabledInputMethodList();
        ArrayList<String> uniqueLanguages = new ArrayList<>();
        for (InputMethodInfo method : ims) {
            List<InputMethodSubtype> submethods =
                    imm.getEnabledInputMethodSubtypeList(method, true);
            for (InputMethodSubtype submethod : submethods) {
                if (submethod.getMode().equals("keyboard")) {
                    String language = submethod.getLocale().split("_")[0];
                    if (!uniqueLanguages.contains(language)) {
                        uniqueLanguages.add(language);
                    }
                }
            }
        }
        RecordHistogram.recordCount1MHistogram("InputMethod.ActiveCount", uniqueLanguages.size());
    }
}
