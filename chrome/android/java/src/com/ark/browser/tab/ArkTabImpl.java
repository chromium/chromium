// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.UserAgentManager;
import com.ark.browser.core.utils.ContentUtils;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.IPageGroup;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.PageImpl;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextmenu.ContextMenuNativeDelegate;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuPopulator;
import org.chromium.chrome.browser.tab.TabContextMenuPopulatorFactory;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabImportanceManager;
import org.chromium.chrome.browser.tab.TabJni;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.TabViewAndroidDelegate;
import org.chromium.chrome.browser.tab.TabViewManager;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroidImpl;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.WebContentsStateBridge;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of the interface {@link Tab}. Contains and manages a {@link ContentView}.
 * This class is not intended to be extended.
 */
public class ArkTabImpl implements Tab, TabObscuringHandler.Observer {
    private static final long INVALID_TIMESTAMP = -1;

    /**
     * Used for logging.
     */
    private static final String TAG = "ArkTab";

    private static final String PRODUCT_VERSION = VersionInfo.getProductVersion();

    private static final String REQUEST_DESKTOP_ENABLED_PARAM = "enabled";

    private long mNativeTabAndroid;

//    /** Unique id of this tab (within its container). */
//    private final int mId;
//
//    /** Whether or not this tab is an incognito tab. */
//    private final boolean mIncognito;

    /**
     * Gives {@link Tab} a way to interact with the Android window.
     */
    private ArkWindowAndroid mWindowAndroid;

    private ArkWebContents mArkWeb;

    /**
     * The view provided by {@link TabViewManager} to be shown on top of Content view.
     */
    private View mCustomView;

    /**
     * The {@link TabViewManager} associated with this Tab that is responsible for managing custom
     * views.
     */
    private final ArkTabViewManagerImpl mTabViewManager;

    /**
     * A list of Tab observers.  These are used to broadcast Tab events to listeners.
     */
    @VisibleForTesting()
    protected final ObserverList<TabObserver> mObservers = new ObserverList<>();

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
    private final @NonNull
    @TabLaunchType
    Integer mLaunchType;

    private @Nullable
    @TabCreationState
    Integer mCreationState;

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
    private @ChildProcessImportance
    int mImportance = ChildProcessImportance.NORMAL;

    /**
     * Whether the renderer is currently unresponsive.
     */
    private boolean mIsRendererUnresponsive;

//    private TabDelegateFactory mDelegateFactory;

    /**
     * Listens for views related to the tab to be attached or detached.
     */
    public final OnAttachStateChangeListener mAttachStateChangeListener;

    /**
     * Whether the tab can currently be interacted with.
     */
    private boolean mInteractableState;

    /**
     * Whether or not the tab's active view is attached to the window.
     */
    private boolean mIsViewAttachedToWindow;

    private final UserDataHost mUserDataHost = new UserDataHost();

    private boolean mIsDestroyed;
    private ObservableSupplierImpl<Boolean> mIsTabSaveEnabledSupplier =
            new ObservableSupplierImpl<>();

    private final TabThemeColorHelper mThemeColorHelper;
    private int mThemeColor;

    private final TabWebContentsDelegateAndroidImpl mWebContentsDelegate = new TabWebContentsDelegateAndroidImpl(this, null);

    private final ITab mTab;
    private final TabInfo mTabInfo;

    public ArkTabImpl(ITab tab, @NonNull @TabLaunchType Integer launchType) {
        mIsTabSaveEnabledSupplier.set(false);

        mTab = tab;
        mTabInfo = tab.getTabInfo();

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
        mTabViewManager = new ArkTabViewManagerImpl(this);
        mThemeColorHelper = new TabThemeColorHelper(this, this::updateThemeColor);
        mThemeColor = TabState.UNSPECIFIED_THEME_COLOR;
    }

