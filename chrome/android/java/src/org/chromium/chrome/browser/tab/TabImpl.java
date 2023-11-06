// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_PERIPHERAL_SETTING_ENABLED;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.native_page.NativePageAssassin;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;

/**
 * Implementation of the interface {@link Tab}. Contains and manages a {@link ContentView}.
 * This class is not intended to be extended.
 */
public class TabImpl implements Tab {
    /** Used for logging. */
    private static final String TAG = "Tab";

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

    /** {@link WebContents} showing the current page, or {@code null} if the tab is frozen. */
    private WebContents mWebContents;

    /** The parent view of the ContentView and the InfoBarContainer. */
    private ContentView mContentView;

    /** The view provided by {@link TabViewManager} to be shown on top of Content view. */
    private View mCustomView;

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

    /**
     * Tab id to be used as a source tab in SyncedTabDelegate.
     */
    private int mSourceTabId = INVALID_TAB_ID;

    private boolean mIsClosing;
    private boolean mIsShowingErrorPage;

    /**
     * Saves how this tab was launched (from a link, external app, etc) so that
     * we can determine the different circumstances in which it should be
     * closed. For example, a tab opened from an external app should be closed
     * when the back stack is empty and the user uses the back hardware key. A
     * standard tab however should be kept open and the entire activity should
     * be moved to the background.
     */
    private final @Nullable @TabLaunchType Integer mLaunchType;

    private @Nullable @TabCreationState Integer mCreationState;

    /**
     * URL load to be performed lazily when the Tab is next shown.
     */
    private LoadUrlParams mPendingLoadParams;

    /**
     * True while a page load is in progress.
     */
    private boolean mIsLoading;

    /**
     * True while a restore page load is in progress.
     */
    private boolean mIsBeingRestored;

    /**
     * Whether or not the Tab is currently visible to the user.
     */
    private boolean mIsHidden = true;

    /**
     * Importance of the WebContents currently attached to this tab. Note the key difference from
     * |mIsHidden| is that a tab is hidden when the application is hidden, but the importance is
     * not affected by this signal.
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

    /** Whether or not the tab's active view is attached to the window. */
    private boolean mIsViewAttachedToWindow;

    private final UserDataHost mUserDataHost = new UserDataHost();

    private boolean mIsDestroyed;

    private final TabThemeColorHelper mThemeColorHelper;
    private int mThemeColor;
    private boolean mIsWebContentObscured;
    private long mTimestampMillis;
    private int mParentId = INVALID_TAB_ID;
    private int mRootId;
    private @TabUserAgent int mUserAgent = TabUserAgent.DEFAULT;
    /**
     * Navigation state of the WebContents as returned by nativeGetContentsStateAsByteBuffer(),
     * stored to be inflated on demand using unfreezeContents(). If this is not null, there is no
     * WebContents around. Upon tab switch WebContents will be unfrozen and the variable will be set
     * to null.
     */
    private WebContentsState mWebContentsState;

    /**
     * Title of the ContentViews webpage.
     */
    private String mTitle;

    /**
     * URL of the page currently loading. Used as a fall-back in case tab restore fails.
     */
    private GURL mUrl;

    private long mLastNavigationCommittedTimestampMillis = INVALID_TIMESTAMP;

    /**
     * Saves how this tab was initially launched so that we can record metrics on how the
     * tab was created. This is different than {@link Tab#getLaunchType()}, since {@link
     * Tab#getLaunchType()} will be overridden to "FROM_RESTORE" during tab restoration.
     */
    private @Nullable @TabLaunchType Integer mTabLaunchTypeAtCreation;

