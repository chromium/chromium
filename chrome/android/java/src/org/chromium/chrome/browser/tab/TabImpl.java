// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
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
import org.chromium.base.process_launcher.ScopedServiceBindingBatch;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.desktop_site.DesktopSiteUtils;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageAssassin;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.SmoothTransitionDelegate;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.autofill.AutofillManagerWrapper;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillProviderUMA;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.components.autofill.AutofillSelectionMenuItemHelper;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.embedder_support.virtual_structure.PageContentProtoViewStructureBuilder;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.sensitive_content.SensitiveContentClient;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * Implementation of the interface {@link Tab}. Contains and manages a {@link ContentView}. This
 * class is not intended to be extended.
 */
@NullMarked
class TabImpl implements Tab {
    /** Used for logging. */
    private static final String TAG = "Tab";

    private static final String BACKGROUND_COLOR_CHANGE_PRE_OPTIMIZATION_HISTOGRAM =
            "Android.Tab.BackgroundColorChange.PreOptimization";
    private static final String BACKGROUND_COLOR_CHANGE_HISTOGRAM =
            "Android.Tab.BackgroundColorChange";
    private static final String UMA_AUTOFILL_THIRD_PARTY_MODE_DISABLED_PROVIDER =
            "Autofill.ThirdPartyModeDisabled.Provider";

    /**
     * A pref from //components/autofill/core/common/autofill_prefs.h which allows the use of
     * virtual viewstructures for Autofill when set.
     */
    @VisibleForTesting
    static final String AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE =
            "autofill.using_virtual_view_structure";

    private static final String PRODUCT_VERSION = VersionInfo.getProductVersion();

    // LINT.IfChange(DiscardReason)

    @IntDef({DiscardReason.ON_DEMAND, DiscardReason.APPEND_NAVIGATION, DiscardReason.COUNT})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    private @interface DiscardReason {
        int ON_DEMAND = 0;
        int APPEND_NAVIGATION = 1;
        int COUNT = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:DiscardReason)

    private long mNativeTabAndroid;

    /** Unique id of this tab (within its container). */
    private final int mId;

    /** Whether the tab is archived. */
    private final boolean mIsArchived;

    /** The Profile associated with this tab. */
    private final Profile mProfile;

    /** The tab model this tab is currently attached to. */
    private @Nullable ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;

    /** Whether or not this tab is a part of multi selection. */
    private @Nullable SelectionStateSupplier mSelectionStateSupplier;

    /**
     * An Application {@link Context}. Unlike {@link #mActivity}, this is the only one that is
     * publicly exposed to help prevent leaking the {@link Activity}.
     */
    private final Context mThemedApplicationContext;

    /** Gives {@link Tab} a way to interact with the Android window. */
    private @Nullable WindowAndroid mWindowAndroid;

    /** The current native page (e.g. chrome-native://newtab), or {@code null} if there is none. */
    private @Nullable NativePage mNativePage;

    /**
     * True after a native page has been hidden, before a new background color has been explicitly
     * set. This is useful when the implicit background color (previously set before showing the
     * native page) is no longer necessarily relevant.
     */
    private boolean mWaitingOnBgColorAfterHidingNativePage;

    /** {@link WebContents} showing the current page, or {@code null} if the tab is frozen. */
    private @Nullable WebContents mWebContents;

    /** The parent view of the ContentView and the InfoBarContainer. */
    private @Nullable ContentView mContentView;

    /** The view provided by {@link TabViewManager} to be shown on top of Content view. */
    private @Nullable View mCustomView;

    private @Nullable @ColorInt Integer mCustomViewBackgroundColor;

    @Nullable AutofillProvider mAutofillProvider;

    /**
     * The {@link TabViewManager} associated with this Tab that is responsible for managing custom
     * views.
     */
    private final TabViewManagerImpl mTabViewManager;

    /** A list of Tab observers. These are used to broadcast Tab events to listeners. */
    @VisibleForTesting protected final ObserverList<TabObserver> mObservers = new ObserverList<>();

    // Content layer Delegates
    private @Nullable TabWebContentsDelegateAndroidImpl mWebContentsDelegate;

    private boolean mIsClosing;
    private boolean mDidCloseWhileDetached;
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
    private @Nullable LoadUrlParams mPendingLoadParams;

    /** True while a page load is in progress. */
    private boolean mIsLoading;

    /** True while a restore page load is in progress. */
    private boolean mIsBeingRestored;

    /** Whether or not the Tab is currently visible to the user. */
    private boolean mIsHidden = true;

    /** Called when the current window's occlusion changes. */
    private final Callback<Boolean> mOcclusionCallback = (v) -> updateWebContentsVisibility();

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

    private @Nullable TabDelegateFactory mDelegateFactory;

    /** Listens for views related to the tab to be attached or detached. */
    private final OnAttachStateChangeListener mAttachStateChangeListener;

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
    private boolean mIsPinned;
    private @MediaState int mMediaState;
    private @TabUserAgent int mUserAgent = TabUserAgent.DEFAULT;

    /**
     * Navigation state of the WebContents as returned by nativeGetContentsStateAsByteBuffer(),
     * stored to be inflated on demand using unfreezeContents(). If this is not null, there is no
     * WebContents around. Upon tab switch WebContents will be unfrozen and the variable will be set
     * to null.
     */
    private @Nullable WebContentsState mWebContentsState;

    /** Title of the ContentViews webpage. */
    private String mTitle = "";

    /** URL of the page currently loading. Used as a fall-back in case tab restore fails. */
    private @Nullable GURL mUrl;

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

    private @Nullable String mPendingNativePageHost;

    private @Nullable SmoothTransitionDelegate mNativePageSmoothTransitionDelegate;