    /**
     * Initializes {@link Tab} with {@code webContents}.  If {@code webContents} is {@code null}
     * a new {@link WebContents} will be created for this {@link Tab}.
     *
     * @param parent          The tab that caused this tab to be opened.
     * @param creationState   State in which the tab is created.
     * @param loadUrlParams   Parameters used for a lazily loaded Tab.
     * @param webContents     A {@link WebContents} object or {@code null} if one should be created.
     * @param delegateFactory The {@link TabDelegateFactory} to be used for delegate creation.
     * @param initiallyHidden Only used if {@code webContents} is {@code null}.  Determines
     *                        whether or not the newly created {@link WebContents} will be hidden or not.
     * @param tabState        State containing information about this Tab, if it was persisted.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void initialize(Tab parent, @Nullable @TabCreationState Integer creationState,
                           LoadUrlParams loadUrlParams, WebContents webContents, boolean initiallyHidden,
                           TabState tabState) {
        ArkLogger.e(TAG, "initialize this=" + this);
        if (isInitialized()) {
            return;
        }
        try {
            TraceEvent.begin("Tab.initialize");

            if (parent != null) {
                mTabInfo.setParentId(parent.getId());
                CriticalPersistedTabData.from(this).setParentId(parent.getId());
                mSourceTabId = parent.isIncognito() == mTabInfo.isIncognito() ? parent.getId() : INVALID_TAB_ID;
            }

            CriticalPersistedTabData.from(this).setLaunchTypeAtCreation(mLaunchType);
            mCreationState = creationState;
            mPendingLoadParams = loadUrlParams;
            if (loadUrlParams != null) {
                CriticalPersistedTabData.from(this).setUrl(new GURL(loadUrlParams.getUrl()));
            }

//            // TODO 去掉mDelegateFactory有没有问题？
//            // The {@link mDelegateFactory} needs to be set before calling
//            // {@link TabHelpers.initTabHelpers()}. This is because it creates a
//            // TabBrowserControlsConstraintsHelper, and
//            // {@link TabBrowserControlsConstraintsHelper#updateVisibilityDelegate()} will call the
//            // Tab#getDelegateFactory().createBrowserControlsVisibilityDelegate().
//            // See https://crbug.com/1179419.
//            mDelegateFactory = delegateFactory;

            ArkTabHelpers.initTabHelpers(this, parent);

            if (tabState != null) {
                restoreFieldsFromState(tabState);
            }

            initializeNative();

            // If there is a frozen WebContents state or a pending lazy load, don't create a new
            // WebContents. Restoring will be done when showing the tab in the foreground.
            if (CriticalPersistedTabData.from(this).getWebContentsState() != null
                    || getPendingLoadParams() != null) {
                return;
            }


            if (webContents != null) {
                ArkWebContents arkWeb = new ArkWebContents(getPageInfo(), webContents);
                initWebContents(arkWeb, mWindowAndroid);
                // Avoid an empty title by updating the title here. This could happen if restoring from
                // a WebContents that has no renderer and didn't force a reload. This happens on
                // background tab creation from Recent Tabs (TabRestoreService).
                updateTitle();

                if (webContents.shouldShowLoadingUI()) {
                    didStartPageLoad(webContents.getVisibleUrl());
                }
            }
//            else if (loadUrlParams != null) {
//                PageInfo pageInfo = PageInfo.from(mTabInfo.getParentId(), getId(),
//                        mTabInfo.getIndex() + 1, isIncognito());
//                ArkWebContents arkWeb = new ArkWebContents.Builder(pageInfo)
//                        .setLoadUrlParams(loadUrlParams)
//                        .setInitiallyHidden(isHidden())
//                        .build();
//                initWebContents(arkWeb, mWindowAndroid);
//
//                loadUrl(loadUrlParams);
//
//                updateTitle();
//
//                if (webContents.shouldShowLoadingUI()) {
//                    didStartPageLoad(webContents.getVisibleUrl());
//                }
//            }




//            boolean creatingWebContents = webContents == null;
//            ArkWebContents arkWeb;
//            if (creatingWebContents) {
//
//                arkWeb = new ArkWebContents.Builder(mPageInfo)
//                        .setInitiallyHidden(isHidden())
//                        .build();
//
////                webContents = WarmupManager.getInstance().takeSpareWebContents(
////                        isIncognito(), initiallyHidden, isCustomTab());
////                if (webContents == null) {
////                    Profile profile = IncognitoUtils.getProfileFromWindowAndroid(
////                            mWindowAndroid, isIncognito());
////                    webContents = WebContentsFactory.createWebContents(profile, initiallyHidden);
////                }
//            } else {
//                arkWeb = new ArkWebContents(webContents);
//            }
//
//            initWebContents(arkWeb, mWindowAndroid);
//            // Avoid an empty title by updating the title here. This could happen if restoring from
//            // a WebContents that has no renderer and didn't force a reload. This happens on
//            // background tab creation from Recent Tabs (TabRestoreService).
//            updateTitle();
//
//            if (!creatingWebContents && webContents.shouldShowLoadingUI()) {
//                didStartPageLoad(webContents.getVisibleUrl());
//            }

        } finally {
            if (CriticalPersistedTabData.from(this).getTimestampMillis() == INVALID_TIMESTAMP) {
                CriticalPersistedTabData.from(this).setTimestampMillis(System.currentTimeMillis());
            }
            registerTabSaving();
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

    public PageInfo getPageInfo() {
        return mTab.getCurrentPageInfo();
    }

    public TabInfo getTabInfo() {
        return mTabInfo;
    }

    public void loadInNewPage(LoadUrlParams params) {
        int index = mTabInfo.getPageIndex();
        int nextIndex = index + 1;
        PageInfo pageInfo = PageInfo.from(getId(), nextIndex, isIncognito());

        IPage page = new PageImpl(pageInfo);

        IPageGroup pageInfoList = mTab.getPageGroup();
        pageInfoList.getPageInfoList().add(nextIndex, page);

        if (++nextIndex < pageInfoList.getCount()) {
            List<IPage> pageRemoved = pageInfoList.getPageInfoList()
                    .subList(nextIndex, pageInfoList.getCount());

            List<IPage> tempPages = new ArrayList<>(pageRemoved);
            ThreadPool.postOnUIThread(() -> {
                long start = System.currentTimeMillis();
                ArkLogger.d(mTab, "openNewPage pageRemovedCount=" + tempPages.size());

                for (IPage info : tempPages) {
                    info.remove();
                }

                ArkLogger.d(mTab, "openNewPage pageRemoved deltaTime=" + (System.currentTimeMillis() - start));
            });
            pageRemoved.clear();
        }

        ArkWebContents arkWeb = new ArkWebContents.Builder(pageInfo)
                .setInitiallyHidden(isHidden())
                .build();
        swapWebContents(arkWeb, false, false);

        mTab.selectPage(page);

        GURL fixedUrl = UrlFormatter.fixupUrl(params.getUrl());
        params.setUrl(fixedUrl.getSpec());
        ContentUtils.setUserAgentOverride(arkWeb.getWebContents(), UserAgentManager.getUserAgentByUrl(fixedUrl));
        arkWeb.getWebContents().getNavigationController().loadUrl(params);
    }

    public void selectPage(IPage page) {
        boolean hastWeb = ArkWebContents.get(page.getId()) != null;
        ArkLogger.e(TAG, "selectPage page=" + page.getId() + " hastWeb=" + hastWeb);
        ArkWebContents arkWeb = new ArkWebContents.Builder(page)
                .setInitiallyHidden(isHidden())
                .build();
        swapWebContents(arkWeb, arkWeb.isStartLoad(), arkWeb.isFinishLoad());

        mTab.selectPage(page);
        if (!hastWeb) {
            LoadUrlParams params = new LoadUrlParams(UrlFormatter.fixupUrl(page.getPageInfo().getUrl()));
            params.setTransitionType(TabLaunchType.FROM_CHROME_UI);
            loadUrl(params);
        }
    }

    @Override
    public void addObserver(TabObserver observer) {
        mObservers.addObserver(observer);
        if (mWindowAndroid != null) {
            observer.onAttachToWindowAndroid(this, mWindowAndroid);
        }
    }

    @Override
    public void removeObserver(TabObserver observer) {
        mObservers.removeObserver(observer);
        observer.onDetachToWindowAndroid(this, mWindowAndroid);
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
    public WebContents getWebContents() {
        return mArkWeb == null ? null : mArkWeb.getWebContents();
    }

    @Override
    public Context getContext() {
        if (mWindowAndroid == null) return ContextUtils.getApplicationContext();
        Context context = mWindowAndroid.getContext().get();
        return context == context.getApplicationContext() ? ContextUtils.getApplicationContext() : context;
    }

    @Override
    public ArkWindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public boolean canResolveActivity(Intent intent) {
        // TODO 为什么原始逻辑需要WindowAndroid？
//        return getWindowAndroid().canResolveActivity(intent);
        return !PackageManagerUtils.queryIntentActivities(intent, 0).isEmpty();
    }

    public boolean openNewPage(LoadUrlParams params) {
        ArkLogger.d(TAG, "openNewPage params=" + params);
        if (mWindowAndroid == null) {
            return false;
        }
        return mWindowAndroid.getCompositorViewHolder().openNewPage(this, params);
    }

    @Override
    public void updateAttachment(
            @Nullable WindowAndroid window, @Nullable TabDelegateFactory tabDelegateFactory) {
        // Non-null delegate factory while being detached is not valid.
        assert !(window == null && tabDelegateFactory != null);

        WindowAndroid old = mWindowAndroid;
        mWindowAndroid = (ArkWindowAndroid) window;
//        if (window != null) {
//            updateWindowAndroid((ArkWindowAndroid) window);
//            if (tabDelegateFactory != null) setDelegateFactory(tabDelegateFactory);
//        }


        WebContents webContents = getWebContents();
        if (webContents != null) {
            webContents.setTopLevelNativeWindow(window);
        }

        if (tabDelegateFactory == null) {
            mWebContentsDelegate.setDelegate(null);
        } else {
            mWebContentsDelegate.setDelegate(tabDelegateFactory.createWebContentsDelegate(this));
        }

//        setDelegateFactory(tabDelegateFactory);

        ArkLogger.e(this, "updateAttachment webContents=" + webContents);
        loadIfNeeded();

        // Notify the event to observers only when we do the reparenting task, not when we simply
        // switch window in which case a new window is non-null but delegate is null.
        boolean notify = (window != null && tabDelegateFactory != null)
                || (window == null && tabDelegateFactory == null);
        if (notify) {
            for (TabObserver observer : mObservers) {
                observer.onActivityAttachmentChanged(this, window);
                if (mWindowAndroid != null) {
                    observer.onAttachToWindowAndroid(this, mWindowAndroid);
                } else if (old != null) {
                    observer.onDetachToWindowAndroid(this, old);
                }
            }
        }

        updateInteractableState();


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
        return mArkWeb == null ? null : mArkWeb.getContentView();
    }

    @Override
    public View getView() {
        if (mCustomView != null) return mCustomView;

        return getContentView();
    }

    @Override
    public TabViewManager getTabViewManager() {
        return mTabViewManager;
    }

    @Override
    public int getId() {
        return mTabInfo.getId();
    }

    @Override
    // TODO(crbug.com/1113249) move getUrl() to CriticalPersistedTabData
    public GURL getUrl() {
        if (!isInitialized()) {
            return GURL.emptyGURL();
        }
        GURL url = getWebContents() != null ? getWebContents().getVisibleUrl() : GURL.emptyGURL();

        // If we have a ContentView, or the url is not empty, we have a WebContents
        // so cache the WebContent's url. If not use the cached version.
        if (getWebContents() != null || !url.getSpec().isEmpty()) {
            CriticalPersistedTabData.from(this).setUrl(url);
        }

        return CriticalPersistedTabData.from(this).getUrl() != null
                ? CriticalPersistedTabData.from(this).getUrl()
                : GURL.emptyGURL();
    }

    @Override
    public GURL getOriginalUrl() {
        return DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(getUrl());
    }

    @Override
    // TODO(crbug.com/1113834) migrate getTitle() to CriticalPersistedTabData.from(tab).getTitle()
    public String getTitle() {
        if (CriticalPersistedTabData.from(this).getTitle() == null) updateTitle();
        return CriticalPersistedTabData.from(this).getTitle();
    }

    @Override
    public NativePage getNativePage() {
        return null;
    }

    @Override
    public boolean isNativePage() {
        return false;
    }

    @Override
    public boolean isShowingCustomView() {
        return mCustomView != null;
    }

    @Override
    public void freezeNativePage() {
    }

    @Override
    public @TabLaunchType
    int getLaunchType() {
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

    @Override
    public boolean isIncognito() {
        return mTabInfo.isIncognito();
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
        return mArkWeb == null;
    }

    @Override
    public boolean isUserInteractable() {
        return mInteractableState;
    }

    @Override
    public int loadUrl(LoadUrlParams params) {
        try {
            TraceEvent.begin("Tab.loadUrl");

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

            // Request desktop sites for large screen tablets if necessary.
            params.setOverrideUserAgent(calculateUserAgentOverrideOption());

            @TabLoadStatus int result;
            if (mArkWeb == null) {
                result = TabLoadStatus.PAGE_LOAD_FAILED;
            } else {
                result = mArkWeb.loadUrlInternal(params);
            }

            for (TabObserver observer : mObservers) {
                observer.onLoadUrl(this, params, result);
            }
            return result;
        } finally {
            TraceEvent.end("Tab.loadUrl");
        }
    }

    @Override
    public boolean loadIfNeeded() {
        ArkLogger.e(TAG, "loadIfNeeded");
        if (mWindowAndroid == null) {
            ArkLogger.e(TAG, "Tab couldn't be loaded because Context was null.");
            return false;
        }


//        if (mArkWeb == null) {
//            ArkLogger.e(TAG, "Tab couldn't be loaded because WebContents was null.");
//            return false;
//        }

//        return mArkWeb.loadIfNeeded(this);


//        LoadUrlParams pendingLoadParams = mArkWeb.getPendingLoadParams();
//        if (pendingLoadParams != null) {
//            mArkWeb.setPendingLoadParams(null);
//            loadUrl(mPendingLoadParams);
//            return true;
//        }

        if (mPendingLoadParams != null) {
            assert isFrozen();
//            WebContents webContents = WarmupManager.getInstance().takeSpareWebContents(
//                    isIncognito(), isHidden(), isCustomTab());
//            if (webContents == null) {
//                Profile profile =
//                        IncognitoUtils.getProfileFromWindowAndroid(mWindowAndroid, isIncognito());
//                webContents = WebContentsFactory.createWebContents(profile, isHidden());
//            }

            LoadUrlParams params = mPendingLoadParams;
            mPendingLoadParams = null;
            ArkWebContents arkWeb = new ArkWebContents.Builder(getPageInfo())
                    .setLoadUrlParams(params)
                    .setInitiallyHidden(isHidden())
                    .build();

            initWebContents(arkWeb, mWindowAndroid);
            loadUrl(params);

            return true;
        }

        switchUserAgentIfNeeded();
        restoreIfNeeded(mWindowAndroid);
        return true;
    }

    @Override
    public void reload() {

        // TODO(dtrainor): Should we try to rebuild the ContentView if it's frozen?
        if (OfflinePageUtils.isOfflinePage(this)) {
            // If current page is an offline page, reload it with custom behavior defined in extra
            // header respected.
            OfflinePageUtils.reload(getWebContents(),
                    /*loadUrlDelegate=*/new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(this));
            return;
        }

        if (getWebContents() == null) return;
        switchUserAgentIfNeeded();
        getWebContents().getNavigationController().reload(true);
    }

    @Override
    public void reloadIgnoringCache() {
        if (getWebContents() != null) {
            switchUserAgentIfNeeded();
            getWebContents().getNavigationController().reloadBypassingCache(true);
        }
    }

    @Override
    public void stopLoading() {
        cacheThumbnail();
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
        return mArkWeb != null && mArkWeb.needsReload();
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
        if (isLoading()) {
            if (mArkWeb == null) {
                return 0;
            }
            return mArkWeb.getWebContents().getLoadProgress();
        }
        return 1;
    }

    @Override
    public boolean canGoBack() {
        if (mArkWeb != null && mArkWeb.canGoBack()) {
            return true;
        }
        ArkLogger.e(TAG, "canGoBack pageIndex=" + mTabInfo.getPageIndex());
        return mTabInfo.getPageIndex() > 0;
    }

    @Override
    public boolean canGoForward() {
        if (mArkWeb != null && mArkWeb.canGoForward()) {
            return true;
        }
        return mTabInfo.getPageIndex() < mTab.getPageSize() - 1;
    }

    @Override
    public void goBack() {
        if (mArkWeb != null && mArkWeb.canGoBack()) {
            mArkWeb.goBack();
            return;
        }
        IPage page = mTab.getPreviousPage();
        if (page != null) {
            selectPage(page);
//            ArkWebContents arkWeb = new ArkWebContents.Builder(page)
//                    .setInitiallyHidden(isHidden())
//                    .build();
//            swapWebContents(arkWeb, false, false);
//            mTab.selectPage(page);
        }
    }

    @Override
    public void goForward() {
        if (mArkWeb != null && mArkWeb.canGoForward()) {
            mArkWeb.goForward();
            return;
        }
        IPage page = mTab.getNextPage();
        if (page != null) {
//            ArkWebContents arkWeb = new ArkWebContents.Builder(page)
//                    .setInitiallyHidden(isHidden())
//                    .build();
//            swapWebContents(arkWeb, false, false);
//            mTab.selectPage(page);
            selectPage(page);
        }
    }

    public boolean canGoBack2() {
        if (mArkWeb != null && mArkWeb.canGoBack()) {
            return true;
        }
        return mWindowAndroid != null && mWindowAndroid.getNavigationHandler().canGoBack();
    }

    public boolean canGoForward2() {
        if (mArkWeb != null && mArkWeb.canGoForward()) {
            return true;
        }
        return mWindowAndroid != null && mWindowAndroid.getNavigationHandler().canGoForward();
    }

    public void goBack2() {
        if (mWindowAndroid != null) {
            mWindowAndroid.getNavigationHandler().goBack();
        }
    }

    public void goForward2() {
        if (mWindowAndroid != null) {
            mWindowAndroid.getNavigationHandler().goForward();
        }
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
    public final void show(@TabSelectionType int type) {
        ArkLogger.e(TAG, "show type=" + type);
        try {
            TraceEvent.begin("Tab.show");
            if (!isHidden()) return;
            // Keep unsetting mIsHidden above loadIfNeeded(), so that we pass correct visibility
            // when spawning WebContents in loadIfNeeded().
            mIsHidden = false;
            updateInteractableState();

            loadIfNeeded();

            if (getWebContents() != null) getWebContents().onShow();

            TabImportanceManager.tabShown(this);

            // If the page is still loading, update the progress bar (otherwise it would not show
            // until the renderer notifies of new progress being made).
            if (getProgress() < 100) {
                notifyLoadProgress(getProgress());
            }

            for (TabObserver observer : mObservers) observer.onShown(this, type);

            // Updating the timestamp has to happen after the showInternal() call since subclasses
            // may use it for logging.
            CriticalPersistedTabData.from(this).setTimestampMillis(System.currentTimeMillis());
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
        mIsClosing = closing;
        for (TabObserver observer : mObservers) observer.onClosingStateChanged(this, closing);
    }

    @Override
    public boolean isHidden() {
        return mIsHidden;
    }

    @Override
    public void destroy() {

        if (mWindowAndroid != null) {
            TabContentManager manager = mWindowAndroid.getTabContentManager();
            if (manager != null) {
                manager.detachTab(this);
            }
        }

        // Set at the start since destroying the WebContents can lead to calling back into
        // this class.
        mIsDestroyed = true;

        // Update the title before destroying the tab. http://b/5783092
        updateTitle();

        for (TabObserver observer : mObservers) observer.onDestroyed(this);
        mObservers.clear();

        mUserDataHost.destroy();
        mTabViewManager.destroy();
        destroyWebContents(true);

        TabImportanceManager.tabDestroyed(this);

        // Destroys the native tab after destroying the ContentView but before destroying the
        // InfoBarContainer. The native tab should be destroyed before the infobar container as
        // destroying the native tab cleanups up any remaining infobars. The infobar container
        // expects all infobars to be cleaned up before its own destruction.
        if (mNativeTabAndroid != 0) {
            TabJni.get().destroy(mNativeTabAndroid);
            assert mNativeTabAndroid == 0;
        }
        this.mWindowAndroid = null;
    }

    /**
     * WARNING: This method is deprecated. Consider other ways such as passing the dependencies
     * to the constructor, rather than accessing ChromeActivity from Tab and using getters.
     *
     * @return {@link AsyncInitializationActivity} that currently contains this {@link Tab} in its
     * {@link TabModel}.
     */
    @Deprecated
    AsyncInitializationActivity getActivity() {
        if (mWindowAndroid == null) return null;
        Activity activity = ContextUtils.activityFromContext(mWindowAndroid.getContext().get());
        if (activity instanceof AsyncInitializationActivity)
            return (AsyncInitializationActivity) activity;
        return null;
    }

    /**
     * @param tab {@link Tab} instance being checked.
     * @return Whether the tab is detached from any Activity and its {@link WindowAndroid}.
     * Certain functionalities will not work until it is attached to an activity
     * with {@link ReparentingTask#finish}.
     */
    static boolean isDetached(Tab tab) {
        if (tab.getWebContents() == null) return true;
        // Should get WindowAndroid from WebContents since the one from |getWindowAndroid()|
        // is always non-null even when the tab is in detached state. See the comment in |detach()|.
        WindowAndroid window = tab.getWebContents().getTopLevelNativeWindow();
        if (window == null) return true;
        Activity activity = ContextUtils.activityFromContext(window.getContext().get());
        return !(activity instanceof ArkBrowserActivity);
    }

    @Override
    public void setIsTabSaveEnabled(boolean isTabSaveEnabled) {
        mIsTabSaveEnabledSupplier.set(isTabSaveEnabled);
    }

    @VisibleForTesting
    public ObservableSupplierImpl<Boolean> getIsTabSaveEnabledSupplierForTesting() {
        return mIsTabSaveEnabledSupplier;
    }

    // TabObscuringHandler.Observer

    @Override
    public void updateObscured(boolean isObscured) {
        // Update whether or not the current native tab and/or web contents are
        // currently visible (from an accessibility perspective), or whether
        // they're obscured by another view.
        View view = getView();
        if (view != null) {
            int importantForAccessibility = isObscured
                    ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                    : View.IMPORTANT_FOR_ACCESSIBILITY_YES;
            if (view.getImportantForAccessibility() != importantForAccessibility) {
                view.setImportantForAccessibility(importantForAccessibility);
                view.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
            }
        }

        WebContentsAccessibility wcax = getWebContentsAccessibility(getWebContents());
        if (wcax != null) {
            boolean isWebContentObscured = isObscured || isShowingCustomView();
            wcax.setObscuredByAnotherView(isWebContentObscured);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void registerTabSaving() {
        CriticalPersistedTabData.from(this).registerIsTabSaveEnabledSupplier(
                mIsTabSaveEnabledSupplier);
    }

    private boolean useCriticalPersistedTabData() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA);
    }

    @Nullable
    @TabCreationState
    Integer getCreationState() {
        return mCreationState;
    }

    /**
     * Restores member fields from the given TabState.
     *
     * @param state TabState containing information about this Tab.
     */
    void restoreFieldsFromState(TabState state) {
        assert state != null;
        CriticalPersistedTabData.from(this).setWebContentsState(state.contentsState);
        CriticalPersistedTabData.from(this).setTimestampMillis(state.timestampMillis);
        String url = state.contentsState.getVirtualUrlFromState();
        CriticalPersistedTabData.from(this).setUrl(new GURL(url));
        CriticalPersistedTabData.from(this).setTitle(
                state.contentsState.getDisplayTitleFromState());
        CriticalPersistedTabData.from(this).setLaunchTypeAtCreation(state.tabLaunchTypeAtCreation);
        CriticalPersistedTabData.from(this).setRootId(
                state.rootId == Tab.INVALID_TAB_ID ? mTabInfo.getId() : state.rootId);
        CriticalPersistedTabData.from(this).setUserAgent(state.userAgent);
    }

    /**
     * @return An {@link RewindableIterator} instance that points to all of
     * the current {@link TabObserver}s on this class.  Note that calling
     * {@link java.util.Iterator#remove()} will throw an
     * {@link UnsupportedOperationException}.
     */
    @Override
    public RewindableIterator<TabObserver> getTabObservers() {
        return mObservers.rewindableIterator();
    }

    @Override
    public final void setImportance(@ChildProcessImportance int importance) {
        if (mImportance == importance) return;
        mImportance = importance;
        if (mArkWeb != null) {
            mArkWeb.setImportance(importance);
        }
    }

    /**
     * Hides the current {@link }, if any, and shows the {@link WebContents}'s view.
     */
    void showRenderedPage() {
        updateTitle();
    }

    public void updateWindowAndroid(ArkWindowAndroid windowAndroid) {
        // TODO(yusufo): mWindowAndroid can never be null until crbug.com/657007 is fixed.
//        assert windowAndroid != null;
//        mWindowAndroid = windowAndroid;
//        WebContents webContents = getWebContents();
//        if (webContents != null) webContents.setTopLevelNativeWindow(mWindowAndroid);

        ArkLogger.e(this, "updateWindowAndroid windowAndroid=" + windowAndroid
                + " this=" + this);
        if (windowAndroid == null) {
            updateAttachment(null, null);
        } else {
            updateAttachment(windowAndroid, windowAndroid.getTabDelegateFactory());
        }
    }

//    public TabDelegateFactory getDelegateFactory() {
//        return mDelegateFactory;
//    }

    // Forwarded from TabWebContentsDelegateAndroid.

    /**
     * Called when a navigation begins and no navigation was in progress
     *
     * @param toDifferentDocument Whether this navigation will transition between
     *                            documents (i.e., not a fragment navigation or JS History API call).
     */
    void onLoadStarted(boolean toDifferentDocument) {
        if (toDifferentDocument) mIsLoading = true;
        for (TabObserver observer : mObservers) observer.onLoadStarted(this, toDifferentDocument);
    }

    /**
     * Called when a navigation completes and no other navigation is in progress.
     */
    void onLoadStopped() {
        cacheThumbnail();
        // mIsLoading should only be false if this is a same-document navigation.
        boolean toDifferentDocument = mIsLoading;
        mIsLoading = false;
        for (TabObserver observer : mObservers) observer.onLoadStopped(this, toDifferentDocument);
    }

    public void handleRendererResponsiveStateChanged(boolean isResponsive) {
        mIsRendererUnresponsive = !isResponsive;
        for (TabObserver observer : mObservers) {
            observer.onRendererResponsiveStateChanged(this, isResponsive);
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
     *
     * @param url URL that was loaded.
     */
    void didFinishPageLoad(GURL url) {
        updateTitle();

        for (TabObserver observer : mObservers) observer.onPageLoadFinished(this, url);
        mIsBeingRestored = false;
    }

    /**
     * Called when a page has failed loading.
     *
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
     * @param url            The URL that was loaded.
     * @param transitionType The transition type to the current URL.
     */
    void handleDidFinishNavigation(GURL url, int transitionType) {
        showRenderedPage();
    }

    /**
     * Notify the observers that the load progress has changed.
     *
     * @param progress The current percentage of progress.
     */
    void notifyLoadProgress(float progress) {
        ArkLogger.e(TAG, "notifyLoadProgress progress=" + progress);
        for (TabObserver observer : mObservers) observer.onLoadProgressChanged(this, progress);
    }

    /**
     * Add a new navigation entry for the current URL and page title.
     */
    void pushNativePageStateToNavigationEntry() {
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
            TabJni.get().loadOriginalImage(mNativeTabAndroid);
        }
    }

    /**
     * Sets whether the tab is showing an error page.  This is reset whenever the tab finishes a
     * navigation.
     * Note: This is kept here to keep the build green. Remove from interface as soon as
     * the downstream patch lands.
     *
     * @param isShowingErrorPage Whether the tab shows an error page.
     */
    void setIsShowingErrorPage(boolean isShowingErrorPage) {
        mIsShowingErrorPage = isShowingErrorPage;
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

    public void updateTitle() {
        if (isFrozen()) return;

        // When restoring the tabs, the title will no longer be populated, so request it from the
        // WebContents or NativePage (if present).
        String title = "";
        if (getWebContents() != null) {
            title = getWebContents().getTitle();
        }
        updateTitle(title);
    }

    /**
     * Cache the title for the current page.
     * <p>
     * {@link ContentViewClient#onUpdateTitle} is unreliable, particularly for navigating backwards
     * and forwards in the history stack, so pull the correct title whenever the page changes.
     * onUpdateTitle is only called when the title of a navigation entry changes. When the user goes
     * back a page the navigation entry exists with the correct title, thus the title is not
     * actually changed, and no notification is sent.
     *
     * @param title Title of the page.
     */
    void updateTitle(String title) {
        if (TextUtils.isEmpty(title)) {
            title = getUrl().getSpec();
        }
//        if (!TextUtils.equals(mPageInfo.getTitle(), title)) {
//            mPageInfo.setTitle(title);
//        }

        if (TextUtils.equals(CriticalPersistedTabData.from(this).getTitle(), title)) return;

        CriticalPersistedTabData.from(this).setTitle(title);
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
        while (observers.hasNext()) observers.next().onCrash(this);
        mIsBeingRestored = false;
    }

    /**
     * Called when the background color for the content changes.
     *
     * @param color The current for the background.
     */
    void onBackgroundColorChanged(int color) {
        for (TabObserver observer : mObservers) observer.onBackgroundColorChanged(this, color);
    }

//    private ArkWebContents mTestWebContents;

    /**
     * This is currently called when committing a pre-rendered page or activating a portal.
     */
    @Override
    public void swapWebContents(WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        ArkWebContents arkWeb = new ArkWebContents(getPageInfo(), webContents);
        swapWebContents(arkWeb, didStartLoad, didFinishLoad);
        ArkWebContents.put(getPageInfo().pageId, arkWeb);
    }

    public void swapWebContents(ArkWebContents arkWeb, boolean didStartLoad, boolean didFinishLoad) {
        // TODO swapWebContents
        ArkLogger.e(TAG, "swapWebContents");
        if (arkWeb == mArkWeb) {
            ArkLogger.e(TAG, "swapWebContents same web!");
            return;
        }
        boolean hasWebContents = mArkWeb != null && mArkWeb.getContentView() != null;
        Rect original = hasWebContents
                ? new Rect(0, 0, mArkWeb.getContentView().getWidth(), mArkWeb.getContentView().getHeight())
                : new Rect();
        for (TabObserver observer : mObservers) {
            observer.webContentsWillSwap(this);
        }
        if (hasWebContents) mArkWeb.getWebContents().onHide();
        Context appContext = ContextUtils.getApplicationContext();
        Rect bounds = original.isEmpty() ? TabUtils.estimateContentSize(appContext) : null;
        if (bounds != null) original.set(bounds);

        mArkWeb.getWebContents().setFocus(false);
//        mTestWebContents = mArkWeb;
        destroyWebContents(false /* do not delete native web contents */);
        hideNativePage(false, () -> {
            // Size of the new content is zero at this point. Set the view size in advance
            // so that next onShow() call won't send a resize message with zero size
            // to the renderer process. This prevents the size fluttering that may confuse
            // Blink and break rendered result (see http://crbug.com/340987).
            arkWeb.getWebContents().setSize(original.width(), original.height());

            if (bounds != null) {
                assert mNativeTabAndroid != 0;
                TabJni.get().onPhysicalBackingSizeChanged(
                        mNativeTabAndroid, arkWeb.getWebContents(), bounds.right, bounds.bottom);
            }
            initWebContents(arkWeb, mWindowAndroid);
            arkWeb.getWebContents().onShow();
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
        if (mNativeTabAndroid == 0) TabJni.get().init(ArkTabImpl.this);
        assert mNativeTabAndroid != 0;
    }

    /**
     * @return The native pointer representing the native side of this {@link ArkTabImpl} object.
     */
    @Override
    public long getNativePtr() {
        return mNativeTabAndroid;
    }

    @Override
    public void clearNativePtr() {
        assert mNativeTabAndroid != 0;
        mNativeTabAndroid = 0;
    }

    @Override
    public void setNativePtr(long nativePtr) {
        assert nativePtr != 0;
        assert mNativeTabAndroid == 0;
        mNativeTabAndroid = nativePtr;
    }

    /**
     * Initializes the {@link WebContents}. Completes the browser content components initialization
     * around a native WebContents pointer.
     * <p>
     * {@link #getNativePage()} will still return the {@link NativePage} if there is one.
     * All initialization that needs to reoccur after a web contents swap should be added here.
     * <p/>
     * NOTE: If you attempt to pass a native WebContents that does not have the same incognito
     * state as this tab this call will fail.
     *
     * @param webContents The WebContents object that will initialize all the browser components.
     */
    private void initWebContents(@NonNull ArkWebContents webContents, ArkWindowAndroid windowAndroid) {
        try {
            ArkLogger.e(TAG, "initWebContents webContents=" + webContents);
            TraceEvent.begin("ChromeTab.initWebContents");
            ArkWebContents oldWebContents = mArkWeb;
            mArkWeb = webContents;

            mArkWeb.attach(this);

            if (oldWebContents != null) {
                oldWebContents.detach(this);
            }

            updateInteractableState();

            assert mNativeTabAndroid != 0;
            TabJni.get().initWebContents(mNativeTabAndroid, mTabInfo.isIncognito(), isDetached(this),
                    mArkWeb.getWebContents(), mSourceTabId, mWebContentsDelegate,
                    new ArkTabContextMenuPopulatorFactory(this));

            if (oldWebContents == null) {
                mArkWeb.notifyRendererPreferenceUpdate();
            }
            ArkTabHelpers.initWebContentsHelpers(this);
            notifyContentChanged();
        } finally {
            TraceEvent.end("ChromeTab.initWebContents");
        }
    }

    private static class ArkTabContextMenuPopulatorFactory extends TabContextMenuPopulatorFactory {
        private ContextMenuPopulatorFactory mPopulatorFactory;
        private final Tab mTab;

        /**
         * Constructs an instance of {@link org.chromium.chrome.browser.tab.TabContextMenuPopulatorFactory}.
         *
         * @param populatorFactory The {@link ContextMenuPopulatorFactory} to delegate the calls to.
         * @param tab              The {@link Tab} that is using the populated context menus.
         */
        public ArkTabContextMenuPopulatorFactory(Tab tab) {
            super(null, tab);
            mTab = tab;
        }

        @Override
        public void onDestroy() {
            // |mPopulatorFactory| can be null for activities that do not use context menu.
            if (mPopulatorFactory != null) mPopulatorFactory.onDestroy();
        }

        @Override
        public ContextMenuPopulator createContextMenuPopulator(
                WindowAndroid windowAndroid, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate) {

            mPopulatorFactory = ((ArkWindowAndroid) windowAndroid)
                    .getTabDelegateFactory().createContextMenuPopulatorFactory(mTab);

            return new TabContextMenuPopulator(mPopulatorFactory.createContextMenuPopulator(
                    windowAndroid, params, nativeDelegate
            ), mTab);
        }
    }

    private TabWebContentsDelegateAndroidImpl createWebContentsDelegate(TabDelegateFactory delegateFactory) {
        if (delegateFactory == null) {
            return null;
        }
        TabWebContentsDelegateAndroid delegate = delegateFactory.createWebContentsDelegate(this);
        return new TabWebContentsDelegateAndroidImpl(this, delegate);
    }

    /**
     * Hide and destroy the native page if it was being shown.
     *
     * @param notify       {@code true} to trigger {@link #onContentChanged} event.
     * @param postHideTask {@link Runnable} task to run before actually destroying the
     *                     native page. This is necessary to keep the tasks to perform in order.
     */
    private void hideNativePage(boolean notify, Runnable postHideTask) {
        if (postHideTask != null) postHideTask.run();
        if (notify) notifyContentChanged();
    }

    /**
     * Set {@link TabDelegateFactory} instance and updates the references.
     *
     * @param factory TabDelegateFactory instance.
     */
    private void setDelegateFactory(TabDelegateFactory factory) {
        // Update the delegate factory, then recreate and propagate all delegates.
//        mDelegateFactory = factory;

        WebContents webContents = getWebContents();
        if (webContents != null) {
            TabJni.get().updateDelegates(mNativeTabAndroid, createWebContentsDelegate(factory),
                    new TabContextMenuPopulatorFactory(
                            factory.createContextMenuPopulatorFactory(this), this));
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
            observers.next().onFaviconUpdated(this, null);
        }
    }

    /**
     * Update the interactable state of the tab. If the state has changed, it will call the
     * {@link #onInteractableStateChanged(boolean)} method.
     */
    private void updateInteractableState() {
        boolean currentState = !mIsHidden && !isFrozen()
                && mIsViewAttachedToWindow
                && !isDetached(this);

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
    private void restoreIfNeeded(ArkWindowAndroid windowAndroid) {

        try {
            TraceEvent.begin("Tab.restoreIfNeeded");
            // Restore is needed for a tab that is loaded for the first time. WebContents will
            // be restored from a saved state.
            if ((isFrozen() && CriticalPersistedTabData.from(this).getWebContentsState() != null
                    && !unfreezeContents(windowAndroid))
                    || !needsReload()) {
                return;
            }

            if (mArkWeb != null) {
                mArkWeb.loadIfNecessary();
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
     *
     * @return Whether or not the restoration was successful.
     */
    private boolean unfreezeContents(ArkWindowAndroid windowAndroid) {
        boolean restored = true;
        try {
            TraceEvent.begin("Tab.unfreezeContents");
            WebContentsState webContentsState =
                    CriticalPersistedTabData.from(this).getWebContentsState();
            assert webContentsState != null;


            WebContents webContents = WebContentsStateBridge.restoreContentsFromByteBuffer(
                    webContentsState, isHidden());
            ArkWebContents arkWeb;
            if (webContents == null) {

                arkWeb = new ArkWebContents.Builder(getPageInfo())
                        .build();


                // State restore failed, just create a new empty web contents as that is the best
                // that can be done at this point. TODO(jcivelli) http://b/5910521 - we should show
                // an error page instead of a blank page in that case (and the last loaded URL).
//                Profile profile =
//                        IncognitoUtils.getProfileFromWindowAndroid(windowAndroid, isIncognito());
//                webContents = WebContentsFactory.createWebContents(profile, isHidden());
                for (TabObserver observer : mObservers) observer.onRestoreFailed(this);
                restored = false;
            } else {
                arkWeb = new ArkWebContents(getPageInfo(), webContents);
                ArkWebContents.put(getPageInfo().pageId, arkWeb);
            }

            View compositorView = windowAndroid.getCompositorViewHolder();
            if (compositorView != null) {
                arkWeb.getWebContents().setSize(compositorView.getWidth(), compositorView.getHeight());
            }


            CriticalPersistedTabData.from(this).setWebContentsState(null);
            initWebContents(arkWeb, windowAndroid);

            if (!restored) {
                String url = CriticalPersistedTabData.from(this).getUrl().getSpec().isEmpty()
                        ? UrlConstants.NTP_URL
                        : CriticalPersistedTabData.from(this).getUrl().getSpec();
                loadUrl(new LoadUrlParams(url, PageTransition.GENERATED));
            }
        } finally {
            TraceEvent.end("Tab.unfreezeContents");
        }
        return restored;
    }

    @Override
    public boolean isCustomTab() {
        return false;
    }

    /**
     * Throws a RuntimeException. Useful for testing crash reports with obfuscated Java stacktraces.
     */
    private int handleJavaCrash() {
        throw new RuntimeException("Intentional Java Crash");
    }

    /**
     * Delete navigation entries from frozen state matching the predicate.
     *
     * @param predicate Handle for a deletion predicate interpreted by native code.
     *                  Only valid during this call frame.
     */
    @Override
    public void deleteNavigationEntriesFromFrozenState(long predicate) {
        WebContentsState webContentsState =
                CriticalPersistedTabData.from(this).getWebContentsState();
        if (webContentsState == null) return;
        WebContentsState newState =
                WebContentsStateBridge.deleteNavigationEntries(webContentsState, predicate);
        if (newState != null) {
            CriticalPersistedTabData.from(this).setWebContentsState(newState);
            notifyNavigationEntriesDeleted();
        }
    }

    private static WebContentsAccessibility getWebContentsAccessibility(WebContents webContents) {
        return webContents != null ? WebContentsAccessibility.fromWebContents(webContents) : null;
    }

    /**
     * Destroys the current {@link WebContents}.
     *
     * @param deleteNativeWebContents Whether or not to delete the native WebContents pointer.
     */
    private void destroyWebContents(boolean deleteNativeWebContents) {
        if (mArkWeb == null) return;

        mArkWeb.detach(this);

        updateInteractableState();

        ArkWebContents contentsToDestroy = mArkWeb;
        if (contentsToDestroy.getViewAndroidDelegate() != null
                && contentsToDestroy.getViewAndroidDelegate() instanceof TabViewAndroidDelegate) {
            contentsToDestroy.getViewAndroidDelegate().destroy();
        }
        mArkWeb = null;

        assert mNativeTabAndroid != 0;
        if (deleteNativeWebContents) {
            // Destruction of the native WebContents will call back into Java to destroy the Java
            // WebContents.
            TabJni.get().destroyWebContents(mNativeTabAndroid);
        } else {
            // This branch is to not delete the WebContents, but just to release the WebContent from
            // the Tab and clear the WebContents for two different cases a) The WebContents will be
            // destroyed eventually, but from the native WebContents. b) The WebContents will be
            // reused later. We need to clear the reference to the Tab from WebContentsObservers or
            // the UserData. If the WebContents will be reused, we should set the necessary
            // delegates again.
            TabJni.get().releaseWebContents(mNativeTabAndroid);
            // This call is just a workaround, Chrome should clean up the WebContentsObservers
            // itself.
            contentsToDestroy.reset();
        }
    }

    private @UserAgentOverrideOption
    int calculateUserAgentOverrideOption() {
        return UserAgentOverrideOption.TRUE;
//        WebContents webContents = getWebContents();
//        boolean currentRequestDesktopSite = webContents != null && webContents.getNavigationController().getUseDesktopUserAgent();
//
//        @TabUserAgent
//        int tabUserAgent = CriticalPersistedTabData.from(this).getUserAgent();
//        // TabUserAgent.UNSET means this is a pre-existing tab from an earlier build. In this case
//        // we set the TabUserAgent bit based on last committed entry's user agent. If webContents is
//        // null, this method is triggered too early, and we cannot read the last committed entry's
//        // user agent yet. We will skip for now and let the following call set the TabUserAgent bit.
//        if (webContents != null && tabUserAgent == TabUserAgent.UNSET) {
//            if (currentRequestDesktopSite) {
//                tabUserAgent = TabUserAgent.DESKTOP;
//            } else {
//                tabUserAgent = TabUserAgent.DEFAULT;
//            }
//            CriticalPersistedTabData.from(this).setUserAgent(tabUserAgent);
//        }
//        // We only calculate the user agent when users did not manually choose one.
//        if (tabUserAgent == TabUserAgent.DEFAULT
//                && ContentFeatureList.isEnabled(ContentFeatureList.REQUEST_DESKTOP_SITE_GLOBAL)) {
//            // We only do the following logic to choose the desktop/mobile user agent if:
//            // 1. User never manually made a choice in the app menu for requesting desktop site.
//            // 2. User-enabled request desktop site in site settings.
//            Profile profile =
//                    IncognitoUtils.getProfileFromWindowAndroid(mWindowAndroid, isIncognito());
//            boolean shouldRequestDesktopSite;
//            if (ContentFeatureList.isEnabled(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)) {
//                shouldRequestDesktopSite = getWebContents() != null
//                        && TabUtils.isDesktopSiteEnabled(profile, getWebContents().getVisibleUrl());
//            } else {
//                shouldRequestDesktopSite = TabUtils.isDesktopSiteGlobalEnabled(profile);
//            }
//
//            if (shouldRequestDesktopSite != currentRequestDesktopSite) {
//                // TODO(crbug.com/1243758): Confirm if a new histogram should be used.
//                RecordHistogram.recordBooleanHistogram(
//                        "Android.RequestDesktopSite.UseDesktopUserAgent", shouldRequestDesktopSite);
//
//                // The user is not forcing any mode and we determined that we need to
//                // change, therefore we are using TRUE or FALSE option. On Android TRUE mean
//                // override to Desktop user agent, while FALSE means go with Mobile version.
//                return shouldRequestDesktopSite ? UserAgentOverrideOption.TRUE
//                                                : UserAgentOverrideOption.FALSE;
//            }
//        }
//
//        RecordHistogram.recordBooleanHistogram(
//                "Android.RequestDesktopSite.UseDesktopUserAgent", currentRequestDesktopSite);
//
//        // INHERIT means use the same that was used last time.
//        return UserAgentOverrideOption.INHERIT;
    }

    private void switchUserAgentIfNeeded() {
//        if (calculateUserAgentOverrideOption() == UserAgentOverrideOption.INHERIT
//                || getWebContents() == null) {
//            return;
//        }
//        boolean usingDesktopUserAgent =
//                getWebContents().getNavigationController().getUseDesktopUserAgent();
//        TabUtils.switchUserAgent(this, /* switchToDesktop */ !usingDesktopUserAgent,
//                /* forcedByUser */ false);
    }

    @Override
    public void loadingStateChanged(boolean shouldShowLoadingUI) {
        boolean isLoading = getWebContents() != null && getWebContents().isLoading();
        if (isLoading) {
            onLoadStarted(shouldShowLoadingUI);
        } else {
            onLoadStopped();
        }
    }

    @Override
    public void saveState() {
        ThreadPool.executeIO(() -> {
            long start = System.currentTimeMillis();
            ArkTabDao.savePageState(ArkTabImpl.this);
            ArkLogger.d(TAG, "saveState id=" + getId() + " deltaTime=" + (System.currentTimeMillis() - start));
        });
    }

    @Override
    public void cacheThumbnail() {
//        if (mWindowAndroid == null) {
//            return;
//        }
//        mWindowAndroid.getTabContentManager().cacheTabThumbnail(this);
        TabSnapshotManager.getInstance().cachePage(getPageInfo());
    }

}
