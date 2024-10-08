// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.net.Uri;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.autofill.AutofillValue;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.native_page.NativePageAssassin;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.SmoothTransitionDelegate;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.components.autofill.AutofillSelectionMenuItemHelper;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.Objects;

/**
 * Implementation of the interface {@link Tab}. Contains and manages a {@link ContentView}. This
 * class is not intended to be extended.
 */
class TabImpl implements Tab {
    /** Used for logging. */
    private static final String TAG = "Tab";

    private static final String BACKGROUND_COLOR_CHANGE_PRE_OPTIMIZATION_HISTOGRAM =
            "Android.Tab.BackgroundColorChange.PreOptimization";
    private static final String BACKGROUND_COLOR_CHANGE_HISTOGRAM =
            "Android.Tab.BackgroundColorChange";

    /**
     * A pref from //components/autofill/core/common/autofill_prefs.h which allows the use of
     * virtual viewstructures for Autofill when set.
     */
    @VisibleForTesting
    static final String AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE =
            "autofill.using_virtual_view_structure";

    private static final String PRODUCT_VERSION = VersionInfo.getProductVersion();

    private long mNativeTabAndroid;

    /** Unique id of this tab (within its container). */
    private final int mId;

    /** The Profile associated with this tab. */
    private final Profile mProfile;

    /**
     * An Application {@link Context}.  Unlike {@link #mActivity}, this is the only one that is
     * publicly exposed to help prevent leaking the {@link Activity}.
     */
    private final Context mThemedApplicationContext;

    /** Gives {@link Tab} a way to interact with the Android window. */
    private WindowAndroid mWindowAndroid;

    /** The current native page (e.g. chrome-native://newtab), or {@code null} if there is none. */
    private NativePage mNativePage;

    /**
     * True after a native page has been hidden, before a new background color has been explicitly
     * set. This is useful when the implicit background color (previously set before showing the
     * native page) is no longer necessarily relevant.
     */
    private boolean mWaitingOnBgColorAfterHidingNativePage;

    /** {@link WebContents} showing the current page, or {@code null} if the tab is frozen. */
    private WebContents mWebContents;

    /** The parent view of the ContentView and the InfoBarContainer. */
    private ContentView mContentView;

    /** The view provided by {@link TabViewManager} to be shown on top of Content view. */
    private View mCustomView;

    private @ColorInt Integer mCustomViewBackgroundColor;

    AutofillProvider mAutofillProvider;

    /**
     * The {@link TabViewManager} associated with this Tab that is responsible for managing custom
     * views.
     */
    private TabViewManagerImpl mTabViewManager;

    /** A list of Tab observers.  These are used to broadcast Tab events to listeners. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected final ObserverList<TabObserver> mObservers = new ObserverList<>();

    // Content layer Delegates
    private TabWebContentsDelegateAndroidImpl mWebContentsDelegate;

    private boolean mIsClosing;
    private boolean mIsShowingErrorPage;

    /**
     * Saves how this tab was launched (from a link, external app, etc) so that we can determine the
     * different circumstances in which it should be closed. For example, a tab opened from an
     * external app should be closed when the back stack is empty and the user uses the back
     * hardware key. A standard tab however should be kept open and the entire activity should be
     * moved to the background.
     */
    private @TabLaunchType int mLaunchType;

    private @Nullable @TabCreationState Integer mCreationState;

    /** URL load to be performed lazily when the Tab is next shown. */
    private LoadUrlParams mPendingLoadParams;

    /** True while a page load is in progress. */
    private boolean mIsLoading;

    /** True while a restore page load is in progress. */
    private boolean mIsBeingRestored;

    /** Whether or not the Tab is currently visible to the user. */
    private boolean mIsHidden = true;

    /**
     * Importance of the WebContents currently attached to this tab. Note the key difference from
     * |mIsHidden| is that a tab is hidden when the application is hidden, but the importance is not
     * affected by this signal.
     */
    private @ChildProcessImportance int mImportance = ChildProcessImportance.NORMAL;

    /** Whether the renderer is currently unresponsive. */
    private boolean mIsRendererUnresponsive;

    /**
     * Whether didCommitProvisionalLoadForFrame() hasn't yet been called for the current native page
     * (page A). To decrease latency, we show native pages in both loadUrl() and
     * didCommitProvisionalLoadForFrame(). However, we mustn't show a new native page (page B) in
     * loadUrl() if the current native page hasn't yet been committed. Otherwise, we'll show each
     * page twice (A, B, A, B): the first two times in loadUrl(), the second two times in
     * didCommitProvisionalLoadForFrame().
     */
    private boolean mIsNativePageCommitPending;

    private TabDelegateFactory mDelegateFactory;

    /** Listens for views related to the tab to be attached or detached. */
    private OnAttachStateChangeListener mAttachStateChangeListener;

    /** Whether the tab can currently be interacted with. */
    private boolean mInteractableState;

    /** Whether the tab is currently detached for reparenting. */
    private boolean mIsDetached;

    /** Whether or not the tab's active view is attached to the window. */
    private boolean mIsViewAttachedToWindow;

    private final UserDataHost mUserDataHost = new UserDataHost();

    private boolean mIsDestroyed;

    private int mThemeColor;
    private int mWebContentBackgroundColor;
    private int mTabBackgroundColor;
    private boolean mIsWebContentObscured;
    private long mTimestampMillis = INVALID_TIMESTAMP;
    private int mParentId = INVALID_TAB_ID;
    private int mRootId;
    private @Nullable Token mTabGroupId;
    private boolean mTabHasSensitiveContent;
    private @TabUserAgent int mUserAgent = TabUserAgent.DEFAULT;

    /**
     * Navigation state of the WebContents as returned by nativeGetContentsStateAsByteBuffer(),
     * stored to be inflated on demand using unfreezeContents(). If this is not null, there is no
     * WebContents around. Upon tab switch WebContents will be unfrozen and the variable will be set
     * to null.
     */
    private WebContentsState mWebContentsState;

    /** Title of the ContentViews webpage. */
    private String mTitle;

    /** URL of the page currently loading. Used as a fall-back in case tab restore fails. */
    private GURL mUrl;

    private long mLastNavigationCommittedTimestampMillis = INVALID_TIMESTAMP;

    /**
     * Saves how this tab was initially launched so that we can record metrics on how the tab was
     * created. This is different than {@link Tab#getLaunchType()}, since {@link
     * Tab#getLaunchType()} will be overridden to "FROM_RESTORE" during tab restoration.
     */
    private @TabLaunchType int mTabLaunchTypeAtCreation;

    /**
     * Variables used to track native page creation prior to mNativePage assignment. Avoids the case
     * where native pages can unintentionally re-create themselves by calling {@link
     * NativePage#onStateChange} during the creation process.
     */
    private boolean mIsAlreadyCreatingNativePage;

    private String mPendingNativePageHost;

    private SmoothTransitionDelegate mNativePageSmoothTransitionDelegate;