    /**
     * Notified when the content sensitivity changes, and sets the content sensitivity property on
     * the {@link TabState}.
     *
     * <p>Can be non-null once V is the minimum SDK.
     */
    private SensitiveContentClient.@Nullable Observer mSensitiveContentClientObserver;

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
     * @param isArchived Whether the tab is archived.
     */
    @SuppressLint("HandlerLeak")
    TabImpl(int id, Profile profile, @TabLaunchType int launchType, boolean isArchived) {
        mId = TabIdManager.getInstance().generateValidId(id);
        mProfile = profile;
        assert mProfile != null;
        mRootId = mId;
        mIsArchived = isArchived;

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
                        if (isNativePage() && assumeNonNull(getNativePage()).getView() == view) {
                            if (mNativePageSmoothTransitionDelegate != null) {
                                mNativePageSmoothTransitionDelegate.cancel();
                                mNativePageSmoothTransitionDelegate = null;
                            } else {
                                // reset ntp view state.
                                assumeNonNull(getView()).setAlpha(1f);
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
    public @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public Context getContext() {
        if (getWindowAndroid() == null) return mThemedApplicationContext;
        Context context = getWindowAndroid().getContext().get();
        assumeNonNull(context);
        return context == context.getApplicationContext() ? mThemedApplicationContext : context;
    }

    @Override
    public @Nullable WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @Override
    public void updateAttachment(
            @Nullable WindowAndroid window, @Nullable TabDelegateFactory tabDelegateFactory) {
        // Non-null delegate factory while being detached is not valid.
        assert !(window == null && tabDelegateFactory != null);

        if (window != null) {
            // Firstly updating the delegates as the fullscreen state is now checked by the delegate
            if (tabDelegateFactory != null) setDelegateFactory(tabDelegateFactory);

            // Updating window as the WebContentsDelegate is now set and delegate can validate the
            // full screen state.
            updateWindowAndroid(window);

            // Reload the NativePage (if any), since the old NativePage has a reference to the old
            // activity.
            if (isNativePage()) {
                maybeShowNativePage(getUrl().getSpec(), true, PdfUtils.getPdfInfo(getNativePage()));
            }
        } else {
            updateIsDetached(window);

            // Clear the current tab supplier during detachment/reparenting to indicate that the
            // tab is not held by another tab model. For unclear reasons, removeTab() doesn't
            // always get invoked on the previous tab model before the tab is attached to the new
            // tab model (at least in tests).
            mCurrentTabSupplier = null;
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
    public @Nullable ContentView getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getView() {
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
        if (mPendingLoadParams != null) {
            return mUrl != null ? mUrl : new GURL(mPendingLoadParams.getUrl());
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
    public @JniType("std::u16string") String getTitle() {
        if (TextUtils.isEmpty(mTitle)) updateTitle();
        return mTitle;
    }

    Context getThemedApplicationContext() {
        return mThemedApplicationContext;
    }

    @Override
    public @Nullable NativePage getNativePage() {
        return mNativePage;
    }

    @Override
    @CalledByNative
    @EnsuresNonNullIf("mNativePage")
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
                || assumeNonNull(mNativePage.getView()).getParent() != null) {
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
        if (mCustomView != null && mCustomViewBackgroundColor != null) {
            return mCustomViewBackgroundColor;
        }
        if (mNativePage != null) {
            return mNativePage.getBackgroundColor();
        }
        return mWebContentBackgroundColor;
    }

    @Override
    public boolean isThemingAllowed() {
        // Do not apply the theme color if there are any security issues on the page.
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(getWebContents());
        boolean hasSecurityIssue = securityLevel == ConnectionSecurityLevel.DANGEROUS;
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
     *     the tab holds frozen WebContents state that is yet to be inflated.
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

    private void updateIsDetached(@Nullable WindowAndroid window) {
        // HiddenTabHolder relies on isDetached() being true to determine whether the tab is
        // a background tab during initWebContents() before invoking ReparentingTask#detach().
        // In this scenario, the tab owns its own WindowAndroid and has no activity attachment.
        // We must check this as an additional condition to detachment for this case to continue
        // to work. See https://crbug.com/1501849.
        mIsDetached = window == null || !windowHasActivity(window);
        updateWebContentsVisibility();
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

    @CalledByNative
    @Override
    public boolean isActivated() {
        if (mCurrentTabSupplier == null) return false;

        return this == mCurrentTabSupplier.get();
    }

    @Override
    public boolean hasParentCollection() {
        if (mNativeTabAndroid == 0 || mIsDestroyed) return false;
        return TabImplJni.get().hasParentCollection(mNativeTabAndroid);
    }

    /**
     * The parent tab for the current tab is set and the DelegateFactory is updated if it is not set
     * already. This happens only if the tab has been detached and the parent has not been set yet,
     * for example, for the spare tab before loading url.
     *
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
                setDelegateFactory(assumeNonNull(mDelegateFactory));
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

            try (TraceEvent event = TraceEvent.scoped("Tab.loadUrl.TabObservers.onLoadUrl")) {
                for (TabObserver observer : mObservers) {
                    observer.onLoadUrl(this, params, result);
                }
            }
            return result;
        } finally {
            TraceEvent.end("Tab.loadUrl");
        }
    }

    private LoadUrlResult loadUrlInternal(LoadUrlParams params, GURL fixedUrl) {
        if (mWebContents == null) return new LoadUrlResult(TabLoadStatus.PAGE_LOAD_FAILED, null);

        if (!fixedUrl.isValid()) return new LoadUrlResult(TabLoadStatus.PAGE_LOAD_FAILED, null);

        // Discard pending load params if they exist. At this point we are navigating to a new URL
        // programmatically and the pending load never occurred.
        if (mPendingLoadParams != null) {
            mPendingLoadParams = null;
        }

        // Record UMA "ShowHistory" here. That way it'll pick up both user
        // typing chrome://history as well as selecting from the drop down menu.
        String fixedUrlSpec = fixedUrl.getSpec();
        if (UrlConstants.HISTORY_HOST.equals(fixedUrlSpec)
                || UrlConstants.NATIVE_HISTORY_URL.equals(fixedUrlSpec)) {
            RecordUserAction.record("ShowHistory");
        }

        if (TabImplJni.get().handleNonNavigationAboutURL(fixedUrl)) {
            return new LoadUrlResult(TabLoadStatus.DEFAULT_PAGE_LOAD, null);
        }

        params.setUrl(fixedUrlSpec);
        NavigationHandle handle = mWebContents.getNavigationController().loadUrl(params);
        return new LoadUrlResult(TabLoadStatus.DEFAULT_PAGE_LOAD, handle);
    }

    @Override
    public void freeze() {
        if (useDiscardForFreeze()) {
            discardInternal(DiscardReason.ON_DEMAND);
        } else {
            freezeInternal();
        }
    }

    private String getMetricsTag(@DiscardReason int discardReason) {
        switch (discardReason) {
            case DiscardReason.ON_DEMAND:
                return "OnDemand";
            case DiscardReason.APPEND_NAVIGATION:
                return "AppendNavigation";
            default:
                assert false : "Unknown discard reason: " + discardReason;
                return "Unknown";
        }
    }

    private boolean useDiscardForFreeze() {
        return ChromeFeatureList.sTabFreezingUsesDiscard.isEnabled()
                && ContentFeatureMap.isEnabled(ContentFeatures.WEB_CONTENTS_DISCARD);
    }

    private void discardInternal(@DiscardReason int discardReason) {
        assert isHidden() || isClosing() : "Should only discard a closing or hidden tab.";

        if (mWebContents == null) return;

        long start = SystemClock.uptimeMillis();

        RecordHistogram.recordEnumeratedHistogram(
                "Tab.Android.DiscardStarted", discardReason, DiscardReason.COUNT);
        mWebContents.discard(
                () -> {
                    RecordHistogram.recordTimesHistogram(
                            "Tab.Android.DiscardLatency." + getMetricsTag(discardReason),
                            SystemClock.uptimeMillis() - start);
                });
        // TODO(crbug.com/449784092): Check if the tab gets stuck in a loading state when using
        // discard.
    }

    private void freezeInternal() {
        assert isHidden() || isClosing() : "Should only freeze a closing or hidden tab.";
        // If the native page is not already torn down make sure we remove it so it isn't visible if
        // this tab is foregrounded again in the current session.
        hideNativePage(/* notify= */ false, /* postHideTask= */ null);
        WebContentsState oldWebContentsState = TabStateExtractor.getWebContentsState(this);
        WebContents oldWebContents = mWebContents;
        destroyWebContents(false);
        mWebContents = null;
        if (mWebContentsState != oldWebContentsState) {
            if (mWebContentsState != null) {
                mWebContentsState.destroy();
                mWebContentsState = null;
            }
            mWebContentsState = oldWebContentsState;
        }
        mIsLoading = false;
        // In case extracting the WebContentsState fails make sure we reload to the same URL.
        if (mWebContentsState == null) {
            mPendingLoadParams = new LoadUrlParams(mUrl == null ? GURL.emptyGURL() : mUrl);
        } else {
            // getWebContentsState should already have consumed the pending load params if one
            // existed. Only one of mPendingLoadParams and mWebContentsState should be populated at
            // a time so since we set mWebContentsState earlier we can clear this out.
            mPendingLoadParams = null;
        }

        if (oldWebContents != null) {
            for (TabObserver observer : mObservers) {
                observer.onContentChanged(this);
            }
            oldWebContents.destroy();
        }
    }

    @Override
    public void freezeAndAppendPendingNavigation(LoadUrlParams params, @Nullable String title) {
        if (useDiscardForFreeze()) {
            discardAndAppendPendingNavigation(params, title);
        } else {
            freezeAndAppendPendingNavigationInternal(params, title);
        }
    }

    private void discardAndAppendPendingNavigation(LoadUrlParams params, @Nullable String title) {
        assert isHidden() : "Should only discard and append a navigation to a tab that is hidden.";

        if (mWebContents == null && mWebContentsState == null && mPendingLoadParams != null) {
            // Case 1: We have a pending load params but no WebContents or WebContentsState. Just
            // clobber the existing pending load params.
            mPendingLoadParams = params;
            mUrl = new GURL(params.getUrl());
        } else if (mWebContentsState != null) {
            assert mPendingLoadParams == null
                    : "Should not have both a WebContentsState and a pending load params.";

            // Case 2: We have a WebContentsState. Append the pending navigation to it.
            boolean success =
                    mWebContentsState.appendPendingNavigation(
                            mProfile, title, params, /* trackLastEntryWasPending= */ true);
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.FreezeAndAppendPendingNavigationResult", success);
            if (success) {
                // The pending load params were consumed to make the WebContentsState. Invalidate
                // them.
                mPendingLoadParams = null;
                mUrl = new GURL(mWebContentsState.getVirtualUrlFromState());
            } else {
                // If we failed to append the pending navigation, clear the WebContentsState and
                // clobber with the new pending load params.
                mWebContentsState.destroy();
                mWebContentsState = null;
                mPendingLoadParams = params;
                mUrl = new GURL(params.getUrl());
            }
        } else {
            // Case 3: The tab has a live WebContents and maybe a pending load params. Clobber
            // the previous pending load params (if one existed) and discard the WebContents.
            assert mWebContents != null;
            discardInternal(DiscardReason.APPEND_NAVIGATION);
            mPendingLoadParams = params;
            mUrl = new GURL(params.getUrl());
        }
        triggerUpdatesOnAppendingNavigation(title);
    }

    private void freezeAndAppendPendingNavigationInternal(
            LoadUrlParams params, @Nullable String title) {
        assert isHidden() : "Should only freeze and append a navigation to a tab that is hidden.";
        freezeInternal();
        assumeNonNull(mWebContentsState);
        // The only reason this should still be null is if we failed to allocate a byte buffer,
        // which probably means we are close to an OOM.
        boolean success =
                mWebContentsState.appendPendingNavigation(
                        mProfile, title, params, /* trackLastEntryWasPending= */ false);

        RecordHistogram.recordBooleanHistogram(
                "Tabs.FreezeAndAppendPendingNavigationResult", success);
        if (success) {
            // The pending load params were consumed to make the WebContentsState. Invalidate them.
            mPendingLoadParams = null;
            mUrl = new GURL(assumeNonNull(mWebContentsState).getVirtualUrlFromState());
        } else {
            // If we failed to append the pending navigation, clear the WebContentsState and restore
            // the tab to a blank state.
            mWebContentsState.destroy();
            mWebContentsState = null;

            // Since we are not allowed to auto-navigate the only remaining fallback is to clobber
            // all navigation state and treat the tab as if it is in a pending load state. All the
            // previous state was already cleaned up so we just need to set the params here.
            mPendingLoadParams = params;
            mUrl = new GURL(params.getUrl());
        }
        triggerUpdatesOnAppendingNavigation(title);
    }

    private void triggerUpdatesOnAppendingNavigation(@Nullable String title) {
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onUrlUpdated(this);
        }
        observers.rewind();
        notifyFaviconChanged();
        assumeNonNull(mUrl);
        updateTitle(title == null ? mUrl.getSpec() : title);

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
            if (mWebContents == null) {
                WebContents webContents =
                        WebContentsFactory.createWebContents(mProfile, isHidden(), false);
                initWebContents(webContents);
            } else {
                assert useDiscardForFreeze()
                        : "mWebContents should be null with mPendingLoadParams unless"
                                + " TAB_FREEZING_USES_DISCARD is enabled.";
            }
            loadUrl(mPendingLoadParams);
            mPendingLoadParams = null;
        } else {
            restoreIfNeeded();
        }

        // If we are trying to share a tab, and it has never been loaded, then it will not have its
        // physical backing size set, which means it will never produce any frames. In this case,
        // set the physical backing size to an estimate of what it would be if it were shown.
        if (caller == TabLoadIfNeededCaller.MEDIA_CAPTURE_PICKER && !hasBacking()) {
            assumeNonNull(mWindowAndroid);
            var display = mWindowAndroid.getDisplay();
            assumeNonNull(mWebContents);
            int width = (int) (mWebContents.getWidth() * display.getDipScale());
            int height = (int) (mWebContents.getHeight() * display.getDipScale());
            TabImplJni.get()
                    .onPhysicalBackingSizeChanged(mNativeTabAndroid, mWebContents, width, height);
        }

        return true;
    }

    @Override
    public void reload() {
        NativePage nativePage = getNativePage();
        if (nativePage != null) {
            nativePage.reload();
            return;
        }

        if (getWebContents() == null) return;

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
        return !isLoading() ? 1 : (int) assumeNonNull(mWebContents).getLoadProgress();
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

    private void updateWebContentsVisibility() {
        var webContents = getWebContents();
        if (webContents == null) return;
        if (mIsHidden) {
            webContents.updateWebContentsVisibility(Visibility.HIDDEN);
        } else if (!mIsDetached && assumeNonNull(mWindowAndroid).getOcclusionSupplier().get()) {
            // If we are not attached to a window, occlusion does not make sense.
            webContents.updateWebContentsVisibility(Visibility.OCCLUDED);
        } else {
            webContents.updateWebContentsVisibility(Visibility.VISIBLE);
        }
    }

    @Override
    public void show(@TabSelectionType int type, @TabLoadIfNeededCaller int caller) {
        // Batch service binding updates for the tab including the subframes. TabImpl.show() is
        // triggered not only on tab switch, but also when the window is shown.
        try (ScopedServiceBindingBatch scope = ScopedServiceBindingBatch.scoped()) {
            TraceEvent.begin("Tab.show");
            if (!isHidden()) return;
            // Keep unsetting mIsHidden above loadIfNeeded(), so that we pass correct visibility
            // when spawning WebContents in loadIfNeeded().
            mIsHidden = false;
            updateInteractableState();

            loadIfNeeded(caller);

            if (mNativeTabAndroid == 0) {
                throw new IllegalStateException("TabImpl's native pointer is 0 when showing.");
            }
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

            updateWebContentsVisibility();

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
        // Batch service binding updates for the tab including the subframes. TabImpl.hide() is
        // triggered not only on tab switch, but also when the window is hidden.
        try (ScopedServiceBindingBatch scope = ScopedServiceBindingBatch.scoped()) {
            TraceEvent.begin("Tab.hide");
            if (isHidden()) return;
            mIsHidden = true;
            updateInteractableState();
            updateWebContentsVisibility();

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

    @Override
    public void setDidCloseWhileDetached() {
        mDidCloseWhileDetached = true;
    }

    @Override
    public boolean didCloseWhileDetached() {
        return mDidCloseWhileDetached;
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
        boolean abortNavigationsFromTabClosures =
                ChromeFeatureList.isEnabled(ChromeFeatureList.ABORT_NAVIGATIONS_FROM_TAB_CLOSURES);
        if (abortNavigationsFromTabClosures) {
            mUserDataHost.destroy();
            destroyWebContents(true);
        }

        mObservers.clear();
        if (!abortNavigationsFromTabClosures) mUserDataHost.destroy();
        mTabViewManager.destroy();
        hideNativePage(false, null);
        if (!abortNavigationsFromTabClosures) destroyWebContents(true);
        if (mWebContentsState != null) {
            mWebContentsState.destroy();
            mWebContentsState = null;
        }

        TabImportanceManager.tabDestroyed(this);

        if (mWindowAndroid != null) {
            mWindowAndroid.getOcclusionSupplier().removeObserver(mOcclusionCallback);
        }

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
     * WARNING: This method is deprecated. Consider other ways such as passing the dependencies to
     * the constructor, rather than accessing ChromeActivity from Tab and using getters.
     *
     * @return {@link ChromeActivity} that currently contains this {@link Tab} in its {@link
     *     TabModel}.
     */
    @Deprecated
    @Nullable ChromeActivity getActivity() {
        if (getWindowAndroid() == null) return null;
        Activity activity = ContextUtils.activityFromContext(getWindowAndroid().getContext().get());
        if (activity instanceof ChromeActivity) return (ChromeActivity) activity;
        return null;
    }

    /**
     * Helper method to access the activity context if there is one.
     *
     * @return a {@link WeakReference} to the {@link Context} belonging to the current activity. It
     *     can be null if the context has been invalidated (e.g. by destruction) or if there is none
     *     (e.g. because the window is detached).
     */
    private WeakReference<Context> getActivityContext() {
        return getWindowAndroid() != null && windowHasActivity(getWindowAndroid())
                ? getWindowAndroid().getContext()
                : new ImmutableWeakReference<>(null);
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
    @Initializer
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    void initialize(
            @Nullable Tab parent,
            @Nullable @TabCreationState Integer creationState,
            @Nullable LoadUrlParams loadUrlParams,
            @Nullable String pendingTitle,
            @Nullable WebContents webContents,
            TabDelegateFactory delegateFactory,
            boolean initiallyHidden,
            @Nullable TabState tabState,
            boolean initializeRenderer,
            boolean isPinned) {
        try {
            TraceEvent.begin("Tab.initialize");

            if (parent != null) {
                mParentId = parent.getId();
            }

            mTabLaunchTypeAtCreation = mLaunchType;
            mCreationState = creationState;
            mIsPinned = isPinned;

            // If applicable set up for a lazy background tab load.
            mPendingLoadParams = loadUrlParams;
            if (loadUrlParams != null) {
                mUrl = new GURL(loadUrlParams.getUrl());
                setTitle(pendingTitle != null ? pendingTitle : mUrl.getSpec());
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

            boolean needsInitWebContents = true;
            boolean createWebContents = webContents == null;
            // TODO(crbug.com/448420873): For HeadlessTabModel we might not have a WindowAndroid.
            // For archived tabs, we don't want to create a WebContents. Archived and headless tab
            // models are not associated with BrowserWindowInterface so this shouldn't be an issue
            // for now. In future we should reconsider whether these tab models should even hold a
            // TabImpl vs some kind of light weight tab representation.
            if (ChromeFeatureList.sLoadAllTabsAtStartup.isEnabled()
                    && mWindowAndroid != null
                    && !mIsArchived) {
                if (mWebContentsState != null) {
                    assert webContents == null;

                    unfreezeContents(/* noRenderer= */ true);
                    webContents = getWebContents();
                    needsInitWebContents = false;
                    assert webContents != null;
                } else if (getPendingLoadParams() != null) {
                    assert webContents == null;

                    webContents =
                            WebContentsFactory.createWebContents(
                                    mProfile, isHidden(), initializeRenderer);
                } else if (createWebContents) {
                    webContents =
                            WebContentsFactory.createWebContents(
                                    mProfile, initiallyHidden, initializeRenderer);
                }
            } else {
                // If there is a frozen WebContents state or a pending lazy load, don't create a new
                // WebContents. Restoring will be done when showing the tab in the foreground.
                if (mWebContentsState != null || getPendingLoadParams() != null) {
                    return;
                }
                if (createWebContents) {
                    webContents =
                            WebContentsFactory.createWebContents(
                                    mProfile, initiallyHidden, initializeRenderer);
                }
            }

            assumeNonNull(webContents);
            if (needsInitWebContents) {
                initWebContents(webContents);
            }
            // Avoid an empty title by updating the title here. This could happen if restoring from
            // a WebContents that has no renderer and didn't force a reload. This happens on
            // background tab creation from Recent Tabs (TabRestoreService).
            updateTitle();

            if (!createWebContents && webContents.shouldShowLoadingUI()) {
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
                themeColor = tabState.themeColor;
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
     *
     * @param state TabState containing information about this Tab.
     */
    void restoreFieldsFromState(TabState state) {
        assert state != null;
        mWebContentsState = state.contentsState;
        setTimestampMillis(state.timestampMillis);
        setLastNavigationCommittedTimestampMillis(state.lastNavigationCommittedTimestampMillis);
        assumeNonNull(state.contentsState);
        mUrl = new GURL(state.contentsState.getVirtualUrlFromState());
        setTitle(assumeNonNull(state.contentsState.getDisplayTitleFromState()));
        mTabLaunchTypeAtCreation = state.tabLaunchTypeAtCreation;
        setRootId(state.rootId == Tab.INVALID_TAB_ID ? mId : state.rootId);
        setTabGroupId(state.tabGroupId);
        setUserAgent(state.userAgent);
        setTabHasSensitiveContent(state.tabHasSensitiveContent);
        setIsPinned(state.isPinned);
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
        updateImportance(getWebContents(), mImportance);
    }

    private static void updateImportance(
            @Nullable WebContents webContents, @ChildProcessImportance int importance) {
        if (webContents == null
                || ChromeFeatureList.isEnabled(ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID)) {
            // When ProcessRankPolicyAndroid of performance manager is enabled, the policy updates
            // the page importance.
            return;
        }
        webContents.setPrimaryPageImportance(importance, ChildProcessImportance.NORMAL);
    }

    /** Hides the current {@link NativePage}, if any, and shows the {@link WebContents}'s view. */
    void showRenderedPage() {
        // During title update, we prioritize titles in NativePage instead of those from
        // WebContents. Thus we should remove the obsolete NativePage before title update.
        if (mNativePage != null) hideNativePage(true, null);
        updateTitle();
    }

    void updateWindowAndroid(@Nullable WindowAndroid windowAndroid) {
        if (mWindowAndroid != null) {
            mWindowAndroid.getOcclusionSupplier().removeObserver(mOcclusionCallback);
        }

        mWindowAndroid = windowAndroid;
        if (mAutofillProvider != null
                && AndroidAutofillFeatures.ANDROID_AUTOFILL_UPDATE_CONTEXT_FOR_WEBCONTENTS
                        .isEnabled()) {
            mAutofillProvider.switchToContext(getActivityContext());
        }
        WebContents webContents = getWebContents();
        if (webContents != null) {
            assert mWindowAndroid != null;
            webContents.setTopLevelNativeWindow(mWindowAndroid);
        }

        if (windowAndroid != null) {
            windowAndroid.getOcclusionSupplier().addObserver(mOcclusionCallback);
        }

        // updateIsDetached will also update the web contents visibility if the
        // occlusion has changed.
        updateIsDetached(windowAndroid);
    }

    @Nullable TabDelegateFactory getDelegateFactory() {
        return mDelegateFactory;
    }

    @VisibleForTesting
    @Nullable TabWebContentsDelegateAndroidImpl getTabWebContentsDelegateAndroid() {
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
        if (isDestroyed()) return;

        for (TabObserver observer : mObservers) {
            observer.didBackForwardTransitionAnimationChange(this);
        }

        // Start the cross-fade animation after the invoking animation is done.
        WebContents webContents = getWebContents();
        assumeNonNull(webContents);
        View view = getView();
        assumeNonNull(view);
        switch (webContents.getCurrentBackForwardTransitionStage()) {
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
                    view.setAlpha(1f);
                }
                return;
            case AnimationStage.OTHER:
                if (isNativePage()) {
                    // A transition is starting. Hide the Java view to present that.
                    // Wait until the content/ draws the transition.
                    CompositorViewHolder viewHolder =
                            assumeNonNull(getActivity()).getCompositorViewHolderSupplier().get();
                    viewHolder.requestRender(
                            () -> {
                                var currView = getView();
                                if (currView != null) {
                                    currView.setAlpha(0f);
                                }
                            });
                }
                return;
            case AnimationStage.WAITING_FOR_EMBEDDER_CONTENT_FOR_COMMITTED_ENTRY:
                if (mNativePageSmoothTransitionDelegate != null) {
                    // Navigating back to native pages.
                    mNativePageSmoothTransitionDelegate.start(
                            () -> {
                                if (isDestroyed()) return;
                                assumeNonNull(getWebContents()).onContentForNavigationEntryShown();
                                notifyContentChanged();
                            });
                    mNativePageSmoothTransitionDelegate = null;
                } else if (isNativePage()) { // Navigation from native page was cancelled.
                    if (view.getAlpha() != 1f) {
                        // This means the content/ is waiting for the NTP to be fully visible.
                        view.setAlpha(1f);
                        view.post(webContents::onContentForNavigationEntryShown);
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
     *
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
     * Sets whether the tab is showing an error page. This is reset whenever the tab finishes a
     * navigation. Note: This is kept here to keep the build green. Remove from interface as soon as
     * the downstream patch lands.
     *
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
    boolean maybeShowNativePage(String url, boolean forceReload, @Nullable PdfInfo pdfInfo) {
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
        assumeNonNull(mDelegateFactory);
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
        if (mPendingLoadParams != null) {
            // Ignore updates when there are pending load params.
            if (!TextUtils.isEmpty(mTitle)) return;

            assumeNonNull(mUrl);
            title = mUrl.getSpec();
        } else if (isNativePage()) {
            title = mNativePage.getTitle();
        } else if (getWebContents() != null) {
            title = getWebContents().getTitle();
        }
        updateTitle(title);
    }

    /**
     * Cache the title for the current page.
     *
     * <p>{@link ContentViewClient#onUpdateTitle} is unreliable, particularly for navigating
     * backwards and forwards in the history stack, so pull the correct title whenever the page
     * changes. onUpdateTitle is only called when the title of a navigation entry changes. When the
     * user goes back a page the navigation entry exists with the correct title, thus the title is
     * not actually changed, and no notification is sent.
     *
     * @param title Title of the page.
     */
    void updateTitle(String title) {
        if (TextUtils.equals(mTitle, title)) return;

        setTitle(title);
        notifyPageTitleChanged();
    }

    @Override
    public @Nullable LoadUrlParams getPendingLoadParams() {
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
        if (mWaitingOnBgColorAfterHidingNativePage) {
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
        if (mTabBackgroundColor == newBackgroundColor) return;

        mTabBackgroundColor = newBackgroundColor;

        RecordHistogram.recordEnumeratedHistogram(
                BACKGROUND_COLOR_CHANGE_HISTOGRAM,
                backgroundColorChangeOrigin,
                BackgroundColorChangeOrigin.NUM_ENTRIES);
        for (TabObserver observer : mObservers) {
            observer.onBackgroundColorChanged(this, mTabBackgroundColor);
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
    private static long @Nullable [] getAllNativePtrs(Tab @Nullable [] tabsArray) {
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
     *
     * <p>{@link #getNativePage()} will still return the {@link NativePage} if there is one. All
     * initialization that needs to reoccur after a web contents swap should be added here.
     *
     * <p>NOTE: If you attempt to pass a native WebContents that does not have the same incognito
     * state as this tab this call will fail.
     *
     * @param webContents The WebContents object that will initialize all the browser components.
     */
    private void initWebContents(WebContents webContents) {
        try {
            TraceEvent.begin("ChromeTab.initWebContents");
            WebContents oldWebContents = mWebContents;
            mWebContents = webContents;

            ContentView cv = ContentView.createContentView(mThemedApplicationContext, webContents);
            cv.setContentDescription(
                    mThemedApplicationContext.getString(R.string.accessibility_content_view));
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ANNOTATED_PAGE_CONTENTS_VIRTUAL_STRUCTURE)) {
                cv.setVirtualStructureProvider(new PageContentProtoViewStructureBuilder());
            }
            mContentView = cv;
            webContents.setDelegates(
                    PRODUCT_VERSION,
                    new TabViewAndroidDelegate(this, cv),
                    cv,
                    getWindowAndroid(),
                    WebContents.createDefaultInternalsHolder());
            hideNativePage(false, null);

            if (oldWebContents != null) {
                updateImportance(oldWebContents, ChildProcessImportance.NORMAL);
                assumeNonNull(getWebContentsAccessibility(oldWebContents))
                        .setObscuredByAnotherView(false);
            }

            updateImportance(mWebContents, mImportance);

            ContentUtils.setUserAgentOverride(
                    mWebContents,
                    calculateUserAgentOverrideOption(null) == UserAgentOverrideOption.TRUE);

            mContentView.addOnAttachStateChangeListener(mAttachStateChangeListener);
            updateInteractableState();

            updateWebContentsDelegate();

            // TODO(crbug.com/40942165): Find a better way of indicating this is a background tab
            // (or
            // move the logic elsewhere).
            boolean isBackgroundTab = isDetached();

            assert mNativeTabAndroid != 0;
            assumeNonNull(mDelegateFactory);
            ContextMenuPopulatorFactory contextMenuPopulatorFactory =
                    mDelegateFactory.createContextMenuPopulatorFactory(this);
            assumeNonNull(contextMenuPopulatorFactory);
            assumeNonNull(mWebContentsDelegate);
            TabImplJni.get()
                    .initWebContents(
                            mNativeTabAndroid,
                            isOffTheRecord(),
                            isBackgroundTab,
                            webContents,
                            mWebContentsDelegate,
                            new TabContextMenuPopulatorFactory(contextMenuPopulatorFactory, this));

            mWebContents.notifyRendererPreferenceUpdate();
            mContentView.setImportantForAutofill(
                    prepareAutofillProvider(webContents)
                            ? View.IMPORTANT_FOR_AUTOFILL_YES
                            : View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
            TabHelpers.initWebContentsHelpers(this);
            notifyContentChanged();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    && ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                    && ChromeFeatureList.isEnabled(
                            SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
                mSensitiveContentClientObserver = this::setTabHasSensitiveContent;
                // Adding the observation has to happen after the native `initWebContents`, so that
                // the {@link SensitiveContentClient} is properly initialized.
                SensitiveContentClient sensitiveContentClient =
                        SensitiveContentClient.fromWebContents(webContents);
                sensitiveContentClient.addObserver(mSensitiveContentClientObserver);
                sensitiveContentClient.restoreContentSensitivityFromTabState(
                        getTabHasSensitiveContent());
            }
        } finally {
            TraceEvent.end("ChromeTab.initWebContents");
        }
    }

    private void updateWebContentsDelegate() {
        if (mWebContentsDelegate != null) {
            mWebContentsDelegate.destroy();
        }
        assumeNonNull(mDelegateFactory);
        TabWebContentsDelegateAndroid delegate = mDelegateFactory.createWebContentsDelegate(this);
        mWebContentsDelegate = new TabWebContentsDelegateAndroidImpl(this, delegate);
    }

    /**
     * Shows the given {@code nativePage} if it's not already showing.
     *
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
                        View view = mNativePage.getView();
                        assumeNonNull(view);
                        view.addOnAttachStateChangeListener(mAttachStateChangeListener);
                    }
                    if (isDisplayingBackForwardAnimation()) {
                        mNativePageSmoothTransitionDelegate = mNativePage.enableSmoothTransition();
                        assumeNonNull(mNativePageSmoothTransitionDelegate);
                        mNativePageSmoothTransitionDelegate.prepare();
                    }
                    pushNativePageStateToNavigationEntry();
                    onBackgroundColorChanged(BackgroundColorChangeOrigin.NATIVE_PAGE_SHOWN);
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
    private void hideNativePage(boolean notify, @Nullable Runnable postHideTask) {
        if (mNativePageSmoothTransitionDelegate != null) {
            mNativePageSmoothTransitionDelegate.cancel();
            mNativePageSmoothTransitionDelegate = null;
        } else if (isNativePage() && getView() != null) {
            getView().setAlpha(1.f);
        }
        NativePage previousNativePage = mNativePage;
        if (mNativePage != null) {
            if (!mNativePage.isFrozen()) {
                View view = mNativePage.getView();
                assumeNonNull(view);
                view.removeOnAttachStateChangeListener(mAttachStateChangeListener);
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
     *
     * @param factory TabDelegateFactory instance.
     */
    private void setDelegateFactory(TabDelegateFactory factory) {
        // Update the delegate factory, then recreate and propagate all delegates.
        mDelegateFactory = factory;

        updateWebContentsDelegate();

        WebContents webContents = getWebContents();
        if (webContents != null) {
            ContextMenuPopulatorFactory contextMenuPopulatorFactory =
                    mDelegateFactory.createContextMenuPopulatorFactory(this);
            assumeNonNull(contextMenuPopulatorFactory);
            assumeNonNull(mWebContentsDelegate);
            TabImplJni.get()
                    .updateDelegates(
                            mNativeTabAndroid,
                            mWebContentsDelegate,
                            new TabContextMenuPopulatorFactory(contextMenuPopulatorFactory, this));
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
     * Update the interactable state of the tab. If the state has changed, it will call the {@link
     * #onInteractableStateChanged(boolean)} method.
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
    private void restoreIfNeeded() {
        // Attempts to display the Paint Preview representation of this Tab.
        if (isFrozen()) StartupPaintPreviewHelper.showPaintPreviewOnRestore(this);

        try {
            TraceEvent.begin("Tab.restoreIfNeeded");
            assert !isFrozen() || mWebContentsState != null
                    : "crbug/1393848: A frozen tab must have WebContentsState to restore from.";
            // Restore is needed for a tab that is loaded for the first time. WebContents will
            // be restored from a saved state.
            if ((isFrozen()
                            && mWebContentsState != null
                            && !unfreezeContents(/* noRenderer= */ false))
                    || !needsReload()) {
                return;
            }

            if (mWebContents != null) {
                // Invoke switchUserAgentIfNeeded() from restoreIfNeeded() instead of loadIfNeeded()
                // to avoid reload without explicit user intent.
                switchUserAgentIfNeeded();
                mWebContents.getNavigationController().loadIfNecessary();
            }
            mIsBeingRestored = true;
            for (TabObserver observer : mObservers) observer.onRestoreStarted(this);
        } finally {
            TraceEvent.end("Tab.restoreIfNeeded");
        }
    }

    /**
     * Restores the WebContents from its saved state. This should only be called if the tab is
     * frozen with a saved TabState, and NOT if it was frozen for a lazy load.
     *
     * @param noRenderer Whether or not to create the WebContents without a renderer.
     * @return Whether or not the restoration was successful.
     */
    private boolean unfreezeContents(boolean noRenderer) {
        boolean restored = true;
        try {
            TraceEvent.begin("Tab.unfreezeContents");
            assert mWebContentsState != null;

            WebContents webContents =
                    mWebContentsState.restoreWebContents(getProfile(), isHidden(), noRenderer);

            UrlConstantResolver urlConstantResolver =
                    UrlConstantResolverFactory.getForProfile(mProfile);
            String failedRestoreUrl = urlConstantResolver.getNtpUrl();
            if (webContents == null) {
                // State restore failed, just create a new empty web contents as that is the best
                // that can be done at this point.
                webContents = WebContentsFactory.createWebContents(mProfile, isHidden(), false);
                for (TabObserver observer : mObservers) observer.onRestoreFailed(this);
                restored = false;

                assumeNonNull(mUrl);
                if (!mUrl.getSpec().isEmpty()) {
                    failedRestoreUrl = mUrl.getSpec();
                } else if (!TextUtils.isEmpty(
                        mWebContentsState.getFallbackUrlForRestorationFailure())) {
                    failedRestoreUrl = mWebContentsState.getFallbackUrlForRestorationFailure();
                }
            }
            Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                    assumeNonNull(getActivity()).getCompositorViewHolderSupplier();
            View compositorView = compositorViewHolderSupplier.get();
            webContents.setSize(compositorView.getWidth(), compositorView.getHeight());

            mWebContentsState.destroy();
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
            maybeLogAutofillProviderDoesntUseVirtualStructureMetric();
            mAutofillProvider = null;
            return false; // Autofill provider can't be prepared.
        }
        if (mAutofillProvider != null) {
            // Provider already existed. Swapping contents suffices.
            mAutofillProvider.setWebContents(newWebContents);
        } else {
            // TODO: crbug.com/432447902 — Provide only an activity context and push changes.
            mAutofillProvider =
                    new AutofillProvider(
                            new WeakReference(getContext()),
                            mContentView,
                            newWebContents,
                            getContext().getString(R.string.app_name));
            TabImplJni.get().initializeAutofillIfNecessary(mNativeTabAndroid);
        }
        addAutofillItemsToSelectionActionMenu(newWebContents);
        return true;
    }

    private void maybeLogAutofillProviderDoesntUseVirtualStructureMetric() {

        AutofillManager manager =
                ContextUtils.getApplicationContext().getSystemService(AutofillManager.class);
        if (!AutofillManagerWrapper.isAutofillSupported(manager)) {
            // If Android Autofill is not supported, the metric shouldn't be logged because it tells
            // about cases when 3P mode could be used, but it isn't used.
            return;
        }

        ComponentName componentName =
                AutofillManagerWrapper.getAutofillServiceComponentName(manager);
        if (componentName != null) {
            RecordHistogram.recordEnumeratedHistogram(
                    UMA_AUTOFILL_THIRD_PARTY_MODE_DISABLED_PROVIDER,
                    AutofillProviderUMA.getCurrentProvider(componentName.getPackageName()),
                    AutofillProviderUMA.Provider.MAX_VALUE);
        }
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
                new AutofillSelectionMenuItemHelper(mAutofillProvider));
        controller.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
    }

    @CalledByNative
    @Override
    public boolean isCustomTab() {
        ChromeActivity activity = getActivity();
        return activity != null && activity.isCustomTab();
    }

    @Override
    public boolean isTabInPWA() {
        // TODO(crbug.com/417720713): replace deprecated getActivity with something else.
        ChromeActivity activity = getActivity();
        if (activity == null) return false;
        @ActivityType int activityType = activity.getActivityType();
        return activityType == ActivityType.WEB_APK
                || activityType == ActivityType.TRUSTED_WEB_ACTIVITY;
    }

    @Override
    public boolean isTabInBrowser() {
        // TODO(crbug.com/417720713): replace deprecated getActivity with something else.
        ChromeActivity activity = getActivity();
        if (activity == null) return false;
        return activity.getActivityType() == ActivityType.TABBED;
    }

    @Override
    public long getTimestampMillis() {
        return mTimestampMillis;
    }

    @Override
    public void setTimestampMillis(long timestampMillis) {
        mTimestampMillis = timestampMillis;
        for (TabObserver tabObserver : mObservers) {
            tabObserver.onTimestampChanged(this, timestampMillis);
        }
    }

    /**
     * @return parent identifier for the {@link Tab}
     */
    @Override
    @CalledByNative
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
    @CalledByNative
    public @JniType("std::optional<base::Token>") @Nullable Token getTabGroupId() {
        return mTabGroupId;
    }

    @Override
    @CalledByNative
    public void setTabGroupId(@Nullable Token tabGroupId) {
        assert tabGroupId == null || !tabGroupId.isZero() : "A TabGroupId token must be non-zero.";
        if (Objects.equals(mTabGroupId, tabGroupId) || isDestroyed()) return;
        mTabGroupId = tabGroupId;
        for (TabObserver observer : mObservers) {
            observer.onTabGroupIdChanged(this, tabGroupId);
        }
        // This may be called before the native TabAndroid is initialized.
        if (mNativeTabAndroid != 0) {
            TabImplJni.get().notifyTabGroupChanged(mNativeTabAndroid, tabGroupId);
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
    public @Nullable WebContentsState getWebContentsState() {
        return mWebContentsState;
    }

    @VisibleForTesting
    void setWebContentsState(WebContentsState webContentsState) {
        if (mWebContentsState != null) {
            mWebContentsState.destroy();
        }
        mWebContentsState = webContentsState;
    }

    @VisibleForTesting
    void setAutofillProvider(AutofillProvider autofillProvider) {
        mAutofillProvider = autofillProvider;
    }

    @VisibleForTesting
    boolean hasBacking() {
        if (mWebContents == null) return false;
        return !TabImplJni.get().isPhysicalBackingSizeEmpty(mNativeTabAndroid, mWebContents);
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
    @CalledByNative
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
     *
     * @param predicate Handle for a deletion predicate interpreted by native code. Only valid
     *     during this call frame.
     */
    @CalledByNative
    private void deleteNavigationEntriesFromFrozenState(long predicate) {
        if (mWebContentsState == null) return;
        boolean success = mWebContentsState.deleteNavigationEntries(predicate);
        if (success) {
            notifyNavigationEntriesDeleted();
        }
    }

    private static @Nullable WebContentsAccessibility getWebContentsAccessibility(
            @Nullable WebContents webContents) {
        return webContents != null ? WebContentsAccessibility.fromWebContents(webContents) : null;
    }

    private void destroyNativePageInternal(@Nullable NativePage nativePage) {
        if (nativePage == null) return;
        assert nativePage != mNativePage : "Attempting to destroy active page.";

        nativePage.destroy();
    }

    /**
     * Destroys the current {@link WebContents}.
     *
     * @param deleteNativeWebContents Whether or not to delete the native WebContents pointer.
     */
    private void destroyWebContents(boolean deleteNativeWebContents) {
        if (mWebContents == null) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                && ChromeFeatureList.isEnabled(
                        SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
            SensitiveContentClient.fromWebContents(mWebContents)
                    .removeObserver(assumeNonNull(mSensitiveContentClientObserver));
        }

        if (mAutofillProvider != null) {
            mAutofillProvider.destroy();
            mAutofillProvider = null;
        }

        assumeNonNull(mContentView);
        mContentView.removeOnAttachStateChangeListener(mAttachStateChangeListener);
        mContentView = null;
        updateInteractableState();

        WebContents contentsToDestroy = mWebContents;
        if (contentsToDestroy.getViewAndroidDelegate() != null
                && contentsToDestroy.getViewAndroidDelegate() instanceof TabViewAndroidDelegate) {
            ((TabViewAndroidDelegate) contentsToDestroy.getViewAndroidDelegate()).destroy();
        }
        mWebContents = null;
        if (mWebContentsDelegate != null) {
            mWebContentsDelegate.destroy();
            mWebContentsDelegate = null;
        }

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

    public @UserAgentOverrideOption int calculateUserAgentOverrideOption(@Nullable GURL url) {
        WebContents webContents = getWebContents();
        boolean currentRequestDesktopSite = TabUtils.isUsingDesktopUserAgent(webContents);
        // INHERIT means use the same UA that was used last time.
        @UserAgentOverrideOption int userAgentOverrideOption = UserAgentOverrideOption.INHERIT;

        if (url == null && webContents != null) {
            url = webContents.getVisibleUrl();
        }

        boolean shouldRequestDesktopSite =
                DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, url, getContext());

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

    private void switchUserAgentIfNeeded() {
        if (calculateUserAgentOverrideOption(null) == UserAgentOverrideOption.INHERIT
                || getWebContents() == null) {
            return;
        }
        boolean usingDesktopUserAgent =
                getWebContents().getNavigationController().getUseDesktopUserAgent();
        TabUtils.switchUserAgent(this, /* switchToDesktop= */ !usingDesktopUserAgent);
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
        WebContents webContents = getWebContents();
        if (webContents == null || mWebContentsDelegate == null) return false;
        return mWebContentsDelegate.isTrustedWebActivity(webContents);
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

    @Override
    @CalledByNative
    public boolean getIsPinned() {
        return mIsPinned;
    }

    @Override
    @CalledByNative
    public void setIsPinned(boolean isPinned) {
        boolean isPinnedTabFeatureEnabled =
                StripLayoutUtils.isTabPinningFromStripEnabled()
                        || ChromeFeatureList.sAndroidPinnedTabs.isEnabled();

        // Remove the tab pinned state if the feature is disabled.
        isPinned = isPinnedTabFeatureEnabled && isPinned;

        if (mIsPinned == isPinned || isDestroyed()) return;
        mIsPinned = isPinned;
        for (TabObserver observer : mObservers) {
            observer.onTabPinnedStateChanged(this, isPinned);
        }
        // This may be called before the native tab is initialized.
        if (mNativeTabAndroid != 0) {
            TabImplJni.get().notifyPinnedStateChanged(mNativeTabAndroid, isPinned);
        }
    }

    @Override
    public @MediaState int getMediaState() {
        return mMediaState;
    }

    @Override
    @CalledByNative
    public void setMediaState(@MediaState int mediaState) {
        if (mMediaState == mediaState) return;
        mMediaState = mediaState;
        if (ChromeFeatureList.sMediaIndicatorsAndroid.isEnabled()) {
            for (TabObserver observer : mObservers) {
                observer.onMediaStateChanged(this, mediaState);
            }
        }
    }

    @Override
    public void onTabRestoredFromArchivedTabModel() {
        for (TabObserver observer : mObservers) {
            observer.onTabUnarchived(this);
        }
    }

    @Override
    public void onAddedToTabModel(
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            SelectionStateSupplier selectionStateSupplier) {
        // Tabs should not be attached to multiple tab models.
        assert mCurrentTabSupplier == null;

        mCurrentTabSupplier = currentTabSupplier;
        mSelectionStateSupplier = selectionStateSupplier;
    }

    @Override
    public void onRemovedFromTabModel(ObservableSupplier<@Nullable Tab> currentTabSupplier) {
        // Usually mCurrentTabSupplier should equal currentTabSupplier when it's removed from the
        // TabModel. However, during reparenting it appears there are situations where the tab is
        // not removed from the original TabModel before being added to the new TabModel. In these
        // cases, mCurrentTabSupplier will be null as a result of the logic in updateAttachment().
        assert mCurrentTabSupplier == null || mCurrentTabSupplier == currentTabSupplier;
        mCurrentTabSupplier = null;
        mSelectionStateSupplier = null;
    }

    @Override
    @CalledByNative
    public boolean isMultiSelected() {
        if (mSelectionStateSupplier == null) return false;
        return mSelectionStateSupplier.isTabMultiSelected(mId);
    }

    @CalledByNative
    public static void closeTabFromNative(Tab tab) {
        TabWindowManager manager = TabWindowManagerSingleton.getInstance();
        TabModel model = manager.getTabModelForTab(tab);
        if (model == null) return;

        model.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                        /* allowDialog= */ false);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        TabImpl fromWebContents(@Nullable WebContents webContents);

        void init(TabImpl caller, @JniType("Profile*") Profile profile, int id);

        void destroy(long nativeTabAndroid);

        boolean hasParentCollection(long nativeTabAndroid);

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

        boolean isPhysicalBackingSizeEmpty(long nativeTabAndroid, WebContents webContents);

        void onPhysicalBackingSizeChanged(
                long nativeTabAndroid, WebContents webContents, int width, int height);

        void setActiveNavigationEntryTitleForUrl(
                long nativeTabAndroid,
                @JniType("std::string") String url,
                @JniType("std::u16string") String title);

        void loadOriginalImage(long nativeTabAndroid);

        boolean handleNonNavigationAboutURL(GURL url);

        void onShow(long nativeTabAndroid);

        void notifyPinnedStateChanged(long nativeTabAndroid, boolean isPinned);

        void notifyTabGroupChanged(
                long nativeTabAndroid,
                @JniType("std::optional<base::Token>") @Nullable Token tabGroupId);
    }

    @VisibleForTesting
    @ChildProcessImportance
    int getImportance() {
        return mImportance;
    }
}
