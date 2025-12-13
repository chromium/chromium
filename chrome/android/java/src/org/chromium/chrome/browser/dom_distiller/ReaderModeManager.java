// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.embedder_support.util.UrlConstants.DISTILLER_SCHEME;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.RequiredCallback;
import org.chromium.base.SysUtils;
import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.dom_distiller.mojom.Theme;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedHashSet;
import java.util.function.Supplier;

/**
 * Manages UI effects for reader mode including hiding and showing the reader mode and reader mode
 * preferences toolbar icon and hiding the browser controls when a reader mode page has finished
 * loading.
 */
@NullMarked
public class ReaderModeManager extends EmptyTabObserver
        implements UserData, NightModeStateProvider.Observer {

    // LINT.IfChange(DomDistillerEntryPoint)

    /**
     * Possible entry-points into reader mode. These entries are used to record an UMA histogram,
     * don't reorder to change existing values.
     */
    @IntDef({
        EntryPoint.UNKNOWN,
        EntryPoint.MESSAGE,
        EntryPoint.APP_MENU,
        EntryPoint.TOOLBAR_BUTTON,
        EntryPoint.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryPoint {
        int UNKNOWN = 0;

        /** The user opened reader mode through an app message. */
        int MESSAGE = 1;

        /** The user opened reader mode through the app menu. */
        int APP_MENU = 2;

        /** The user opened reader mode through the toolbar button. */
        int TOOLBAR_BUTTON = 3;

        int MAX_VALUE = TOOLBAR_BUTTON;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:DomDistillerEntryPoint)

    /** Possible tab types for reader mode entry points. */
    @IntDef({
        EntryPointTabType.REGULAR_TAB,
        EntryPointTabType.CUSTOM_TAB,
        EntryPointTabType.INCOGNITO_TAB,
        EntryPointTabType.INCOGNITO_CUSTOM_TAB,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryPointTabType {
        int REGULAR_TAB = 0;
        int CUSTOM_TAB = 1;
        int INCOGNITO_TAB = 2;
        int INCOGNITO_CUSTOM_TAB = 3;
    }

    /** Possible states that the distiller can be in on a web page. */
    @IntDef({
        DistillationStatus.POSSIBLE,
        DistillationStatus.NOT_POSSIBLE,
        DistillationStatus.STARTED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DistillationStatus {
        /** POSSIBLE means reader mode can be entered. */
        int POSSIBLE = 0;

        /** NOT_POSSIBLE means reader mode cannot be entered. */
        int NOT_POSSIBLE = 1;

        /** STARTED means reader mode is currently in reader mode. */
        int STARTED = 2;
    }

    /** The key to access this object from a {@Tab}. */
    public static final Class<ReaderModeManager> USER_DATA_KEY = ReaderModeManager.class;

    /** The intent extra that indicates origin from Reader Mode */
    public static final String EXTRA_READER_MODE_PARENT =
            "org.chromium.chrome.browser.dom_distiller.EXTRA_READER_MODE_PARENT";

    /** Histogram name for the state of the reader mode accessibility setting. */
    public static final String ACCESSIBILITY_SETTING_HISTOGRAM =
            "DomDistiller.Android.OnDistillableResult.AccessibilitySettingEnabled";

    /** Histogram name for the end distillability result. */
    public static final String PAGE_DISTILLATION_RESULT_HISTOGRAM =
            "DomDistiller.Android.OnDistillableResult.PageDistillationResult";

    /**
     * Field param for MTB-CCT indicating that the fallback UI for reader mode is overflow menu. If
     * false, the fallback UI will be Message.
     */
    public static final String CPA_FALLBACK_MENU_PARAM = "reader_mode_fallback_menu";

    /** The url of the last page visited if the last page was reader mode page. Otherwise null. */
    private @Nullable GURL mReaderModePageUrl;

    /** Whether the current web page was distillable or not has been determined. */
    private boolean mIsCurrentPageDistillationStatusDetermined;

    /** The WebContentsObserver responsible for updates to the distillation status of the tab. */
    private @Nullable WebContentsObserver mWebContentsObserver;

    /** The distillation status of the tab. */
    @DistillationStatus private int mDistillationStatus;

    /** If the prompt was dismissed by the user. */
    private boolean mIsDismissed;

    /**
     * The URL that distiller is using for this tab. This is used to check if a result comes back
     * from distiller and the user has already loaded a new URL.
     */
    private @Nullable GURL mDistillerUrl;

    /** Used to flag that the prompt was shown and recorded by UMA. */
    private boolean mShowPromptRecorded;

    /** Whether or not the current tab is a Reader Mode page. */
    private boolean mIsViewingReaderModePage;

    /** The time that the user started viewing Reader Mode content. */
    private long mViewStartTimeMs;

    /** The distillability observer attached to the tab. */
    private @Nullable DistillabilityObserver mDistillabilityObserver;

    /** Whether this manager and tab have been destroyed. */
    private boolean mIsDestroyed;

    /** The tab this manager is attached to. */
    private final Tab mTab;

    /** The supplier of MessageDispatcher to display the message. */
    private final Supplier<@Nullable MessageDispatcher> mMessageDispatcherSupplier;

    // Hold on to the InterceptNavigationDelegate that the custom tab uses.
    @Nullable InterceptNavigationDelegate mCustomTabNavigationDelegate;

    /** Whether the messages UI was requested for a navigation. */
    private boolean mMessageRequestedForNavigation;

    // Record the sites which users refuse to view in reader mode.
    // If the size is larger than the capacity, remove the earliest added site first.
    private static final LinkedHashSet<Integer> sMutedSites = new LinkedHashSet<>();
    private static final int MAX_SIZE_OF_DECLINED_SITES = 100;

    /** Whether the message ui is being shown or has already been shown. */
    private boolean mMessageShown;

    /** Property Model of Reader mode message. */
    private @Nullable PropertyModel mMessageModel;

    /** Whether the reader mode button is currently being shown on the toolbar. */
    private boolean mIsReaderModeButtonShowingOnToolbar;

    /** Whether the prompt was put on hold until CPA is ready should be displayed in the end. */
    private boolean mShouldRestorePrompt;

    // Whether the manager has been notified that a contextual page action has been shown for the
    // current navigation.
    private boolean mHasBeenNotifiedOfCpa;

    // Used to keep track of the browser theme.
    private final NightModeStateProvider mNightModeStateProvider;

    ReaderModeManager(Tab tab, Supplier<@Nullable MessageDispatcher> messageDispatcherSupplier) {
        super();
        mTab = tab;
        mTab.addObserver(this);
        mMessageDispatcherSupplier = messageDispatcherSupplier;

        mNightModeStateProvider = GlobalNightModeStateProviderHolder.getInstance();
        mNightModeStateProvider.addObserver(this);
    }

    /**
     * Create an instance of the {@link ReaderModeManager} for the provided tab.
     * @param tab The tab that will have a manager instance attached to it.
     */
    public static void createForTab(Tab tab) {
        tab.getUserDataHost()
                .setUserData(
                        USER_DATA_KEY,
                        new ReaderModeManager(
                                tab, () -> MessageDispatcherProvider.from(tab.getWindowAndroid())));
    }

    /** Clear the status map and references to other objects. */
    @Override
    public void destroy() {
        if (mWebContentsObserver != null) mWebContentsObserver.observe(null);
        mHasBeenNotifiedOfCpa = false;
        mIsReaderModeButtonShowingOnToolbar = false;
        mIsDestroyed = true;
        mNightModeStateProvider.removeObserver(this);
    }

    // NightModeStateProvider.Observer implementation.
    @Override
    public void onNightModeStateChanged() {
        // Update the browser theme stored within DistilledPagePrefs.
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return;
        setDefaultThemeAsBrowserTheme(webContents);
    }

    // TabObserver implementation.
    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
        // If a distiller URL was loaded and this is a custom tab, add a navigation
        // handler to bring any navigations back to the main chrome activity.
        Activity activity = TabUtils.getActivity(tab);
        int uiType = CustomTabsUiType.DEFAULT;
        if (activity != null && activity.getIntent().getExtras() != null) {
            uiType =
                    activity.getIntent()
                            .getExtras()
                            .getInt(CustomTabIntentDataProvider.EXTRA_UI_TYPE);
        }
        if (tab == null
                || uiType != CustomTabsUiType.READER_MODE
                || !DomDistillerUrlUtils.isDistilledPage(params.getUrl())) {
            return;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        mCustomTabNavigationDelegate =
                new InterceptNavigationDelegate() {
                    @Override
                    public void shouldIgnoreNavigation(
                            NavigationHandle navigationHandle,
                            GURL escapedUrl,
                            boolean hiddenCrossFrame,
                            boolean isSandboxedFrame,
                            boolean shouldRunAsync,
                            RequiredCallback<Boolean> resultCallback) {
                        if (DomDistillerUrlUtils.isDistilledPage(navigationHandle.getUrl())
                                || navigationHandle.isExternalProtocol()) {
                            resultCallback.onResult(false);
                            return;
                        }

                        Intent returnIntent =
                                new Intent(Intent.ACTION_VIEW, Uri.parse(escapedUrl.getSpec()));
                        assertNonNull(activity);
                        returnIntent.setClassName(activity, ChromeLauncherActivity.class.getName());

                        // Set the parent ID of the tab to be created.
                        returnIntent.putExtra(
                                EXTRA_READER_MODE_PARENT,
                                IntentUtils.safeGetIntExtra(
                                        activity.getIntent(),
                                        EXTRA_READER_MODE_PARENT,
                                        Tab.INVALID_TAB_ID));

                        activity.startActivity(returnIntent);
                        activity.finish();
                        resultCallback.onResult(true);
                    }
                };

        DomDistillerTabUtils.setInterceptNavigationDelegate(
                mCustomTabNavigationDelegate, webContents);
    }

    @Override
    public void onShown(Tab shownTab, @TabSelectionType int type) {
        // If the reader mode prompt was dismissed, stop here.
        if (mIsDismissed) return;

        mHasBeenNotifiedOfCpa = false;
        mIsReaderModeButtonShowingOnToolbar = false;
        mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
        mDistillerUrl = shownTab.getUrl();

        if (mDistillabilityObserver == null) {
            setDistillabilityObserver(shownTab);
        }

        if (DomDistillerUrlUtils.isDistilledPage(shownTab.getUrl()) && !mIsViewingReaderModePage) {
            onStartedReaderMode();
        }

        // Make sure there is a WebContentsObserver on this tab's WebContents.
        if (mWebContentsObserver == null && mTab.getWebContents() != null) {
            mWebContentsObserver = createWebContentsObserver();
        }
        tryShowingPrompt(/* resetRestorePrompt= */ true);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        boolean isCustomTabDistillation = shouldDistillInCustomTab();
        boolean isHiddenTabCustomTab = tab.isCustomTab();
        // When custom tab distillation is first triggered, this onHidden function will trigger for
        // the non-CCT tab. We want to ensure that we do not trigger onExitReaderMode when starting
        // up reader mode in CCT. Subsequent onHidden calls when in CCT experience will have
        // isHiddenTabCustomTab to be true.
        if (isCustomTabDistillation && !isHiddenTabCustomTab) {
            return;
        } else if (mIsViewingReaderModePage) {
            onExitReaderMode();
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        mHasBeenNotifiedOfCpa = false;
        mIsReaderModeButtonShowingOnToolbar = false;
        if (tab == null) return;

        // If the prompt was not shown for the previous navigation, record it now.
        if (!mShowPromptRecorded) {
            recordPromptVisibilityForNavigation(false);
        }
        if (mIsViewingReaderModePage) {
            onExitReaderMode();
        }
        if (mDistillabilityObserver != null) {
            var provider = TabDistillabilityProvider.get(tab);
            assumeNonNull(provider).removeObserver(mDistillabilityObserver);
        }

        removeTabState();
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    /** Clear the reader mode state for this manager. */
    private void removeTabState() {
        if (mWebContentsObserver != null) mWebContentsObserver.observe(null);
        mDistillationStatus = DistillationStatus.POSSIBLE;
        mIsDismissed = false;
        mMessageRequestedForNavigation = false;
        mDistillerUrl = null;
        mShowPromptRecorded = false;
        mIsViewingReaderModePage = false;
        mHasBeenNotifiedOfCpa = false;
        mIsReaderModeButtonShowingOnToolbar = false;
        mDistillabilityObserver = null;
    }

    @Override
    public void onContentChanged(Tab tab) {
        mHasBeenNotifiedOfCpa = false;
        mIsReaderModeButtonShowingOnToolbar = false;
        // If the content change was because of distiller switching web contents or Reader Mode has
        // already been dismissed for this tab do nothing.
        if (mIsDismissed && !DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) return;

        // If the tab state already existed, only reset the relevant data. Things like view duration
        // need to be preserved.
        mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
        mDistillerUrl = tab.getUrl();

        if (tab.getWebContents() != null) {
            mWebContentsObserver = createWebContentsObserver();
            setDefaultThemeAsBrowserTheme(tab.getWebContents());
            if (DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
                mDistillationStatus = DistillationStatus.STARTED;
                mReaderModePageUrl = tab.getUrl();
            }
        }
    }

    /** A notification that the user started viewing Reader Mode. */
    private void onStartedReaderMode() {
        mIsViewingReaderModePage = true;
        mViewStartTimeMs = SystemClock.elapsedRealtime();

        assertNonNull(mTab.getWebContents());
        new UkmRecorder(mTab.getWebContents(), "DomDistiller.Android.ReaderModeShown")
                .addBooleanMetric("Shown")
                .record();
        ReaderModeMetrics.recordOnStartedReaderMode();
    }

    /**
     * A notification that the user is no longer viewing Reader Mode. This could be because of a
     * navigation away from the page, switching tabs, or closing the browser.
     */
    private void onExitReaderMode() {
        mIsViewingReaderModePage = false;
        ReaderModeMetrics.recordReaderModeViewDuration(
                SystemClock.elapsedRealtime() - mViewStartTimeMs);
        ReaderModeMetrics.recordOnStoppedReaderMode();
    }

    /**
     * Record if the prompt became visible on the current page. This can be overridden for testing.
     * @param visible If the prompt was visible at any time.
     */
    private void recordPromptVisibilityForNavigation(boolean visible) {
        RecordHistogram.recordBooleanHistogram("DomDistiller.ReaderShownForPageLoad", visible);
    }

    /** A notification that the prompt was dismissed without being used. */
    private void onClosed() {
        mIsDismissed = true;
    }

    private WebContentsObserver createWebContentsObserver() {
        return new WebContentsObserver(mTab.getWebContents()) {
            /** Whether or not the previous navigation should be removed. */
            private boolean mShouldRemovePreviousNavigation;

            /** The index of the last committed distiller page in history. */
            private int mLastDistillerPageIndex;

            @Override
            public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                if (navigation.isSameDocument()) return;

                // Reader Mode should not pollute the navigation stack. To avoid this, watch for
                // navigations and prepare to remove any that are "chrome-distiller" urls.
                var webContents = assumeNonNull(getWebContents());
                NavigationController controller = webContents.getNavigationController();
                int index = controller.getLastCommittedEntryIndex();
                NavigationEntry entry = controller.getEntryAtIndex(index);

                if (entry != null && DomDistillerUrlUtils.isDistilledPage(entry.getUrl())) {
                    mShouldRemovePreviousNavigation = true;
                    mLastDistillerPageIndex = index;
                }

                if (mIsDestroyed) return;

                mDistillerUrl = navigation.getUrl();
                if (DomDistillerUrlUtils.isDistilledPage(navigation.getUrl())) {
                    mDistillationStatus = DistillationStatus.STARTED;
                    mReaderModePageUrl = navigation.getUrl();
                }
            }

            @Override
            public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                // TODO(cjhopman): This should possibly ignore navigations that replace the entry
                // (like those from history.replaceState()).
                if (!navigation.hasCommitted() || navigation.isSameDocument()) {
                    return;
                }

                if (mShouldRemovePreviousNavigation) {
                    mShouldRemovePreviousNavigation = false;
                    var webContents = assumeNonNull(getWebContents());
                    NavigationController controller = webContents.getNavigationController();
                    if (controller.getEntryAtIndex(mLastDistillerPageIndex) != null) {
                        controller.removeEntryAtIndex(mLastDistillerPageIndex);
                    }
                }

                if (mIsDestroyed) return;

                mDistillationStatus = DistillationStatus.POSSIBLE;
                if (mReaderModePageUrl == null
                        || !navigation
                                .getUrl()
                                .equals(
                                        DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                                                mReaderModePageUrl))) {
                    mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
                    mIsCurrentPageDistillationStatusDetermined = false;
                }
                mReaderModePageUrl = null;

                if (mDistillationStatus == DistillationStatus.POSSIBLE) {
                    mHasBeenNotifiedOfCpa = false;
                    mIsReaderModeButtonShowingOnToolbar = false;
                    tryShowingPrompt(/* resetRestorePrompt= */ true);
                }
            }

            @Override
            public void navigationEntryCommitted(LoadCommittedDetails details) {
                if (mIsDestroyed) return;
                // Reset closed state of reader mode in this tab once we know a navigation is
                // happening.
                mIsDismissed = false;
                mHasBeenNotifiedOfCpa = false;
                mIsReaderModeButtonShowingOnToolbar = false;
                mMessageRequestedForNavigation = false;

                // If the prompt was not shown for the previous navigation, record it now.
                if (mTab != null && !mTab.isNativePage() && !mTab.isBeingRestored()) {
                    recordPromptVisibilityForNavigation(false);
                }
                mShowPromptRecorded = false;

                if (mTab != null
                        && !DomDistillerUrlUtils.isDistilledPage(mTab.getUrl())
                        && mIsViewingReaderModePage) {
                    onExitReaderMode();
                }
            }
        };
    }

    /**
     * Try showing the reader mode prompt.
     *
     * @param resetRestorePrompt Whether the flag |mShouldRestorePrompt| should be reset at the
     *     beginning before setting it conditionally. This is expected to be {@code true} for all
     *     the call sites except {@link #onContextualPageActionShown()} which should use the current
     *     state of the flag to decide what to do.
     * @return Whether the prompt request was successfully processed.
     */
    @VisibleForTesting
    boolean tryShowingPrompt(boolean resetRestorePrompt) {
        if (resetRestorePrompt) mShouldRestorePrompt = false;

        if (mTab == null || mTab.getWebContents() == null) return false;

        // This prompt should only be shown on incognito or custom tabs, in other cases we'll show a
        // toolbar button (contextual page action) instead.
        if (!shouldUseReaderModeMessages(mTab)) return false;

        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        boolean usingRequestDesktopSite =
                mTab.getWebContents().getNavigationController().getUseDesktopUserAgent()
                        && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        if (usingRequestDesktopSite
                || mDistillationStatus != DistillationStatus.POSSIBLE
                || mIsDismissed) {
            return false;
        }

        if (isSiteAlreadyShown()) return false;

        MessageDispatcher messageDispatcher = mMessageDispatcherSupplier.get();
        if (messageDispatcher != null) {
            if (!mMessageRequestedForNavigation) {
                // If feature is disabled, reader mode message ui is only shown once per tab.
                if (mMessageShown) {
                    return false;
                }

                if (shouldSuppressForCpa()) {
                    return false;
                }

                showReaderModeMessage(messageDispatcher);
                mMessageShown = true;
            }
            mMessageRequestedForNavigation = true;
        }
        return true;
    }

    private boolean isSiteAlreadyShown() {
        return mDistillerUrl != null && sMutedSites.contains(urlToHash(mDistillerUrl));
    }

    private boolean shouldSuppressForCpa() {
        if (mTab.isCustomTab() && ChromeFeatureList.sCctAdaptiveButton.isEnabled()) {
            // If the manager hasn't been notified of the CPA yet, don't show the prompt for now.
            // Later it will be shown if CPA is determined to be hidden.
            if (!mHasBeenNotifiedOfCpa) {
                mShouldRestorePrompt = true;
                return true;
            }

            // Do not proceed to show Message UI if CPA is shown, or the fallback UI will be in
            // the overflow menu.
            if (mIsReaderModeButtonShowingOnToolbar
                    || ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                            CPA_FALLBACK_MENU_PARAM,
                            false)) {
                return true;
            }
        }
        return false;
    }

    private void showReaderModeMessage(MessageDispatcher messageDispatcher) {
        if (mMessageModel != null) {
            // It is safe to dismiss a message which has been dismissed previously.
            messageDispatcher.dismissMessage(mMessageModel, DismissReason.DISMISSED_BY_FEATURE);
        }
        Resources resources = mTab.getContext().getResources();
        // Save url for #onMessageDismissed. mDistillerUrl may have been changed and became
        // different from the url when message is enqueued.
        GURL url = mDistillerUrl;
        mMessageModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.READER_MODE)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(R.string.reader_mode_message_title))
                        .with(
                                MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_mobile_friendly_24dp)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.reader_mode_message_button))
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    activateReaderMode(EntryPoint.MESSAGE);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (reason) -> onMessageDismissed(url, reason))
                        .build();
        assertNonNull(mTab.getWebContents());
        messageDispatcher.enqueueMessage(
                mMessageModel, mTab.getWebContents(), MessageScopeType.NAVIGATION, false);
    }

    private void onMessageDismissed(@Nullable GURL url, @DismissReason int dismissReason) {
        mMessageModel = null;
        if (dismissReason == DismissReason.GESTURE) {
            onClosed();
        }

        if (dismissReason != DismissReason.PRIMARY_ACTION) {
            addUrlToMutedSites(url);
        }
    }

    private void addUrlToMutedSites(@Nullable GURL url) {
        if (url == null) return;
        sMutedSites.add(urlToHash(url));
        while (sMutedSites.size() > MAX_SIZE_OF_DECLINED_SITES) {
            int v = sMutedSites.iterator().next();
            sMutedSites.remove(v);
        }
    }

    private void removeUrlFromMutedSites(@Nullable GURL url) {
        if (url == null) return;
        sMutedSites.remove(urlToHash(url));
    }

    public void activateReaderMode(@EntryPoint int entryPoint) {
        // Contextual page action buttons can't be dismissed, instead we consider a shown but unused
        // button as "dismissed" and mute the site on setReaderModeUiShown(). When the button gets
        // clicked we un-mute the site to prevent the rate limiting logic from showing the CPA
        // button for this site on other tabs.
        removeUrlFromMutedSites(mDistillerUrl);

        if (shouldDistillInCustomTab()) {
            distillInCustomTab();
        } else {
            navigateToReaderMode();
        }
        RecordUserAction.record("MobileReaderModeActivated");
        boolean isCpaFallbackMessage =
                !ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_ADAPTIVE_BUTTON, CPA_FALLBACK_MENU_PARAM, false);
        if (mHasBeenNotifiedOfCpa && !mIsReaderModeButtonShowingOnToolbar && isCpaFallbackMessage) {
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTab.AdaptiveToolbarButton.FallbackUi",
                    AdaptiveToolbarButtonVariant.READER_MODE,
                    AdaptiveToolbarButtonVariant.MAX_VALUE);
        }
        recordEntryPointMetric(entryPoint);
    }

    private void recordEntryPointMetric(@EntryPoint int entryPoint) {
        @EntryPointTabType int entryPointTabType;
        boolean isIncognito = mTab.isIncognito();
        boolean isOrWillBeCustomTab = mTab.isCustomTab() || shouldDistillInCustomTab();
        Activity activity = TabUtils.getActivity(mTab);
        Intent intent = (activity != null) ? activity.getIntent() : null;
        // Incognito CCT does not return true when checking mTab.isIncognito().
        boolean isIncognitoCustomTab =
                (intent != null)
                        && IntentUtils.safeGetBooleanExtra(
                                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false)
                        && isOrWillBeCustomTab;

        if (isIncognitoCustomTab) {
            entryPointTabType = EntryPointTabType.INCOGNITO_CUSTOM_TAB;
        } else if (isOrWillBeCustomTab) {
            entryPointTabType = EntryPointTabType.CUSTOM_TAB;
        } else if (isIncognito) {
            entryPointTabType = EntryPointTabType.INCOGNITO_TAB;
        } else {
            entryPointTabType = EntryPointTabType.REGULAR_TAB;
        }
        ReaderModeMetrics.recordReaderModeEntryPoint(entryPoint, entryPointTabType);
    }

    private boolean shouldDistillInCustomTab() {
        return !SysUtils.isLowEndDevice() && !shouldUseRegularTabsForDistillation();
    }

    private boolean shouldUseRegularTabsForDistillation() {
        return DomDistillerFeatures.sReaderModeDistillInApp.isEnabled();
    }

    /** Navigate the current tab to a Reader Mode URL. */
    @VisibleForTesting
    void navigateToReaderMode() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return;

        onStartedReaderMode();

        FullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager != null) {
            // Make sure to exit fullscreen mode before navigating.
            fullscreenManager.onExitFullscreen(mTab);
        }

        // RenderWidgetHostViewAndroid hides the controls after transitioning to reader mode.
        // See the long history of the issue in https://crbug.com/825765, https://crbug.com/853686,
        // https://crbug.com/861618, https://crbug.com/922388.
        // TODO(pshmakov): find a proper solution instead of this workaround.
        BrowserControlsVisibilityManager browserControlsVisibilityManager =
                getBrowserControlsVisibilityManager();
        if (browserControlsVisibilityManager != null) {
            browserControlsVisibilityManager.getBrowserVisibilityDelegate().showControlsTransient();
        }

        DomDistillerTabUtils.distillCurrentPageAndViewIfSuccessful(
                webContents,
                (success) -> {
                    // If successful, or any of the dependencies needed to show a bottom sheet
                    // aren't available then return early.
                    if (success || mTab == null || mTab.getWindowAndroid() == null) {
                        return;
                    }
                    SnackbarManager snackbarManager =
                            SnackbarManagerProvider.from(mTab.getWindowAndroid());
                    if (snackbarManager == null) {
                        return;
                    }

                    snackbarManager.showSnackbar(
                            Snackbar.make(
                                            mTab.getContext()
                                                    .getString(
                                                            R.string
                                                                    .reader_mode_unavailable_snackbar_message),
                                            new SnackbarManager.SnackbarController() {},
                                            Snackbar.TYPE_NOTIFICATION,
                                            Snackbar.UMA_UNKNOWN)
                                    .setAction(
                                            mTab.getContext().getString(R.string.chrome_dismiss),
                                            null)
                                    // Important to get the full message displayed to the user.
                                    .setDefaultLines(false));
                });
    }

    /**
     * Ensure DistilledPagePrefs is updated with the theme of the browser. It will default to the
     * browser theme if user has has not explicitly set a reader mode theme.
     */
    private void setDefaultThemeAsBrowserTheme(WebContents webContents) {
        DistilledPagePrefs distilledPagePrefs =
                DomDistillerServiceFactory.getForProfile(Profile.fromWebContents(webContents))
                        .getDistilledPagePrefs();

        @Theme.EnumType
        int theme = mNightModeStateProvider.isInNightMode() ? Theme.DARK : Theme.LIGHT;
        distilledPagePrefs.setDefaultTheme(theme);
    }

    private @Nullable BrowserControlsManager getBrowserControlsManager() {
        return BrowserControlsManagerSupplier.getValueOrNullFrom(mTab.getWindowAndroid());
    }

    private @Nullable BrowserControlsVisibilityManager getBrowserControlsVisibilityManager() {
        return getBrowserControlsManager();
    }

    private @Nullable FullscreenManager getFullscreenManager() {
        BrowserControlsManager browserControlsManager = getBrowserControlsManager();
        return browserControlsManager == null
                ? null
                : browserControlsManager.getFullscreenManager();
    }

    private void distillInCustomTab() {
        Activity activity = TabUtils.getActivity(mTab);
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return;

        GURL url = webContents.getLastCommittedUrl();

        onStartedReaderMode();

        DomDistillerTabUtils.distillCurrentPage(webContents);

        String distillerUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                        DISTILLER_SCHEME, url.getSpec(), webContents.getTitle());

        assertNonNull(activity);
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setColorScheme(
                ColorUtils.inNightMode(activity)
                        ? CustomTabsIntent.COLOR_SCHEME_DARK
                        : CustomTabsIntent.COLOR_SCHEME_LIGHT);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setClassName(activity, CustomTabActivity.class.getName());

        // Customize items on menu as Reader Mode UI to show 'Find in page' and 'Preference' only.
        CustomTabIntentDataProvider.addReaderModeUiExtras(customTabsIntent.intent);

        // Add the parent ID as an intent extra for back button functionality.
        customTabsIntent.intent.putExtra(EXTRA_READER_MODE_PARENT, mTab.getId());

        // Use Incognito CCT if the source page is in Incognito mode.
        if (mTab.isIncognito()) {
            IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                    customTabsIntent.intent,
                    BrowserServicesIntentDataProvider.IncognitoCctCallerId.READER_MODE);
        }

        customTabsIntent.launchUrl(activity, Uri.parse(distillerUrl));
    }

    /**
     * Set the observer for updating reader mode status based on whether or not the page should be
     * viewed in reader mode.
     *
     * @param tabToObserve The tab to attach the observer to.
     */
    private void setDistillabilityObserver(final Tab tabToObserve) {
        mDistillabilityObserver =
                (tab, isDistillable, isLast, isMobileOptimized) -> {
                    // Make sure the page didn't navigate while waiting for a response.
                    if (!tab.getUrl().equals(mDistillerUrl)) return;
                    // Make sure the page distillation status hasn't already been determined.
                    if (mIsCurrentPageDistillationStatusDetermined) return;
                    // Make sure that reader mode messages infra should be used.
                    if (!shouldUseReaderModeMessages(tab)) return;

                    Pair<Boolean, Integer> result =
                            ReaderModeManager.computeDistillationStatus(
                                    tab, isDistillable, isMobileOptimized, isLast);
                    mIsCurrentPageDistillationStatusDetermined = result.first;
                    mDistillationStatus = result.second;
                    if (mIsCurrentPageDistillationStatusDetermined) {
                        mHasBeenNotifiedOfCpa = false;
                        mIsReaderModeButtonShowingOnToolbar = false;
                        tryShowingPrompt(/* resetRestorePrompt= */ true);
                    }
                };
        var provider = TabDistillabilityProvider.get(tabToObserve);
        assumeNonNull(provider).addObserver(mDistillabilityObserver);
    }

    // Navigates away from reader mode. This is intended for in-app distillation, not for CCT.
    public void hideReaderMode() {
        mTab.goBack();
        RecordUserAction.record("MobileReaderModeHidden");
    }

    /**
     * Returns whether reader mode should trigger through messages. This happens for CCTs and
     * incognito tabs.
     *
     * @param tab The tab where Reader Mode is active.
     * @return Whether reader mode should trigger through messages.
     */
    public static boolean shouldUseReaderModeMessages(Tab tab) {
        // Messages are explicitly disabled for in-app distillation.
        return !DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()
                && tab != null
                && (tab.isCustomTab() || tab.isIncognito());
    }

    /**
     * Gets the distillation status for the given arguments, and records metrics if distillability
     * has been fully determined.
     *
     * @param tab The {@link Tab} to determine distillability for.
     * @param isDistillable Whether the tab is considered distillable.
     * @param isMobileOptimized Whether the tab is considered optimized for mobile.
     * @param isLast Whether this is the last signal we'll get for the tab.
     * @returns A pair which contains: pair.first - Whether distillability has been fully
     *     determined. pair.second - The current distillation status.
     */
    public static Pair<Boolean, Integer> computeDistillationStatus(
            Tab tab, boolean isDistillable, boolean isMobileOptimized, boolean isLast) {
        // Compute if mobile friendly pages should be excluded for use in distillation status as
        // well as metrics recording.
        boolean shouldExcludeMobileFriendly = DomDistillerTabUtils.shouldExcludeMobileFriendly(tab);
        boolean excludeCurrentMobilePage = isMobileOptimized && shouldExcludeMobileFriendly;
        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        // TODO(crbug.com/405186704): Add histogram when RDS results in a RM exclusion.
        boolean excludeRequestDesktopSite =
                tab.getWebContents() != null
                        && tab.getWebContents().getNavigationController().getUseDesktopUserAgent()
                        && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        // Determine and store distillation status.
        @DistillationStatus int distillationStatus;
        if (isDistillable && !excludeCurrentMobilePage && !excludeRequestDesktopSite) {
            distillationStatus = DistillationStatus.POSSIBLE;
        } else {
            distillationStatus = DistillationStatus.NOT_POSSIBLE;
        }

        // If we get a positive distillation status, or a signal that this is the last distillation
        // signal we'll receive, record metrics and inform the user.
        if (distillationStatus == DistillationStatus.POSSIBLE || isLast) {
            RecordHistogram.recordBooleanHistogram(
                    ACCESSIBILITY_SETTING_HISTOGRAM,
                    DomDistillerTabUtils.isReaderModeAccessibilitySettingEnabled(tab.getProfile()));
            recordDistillationResult(
                    tab,
                    distillationStatus,
                    isDistillable,
                    excludeCurrentMobilePage,
                    excludeRequestDesktopSite);
            return new Pair<>(true, distillationStatus);
        }
        return new Pair<>(false, distillationStatus);
    }

    private int urlToHash(GURL url) {
        return url.getHost().hashCode();
    }

    @VisibleForTesting
    int getDistillationStatus() {
        return mDistillationStatus;
    }

    void muteSiteForTesting(GURL url) {
        sMutedSites.add(urlToHash(url));
    }

    void clearSavedSitesForTesting() {
        sMutedSites.clear();
    }

    /** @return Whether Reader mode and its new UI are enabled. */
    public static boolean isEnabled() {
        boolean enabled =
                CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_DOM_DISTILLER)
                        && !CommandLine.getInstance()
                                .hasSwitch(ChromeSwitches.DISABLE_READER_MODE_BOTTOM_BAR)
                        && DomDistillerTabUtils.isDistillerHeuristicsEnabled();
        return enabled;
    }

    /**
     * Determine if Reader Mode created the intent for a tab being created.
     * @param intent The Intent creating a new tab.
     * @return True whether the intent was created by Reader Mode.
     */
    public static boolean isReaderModeCreatedIntent(Intent intent) {
        int readerParentId =
                IntentUtils.safeGetIntExtra(
                        intent, ReaderModeManager.EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID);
        return readerParentId != Tab.INVALID_TAB_ID;
    }

    /**
     * Notify that a contextual page action was shown for the current tab and URL. Used when the
     * contextual page action UI is enabled to update the rate limiting logic and to suppress the
     * message prompt if the current tab is a CCT.
     *
     * @param showCpaButton Whether the CPA button can be shown in the UI
     * @param isReaderMode Whether the chose action is reader mode type.
     */
    public void onContextualPageActionShown(
            OneshotSupplier<Boolean> showCpaButton, boolean isReaderMode) {
        // If the feature is enabled and the tab is a custom tab, the manager should be aware if the
        // displayed contextual page action is the reader one. Once determined, #tryShowingPrompt
        // can successfully decide between showing a message prompt or suppressing it in favor of
        // the contextual page action's UI.
        if (ChromeFeatureList.sCctAdaptiveButton.isEnabled() && mTab.isCustomTab()) {
            mHasBeenNotifiedOfCpa = true;
            showCpaButton.runSyncOrOnAvailable(
                    show -> {
                        mIsReaderModeButtonShowingOnToolbar = isReaderMode && show;
                        if (isReaderMode) {
                            RecordHistogram.recordBooleanHistogram(
                                    "Android.ReaderModeCpa.Shown", show);
                        }
                        if (mIsReaderModeButtonShowingOnToolbar) {
                            markUrlAsShown();
                        } else {
                            boolean success = tryShowingPrompt(/* resetRestorePrompt= */ false);
                            if (mShouldRestorePrompt) {
                                mShouldRestorePrompt = false;
                                if (!success) {
                                    RecordUserAction.record(
                                            "CustomTabs.ReaderMode.PromptSuppressed");
                                }
                            }
                        }
                    });
        }
        if (showCpaButton.get() != null && showCpaButton.get()) {
            markUrlAsShown();
        }
    }

    private void markUrlAsShown() {
        if (mMessageShown) return;

        // Contextual page actions can't be dismissed, so we consider an unused button as
        // "dismissed". Interacting with the button will undo this "mute" logic.
        addUrlToMutedSites(mDistillerUrl);
        mMessageShown = true;
    }

    // Describes the end-state of the distillation result, used for metrics reporting. Do not
    // change/reorder existing entries, and keep in sync with accessibility/histograms.xml.
    // LINT.IfChange(DistillationResult)
    @IntDef({
        DistillationResult.NOT_DISTILLABLE,
        DistillationResult.DISTILLABLE,
        DistillationResult.DISTILLABLE_BUT_EXCLUDED_UNKNOWN,
        DistillationResult.DISTILLABLE_BUT_EXCLUDED_MOBILE,
        DistillationResult.DISTILLABLE_BUT_EXCLUDED_RDS,
        DistillationResult.MAX
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface DistillationResult {
        // Native signals that the page isn't distillable.
        int NOT_DISTILLABLE = 0;

        // Determined to distillability.
        int DISTILLABLE = 1;

        // Distillable, but excluded for an unknown reason.
        int DISTILLABLE_BUT_EXCLUDED_UNKNOWN = 2;

        // Distillable, but excluded because the web page is mobile friendly.
        int DISTILLABLE_BUT_EXCLUDED_MOBILE = 3;

        // Distillable, but excluded because the user is requesting the desktop version.
        int DISTILLABLE_BUT_EXCLUDED_RDS = 4;

        int MAX = 5;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:DistillationResult)

    private static void recordDistillationResult(
            Tab tab,
            @DistillationStatus int status,
            boolean isDistillable,
            boolean excludeMobileFriendly,
            boolean excludeRequestDesktopSite) {
        @DistillationResult int result;
        if (status == DistillationStatus.POSSIBLE) {
            result = DistillationResult.DISTILLABLE;
        } else {
            if (isDistillable) {
                if (excludeMobileFriendly) {
                    result = DistillationResult.DISTILLABLE_BUT_EXCLUDED_MOBILE;
                } else if (excludeRequestDesktopSite) {
                    result = DistillationResult.DISTILLABLE_BUT_EXCLUDED_RDS;
                } else {
                    result = DistillationResult.DISTILLABLE_BUT_EXCLUDED_UNKNOWN;
                }
            } else {
                result = DistillationResult.NOT_DISTILLABLE;
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                PAGE_DISTILLATION_RESULT_HISTOGRAM, result, DistillationResult.MAX);
        if (tab.getWebContents() != null) {
            new UkmRecorder(tab.getWebContents(), "DomDistiller.Android.DistillabilityResult")
                    .addMetric("Result", result)
                    .record();
        }
    }
}