    /** Tracks the origin of a background color change. */
    @IntDef({
        BackgroundColorChangeOrigin.WEB_BACKGROUND_COLOR_CHANGE,
        BackgroundColorChangeOrigin.CUSTOM_VIEW_SET,
        BackgroundColorChangeOrigin.NATIVE_PAGE_SHOWN,
        BackgroundColorChangeOrigin.BG_COLOR_UPDATE_AFTER_HIDING_NATIVE_PAGE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BackgroundColorChangeOrigin {
        int WEB_BACKGROUND_COLOR_CHANGE = 0;
        int CUSTOM_VIEW_SET = 1;
        int NATIVE_PAGE_SHOWN = 2;
        int BG_COLOR_UPDATE_AFTER_HIDING_NATIVE_PAGE = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Creates an instance of a {@link TabImpl}. Package-private. Use {@link TabBuilder} to create
     * an instance.
     *
     * @param id The id this tab should be identified with.
     * @param profile The profile associated with this Tab.
     * @param launchType Type indicating how this tab was launched.
     */
    @SuppressLint("HandlerLeak")
    TabImpl(int id, @NonNull Profile profile, @TabLaunchType int launchType) {
        mId = TabIdManager.getInstance().generateValidId(id);
        mProfile = profile;
        assert mProfile != null;
        mRootId = mId;

        // Override the configuration for night mode to always stay in light mode until all UIs in
        // Tab are inflated from activity context instead of application context. This is to
        // avoid getting the wrong night mode state when application context inherits a system UI
        // mode different from the UI mode we need.
        // TODO(crbug.com/41445155): Remove this once Tab UIs are all inflated from
        // activity.
        mThemedApplicationContext =
                NightModeUtils.wrapContextWithNightModeConfig(
                        ContextUtils.getApplicationContext(),
                        ActivityUtils.getThemeId(),
                        /* nightMode= */ false);

        mLaunchType = launchType;

        mAttachStateChangeListener =
                new OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        mIsViewAttachedToWindow = true;
                        updateInteractableState();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {
                        if (isNativePage() && getNativePage().getView() == view) {
                            if (mNativePageSmoothTransitionDelegate != null) {
                                mNativePageSmoothTransitionDelegate.cancel();
                                mNativePageSmoothTransitionDelegate = null;
                            } else {
                                // reset ntp view state.
                                getView().setAlpha(1f);
                            }
                        }
                        mIsViewAttachedToWindow = false;
                        updateInteractableState();
                    }
                };
        mTabViewManager = new TabViewManagerImpl(this);
        new TabThemeColorHelper(this, this::updateThemeColor);
        mThemeColor = TabState.UNSPECIFIED_THEME_COLOR;
    }

    @Override
    public void addObserver(TabObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public boolean hasObserver(TabObserver observer) {
        return mObservers.hasObserver(observer);
    }

    @Override
    public UserDataHost getUserDataHost() {
        return mUserDataHost;
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public Context getContext() {
        if (getWindowAndroid() == null) return mThemedApplicationContext;
        Context context = getWindowAndroid().getContext().get();
        return context == context.getApplicationContext() ? mThemedApplicationContext : context;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @Override
    public void updateAttachment(
            @Nullable WindowAndroid window, @Nullable TabDelegateFactory tabDelegateFactory) {
        // Non-null delegate factory while being detached is not valid.
        assert !(window == null && tabDelegateFactory != null);

        if (window != null) {
            updateWindowAndroid(window);

            if (tabDelegateFactory != null) setDelegateFactory(tabDelegateFactory);

            // Reload the NativePage (if any), since the old NativePage has a reference to the old
            // activity.
            if (isNativePage()) {
                maybeShowNativePage(getUrl().getSpec(), true, PdfUtils.getPdfInfo(getNativePage()));
            }
        } else {
            updateIsDetached(window);
        }

        // Notify the event to observers only when we do the reparenting task, not when we simply
        // switch window in which case a new window is non-null but delegate is null.
        boolean notify =
                (window != null && tabDelegateFactory != null)
                        || (window == null && tabDelegateFactory == null);
        if (notify) {
            for (TabObserver observer : mObservers) {
                observer.onActivityAttachmentChanged(this, window);
            }
        }

        updateInteractableState();
    }

    public void didChangeCloseSignalInterceptStatus() {
        for (TabObserver observer : mObservers) {
            observer.onDidChangeCloseSignalInterceptStatus();
        }
    }

    /** Sets a custom {@link View} for this {@link Tab} that replaces Content view. */
    void setCustomView(@Nullable View view, @Nullable Integer backgroundColor) {
        mCustomView = view;
        mCustomViewBackgroundColor = backgroundColor;
        notifyContentChanged();
        onBackgroundColorChanged(BackgroundColorChangeOrigin.CUSTOM_VIEW_SET);
    }

    @Override
    public ContentView getContentView() {
        return mContentView;
    }

    @Override
    public View getView() {
        if (mCustomView != null) return mCustomView;

        if (mNativePage != null && !mNativePage.isFrozen()) return mNativePage.getView();

        return mContentView;
    }

    @Override
    public TabViewManager getTabViewManager() {
        return mTabViewManager;
    }

    @Override
    public int getId() {
        return mId;
    }

    @CalledByNative
    @Override
    public GURL getUrl() {
        if (!isInitialized()) {
            return GURL.emptyGURL();
        }
        GURL url = getWebContents() != null ? getWebContents().getVisibleUrl() : GURL.emptyGURL();

        // If we have a ContentView, or a NativePage, or the url is not empty, we have a WebContents
        // so cache the WebContent's url. If not use the cached version.
        if (getWebContents() != null || isNativePage() || !url.getSpec().isEmpty()) {
            mUrl = url;
        }

        return mUrl != null ? mUrl : GURL.emptyGURL();
    }

    @Override
    public GURL getOriginalUrl() {
        return DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(getUrl());
    }

    @CalledByNative
    @Override
    public String getTitle() {
        if (TextUtils.isEmpty(mTitle)) updateTitle();
        return mTitle;
    }

    Context getThemedApplicationContext() {
        return mThemedApplicationContext;
    }

    @Override
    public NativePage getNativePage() {
        return mNativePage;
    }

    @Override
    @CalledByNative
    public boolean isNativePage() {
        return mNativePage != null;
    }

    @Override
    public boolean isShowingCustomView() {
        return mCustomView != null;
    }

    @Override
    public void freezeNativePage() {
        if (mNativePage == null
                || mNativePage.isFrozen()
                || mNativePage.getView().getParent() != null) {
            return;
        }
        mNativePage = FrozenNativePage.freeze(mNativePage);
        updateInteractableState();
    }

    @Override
    @CalledByNative
    public @TabLaunchType int getLaunchType() {
        return mLaunchType;
    }

    @Override
    public int getThemeColor() {
        return mThemeColor;
    }

    @Override
    public int getBackgroundColor() {
        if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()) {
            if (mCustomView != null && mCustomViewBackgroundColor != null) {
                return mCustomViewBackgroundColor;
            }
            if (mNativePage != null) {
                return mNativePage.getBackgroundColor();
            }
        }
        return mWebContentBackgroundColor;
    }

    @Override
    public boolean isThemingAllowed() {
        // Do not apply the theme color if there are any security issues on the page.
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(getWebContents());
        boolean hasSecurityIssue =
                securityLevel == ConnectionSecurityLevel.DANGEROUS
                        || securityLevel
                                == ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT;
        // If chrome is showing an error page, allow theming so the system UI can match the page.
        // This is considered acceptable since chrome is in control of the error page. Otherwise, if
        // the page has a security issue, disable theming.
        return isShowingErrorPage() || !hasSecurityIssue;
    }

    @CalledByNative
    @Deprecated
    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean isOffTheRecord() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean isIncognitoBranded() {
        return mProfile.isIncognitoBranded();
    }

    @Override
    public boolean isShowingErrorPage() {
        return mIsShowingErrorPage;
    }

    /**
     * @return true iff the tab doesn't hold a live page. This happens before initialize() and when
     * the tab holds frozen WebContents state that is yet to be inflated.
     */
    @Override
    public boolean isFrozen() {
        return !isNativePage() && getWebContents() == null;
    }

    @CalledByNative
    @Override
    public boolean isUserInteractable() {
        return mInteractableState;
    }

    @Override
    public boolean isDetached() {
        assert !checkAttached() == mIsDetached
                : "Activity/Window attachment does not match Tab.mIsDetached == " + mIsDetached;
        return mIsDetached;
    }

    private void updateIsDetached(WindowAndroid window) {
        // HiddenTabHolder relies on isDetached() being true to determine whether the tab is
        // a background tab during initWebContents() before invoking ReparentingTask#detach().
        // In this scenario, the tab owns its own WindowAndroid and has no activity attachment.
        // We must check this as an additional condition to detachment for this case to continue
        // to work. See https://crbug.com/1501849.
        mIsDetached = window == null || !windowHasActivity(window);
    }

    private boolean checkAttached() {
        // getWindowAndroid() cannot be null (see updateWindowAndroid()) so this is effectively
        // checking to ensure whether the WebContents has a window and the tab is attached to an
        // activity.
        boolean hasActivity = getWindowAndroid() != null && windowHasActivity(getWindowAndroid());
        WebContents webContents = getWebContents();
        return webContents == null
                ? !mIsDetached && hasActivity
                : (webContents.getTopLevelNativeWindow() != null && hasActivity);
    }

    private boolean windowHasActivity(WindowAndroid window) {
        return ContextUtils.activityFromContext(window.getContext().get()) != null;
    }

    /**
     * The parent tab for the current tab is set and the DelegateFactory is updated if it is not set
     * already. This happens only if the tab has been detached and the parent has not been set yet,
     * for example, for the spare tab before loading url.
     * @param parent The tab that caused this tab to be opened.
     */
    @Override
    public void reparentTab(Tab parent) {
        // When parent is null, no action is taken since it is the same as the default setting (no
        // parent).
        if (parent != null) {
            mParentId = parent.getId();

            // Update the DelegateFactory if it is not already set, since it is associated with the
            // parent tab.
            if (mDelegateFactory == null) {
                mDelegateFactory = ((TabImpl) parent).getDelegateFactory();
                setDelegateFactory(mDelegateFactory);
            }
        }
    }

    @Override
    public LoadUrlResult loadUrl(LoadUrlParams params) {
        try {
            TraceEvent.begin("Tab.loadUrl");
            // TODO(tedchoc): When showing the android NTP, delay the call to
            // TabImplJni.get().loadUrl until the android view has entirely rendered.
            if (!mIsNativePageCommitPending) {
                boolean isPdf =
                        PdfUtils.shouldOpenPdfInline(isIncognito())
                                && PdfUtils.isPdfNavigation(params.getUrl(), params);
                mIsNativePageCommitPending =
                        maybeShowNativePage(params.getUrl(), false, isPdf ? new PdfInfo() : null);
                if (isPdf) {
                    params.setIsPdf(true);
                }
            }

            if ("chrome://java-crash/".equals(params.getUrl())) {
                return handleJavaCrash();
            }

            if (isDestroyed()) {
                // This will crash below, but we want to know if the tab was destroyed or just never
                // initialize.
                throw new RuntimeException("Tab.loadUrl called on a destroyed tab");
            }
            if (mNativeTabAndroid == 0) {
                // if mNativeTabAndroid is null then we are going to crash anyways on the
                // native side. Lets crash on the java side so that we can have a better stack
                // trace.
                throw new RuntimeException("Tab.loadUrl called when no native side exists");
            }

            // TODO(crbug.com/40549331): Don't fix up all URLs. Documentation on
            // FixupURL explicitly says not to use it on URLs coming from untrustworthy
            // sources, like other apps. Once migrations of Java code to GURL are complete
            // and incoming URLs are converted to GURLs at their source, we can make
            // decisions of whether or not to fix up GURLs on a case-by-case basis based
            // on trustworthiness of the incoming URL.
            GURL fixedUrl = UrlFormatter.fixupUrl(params.getUrl());
            // Request desktop sites if necessary.
            if (fixedUrl.isValid()) {
                params.setOverrideUserAgent(calculateUserAgentOverrideOption(fixedUrl));
            } else {
                // Fall back to the Url in webContents for site level setting.
                params.setOverrideUserAgent(calculateUserAgentOverrideOption(null));
            }

            LoadUrlResult result = loadUrlInternal(params, fixedUrl);

            for (TabObserver observer : mObservers) {
                observer.onLoadUrl(this, params, result);
            }
            return result;
        } finally {
            TraceEvent.end("Tab.loadUrl");
        }
    }

    private LoadUrlResult loadUrlInternal(LoadUrlParams params, GURL fixedUrl) {
        if (mWebContents == null) return new LoadUrlResult(TabLoadStatus.PAGE_LOAD_FAILED, null);

        if (!fixedUrl.isValid()) return new LoadUrlResult(TabLoadStatus.PAGE_LOAD_FAILED, null);

        // Record UMA "ShowHistory" here. That way it'll pick up both user
        // typing chrome://history as well as selecting from the drop down menu.
        if (fixedUrl.getSpec().equals(UrlConstants.HISTORY_URL)) {
            RecordUserAction.record("ShowHistory");
        }

        if (TabImplJni.get().handleNonNavigationAboutURL(fixedUrl)) {
            return new LoadUrlResult(TabLoadStatus.DEFAULT_PAGE_LOAD, null);
        }

        params.setUrl(fixedUrl.getSpec());
        NavigationHandle handle = mWebContents.getNavigationController().loadUrl(params);
        return new LoadUrlResult(TabLoadStatus.DEFAULT_PAGE_LOAD, handle);
    }

    @Override
    public void freezeAndAppendPendingNavigation(LoadUrlParams params, @Nullable String title) {
        assert isHidden() : "Should only freeze and apprend a navigation to a tab that is hidden.";
        // If the native page is not already torn down make sure we remove it so it isn't visible if
        // this tab is foregrounded again in the current session.
        hideNativePage(/* notify= */ false, /* postHideTask= */ null);
        WebContentsState oldWebContentsState = TabStateExtractor.getWebContentsState(this);
        WebContents oldWebContents = mWebContents;
        destroyWebContents(false);
        mWebContents = null;
        RewindableIterator<TabObserver> observers = getTabObservers();
        if (oldWebContents != null) {
            while (observers.hasNext()) {
                observers.next().onContentChanged(this);
            }
            observers.rewind();
            oldWebContents.destroy();
        }
        Referrer referrer = params.getReferrer();
        mWebContentsState =
                WebContentsStateBridge.appendPendingNavigation(
                        oldWebContentsState,
                        title,
                        params.getUrl(),
                        referrer != null ? referrer.getUrl() : null,
                        // Policy will be ignored for null referrer url, 0 is just a placeholder.
                        referrer != null ? referrer.getPolicy() : 0,
                        params.getInitiatorOrigin(),
                        isOffTheRecord());
        mIsLoading = false;

        // The only reason this should still be null is if we failed to allocate a byte buffer,
        // which probably means we are close to an OOM.
        boolean success = mWebContentsState != null;
        RecordHistogram.recordBooleanHistogram(
                "Tabs.FreezeAndAppendPendingNavigationResult", success);
        if (success) {
            // The pending load params were consumed to make the WebContentsState. Invalidate them.
            mPendingLoadParams = null;
            mUrl = new GURL(mWebContentsState.getVirtualUrlFromState());
        } else {
            // Since we are not allowed to auto-navigate the only remaining fallback is to clobber
            // all navigation state and treat the tab as if it is in a pending load state. All the
            // previous state was already cleaned up so we just need to set the params here.
            mPendingLoadParams = params;
            mUrl = new GURL(params.getUrl());
        }
        while (observers.hasNext()) {
            observers.next().onUrlUpdated(this);
        }
        observers.rewind();
        notifyFaviconChanged();
        updateTitle(title);

        while (observers.hasNext()) {
            observers.next().onNavigationEntriesAppended(this);
        }
    }

    @Override
    public boolean loadIfNeeded(@TabLoadIfNeededCaller int caller) {
        if (getActivity() == null) {
            Log.e(TAG, "Tab couldn't be loaded because Context was null.");
            return false;
        }

        if (mPendingLoadParams != null) {
            assert isFrozen();
            WebContents webContents =
                    WarmupManager.getInstance().takeSpareWebContents(isIncognito(), isHidden());
            if (webContents == null) {
                webContents = WebContentsFactory.createWebContents(mProfile, isHidden(), false);
            }
            initWebContents(webContents);
            loadUrl(mPendingLoadParams);
            mPendingLoadParams = null;
            return true;
        }

        restoreIfNeeded(caller);
        return true;
    }

    @Override
    public void reload() {
        NativePage nativePage = getNativePage();
        if (nativePage != null) {
            nativePage.reload();
            return;
        }

        // TODO(dtrainor): Should we try to rebuild the ContentView if it's frozen?
        if (OfflinePageUtils.isOfflinePage(this)) {
            // If current page is an offline page, reload it with custom behavior defined in extra
            // header respected.
            OfflinePageUtils.reload(
                    getWebContents(),
                    /* loadUrlDelegate= */ new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(
                            this));
            return;
        }

        if (getWebContents() == null) return;
        switchUserAgentIfNeeded(UseDesktopUserAgentCaller.RELOAD);
        getWebContents().getNavigationController().reload(true);
    }

    @Override
    public void reloadIgnoringCache() {
        if (getWebContents() != null) {
            switchUserAgentIfNeeded(UseDesktopUserAgentCaller.RELOAD_IGNORING_CACHE);
            getWebContents().getNavigationController().reloadBypassingCache(true);
        }
    }

    @Override
    public void stopLoading() {
        if (isLoading()) {
            RewindableIterator<TabObserver> observers = getTabObservers();
            while (observers.hasNext()) {
                observers.next().onPageLoadFinished(this, getUrl());
            }
        }
        if (getWebContents() != null) getWebContents().stop();
    }

    @Override
    public boolean needsReload() {
        return getWebContents() != null && getWebContents().getNavigationController().needsReload();
    }

    @Override
    public boolean isLoading() {
        return mIsLoading;
    }

    @Override
    public boolean isBeingRestored() {
        return mIsBeingRestored;
    }

    @Override
    public float getProgress() {
        return !isLoading() ? 1 : (int) mWebContents.getLoadProgress();
    }

    @Override
    public boolean canGoBack() {
        return getWebContents() != null && getWebContents().getNavigationController().canGoBack();
    }

    @Override
    public boolean canGoForward() {
        return getWebContents() != null
                && getWebContents().getNavigationController().canGoForward();
    }

    @Override
    public void goBack() {
        if (getWebContents() != null) getWebContents().getNavigationController().goBack();
    }

    @Override
    public void goForward() {
        if (getWebContents() != null) getWebContents().getNavigationController().goForward();
    }

    // TabLifecycle implementation.

    @Override
    public boolean isInitialized() {
        return mNativeTabAndroid != 0;
    }

    @Override
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    @Override
    public final void show(@TabSelectionType int type, @TabLoadIfNeededCaller int caller) {
        try {
            TraceEvent.begin("Tab.show");
            if (!isHidden()) return;
            // Keep unsetting mIsHidden above loadIfNeeded(), so that we pass correct visibility
            // when spawning WebContents in loadIfNeeded().
            mIsHidden = false;
            updateInteractableState();

            loadIfNeeded(caller);

            // TODO(crbug.com/40199376): We should provide a timestamp that apporoximates the input
            // event timestamp. When presenting a Tablet UI, StripLayoutTab.handleClick does
            // receive a timestamp. When presenting a Phone UI
            // TabGridViewBinder.bindClosableTabProperties is called by Android.View.performClick,
            // and does not receive the event timestamp. This currently triggers an animation in
            // TabSwitcherLayout.startHidingImpl which lasts around 300ms.
            // TabSwitcherLayout.doneHiding runs after the animation, actually triggering this tab
            // change.
            //
            // We should also consider merging the TabImpl and WebContents onShow into a single Jni
            // call.
            TabImplJni.get().onShow(mNativeTabAndroid);

            if (getWebContents() != null) {
                getWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
            }

            // If the NativePage was frozen while in the background (see NativePageAssassin),
            // recreate the NativePage now.
            NativePage nativePage = getNativePage();
            PdfUtils.recordIsPdfFrozen(nativePage);
            if (nativePage != null && nativePage.isFrozen()) {
                maybeShowNativePage(nativePage.getUrl(), true, PdfUtils.getPdfInfo(nativePage));
            }
            NativePageAssassin.getInstance().tabShown(this);
            TabImportanceManager.tabShown(this);

            // If the page is still loading, update the progress bar (otherwise it would not show
            // until the renderer notifies of new progress being made).
            if (getProgress() < 100) {
                notifyLoadProgress(getProgress());
            }

            for (TabObserver observer : mObservers) observer.onShown(this, type);

            // Updating the timestamp has to happen after the showInternal() call since subclasses
            // may use it for logging.
            setTimestampMillis(System.currentTimeMillis());
        } finally {
            TraceEvent.end("Tab.show");
        }
    }

    @Override
    public final void hide(@TabHidingType int type) {
        try {
            TraceEvent.begin("Tab.hide");
            if (isHidden()) return;
            mIsHidden = true;
            updateInteractableState();

            if (getWebContents() != null) {
                getWebContents().updateWebContentsVisibility(Visibility.HIDDEN);
            }

            // Allow this tab's NativePage to be frozen if it stays hidden for a while.
            NativePageAssassin.getInstance().tabHidden(this);

            for (TabObserver observer : mObservers) observer.onHidden(this, type);
        } finally {
            TraceEvent.end("Tab.hide");
        }
    }

    @Override
    public boolean isClosing() {
        return mIsClosing;
    }

    @Override
    public void setClosing(boolean closing) {
        if (mIsClosing == closing) return;
        mIsClosing = closing;
        for (TabObserver observer : mObservers) observer.onClosingStateChanged(this, closing);
    }

    @CalledByNative
    @Override
    public boolean isHidden() {
        return mIsHidden;
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        // Set at the start since destroying the WebContents can lead to calling back into
        // this class.
        mIsDestroyed = true;

        // Update the title before destroying the tab. http://b/5783092
        updateTitle();

        for (TabObserver observer : mObservers) observer.onDestroyed(this);
        mObservers.clear();

        mUserDataHost.destroy();
        mTabViewManager.destroy();
        hideNativePage(false, null);
        destroyWebContents(true);

        TabImportanceManager.tabDestroyed(this);

        // Destroys the native tab after destroying the ContentView but before destroying the
        // InfoBarContainer. The native tab should be destroyed before the infobar container as
        // destroying the native tab cleanups up any remaining infobars. The infobar container
        // expects all infobars to be cleaned up before its own destruction.
        if (mNativeTabAndroid != 0) {
            TabImplJni.get().destroy(mNativeTabAndroid);
            assert mNativeTabAndroid == 0;
        }
    }

    /**
     * WARNING: This method is deprecated. Consider other ways such as passing the dependencies
     *          to the constructor, rather than accessing ChromeActivity from Tab and using getters.
     * @return {@link ChromeActivity} that currently contains this {@link Tab} in its
     *         {@link TabModel}.
     */
    @Deprecated
    ChromeActivity<?> getActivity() {
        if (getWindowAndroid() == null) return null;
        Activity activity = ContextUtils.activityFromContext(getWindowAndroid().getContext().get());
        if (activity instanceof ChromeActivity) return (ChromeActivity<?>) activity;
        return null;
    }

    protected void updateWebContentObscured(boolean obscureWebContent) {
        // Update whether or not the current native tab and/or web contents are
        // currently visible (from an accessibility perspective), or whether
        // they're obscured by another view.
        View view = getView();
        if (view != null) {
            int importantForAccessibility =
                    obscureWebContent
                            ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            : View.IMPORTANT_FOR_ACCESSIBILITY_YES;
            if (view.getImportantForAccessibility() != importantForAccessibility) {
                view.setImportantForAccessibility(importantForAccessibility);
                view.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
            }
        }

        WebContentsAccessibility wcax = getWebContentsAccessibility(getWebContents());
        if (wcax != null) {
            if (mIsWebContentObscured == obscureWebContent) return;
            wcax.setObscuredByAnotherView(obscureWebContent);
            mIsWebContentObscured = obscureWebContent;
        }
    }

    /**
     * Initializes {@link Tab} with {@code webContents}. If {@code webContents} is {@code null} a
     * new {@link WebContents} will be created for this {@link Tab}.
     *
     * @param parent The tab that caused this tab to be opened.
     * @param creationState State in which the tab is created.
     * @param loadUrlParams Parameters used for a lazily loaded Tab or null if we initialize a tab
     *     without an URL.
     * @param pendingTitle The title used for a lazily load Tab. Ignored if {@code loadUrlParams} is
     *     {@code null}.
     * @param webContents A {@link WebContents} object or {@code null} if one should be created.
     * @param delegateFactory The {@link TabDelegateFactory} to be used for delegate creation.
     * @param initiallyHidden Only used if {@code webContents} is {@code null}. Determines whether
     *     or not the newly created {@link WebContents} will be hidden or not.
     * @param tabState State containing information about this Tab, if it was persisted.
     * @param initializeRenderer Determines whether or not we initialize renderer with {@link
     *     WebContents} creation.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    void initialize(
            Tab parent,
            @Nullable @TabCreationState Integer creationState,
            @Nullable LoadUrlParams loadUrlParams,
            @Nullable String pendingTitle,
            WebContents webContents,
            @Nullable TabDelegateFactory delegateFactory,
            boolean initiallyHidden,
            TabState tabState,
            boolean initializeRenderer) {
        try {
            TraceEvent.begin("Tab.initialize");

            if (parent != null) {
                mParentId = parent.getId();
            }

            mTabLaunchTypeAtCreation = mLaunchType;
            mCreationState = creationState;

            // If applicable set up for a lazy background tab load.
            mPendingLoadParams = loadUrlParams;
            if (loadUrlParams != null) {
                mUrl = new GURL(loadUrlParams.getUrl());
                if (pendingTitle != null) {
                    setTitle(pendingTitle);
                }
            }

            // The {@link mDelegateFactory} needs to be set before calling
            // {@link TabHelpers.initTabHelpers()}. This is because it creates a
            // TabBrowserControlsConstraintsHelper, and {@link
            // TabBrowserControlsConstraintsHelper#updateVisibilityDelegate()} will call the
            // Tab#getDelegateFactory().createBrowserControlsVisibilityDelegate().
            // See https://crbug.com/1179419.
            mDelegateFactory = delegateFactory;

            TabHelpers.initTabHelpers(this, parent);

            if (tabState != null) {
                restoreFieldsFromState(tabState);
            }

            initializeNative();

            RevenueStats.getInstance().tabCreated(this);

            // If there is a frozen WebContents state or a pending lazy load, don't create a new
            // WebContents. Restoring will be done when showing the tab in the foreground.
            if (mWebContentsState != null || getPendingLoadParams() != null) {
                return;
            }

            boolean creatingWebContents = webContents == null;
            if (creatingWebContents) {
                webContents =
                        WarmupManager.getInstance()
                                .takeSpareWebContents(isIncognito(), initiallyHidden);
                if (webContents == null) {
                    webContents =
                            WebContentsFactory.createWebContents(
                                    mProfile, initiallyHidden, initializeRenderer);
                }
            }

            initWebContents(webContents);
            // Avoid an empty title by updating the title here. This could happen if restoring from
            // a WebContents that has no renderer and didn't force a reload. This happens on
            // background tab creation from Recent Tabs (TabRestoreService).
            updateTitle();

            if (!creatingWebContents && webContents.shouldShowLoadingUI()) {
                didStartPageLoad(webContents.getVisibleUrl());
            }

        } finally {
            if (mTimestampMillis == INVALID_TIMESTAMP) {
                setTimestampMillis(System.currentTimeMillis());
            }
            String appId = null;
            Boolean hasThemeColor = null;
            int themeColor = 0;
            if (tabState != null) {
                appId = tabState.openerAppId;
                themeColor = tabState.getThemeColor();
                hasThemeColor = tabState.hasThemeColor();
            }
            if (hasThemeColor != null) {
                updateThemeColor(hasThemeColor ? themeColor : TabState.UNSPECIFIED_THEME_COLOR);
            }

            for (TabObserver observer : mObservers) observer.onInitialized(this, appId);
            TraceEvent.end("Tab.initialize");
        }
    }

    @Nullable
    @TabCreationState
    Integer getCreationState() {
        return mCreationState;
    }

    /**
     * Restores member fields from the given TabState.
     * @param state TabState containing information about this Tab.
     */
    void restoreFieldsFromState(TabState state) {
        assert state != null;
        mWebContentsState = state.contentsState;
        setTimestampMillis(state.timestampMillis);
        setLastNavigationCommittedTimestampMillis(state.lastNavigationCommittedTimestampMillis);
        mUrl = new GURL(state.contentsState.getVirtualUrlFromState());
        setTitle(state.contentsState.getDisplayTitleFromState());
        mTabLaunchTypeAtCreation = state.tabLaunchTypeAtCreation;
        setRootId(state.rootId == Tab.INVALID_TAB_ID ? mId : state.rootId);
        setTabGroupId(state.tabGroupId);
        setUserAgent(state.userAgent);
        setTabHasSensitiveContent(state.tabHasSensitiveContent);
    }

    /**
     * @return An {@link ObserverList.RewindableIterator} instance that points to all of the current
     *     {@link TabObserver}s on this class. Note that calling {@link java.util.Iterator#remove()}
     *     will throw an {@link UnsupportedOperationException}.
     */
    ObserverList.RewindableIterator<TabObserver> getTabObservers() {
        return mObservers.rewindableIterator();
    }

    final void setImportance(@ChildProcessImportance int importance) {
        if (mImportance == importance) return;
        mImportance = importance;
        WebContents webContents = getWebContents();
        if (webContents == null) return;
        webContents.setImportance(mImportance);
    }

    /** Hides the current {@link NativePage}, if any, and shows the {@link WebContents}'s view. */
    void showRenderedPage() {
        // During title update, we prioritize titles in NativePage instead of those from
        // WebContents. Thus we should remove the obsolete NativePage before title update.
        if (mNativePage != null) hideNativePage(true, null);
        updateTitle();
    }

    void updateWindowAndroid(WindowAndroid windowAndroid) {
        // TODO(yusufo): mWindowAndroid can never be null until crbug.com/657007 is fixed.
        assert windowAndroid != null;
        mWindowAndroid = windowAndroid;
        WebContents webContents = getWebContents();
        if (webContents != null) webContents.setTopLevelNativeWindow(mWindowAndroid);

        updateIsDetached(windowAndroid);
    }

    TabDelegateFactory getDelegateFactory() {
        return mDelegateFactory;
    }

    @VisibleForTesting
    TabWebContentsDelegateAndroidImpl getTabWebContentsDelegateAndroid() {
        return mWebContentsDelegate;
    }

    // Forwarded from TabViewAndroidDelegate.

    /**
     * Implementation of the {@link View#onProvideAutofillVirtualStructure(ViewStructure, int)}
     * method for this tab. Noop if {@link AutofillProvider} isn't used on this tab.
     *
     * @see View#onProvideAutofillVirtualStructure(ViewStructure structure, int flags)
     * @see ViewAndroidDelegate#onProvideAutofillVirtualStructure(ViewStructure structure, int
     *     flags)
     */
    void onProvideAutofillVirtualStructure(ViewStructure structure, int flags) {
        if (mAutofillProvider != null) {
            mAutofillProvider.onProvideAutoFillVirtualStructure(structure, flags);
        }
    }

    /**
     * Implementation of the {@link View#autofill(SparseArray))} method for this tab. Noop if {@link
     * AutofillProvider} isn't used on this tab.
     *
     * @see View#autofill(SparseArray)
     * @see ViewAndroidDelegate#autofill(SparseArray)
     */
    void autofill(final SparseArray<AutofillValue> values) {
        if (mAutofillProvider != null) {
            mAutofillProvider.autofill(values);
        }
    }

    /**
     * Check whether the platform can request a ViewStructure.
     *
     * @return iff the AutofillProvider should provide a ViewStructure when prompted.
     */
    boolean providesAutofillStructure() {
        if (!ChromeFeatureList.isEnabled(
                AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)) {
            return false;
        }
        if (mProfile == null || !mProfile.isNativeInitialized()) {
            return false;
        }
        @Nullable PrefService prefs = UserPrefs.get(mProfile);
        return prefs != null && prefs.getBoolean(AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE);
    }

    // Forwarded from TabWebContentsDelegateAndroid.

    /**
     * Called when a navigation begins and no navigation was in progress
     *
     * @param toDifferentDocument Whether this navigation will transition between documents (i.e.,
     *     not a fragment navigation or JS History API call).
     */
    void onLoadStarted(boolean toDifferentDocument) {
        if (toDifferentDocument) mIsLoading = true;
        for (TabObserver observer : mObservers) observer.onLoadStarted(this, toDifferentDocument);
    }

    /** Called when a navigation completes and no other navigation is in progress. */
    void onLoadStopped() {
        // mIsLoading should only be false if this is a same-document navigation.
        boolean toDifferentDocument = mIsLoading;
        mIsLoading = false;
        for (TabObserver observer : mObservers) observer.onLoadStopped(this, toDifferentDocument);
    }

    void handleRendererResponsiveStateChanged(boolean isResponsive) {
        mIsRendererUnresponsive = !isResponsive;
        for (TabObserver observer : mObservers) {
            observer.onRendererResponsiveStateChanged(this, isResponsive);
        }
    }

    void handleBackForwardTransitionUiChanged() {
        for (TabObserver observer : mObservers) {
            observer.didBackForwardTransitionAnimationChange();
        }

        // Start the cross-fade animation after the invoking animation is done.
        switch (getWebContents().getCurrentBackForwardTransitionStage()) {
            case AnimationStage.NONE:
                // Native animator is destroy before animation is done.
                // Non-null nativePageSmoothTransitionDelegate means the page is transiting to
                // a native page; otherwise, it possibly means transiting from a native page to
                // another page.
                if (mNativePageSmoothTransitionDelegate != null) {
                    mNativePageSmoothTransitionDelegate.cancel();
                    mNativePageSmoothTransitionDelegate = null;
                } else if (isNativePage()) {
                    // May reach this if a navigation is committed in the mid of gesture.
                    getView().setAlpha(1f);
                }
                return;
            case AnimationStage.OTHER:
                if (isNativePage()) {
                    // A transition is starting. Hide the Java view to present that.
                    // Wait until the content/ draws the transition.
                    CompositorViewHolder viewHolder =
                            getActivity().getCompositorViewHolderSupplier().get();
                    viewHolder.requestRender(() -> getView().setAlpha(0f));
                }
                return;
            case AnimationStage.WAITING_FOR_EMBEDDER_CONTENT_FOR_COMMITTED_ENTRY:
                if (mNativePageSmoothTransitionDelegate != null) {
                    // Navigating back to native pages.
                    mNativePageSmoothTransitionDelegate.start(
                            () -> {
                                getWebContents().onContentForNavigationEntryShown();
                                notifyContentChanged();
                            });
                    mNativePageSmoothTransitionDelegate = null;
                } else if (isNativePage()) { // Navigation from native page was cancelled.
                    if (getView().getAlpha() != 1f) {
                        // This means the content/ is waiting for the NTP to be fully visible.
                        getView().setAlpha(1f);
                        getView().post(getWebContents()::onContentForNavigationEntryShown);
                    }
                }
        }
    }

    // Forwarded from TabWebContentsObserver.

    /**
     * Called when a page has started loading.
     *
     * @param validatedUrl URL being loaded.
     */
    void didStartPageLoad(GURL validatedUrl) {
        updateTitle();
        if (mIsRendererUnresponsive) handleRendererResponsiveStateChanged(true);
        for (TabObserver observer : mObservers) {
            observer.onPageLoadStarted(this, validatedUrl);
        }
    }

    /**
     * Called when a page has finished loading.
     * @param url URL that was loaded.
     */
    void didFinishPageLoad(GURL url) {
        updateTitle();

        for (TabObserver observer : mObservers) observer.onPageLoadFinished(this, url);
        mIsBeingRestored = false;
    }

    /**
     * Called when a page has failed loading.
     * @param errorCode The error code causing the page to fail loading.
     */
    void didFailPageLoad(int errorCode) {
        for (TabObserver observer : mObservers) {
            observer.onPageLoadFailed(this, errorCode);
        }
        mIsBeingRestored = false;
    }

    /**
     * Update internal Tab state when provisional load gets committed.
     *
     * @param url The URL that was loaded.
     * @param transitionType The transition type to the current URL.
     * @param isPdf Whether the navigation is for PDF content.
     */
    void handleDidFinishNavigation(GURL url, int transitionType, boolean isPdf) {
        mIsNativePageCommitPending = false;
        boolean isReload = (transitionType & PageTransition.CORE_MASK) == PageTransition.RELOAD;
        // Set isPdf param based on the url. This is because the isPdf param in NavigationHandle is
        // not set in some cases (e.g. Chrome restart or navigate backward to pdf page). When the
        // pdf file is downloaded to media store, we should set isPdf param and open pdf page
        // immediately, because no re-download is expected.
        isPdf |=
                PdfUtils.shouldOpenPdfInline(isIncognito())
                        && PdfUtils.isDownloadedPdf(url.getSpec());
        if (!maybeShowNativePage(url.getSpec(), isReload, isPdf ? new PdfInfo() : null)) {
            String downloadUrl = PdfUtils.decodePdfPageUrl(url.getSpec());
            if (downloadUrl != null) {
                // When the download url is not null, we are on a pdf native page which requires
                // re-download. Load the download url to trigger the re-download.
                loadUrl(new LoadUrlParams(downloadUrl));
            } else {
                showRenderedPage();
            }
        }

        setLastNavigationCommittedTimestampMillis(System.currentTimeMillis());
    }

    /**
     * Notify the observers that the load progress has changed.
     * @param progress The current percentage of progress.
     */
    void notifyLoadProgress(float progress) {
        for (TabObserver observer : mObservers) observer.onLoadProgressChanged(this, progress);
    }

    /** Add a new navigation entry for the current URL and page title. */
    void pushNativePageStateToNavigationEntry() {
        assert mNativeTabAndroid != 0 && getNativePage() != null;
        TabImplJni.get()
                .setActiveNavigationEntryTitleForUrl(
                        mNativeTabAndroid, getNativePage().getUrl(), getNativePage().getTitle());
    }

    /** Set whether the Tab needs to be reloaded. */
    void setNeedsReload() {
        assert getWebContents() != null;
        getWebContents().getNavigationController().setNeedsReload();
    }

    /** Called when navigation entries were removed. */
    void notifyNavigationEntriesDeleted() {
        for (TabObserver observer : mObservers) observer.onNavigationEntriesDeleted(this);
    }

    //////////////

    /**
     * @return Whether the renderer is currently unresponsive.
     */
    boolean isRendererUnresponsive() {
        return mIsRendererUnresponsive;
    }

    /** Load the original image (uncompressed by spdy proxy) in this tab. */
    void loadOriginalImage() {
        if (mNativeTabAndroid != 0) {
            TabImplJni.get().loadOriginalImage(mNativeTabAndroid);
        }
    }

    /**
     * Sets whether the tab is showing an error page.  This is reset whenever the tab finishes a
     * navigation.
     * Note: This is kept here to keep the build green. Remove from interface as soon as
     *       the downstream patch lands.
     * @param isShowingErrorPage Whether the tab shows an error page.
     */
    void setIsShowingErrorPage(boolean isShowingErrorPage) {
        mIsShowingErrorPage = isShowingErrorPage;
    }

    /**
     * Shows a native page for url if it's a valid chrome-native URL. Otherwise, does nothing.
     *
     * @param url The url of the current navigation.
     * @param forceReload If true, the current native page (if any) will not be reused, even if it
     *     matches the URL.
     * @param pdfInfo Information of the pdf, or null if there is no associated pdf download.
     * @return True, if a native page was displayed for url.
     */
    boolean maybeShowNativePage(String url, boolean forceReload, PdfInfo pdfInfo) {
        // While detached for reparenting we don't have an owning Activity, or TabModelSelector,
        // so we can't create the native page. The native page will be created once reparenting is
        // completed.
        if (isDetached()) return false;
        // TODO(crbug.com/40943608): Remove the assert after determining why WebContents can be
        // null.
        WebContents webContents = getWebContents();
        assert webContents != null;
        if (webContents == null) return false;
        // If the given url is null, there's no work to do.
        if (url == null) return false;

        // We might be in the middle of loading a native page, in that case we should bail to avoid
        // recreating another instance.
        String nativePageHost = Uri.parse(url).getHost();
        if (mIsAlreadyCreatingNativePage
                && TextUtils.equals(mPendingNativePageHost, nativePageHost)) {
            return true;
        }

        mPendingNativePageHost = nativePageHost;
        mIsAlreadyCreatingNativePage = true;
        NativePage candidateForReuse = forceReload ? null : getNativePage();
        NativePage nativePage =
                mDelegateFactory.createNativePage(url, candidateForReuse, this, pdfInfo);
        mIsAlreadyCreatingNativePage = false;
        mPendingNativePageHost = null;

        if (nativePage != null) {
            showNativePage(nativePage);
            notifyPageTitleChanged();
            notifyFaviconChanged();
            return true;
        }
        return false;
    }

    /** Calls onContentChanged on all TabObservers and updates accessibility visibility. */
    void notifyContentChanged() {
        for (TabObserver observer : mObservers) observer.onContentChanged(this);
    }

    void updateThemeColor(int themeColor) {
        if (mThemeColor == themeColor) return;
        mThemeColor = themeColor;
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) observers.next().onDidChangeThemeColor(this, themeColor);
    }

    /** Update the title for the current page if changed. */
    @Override
    public void updateTitle() {
        if (isFrozen()) return;

        // When restoring the tabs, the title will no longer be populated, so request it from the
        // WebContents or NativePage (if present).
        String title = "";
        if (isNativePage()) {
            title = mNativePage.getTitle();
        } else if (getWebContents() != null) {
            title = getWebContents().getTitle();
        }
        updateTitle(title);
    }

    /**
     * Cache the title for the current page.
     *
     * {@link ContentViewClient#onUpdateTitle} is unreliable, particularly for navigating backwards
     * and forwards in the history stack, so pull the correct title whenever the page changes.
     * onUpdateTitle is only called when the title of a navigation entry changes. When the user goes
     * back a page the navigation entry exists with the correct title, thus the title is not
     * actually changed, and no notification is sent.
     * @param title Title of the page.
     */
    void updateTitle(String title) {
        if (TextUtils.equals(mTitle, title)) return;

        setTitle(title);
        notifyPageTitleChanged();
    }

    @Override
    public LoadUrlParams getPendingLoadParams() {
        return mPendingLoadParams;
    }

    /** Performs any subclass-specific tasks when the Tab crashes. */
    void handleTabCrash() {
        mIsLoading = false;

        RewindableIterator<TabObserver> observers = getTabObservers();
        // When the renderer crashes for a hidden spare tab, we can skip notifying the observers to
        // crash the underlying tab. This is because it is safe to keep the spare tab around without
        // a renderer process, and since the tab is hidden, we don't need to show a sad tab. When
        // the spare tab is used for navigation it will create a new renderer process.
        // TODO(crbug.com/40268909): Make this logic more robust for all hidden tab cases.
        if (!WarmupManager.getInstance().isSpareTab(this)) {
            while (observers.hasNext()) observers.next().onCrash(this);
        }
        mIsBeingRestored = false;
    }

    /**
     * Called when the background color for the content changes.
     *
     * @param color The current for the background.
     */
    void changeWebContentBackgroundColor(int color) {
        mWebContentBackgroundColor = color;
        onBackgroundColorChanged(BackgroundColorChangeOrigin.WEB_BACKGROUND_COLOR_CHANGE);
        mWaitingOnBgColorAfterHidingNativePage = false;
    }

    /** Called to notify when the page had painted something non-empty. */
    void notifyDidFirstVisuallyNonEmptyPaint() {
        if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                && mWaitingOnBgColorAfterHidingNativePage) {
            onBackgroundColorChanged(
                    BackgroundColorChangeOrigin.BG_COLOR_UPDATE_AFTER_HIDING_NATIVE_PAGE);
        }
        mWaitingOnBgColorAfterHidingNativePage = false;
    }

    /**
     * @param backgroundColorChangeOrigin The origin of the background color change update. This is
     *     used to track the number of color changes and the potential performance impact those
     *     entail.
     */
    private void onBackgroundColorChanged(
            @BackgroundColorChangeOrigin int backgroundColorChangeOrigin) {
        RecordHistogram.recordEnumeratedHistogram(
                BACKGROUND_COLOR_CHANGE_PRE_OPTIMIZATION_HISTOGRAM,
                backgroundColorChangeOrigin,
                BackgroundColorChangeOrigin.NUM_ENTRIES);

        int newBackgroundColor = getBackgroundColor();
        // Avoid notifying the observers if the background color hasn't actually changed.
        if (mTabBackgroundColor == newBackgroundColor
                && ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()) return;

        mTabBackgroundColor = newBackgroundColor;

        RecordHistogram.recordEnumeratedHistogram(
                BACKGROUND_COLOR_CHANGE_HISTOGRAM,
                backgroundColorChangeOrigin,
                BackgroundColorChangeOrigin.NUM_ENTRIES);
        for (TabObserver observer : mObservers) {
            observer.onBackgroundColorChanged(this, mTabBackgroundColor);
        }
    }

    /** This is currently used when restoring tabs, and by DOMDistiller */
    @CalledByNative
    void swapWebContents(WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        boolean hasWebContents = mContentView != null && mWebContents != null;
        Rect original =
                hasWebContents
                        ? new Rect(0, 0, mContentView.getWidth(), mContentView.getHeight())
                        : new Rect();
        for (TabObserver observer : mObservers) observer.webContentsWillSwap(this);
        if (hasWebContents) mWebContents.updateWebContentsVisibility(Visibility.HIDDEN);
        Context appContext = ContextUtils.getApplicationContext();
        Rect bounds = original.isEmpty() ? TabUtils.estimateContentSize(appContext) : null;
        if (bounds != null) original.set(bounds);

        mWebContents.setFocus(false);
        destroyWebContents(false /* do not delete native web contents */);
        hideNativePage(
                false,
                () -> {
                    // Size of the new content is zero at this point. Set the view size in advance
                    // so that next onShow() call won't send a resize message with zero size
                    // to the renderer process. This prevents the size fluttering that may confuse
                    // Blink and break rendered result (see http://crbug.com/340987).
                    webContents.setSize(original.width(), original.height());

                    if (bounds != null) {
                        assert mNativeTabAndroid != 0;
                        TabImplJni.get()
                                .onPhysicalBackingSizeChanged(
                                        mNativeTabAndroid,
                                        webContents,
                                        bounds.right,
                                        bounds.bottom);
                    }
                    initWebContents(webContents);
                    webContents.updateWebContentsVisibility(Visibility.VISIBLE);
                });

        if (didStartLoad) {
            // Simulate the PAGE_LOAD_STARTED notification that we did not get.
            didStartPageLoad(getUrl());

            // Simulate the PAGE_LOAD_FINISHED notification that we did not get.
            if (didFinishLoad) didFinishPageLoad(getUrl());
        }

        for (TabObserver observer : mObservers) {
            observer.onWebContentsSwapped(this, didStartLoad, didFinishLoad);
        }
    }

    /** Builds the native counterpart to this class. */
    private void initializeNative() {
        if (mNativeTabAndroid == 0) {
            TabImplJni.get().init(TabImpl.this, mProfile, mId);
        }
        assert mNativeTabAndroid != 0;
    }

    /**
     * @return The native pointer representing the native side of this {@link TabImpl} object.
     */
    @CalledByNative
    private long getNativePtr() {
        return mNativeTabAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        assert mNativeTabAndroid != 0;
        mNativeTabAndroid = 0;
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert nativePtr != 0;
        assert mNativeTabAndroid == 0;
        mNativeTabAndroid = nativePtr;
    }

    @CalledByNative
    private long getLastShownTimestamp() {
        return mTimestampMillis;
    }

    @CalledByNative
    private static long[] getAllNativePtrs(Tab[] tabsArray) {
        if (tabsArray == null) return null;

        long[] tabsPtrArray = new long[tabsArray.length];
        for (int i = 0; i < tabsArray.length; i++) {
            tabsPtrArray[i] = ((TabImpl) tabsArray[i]).getNativePtr();
        }
        return tabsPtrArray;
    }

    @CalledByNative
    private ByteBuffer getWebContentsStateByteBuffer() {
        // Return a temp byte buffer if the state is null.
        if (mWebContentsState == null) {
            return ByteBuffer.allocateDirect(0);
        }
        assert mWebContentsState.buffer().isDirect();
        return mWebContentsState.buffer();
    }

    @CalledByNative
    private int getWebContentsStateSavedStateVersion() {
        // Return an invalid saved state version if the state is null.
        return mWebContentsState == null ? -1 : mWebContentsState.version();
    }

    /**
     * Initializes the {@link WebContents}. Completes the browser content components initialization
     * around a native WebContents pointer.
     * <p>
     * {@link #getNativePage()} will still return the {@link NativePage} if there is one.
     * All initialization that needs to reoccur after a web contents swap should be added here.
     * <p />
     * NOTE: If you attempt to pass a native WebContents that does not have the same incognito
     * state as this tab this call will fail.
     *
     * @param webContents The WebContents object that will initialize all the browser components.
     */
    private void initWebContents(@NonNull WebContents webContents) {
        try {
            TraceEvent.begin("ChromeTab.initWebContents");
            WebContents oldWebContents = mWebContents;
            mWebContents = webContents;

            ContentView cv = ContentView.createContentView(mThemedApplicationContext, webContents);
            cv.setContentDescription(
                    mThemedApplicationContext
                            .getResources()
                            .getString(R.string.accessibility_content_view));
            mContentView = cv;
            webContents.setDelegates(
                    PRODUCT_VERSION,
                    new TabViewAndroidDelegate(this, cv),
                    cv,
                    getWindowAndroid(),
                    WebContents.createDefaultInternalsHolder());
            hideNativePage(false, null);

            if (oldWebContents != null) {
                oldWebContents.setImportance(ChildProcessImportance.NORMAL);
                getWebContentsAccessibility(oldWebContents).setObscuredByAnotherView(false);
            }

            mWebContents.setImportance(mImportance);

            ContentUtils.setUserAgentOverride(
                    mWebContents,
                    calculateUserAgentOverrideOption(null) == UserAgentOverrideOption.TRUE);

            mContentView.addOnAttachStateChangeListener(mAttachStateChangeListener);
            updateInteractableState();

            mWebContentsDelegate = createWebContentsDelegate();

            // TODO(crbug.com/40942165): Find a better way of indicating this is a background tab
            // (or
            // move the logic elsewhere).
            boolean isBackgroundTab = isDetached();

            assert mNativeTabAndroid != 0;
            TabImplJni.get()
                    .initWebContents(
                            mNativeTabAndroid,
                            isOffTheRecord(),
                            isBackgroundTab,
                            webContents,
                            mWebContentsDelegate,
                            new TabContextMenuPopulatorFactory(
                                    mDelegateFactory.createContextMenuPopulatorFactory(this),
                                    this));

            mWebContents.notifyRendererPreferenceUpdate();
            mContentView.setImportantForAutofill(
                    prepareAutofillProvider(webContents)
                            ? View.IMPORTANT_FOR_AUTOFILL_YES
                            : View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
            TabHelpers.initWebContentsHelpers(this);
            notifyContentChanged();
        } finally {
            TraceEvent.end("ChromeTab.initWebContents");
        }
    }

    private TabWebContentsDelegateAndroidImpl createWebContentsDelegate() {
        TabWebContentsDelegateAndroid delegate = mDelegateFactory.createWebContentsDelegate(this);
        return new TabWebContentsDelegateAndroidImpl(this, delegate);
    }

    /**
     * Shows the given {@code nativePage} if it's not already showing.
     * @param nativePage The {@link NativePage} to show.
     */
    private void showNativePage(NativePage nativePage) {
        assert nativePage != null;
        if (mNativePage == nativePage) return;
        hideNativePage(
                true,
                () -> {
                    mNativePage = nativePage;
                    if (!mNativePage.isFrozen()) {
                        mNativePage
                                .getView()
                                .addOnAttachStateChangeListener(mAttachStateChangeListener);
                    }
                    if (isDisplayingBackForwardAnimation()) {
                        assert ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS)
                                : "Must not draw bf screenshot if back forward transition is"
                                        + " disabled";
                        mNativePageSmoothTransitionDelegate = mNativePage.enableSmoothTransition();
                        mNativePageSmoothTransitionDelegate.prepare();
                    }
                    pushNativePageStateToNavigationEntry();

                    if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()) {
                        onBackgroundColorChanged(BackgroundColorChangeOrigin.NATIVE_PAGE_SHOWN);
                    }
                    updateThemeColor(TabState.UNSPECIFIED_THEME_COLOR);
                });
    }

    /**
     * Hide and destroy the native page if it was being shown.
     *
     * @param notify {@code true} to trigger {@link #onContentChanged} event.
     * @param postHideTask {@link Runnable} task to run before actually destroying the native page.
     *     This is necessary to keep the tasks to perform in order.
     */
    private void hideNativePage(boolean notify, Runnable postHideTask) {
        if (mNativePageSmoothTransitionDelegate != null) {
            mNativePageSmoothTransitionDelegate.cancel();
            mNativePageSmoothTransitionDelegate = null;
        } else if (isNativePage() && getView() != null) {
            getView().setAlpha(1.f);
        }
        NativePage previousNativePage = mNativePage;
        if (mNativePage != null) {
            if (!mNativePage.isFrozen()) {
                mNativePage.getView().removeOnAttachStateChangeListener(mAttachStateChangeListener);
            }
            mNativePage = null;
            mWaitingOnBgColorAfterHidingNativePage = true;
        }
        if (postHideTask != null) postHideTask.run();
        if (notify) notifyContentChanged();
        destroyNativePageInternal(previousNativePage);
    }

    /**
     * Set {@link TabDelegateFactory} instance and updates the references.
     * @param factory TabDelegateFactory instance.
     */
    private void setDelegateFactory(TabDelegateFactory factory) {
        // Update the delegate factory, then recreate and propagate all delegates.
        mDelegateFactory = factory;

        mWebContentsDelegate = createWebContentsDelegate();

        WebContents webContents = getWebContents();
        if (webContents != null) {
            TabImplJni.get()
                    .updateDelegates(
                            mNativeTabAndroid,
                            mWebContentsDelegate,
                            new TabContextMenuPopulatorFactory(
                                    mDelegateFactory.createContextMenuPopulatorFactory(this),
                                    this));
            webContents.notifyRendererPreferenceUpdate();
        }
    }

    private void notifyPageTitleChanged() {
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onTitleUpdated(this);
        }
    }