    /**
     * Creates an instance of a {@link TabImpl}. Package-private. Use {@link TabBuilder} to create
     * an instance.
     *
     * @param id The id this tab should be identified with.
     * @param profile The profile associated with this Tab.
     * @param launchType Type indicating how this tab was launched.
     */
    @SuppressLint("HandlerLeak")
    TabImpl(int id, @NonNull Profile profile, @Nullable @TabLaunchType Integer launchType) {
        mId = TabIdManager.getInstance().generateValidId(id);
        mProfile = profile;
        assert mProfile != null;
        mRootId = mId;

        // Override the configuration for night mode to always stay in light mode until all UIs in
        // Tab are inflated from activity context instead of application context. This is to
        // avoid getting the wrong night mode state when application context inherits a system UI
        // mode different from the UI mode we need.
        // TODO(https://crbug.com/938641): Remove this once Tab UIs are all inflated from
        // activity.
        mThemedApplicationContext =
                NightModeUtils.wrapContextWithNightModeConfig(ContextUtils.getApplicationContext(),
                        ActivityUtils.getThemeId(), false /*nightMode*/);

        mLaunchType = launchType;

        mAttachStateChangeListener = new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                mIsViewAttachedToWindow = true;
                updateInteractableState();
            }

            @Override
            public void onViewDetachedFromWindow(View view) {
                mIsViewAttachedToWindow = false;
                updateInteractableState();
            }
        };
        mTabViewManager = new TabViewManagerImpl(this);
        mThemeColorHelper = new TabThemeColorHelper(this, this::updateThemeColor);
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
            if (isNativePage()) maybeShowNativePage(getUrl().getSpec(), true);
        }

        // Notify the event to observers only when we do the reparenting task, not when we simply
        // switch window in which case a new window is non-null but delegate is null.
        boolean notify = (window != null && tabDelegateFactory != null)
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

    /**
     * Sets a custom {@link View} for this {@link Tab} that replaces Content view.
     */
    void setCustomView(@Nullable View view) {
        mCustomView = view;
        notifyContentChanged();
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
        if (mTitle == null) updateTitle();
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
        if (mNativePage == null || mNativePage.isFrozen()
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
    public boolean isThemingAllowed() {
        // Do not apply the theme color if there are any security issues on the page.
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(getWebContents());
        boolean hasSecurityIssue = securityLevel == ConnectionSecurityLevel.DANGEROUS
            || securityLevel == ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT;
        // If chrome is showing an error page, allow theming so the system UI can match the page.
        // This is considered acceptable since chrome is in control of the error page. Otherwise, if
        // the page has a security issue, disable theming.
        return isShowingErrorPage() || !hasSecurityIssue;
    }

    @CalledByNative
    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
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
    public int loadUrl(LoadUrlParams params) {
        try {
            TraceEvent.begin("Tab.loadUrl");
            // TODO(tedchoc): When showing the android NTP, delay the call to
            // TabImplJni.get().loadUrl until the android view has entirely rendered.
            if (!mIsNativePageCommitPending) {
                mIsNativePageCommitPending = maybeShowNativePage(params.getUrl(), false);
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

            // TODO(https://crbug.com/783819): Don't fix up all URLs. Documentation on
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

            @TabLoadStatus
            int result = loadUrlInternal(params, fixedUrl);

            for (TabObserver observer : mObservers) {
                observer.onLoadUrl(this, params, result);
            }
            return result;
        } finally {
            TraceEvent.end("Tab.loadUrl");
        }
    }

    private @TabLoadStatus int loadUrlInternal(LoadUrlParams params, GURL fixedUrl) {
        if (mWebContents == null) return TabLoadStatus.PAGE_LOAD_FAILED;

        if (!fixedUrl.isValid()) return TabLoadStatus.PAGE_LOAD_FAILED;

        // Record UMA "ShowHistory" here. That way it'll pick up both user
        // typing chrome://history as well as selecting from the drop down menu.
        if (fixedUrl.getSpec().equals(UrlConstants.HISTORY_URL)) {
            RecordUserAction.record("ShowHistory");
        }

        if (TabImplJni.get().handleNonNavigationAboutURL(fixedUrl)) {
            return TabLoadStatus.DEFAULT_PAGE_LOAD;
        }

        params.setUrl(fixedUrl.getSpec());
        mWebContents.getNavigationController().loadUrl(params);
        return TabLoadStatus.DEFAULT_PAGE_LOAD;
    }

    @Override
    public boolean loadIfNeeded(@LoadIfNeededCaller int caller) {
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

        // If desktop mode window setting is enabled, move switchUserAgentIfNeeded() from
        // loadIfNeeded() to restoreIfNeeded(); to avoid reload without explicit user intent.
        if (!ContentFeatureMap.isEnabled(ContentFeatureList.REQUEST_DESKTOP_SITE_WINDOW_SETTING)) {
            switchUserAgentIfNeeded(UseDesktopUserAgentCaller.LOAD_IF_NEEDED + caller);
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
            OfflinePageUtils.reload(getWebContents(),
                    /*loadUrlDelegate=*/new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(this));
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
    public final void show(@TabSelectionType int type, @LoadIfNeededCaller int caller) {
        try {
            TraceEvent.begin("Tab.show");
            if (!isHidden()) return;
            // Keep unsetting mIsHidden above loadIfNeeded(), so that we pass correct visibility
            // when spawning WebContents in loadIfNeeded().
            mIsHidden = false;
            updateInteractableState();

            loadIfNeeded(caller);

            // TODO(crbug.com/1253938): We should provide a timestamp that apporoximates the input
            // event timestamp. When presenting a Tablet UI, StripLayoutTab.handleClick does
            // receive a timestamp. When presenting a Phone UI
            // TabGridViewBinder.bindClosableTabProperties is called by Android.View.performClick,
            // and does not receive the event timestamp. This currently triggers an animation in
            // TabSwitcherAndStartSurfaceLayout.startHidingImpl which lasts around 300ms.
            // TabSwitcherAndStartSurfaceLayout.doneHiding runs after the animation, actually
            // triggering this tab change.
            //
            // We should also consider merging the TabImpl and WebContents onShow into a single Jni
            // call.
            TabImplJni.get().onShow(mNativeTabAndroid);

            if (getWebContents() != null) getWebContents().onShow();

            // If the NativePage was frozen while in the background (see NativePageAssassin),
            // recreate the NativePage now.
            NativePage nativePage = getNativePage();
            if (nativePage != null && nativePage.isFrozen()) {
                maybeShowNativePage(nativePage.getUrl(), true);
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

            if (getWebContents() != null) getWebContents().onHide();

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
            int importantForAccessibility = obscureWebContent
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
     * Initializes {@link Tab} with {@code webContents}.  If {@code webContents} is {@code null}
     * a new {@link WebContents} will be created for this {@link Tab}.
     * @param parent The tab that caused this tab to be opened.
     * @param creationState State in which the tab is created.
     * @param loadUrlParams Parameters used for a lazily loaded Tab or null if we initialize a tab
     *         without an URL.
     * @param webContents A {@link WebContents} object or {@code null} if one should be created.
     * @param delegateFactory The {@link TabDelegateFactory} to be used for delegate creation.
     * @param initiallyHidden Only used if {@code webContents} is {@code null}.  Determines
     *        whether or not the newly created {@link WebContents} will be hidden or not.
     * @param tabState State containing information about this Tab, if it was persisted.
     * @param initializeRenderer Determines whether or not we initialize renderer with {@link
     *         WebContents} creation. The CREATE_NEW_TAB_INITIALIZE_RENDERER feature also controls
     *         this parameter, which initializes the renderer when it is enabled.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void initialize(Tab parent, @Nullable @TabCreationState Integer creationState,
            @Nullable LoadUrlParams loadUrlParams, WebContents webContents,
            @Nullable TabDelegateFactory delegateFactory, boolean initiallyHidden,
            TabState tabState, boolean initializeRenderer) {
        try {
            TraceEvent.begin("Tab.initialize");
            // If the feature is enabled, the renderer will always be initialized during the
            // WebContents creation. It is an experimental performance optimization to speed
            // up navigation.
            initializeRenderer = ChromeFeatureList.isEnabled(
                                         ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER)
                    ? true
                    : initializeRenderer;
            if (parent != null) {
                mParentId = parent.getId();
                mSourceTabId =
                        parent.isIncognito() == isIncognito() ? parent.getId() : INVALID_TAB_ID;
            }

            mTabLaunchTypeAtCreation = mLaunchType;
            mCreationState = creationState;
            mPendingLoadParams = loadUrlParams;
            if (loadUrlParams != null) {
                mUrl = new GURL(loadUrlParams.getUrl());
            }

            // The {@link mDelegateFactory} needs to be set before calling
            // {@link TabHelpers.initTabHelpers()}. This is because it creates a
            // TabBrowserControlsConstraintsHelper, and
            // {@link TabBrowserControlsConstraintsHelper#updateVisibilityDelegate()} will call the
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
                webContents = WarmupManager.getInstance().takeSpareWebContents(
                        isIncognito(), initiallyHidden);
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
        setUserAgent(state.userAgent);
    }

    /**
     * @return An {@link ObserverList.RewindableIterator} instance that points to all of
     *         the current {@link TabObserver}s on this class.  Note that calling
     *         {@link java.util.Iterator#remove()} will throw an
     *         {@link UnsupportedOperationException}.
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

    /**
     * Hides the current {@link NativePage}, if any, and shows the {@link WebContents}'s view.
     */
    void showRenderedPage() {
        updateTitle();
        if (mNativePage != null) hideNativePage(true, null);
    }

    void updateWindowAndroid(WindowAndroid windowAndroid) {
        // TODO(yusufo): mWindowAndroid can never be null until crbug.com/657007 is fixed.
        assert windowAndroid != null;
        mWindowAndroid = windowAndroid;
        WebContents webContents = getWebContents();
        if (webContents != null) webContents.setTopLevelNativeWindow(mWindowAndroid);
    }

    TabDelegateFactory getDelegateFactory() {
        return mDelegateFactory;
    }

    @VisibleForTesting
    TabWebContentsDelegateAndroidImpl getTabWebContentsDelegateAndroid() {
        return mWebContentsDelegate;
    }

    // Forwarded from TabWebContentsDelegateAndroid.

    /**
     * Called when a navigation begins and no navigation was in progress
     * @param toDifferentDocument Whether this navigation will transition between
     * documents (i.e., not a fragment navigation or JS History API call).
     */
    void onLoadStarted(boolean toDifferentDocument) {
        if (toDifferentDocument) mIsLoading = true;
        for (TabObserver observer : mObservers) observer.onLoadStarted(this, toDifferentDocument);
    }

    /**
     * Called when a navigation completes and no other navigation is in progress.
     */
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

    // Forwarded from TabWebContentsObserver.

    /**
     * Called when a page has started loading.
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
     * @param url The URL that was loaded.
     * @param transitionType The transition type to the current URL.
     */
    void handleDidFinishNavigation(GURL url, int transitionType) {
        mIsNativePageCommitPending = false;
        boolean isReload = (transitionType & PageTransition.CORE_MASK) == PageTransition.RELOAD;
        if (!maybeShowNativePage(url.getSpec(), isReload)) {
            showRenderedPage();
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

    /**
     * Add a new navigation entry for the current URL and page title.
     */
    void pushNativePageStateToNavigationEntry() {
        assert mNativeTabAndroid != 0 && getNativePage() != null;
        TabImplJni.get().setActiveNavigationEntryTitleForUrl(
                mNativeTabAndroid, getNativePage().getUrl(), getNativePage().getTitle());
    }

    /**
     * Set whether the Tab needs to be reloaded.
     */
    void setNeedsReload() {
        assert getWebContents() != null;
        getWebContents().getNavigationController().setNeedsReload();
    }

    /**
     * Called when navigation entries were removed.
     */
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

    /**
     * Load the original image (uncompressed by spdy proxy) in this tab.
     */
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
     * @param url The url of the current navigation.
     * @param forceReload If true, the current native page (if any) will not be reused, even if it
     *                    matches the URL.
     * @return True, if a native page was displayed for url.
     */
    boolean maybeShowNativePage(String url, boolean forceReload) {
        // While detached for reparenting we don't have an owning Activity, or TabModelSelector,
        // so we can't create the native page. The native page will be created once reparenting is
        // completed.
        if (TabUtils.isDetached(this)) return false;
        NativePage candidateForReuse = forceReload ? null : getNativePage();
        NativePage nativePage = mDelegateFactory.createNativePage(url, candidateForReuse, this);
        if (nativePage != null) {
            showNativePage(nativePage);
            notifyPageTitleChanged();
            notifyFaviconChanged();
            return true;
        }
        return false;
    }

    /**
     * Calls onContentChanged on all TabObservers and updates accessibility visibility.
     */
    void notifyContentChanged() {
        for (TabObserver observer : mObservers) observer.onContentChanged(this);
    }

    void updateThemeColor(int themeColor) {
        if (mThemeColor == themeColor) return;
        mThemeColor = themeColor;
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) observers.next().onDidChangeThemeColor(this, themeColor);
    }

    void updateTitle() {
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

    /**
     * Performs any subclass-specific tasks when the Tab crashes.
     */
    void handleTabCrash() {
        mIsLoading = false;

        RewindableIterator<TabObserver> observers = getTabObservers();
        // When the renderer crashes for a hidden spare tab, we can skip notifying the observers to
        // crash the underlying tab. This is because it is safe to keep the spare tab around without
        // a renderer process, and since the tab is hidden, we don't need to show a sad tab. When
        // the spare tab is used for navigation it will create a new renderer process.
        // TODO(crbug.com/1447250): Make this logic more robust for all hidden tab cases.
        if (!WarmupManager.getInstance().isSpareTab(this)) {
            while (observers.hasNext()) observers.next().onCrash(this);
        }
        mIsBeingRestored = false;
    }

    /**
     * Called when the background color for the content changes.
     * @param color The current for the background.
     */
    void onBackgroundColorChanged(int color) {
        for (TabObserver observer : mObservers) observer.onBackgroundColorChanged(this, color);
    }

    /**
     *  This is currently called when committing a pre-rendered page or activating a portal.
     */
    @CalledByNative
    void swapWebContents(WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        boolean hasWebContents = mContentView != null && mWebContents != null;
        Rect original = hasWebContents
                ? new Rect(0, 0, mContentView.getWidth(), mContentView.getHeight())
                : new Rect();
        for (TabObserver observer : mObservers) observer.webContentsWillSwap(this);
        if (hasWebContents) mWebContents.onHide();
        Context appContext = ContextUtils.getApplicationContext();
        Rect bounds = original.isEmpty() ? TabUtils.estimateContentSize(appContext) : null;
        if (bounds != null) original.set(bounds);

        mWebContents.setFocus(false);
        destroyWebContents(false /* do not delete native web contents */);
        hideNativePage(false, () -> {
            // Size of the new content is zero at this point. Set the view size in advance
            // so that next onShow() call won't send a resize message with zero size
            // to the renderer process. This prevents the size fluttering that may confuse
            // Blink and break rendered result (see http://crbug.com/340987).
            webContents.setSize(original.width(), original.height());

            if (bounds != null) {
                assert mNativeTabAndroid != 0;
                TabImplJni.get().onPhysicalBackingSizeChanged(
                        mNativeTabAndroid, webContents, bounds.right, bounds.bottom);
            }
            initWebContents(webContents);
            webContents.onShow();
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

    /**
     * Builds the native counterpart to this class.
     */
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
            byte[] bytes = new byte[0];
            return ByteBuffer.wrap(bytes);
        }
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

            ContentView cv = ContentView.createContentView(
                    mThemedApplicationContext, null /* eventOffsetHandler */, webContents);
            cv.setContentDescription(mThemedApplicationContext.getResources().getString(
                    R.string.accessibility_content_view));
            mContentView = cv;
            webContents.initialize(PRODUCT_VERSION, new TabViewAndroidDelegate(this, cv), cv,
                    getWindowAndroid(), WebContents.createDefaultInternalsHolder());
            hideNativePage(false, null);

            if (oldWebContents != null) {
                oldWebContents.setImportance(ChildProcessImportance.NORMAL);
                getWebContentsAccessibility(oldWebContents).setObscuredByAnotherView(false);
            }

            mWebContents.setImportance(mImportance);

            ContentUtils.setUserAgentOverride(mWebContents,
                    calculateUserAgentOverrideOption(null) == UserAgentOverrideOption.TRUE);

            mContentView.addOnAttachStateChangeListener(mAttachStateChangeListener);
            updateInteractableState();

            mWebContentsDelegate = createWebContentsDelegate();

            assert mNativeTabAndroid != 0;
            TabImplJni.get()
                    .initWebContents(
                            mNativeTabAndroid,
                            isIncognito(),
                            TabUtils.isDetached(this),
                            webContents,
                            mWebContentsDelegate,
                            new TabContextMenuPopulatorFactory(
                                    mDelegateFactory.createContextMenuPopulatorFactory(this),
                                    this));

            mWebContents.notifyRendererPreferenceUpdate();
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
        hideNativePage(true, () -> {
            mNativePage = nativePage;
            if (!mNativePage.isFrozen()) {
                mNativePage.getView().addOnAttachStateChangeListener(mAttachStateChangeListener);
            }
            pushNativePageStateToNavigationEntry();

            updateThemeColor(TabState.UNSPECIFIED_THEME_COLOR);
        });
    }

    /**
     * Hide and destroy the native page if it was being shown.
     * @param notify {@code true} to trigger {@link #onContentChanged} event.
     * @param postHideTask {@link Runnable} task to run before actually destroying the
     *        native page. This is necessary to keep the tasks to perform in order.
     */
    private void hideNativePage(boolean notify, Runnable postHideTask) {
        NativePage previousNativePage = mNativePage;
        if (mNativePage != null) {
            if (!mNativePage.isFrozen()) {
                mNativePage.getView().removeOnAttachStateChangeListener(mAttachStateChangeListener);
            }
            mNativePage = null;
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
            TabImplJni.get().updateDelegates(mNativeTabAndroid, mWebContentsDelegate,
                    new TabContextMenuPopulatorFactory(
                            mDelegateFactory.createContextMenuPopulatorFactory(this), this));
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
                !mIsHidden && !isFrozen() && mIsViewAttachedToWindow && !TabUtils.isDetached(this);

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
    private void restoreIfNeeded(@LoadIfNeededCaller int caller) {
        // Attempts to display the Paint Preview representation of this Tab.
        if (isFrozen()) StartupPaintPreviewHelper.showPaintPreviewOnRestore(this);

        try {
            TraceEvent.begin("Tab.restoreIfNeeded");
            assert !isFrozen()
                    || mWebContentsState
                            != null
                : "crbug/1393848: A frozen tab must have WebContentsState to restore from.";
            // Restore is needed for a tab that is loaded for the first time. WebContents will
            // be restored from a saved state.
            if ((isFrozen() && mWebContentsState != null && !unfreezeContents())
                    || !needsReload()) {
                return;
            }

            if (mWebContents != null) {
                // If desktop mode window setting is enabled, move switchUserAgentIfNeeded() from
                // loadIfNeeded() to restoreIfNeeded(); to avoid reload without explicit user
                // intent.
                if (ContentFeatureMap.isEnabled(
                            ContentFeatureList.REQUEST_DESKTOP_SITE_WINDOW_SETTING)) {
                    switchUserAgentIfNeeded(UseDesktopUserAgentCaller.LOAD_IF_NEEDED + caller);
                }
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

            WebContents webContents = WebContentsStateBridge.restoreContentsFromByteBuffer(
                    mWebContentsState, isHidden());
            if (webContents == null) {
                // State restore failed, just create a new empty web contents as that is the best
                // that can be done at this point. TODO(jcivelli) http://b/5910521 - we should show
                // an error page instead of a blank page in that case (and the last loaded URL).
                webContents = WebContentsFactory.createWebContents(mProfile, isHidden(), false);
                for (TabObserver observer : mObservers) observer.onRestoreFailed(this);
                restored = false;
            }
            Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                    getActivity().getCompositorViewHolderSupplier();
            View compositorView = compositorViewHolderSupplier.get();
            webContents.setSize(compositorView.getWidth(), compositorView.getHeight());

            mWebContentsState = null;
            initWebContents(webContents);

            if (!restored) {
                String url = mUrl.getSpec().isEmpty() ? UrlConstants.NTP_URL : mUrl.getSpec();
                loadUrl(new LoadUrlParams(url, PageTransition.GENERATED));
            }
        } finally {
            TraceEvent.end("Tab.unfreezeContents");
        }
        return restored;
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
    public void setWebContentsState(WebContentsState webContentsState) {
        mWebContentsState = webContentsState;
    }

    @VisibleForTesting
    protected void setTitle(String title) {
        mTitle = title;
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
    public @Nullable @TabLaunchType Integer getTabLaunchTypeAtCreation() {
        return mTabLaunchTypeAtCreation;
    }

    /**
     * Throws a RuntimeException. Useful for testing crash reports with obfuscated Java stacktraces.
     */
    private int handleJavaCrash() {
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
            contentsToDestroy.initialize(PRODUCT_VERSION,
                    ViewAndroidDelegate.createBasicDelegate(/* containerView */ null),
                    /* accessDelegate */ null, /* windowAndroid */ null,
                    WebContents.createDefaultInternalsHolder());
        }
    }

    private @UserAgentOverrideOption int calculateUserAgentOverrideOption(@Nullable GURL url) {
        WebContents webContents = getWebContents();
        boolean currentRequestDesktopSite = TabUtils.isUsingDesktopUserAgent(webContents);
        @TabUserAgent
        int tabUserAgent = TabUtils.getTabUserAgent(this);
        // INHERIT means use the same UA that was used last time.
        @UserAgentOverrideOption
        int userAgentOverrideOption = UserAgentOverrideOption.INHERIT;

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

        if (!shouldRequestDesktopSite
                && ContentFeatureMap.isEnabled(ContentFeatureList.REQUEST_DESKTOP_SITE_ADDITIONS)) {
            // TODO(shuyng): Make additional setting compatible with site level setting.
            PrefService prefService = UserPrefs.get(mProfile);
            boolean peripheralPref =
                    prefService.getBoolean(DESKTOP_SITE_PERIPHERAL_SETTING_ENABLED);
            shouldRequestDesktopSite = TabUtils.isHardwareKeyboardAvailable(this) && peripheralPref;
        }
        if (shouldRequestDesktopSite != currentRequestDesktopSite) {
            // The user is not forcing any mode and we determined that we need to
            // change, therefore we are using TRUE or FALSE option. On Android TRUE mean
            // override to Desktop user agent, while FALSE means go with Mobile version.
            userAgentOverrideOption = shouldRequestDesktopSite ? UserAgentOverrideOption.TRUE
                                                               : UserAgentOverrideOption.FALSE;
        }
        recordHistogramUseDesktopUserAgent(shouldRequestDesktopSite);
        return userAgentOverrideOption;
    }

    // TODO(crbug.com/1243758): Confirm if a new histogram should be used.
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
        TabUtils.switchUserAgent(this, /* switchToDesktop */ !usingDesktopUserAgent,
                /* forcedByUser */ false, caller);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        TabImpl fromWebContents(WebContents webContents);
        void init(TabImpl caller, Profile profile, int id);
        void destroy(long nativeTabAndroid);
        void initWebContents(long nativeTabAndroid, boolean incognito, boolean isBackgroundTab,
                WebContents webContents, TabWebContentsDelegateAndroidImpl delegate,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory);
        void updateDelegates(long nativeTabAndroid, TabWebContentsDelegateAndroidImpl delegate,
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