    private void notifyFaviconChanged() {
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onFaviconUpdated(this, null, null);
        }
    }

    /**
     * Update the interactable state of the tab. If the state has changed, it will call the
     * {@link #onInteractableStateChanged(boolean)} method.
     */
    private void updateInteractableState() {
        boolean currentState =
                !mIsHidden && !isFrozen() && mIsViewAttachedToWindow && !isDetached();

        if (currentState == mInteractableState) return;

        mInteractableState = currentState;
        for (TabObserver observer : mObservers) {
            observer.onInteractabilityChanged(this, currentState);
        }
    }

    /**
     * Loads a tab that was already loaded but since then was lost. This happens either when we
     * unfreeze the tab from serialized state or when we reload a tab that crashed. In both cases
     * the load codepath is the same (run in loadIfNecessary()) and the same caching policies of
     * history load are used.
     */
    private void restoreIfNeeded(@TabLoadIfNeededCaller int caller) {
        // Attempts to display the Paint Preview representation of this Tab.
        if (isFrozen()) StartupPaintPreviewHelper.showPaintPreviewOnRestore(this);

        try {
            TraceEvent.begin("Tab.restoreIfNeeded");
            assert !isFrozen() || mWebContentsState != null
                    : "crbug/1393848: A frozen tab must have WebContentsState to restore from.";
            // Restore is needed for a tab that is loaded for the first time. WebContents will
            // be restored from a saved state.
            if ((isFrozen() && mWebContentsState != null && !unfreezeContents())
                    || !needsReload()) {
                return;
            }

            if (mWebContents != null) {
                // Invoke switchUserAgentIfNeeded() from restoreIfNeeded() instead of loadIfNeeded()
                // to avoid reload without explicit user intent.
                switchUserAgentIfNeeded(UseDesktopUserAgentCaller.LOAD_IF_NEEDED + caller);
                mWebContents.getNavigationController().loadIfNecessary();
            }
            mIsBeingRestored = true;
            for (TabObserver observer : mObservers) observer.onRestoreStarted(this);
        } finally {
            TraceEvent.end("Tab.restoreIfNeeded");
        }
    }

    /**
     * Restores the WebContents from its saved state.  This should only be called if the tab is
     * frozen with a saved TabState, and NOT if it was frozen for a lazy load.
     * @return Whether or not the restoration was successful.
     */
    private boolean unfreezeContents() {
        boolean restored = true;
        try {
            TraceEvent.begin("Tab.unfreezeContents");
            assert mWebContentsState != null;

            WebContents webContents =
                    WebContentsStateBridge.restoreContentsFromByteBuffer(
                            mWebContentsState, isHidden());

            String failedRestoreUrl = UrlConstants.NTP_URL;
            if (webContents == null) {
                // State restore failed, just create a new empty web contents as that is the best
                // that can be done at this point.
                webContents = WebContentsFactory.createWebContents(mProfile, isHidden(), false);
                for (TabObserver observer : mObservers) observer.onRestoreFailed(this);
                restored = false;

                if (!mUrl.getSpec().isEmpty()) {
                    failedRestoreUrl = mUrl.getSpec();
                } else if (!TextUtils.isEmpty(
                        mWebContentsState.getFallbackUrlForRestorationFailure())) {
                    failedRestoreUrl = mWebContentsState.getFallbackUrlForRestorationFailure();
                }
            }
            Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                    getActivity().getCompositorViewHolderSupplier();
            View compositorView = compositorViewHolderSupplier.get();
            webContents.setSize(compositorView.getWidth(), compositorView.getHeight());

            mWebContentsState = null;
            initWebContents(webContents);

            if (!restored) {
                loadUrl(new LoadUrlParams(failedRestoreUrl, PageTransition.GENERATED));
            }
        } finally {
            TraceEvent.end("Tab.unfreezeContents");
        }
        return restored;
    }

    /**
     * Initializes the {@link AutofillProvider} so that it can provide a ViewStructure for the given
     * WebContents. If the provider existed already, it's only assigned the new WebContents.
     *
     * @param newWebContents The webcontents to prepare the provider for.
     * @return true if the the provider is available for the given WebContents.
     */
    private boolean prepareAutofillProvider(WebContents newWebContents) {
        assert isInitialized();
        if (!providesAutofillStructure()) {
            mAutofillProvider = null;
            return false; // Autofill provider can't be prepared.
        }
        if (mAutofillProvider != null) {
            // Provider already existed. Swapping contents suffices.
            mAutofillProvider.setWebContents(newWebContents);
        } else {
            mAutofillProvider =
                    new AutofillProvider(
                            getContext(),
                            mContentView,
                            newWebContents,
                            getContext().getString(R.string.app_name));
            TabImplJni.get().initializeAutofillIfNecessary(mNativeTabAndroid);
        }
        addAutofillItemsToSelectionActionMenu(newWebContents);
        return true;
    }

    private void addAutofillItemsToSelectionActionMenu(WebContents webContents) {
        assert webContents != null;
        assert mAutofillProvider != null;
        SelectionPopupController controller = SelectionPopupController.fromWebContents(webContents);
        if (controller == null) {
            return;
        }
        AutofillSelectionActionMenuDelegate selectionActionMenuDelegate =
                new AutofillSelectionActionMenuDelegate();
        selectionActionMenuDelegate.setAutofillSelectionMenuItemHelper(
                new AutofillSelectionMenuItemHelper(
                        ContextUtils.getApplicationContext(), mAutofillProvider));
        controller.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
    }

    @CalledByNative
    @Override
    public boolean isCustomTab() {
        ChromeActivity activity = getActivity();
        return activity != null && activity.isCustomTab();
    }

    @Override
    public long getTimestampMillis() {
        return mTimestampMillis;
    }

    private void setTimestampMillis(long timestampMillis) {
        mTimestampMillis = timestampMillis;
        for (TabObserver tabObserver : mObservers) {
            tabObserver.onTimestampChanged(this, timestampMillis);
        }
    }

    /**
     * @return parent identifier for the {@link Tab}
     */
    @Override
    public int getParentId() {
        return mParentId;
    }

    @Override
    public void setParentId(int parentId) {
        mParentId = parentId;
    }

    @Override
    public int getRootId() {
        return mRootId;
    }

    @Override
    public void setRootId(int rootId) {
        if (mRootId == rootId || isDestroyed()) return;
        mRootId = rootId;
        for (TabObserver observer : mObservers) {
            observer.onRootIdChanged(this, rootId);
        }
    }

    @Override
    public @Nullable Token getTabGroupId() {
        return mTabGroupId;
    }

    @Override
    public void setTabGroupId(@Nullable Token tabGroupId) {
        assert tabGroupId == null || !tabGroupId.isZero() : "A TabGroupId token must be non-zero.";
        if (Objects.equals(mTabGroupId, tabGroupId) || isDestroyed()) return;
        mTabGroupId = tabGroupId;
        for (TabObserver observer : mObservers) {
            observer.onTabGroupIdChanged(this, tabGroupId);
        }
    }

    @Override
    @CalledByNative
    public @TabUserAgent int getUserAgent() {
        return mUserAgent;
    }

    @Override
    public void setUserAgent(@TabUserAgent int userAgent) {
        mUserAgent = userAgent;
    }

    @Override
    public WebContentsState getWebContentsState() {
        return mWebContentsState;
    }

    @VisibleForTesting
    void setWebContentsState(WebContentsState webContentsState) {
        mWebContentsState = webContentsState;
    }

    @VisibleForTesting
    void setAutofillProvider(AutofillProvider autofillProvider) {
        mAutofillProvider = autofillProvider;
    }

    @VisibleForTesting
    protected void setTitle(String title) {
        mTitle = title;
    }

    public void setTimestampMillisForTesting(long timestamp) {
        mTimestampMillis = timestamp;
    }

    @Override
    public long getLastNavigationCommittedTimestampMillis() {
        return mLastNavigationCommittedTimestampMillis;
    }

    /**
     * Set the last hidden timestamp.
     *
     * @param lastNavigationCommittedTimestampMillis The timestamp when the tab was last interacted.
     */
    @VisibleForTesting
    public void setLastNavigationCommittedTimestampMillis(
            long lastNavigationCommittedTimestampMillis) {
        mLastNavigationCommittedTimestampMillis = lastNavigationCommittedTimestampMillis;
    }

    @Override
    public @TabLaunchType int getTabLaunchTypeAtCreation() {
        return mTabLaunchTypeAtCreation;
    }

    /**
     * Throws a RuntimeException. Useful for testing crash reports with obfuscated Java stacktraces.
     */
    private LoadUrlResult handleJavaCrash() {
        throw new RuntimeException("Intentional Java Crash");
    }

    /**
     * Delete navigation entries from frozen state matching the predicate.
     * @param predicate Handle for a deletion predicate interpreted by native code.
     *                  Only valid during this call frame.
     */
    @CalledByNative
    private void deleteNavigationEntriesFromFrozenState(long predicate) {
        if (mWebContentsState == null) return;
        WebContentsState newState =
                WebContentsStateBridge.deleteNavigationEntries(mWebContentsState, predicate);
        if (newState != null) {
            mWebContentsState = newState;
            notifyNavigationEntriesDeleted();
        }
    }

    private static WebContentsAccessibility getWebContentsAccessibility(WebContents webContents) {
        return webContents != null ? WebContentsAccessibility.fromWebContents(webContents) : null;
    }

    private void destroyNativePageInternal(NativePage nativePage) {
        if (nativePage == null) return;
        assert nativePage != mNativePage : "Attempting to destroy active page.";

        nativePage.destroy();
    }

    /**
     * Destroys the current {@link WebContents}.
     * @param deleteNativeWebContents Whether or not to delete the native WebContents pointer.
     */
    private final void destroyWebContents(boolean deleteNativeWebContents) {
        if (mWebContents == null) return;

        if (mAutofillProvider != null) {
            mAutofillProvider.destroy();
            mAutofillProvider = null;
        }

        mContentView.removeOnAttachStateChangeListener(mAttachStateChangeListener);
        mContentView = null;
        updateInteractableState();

        WebContents contentsToDestroy = mWebContents;
        if (contentsToDestroy.getViewAndroidDelegate() != null
                && contentsToDestroy.getViewAndroidDelegate() instanceof TabViewAndroidDelegate) {
            ((TabViewAndroidDelegate) contentsToDestroy.getViewAndroidDelegate()).destroy();
        }
        mWebContents = null;
        mWebContentsDelegate = null;

        assert mNativeTabAndroid != 0;
        if (deleteNativeWebContents) {
            // Destruction of the native WebContents will call back into Java to destroy the Java
            // WebContents.
            TabImplJni.get().destroyWebContents(mNativeTabAndroid);
        } else {
            // This branch is to not delete the WebContents, but just to release the WebContent from
            // the Tab and clear the WebContents for two different cases a) The WebContents will be
            // destroyed eventually, but from the native WebContents. b) The WebContents will be
            // reused later. We need to clear the reference to the Tab from WebContentsObservers or
            // the UserData. If the WebContents will be reused, we should set the necessary
            // delegates again.
            TabImplJni.get().releaseWebContents(mNativeTabAndroid);
            // This call is just a workaround, Chrome should clean up the WebContentsObservers
            // itself.
            contentsToDestroy.clearJavaWebContentsObservers();
            contentsToDestroy.setDelegates(
                    PRODUCT_VERSION,
                    ViewAndroidDelegate.createBasicDelegate(/* containerView= */ null),
                    /* accessDelegate= */ null,
                    /* windowAndroid= */ null,
                    WebContents.createDefaultInternalsHolder());
        }
    }

    private @UserAgentOverrideOption int calculateUserAgentOverrideOption(@Nullable GURL url) {
        WebContents webContents = getWebContents();
        boolean currentRequestDesktopSite = TabUtils.isUsingDesktopUserAgent(webContents);
        @TabUserAgent int tabUserAgent = TabUtils.getTabUserAgent(this);
        // INHERIT means use the same UA that was used last time.
        @UserAgentOverrideOption int userAgentOverrideOption = UserAgentOverrideOption.INHERIT;

        if (url == null && webContents != null) {
            url = webContents.getVisibleUrl();
        }

        // Do not override UA if there is a tab level setting.
        if (tabUserAgent != TabUserAgent.DEFAULT) {
            recordHistogramUseDesktopUserAgent(currentRequestDesktopSite);
            RequestDesktopUtils.maybeUpgradeTabLevelDesktopSiteSetting(
                    this, mProfile, tabUserAgent, url);
            return userAgentOverrideOption;
        }

        CommandLine commandLine = CommandLine.getInstance();
        // For --request-desktop-sites, always override the user agent.
        boolean alwaysRequestDesktopSite =
                commandLine.hasSwitch(ChromeSwitches.REQUEST_DESKTOP_SITES);

        boolean shouldRequestDesktopSite =
                alwaysRequestDesktopSite
                        || (TabUtils.readRequestDesktopSiteContentSettings(mProfile, url)
                                && !RequestDesktopUtils.shouldApplyWindowSetting(
                                        mProfile, url, getContext()));

        if (shouldRequestDesktopSite != currentRequestDesktopSite) {
            // The user is not forcing any mode and we determined that we need to
            // change, therefore we are using TRUE or FALSE option. On Android TRUE mean
            // override to Desktop user agent, while FALSE means go with Mobile version.
            userAgentOverrideOption =
                    shouldRequestDesktopSite
                            ? UserAgentOverrideOption.TRUE
                            : UserAgentOverrideOption.FALSE;
        }
        recordHistogramUseDesktopUserAgent(shouldRequestDesktopSite);
        return userAgentOverrideOption;
    }

    // TODO(crbug.com/40195571): Confirm if a new histogram should be used.
    private void recordHistogramUseDesktopUserAgent(boolean value) {
        RecordHistogram.recordBooleanHistogram(
                "Android.RequestDesktopSite.UseDesktopUserAgent", value);
    }

    private void switchUserAgentIfNeeded(int caller) {
        if (calculateUserAgentOverrideOption(null) == UserAgentOverrideOption.INHERIT
                || getWebContents() == null) {
            return;
        }
        boolean usingDesktopUserAgent =
                getWebContents().getNavigationController().getUseDesktopUserAgent();
        TabUtils.switchUserAgent(this, /* switchToDesktop= */ !usingDesktopUserAgent, caller);
    }

    /** Sets the TabLaunchType for tabs launched with an unset launch type. */
    @Override
    public void setTabLaunchType(@TabLaunchType int launchType) {
        assert mLaunchType == TabLaunchType.UNSET;
        mLaunchType = launchType;
    }

    @Override
    public boolean isDisplayingBackForwardAnimation() {
        if (getWebContents() == null) return false;
        return getWebContents().getCurrentBackForwardTransitionStage() != AnimationStage.NONE;
    }

    /**
     * Forces a resize of the web contents view to accommodate for browser controls immediately.
     *
     * <p>This is used to force the resize to happen at the same time as the controls are requested
     * to show (potentially animate) so that web content can be adapted to the controls sooner.
     */
    public void willShowBrowserControls() {
        assert mWebContents != null;
        boolean hasViewTransitionOptIn = mWebContents.hasViewTransitionOptIn();
        for (TabObserver observer : mObservers) {
            observer.onWillShowBrowserControls(this, hasViewTransitionOptIn);
        }
    }

    @CalledByNative
    @Override
    public boolean isTrustedWebActivity() {
        if (getWebContents() == null) return false;
        return mWebContentsDelegate.isTrustedWebActivity(getWebContents());
    }

    @Override
    public boolean shouldEnableEmbeddedMediaExperience() {
        if (mWebContentsDelegate == null) return false;
        return mWebContentsDelegate.shouldEnableEmbeddedMediaExperience();
    }

    @Override
    public boolean getTabHasSensitiveContent() {
        return mTabHasSensitiveContent;
    }

    @Override
    public void setTabHasSensitiveContent(boolean contentIsSensitive) {
        if (mTabHasSensitiveContent == contentIsSensitive || isDestroyed()) return;
        mTabHasSensitiveContent = contentIsSensitive;
        for (TabObserver observer : mObservers) {
            observer.onTabContentSensitivityChanged(this, contentIsSensitive);
        }
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        TabImpl fromWebContents(WebContents webContents);

        void init(TabImpl caller, @JniType("Profile*") Profile profile, int id);

        void destroy(long nativeTabAndroid);

        void initWebContents(
                long nativeTabAndroid,
                boolean isOffTheRecord,
                boolean isBackgroundTab,
                WebContents webContents,
                TabWebContentsDelegateAndroidImpl delegate,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory);

        void initializeAutofillIfNecessary(long nativeTabAndroid);

        void updateDelegates(
                long nativeTabAndroid,
                TabWebContentsDelegateAndroidImpl delegate,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory);

        void destroyWebContents(long nativeTabAndroid);

        void releaseWebContents(long nativeTabAndroid);

        void onPhysicalBackingSizeChanged(
                long nativeTabAndroid, WebContents webContents, int width, int height);

        void setActiveNavigationEntryTitleForUrl(long nativeTabAndroid, String url, String title);

        void loadOriginalImage(long nativeTabAndroid);

        boolean handleNonNavigationAboutURL(GURL url);

        void onShow(long nativeTabAndroid);
    }
}
