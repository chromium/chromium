// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActionModeHandler;
import org.chromium.chrome.browser.ChromePowerModeVoter;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.crash.PureJavaExceptionReporter;
import org.chromium.chrome.browser.directactions.DirectActionInitializer;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.messages.ChromeMessageQueueMediator;
import org.chromium.chrome.browser.messages.MessageContainerCoordinator;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.paint_preview.DemoPaintPreview;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareButtonController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.AccessibilityVisibilityHandler;
import org.chromium.chrome.browser.tab.AutofillSessionLifetimeController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.OptionalNewTabButtonController;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinatorFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageContainer;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.vr.VrModeObserver;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator
        implements Destroyable, InflationObserver, NativeInitObserver,
                   MenuOrKeyboardActionController.MenuOrKeyboardActionHandler, AppMenuBlocker {
    protected ChromeActivity mActivity;
    protected @Nullable AppMenuCoordinator mAppMenuCoordinator;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabObscuringHandler mTabObscuringHandler;
    private final AccessibilityVisibilityHandler mAccessibilityVisibilityHandler;
    private final @Nullable AutofillSessionLifetimeController mAutofillSessionLifetimeController;

    private ActivityTabProvider mActivityTabProvider;
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    protected @Nullable FindToolbarManager mFindToolbarManager;
    private @Nullable FindToolbarObserver mFindToolbarObserver;

    private Callback<LayoutManagerImpl> mLayoutManagerSupplierCallback;
    private OverlayPanelManager mOverlayPanelManager;
    private OverlayPanelManager.OverlayPanelManagerObserver mOverlayPanelManagerObserver;

    private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderOneShotSupplier;
    private LayoutStateProvider mLayoutStateProvider;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /** A means of providing the theme color to different features. */
    private TopUiThemeColorProvider mTopUiThemeColorProvider;

    @Nullable
    private Callback<Boolean> mOnOmniboxFocusChangedListener;
    protected ToolbarManager mToolbarManager;
    protected Supplier<Boolean> mCanAnimateBrowserControls;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    private VrModeObserver mVrModeObserver;

    private BottomSheetManager mBottomSheetManager;
    private ManagedBottomSheetController mBottomSheetController;
    private SnackbarManager mBottomSheetSnackbarManager;

    private ScrimCoordinator mScrimCoordinator;
    private DirectActionInitializer mDirectActionInitializer;
    private List<ButtonDataProvider> mButtonDataProviders;
    private IdentityDiscController mIdentityDiscController;
    private ChromeActionModeHandler mChromeActionModeHandler;
    private final ToolbarActionModeCallback mActionModeControllerCallback;
    private ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier =
            new ObservableSupplierImpl<>();
    protected final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<BookmarkBridge> mBookmarkBridgeSupplier;
    private final OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private BottomSheetObserver mContextualSearchSuppressor;
    private final Supplier<ContextualSearchManager> mContextualSearchManagerSupplier;
    protected final CallbackController mCallbackController;
    @Nullable
    private BrowserControlsManager mBrowserControlsManager;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final OneshotSupplier<StartSurface> mStartSurfaceSupplier;
    @Nullable
    private ManagedMessageDispatcher mMessageDispatcher;
    @Nullable
    private MessageContainerCoordinator mMessageContainerCoordinator;
    @Nullable
    private ChromeMessageQueueMediator mMessageQueueMediator;
    private LayoutManagerImpl mLayoutManager;
    protected OneshotSupplier<ToolbarIntentMetadata> mIntentMetadataOneshotSupplier;
    // This supplier only ever updated when feature TOOLBAR_IPH_ANDROID is enabled.
    protected OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier = new OneshotSupplierImpl<>();
    protected Supplier<Tab> mStartSurfaceParentTabSupplier;
    private final ObservableSupplierImpl<Tab> mActivityTabSupplier = new ObservableSupplierImpl<>();
    private final ActivityTabProvider.ActivityTabTabObserver mTabObserver;
    @Nullable
    private VoiceRecognitionHandler.Observer mMicStateObserver;

    /**
     * Create a new {@link RootUiCoordinator} for the given activity.
     * @param activity The containing {@link ChromeActivity}. TODO(https://crbug.com/931496):
     *         Remove this in favor of passing in direct dependencies.
     * @param onOmniboxFocusChangedListener Callback<Boolean> callback to invoke when Omnibox focus
     *         changes.
     * @param shareDelegateSupplier Supplies {@link ShareDelegate} object.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkBridgeSupplier Supplier of the bookmark bridge for the current profile.
     * @param contextualSearchManagerSupplier Supplier of the {@link ContextualSearchManager}.
     * @param tabModelSelectorSupplier Supplier of the {@link TabModelSelector}.
     * @param startSurfaceSupplier Supplier of the {@link StartSurface}.
     * @param intentMetadataOneshotSupplier Supplier with information about the launching intent.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     */
    public RootUiCoordinator(ChromeActivity activity,
            @Nullable Callback<Boolean> onOmniboxFocusChangedListener,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ActivityTabProvider tabProvider, ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier,
            Supplier<ContextualSearchManager> contextualSearchManagerSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<StartSurface> startSurfaceSupplier,
            OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            @NonNull Supplier<Tab> startSurfaceParentTabSupplier) {
        mCallbackController = new CallbackController();
        mActivity = activity;
        mOnOmniboxFocusChangedListener = onOmniboxFocusChangedListener;
        mActivity.getLifecycleDispatcher().register(this);

        mMenuOrKeyboardActionController = mActivity.getMenuOrKeyboardActionController();
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(this);
        mActivityTabProvider = tabProvider;

        mLayoutManagerSupplierCallback = this::onLayoutManagerAvailable;
        mActivity.getLayoutManagerSupplier().addObserver(mLayoutManagerSupplierCallback);

        mShareDelegateSupplier = shareDelegateSupplier;
        mTabObscuringHandler = new TabObscuringHandler();
        mAccessibilityVisibilityHandler = new AccessibilityVisibilityHandler(
                activity.getLifecycleDispatcher(), mActivityTabProvider, mTabObscuringHandler);
        // While Autofill is supported on Android O, meaningful Autofill interactions in Chrome
        // require the compatibility mode introduced in Android P.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mAutofillSessionLifetimeController = new AutofillSessionLifetimeController(
                    activity, activity.getLifecycleDispatcher(), mActivityTabProvider);
        } else {
            mAutofillSessionLifetimeController = null;
        }
        mProfileSupplier = profileSupplier;
        mBookmarkBridgeSupplier = bookmarkBridgeSupplier;
        mAppMenuSupplier = new OneshotSupplierImpl<>();
        mContextualSearchManagerSupplier = contextualSearchManagerSupplier;
        mActionModeControllerCallback = new ToolbarActionModeCallback();

        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        mOmniboxFocusStateSupplier.set(false);

        mLayoutStateProviderOneShotSupplier = layoutStateProviderOneshotSupplier;
        mLayoutStateProviderOneShotSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));

        mStartSurfaceSupplier = startSurfaceSupplier;
        mIntentMetadataOneshotSupplier = intentMetadataOneshotSupplier;

        mStartSurfaceParentTabSupplier = startSurfaceParentTabSupplier;

        mTabObserver = new ActivityTabProvider.ActivityTabTabObserver(mActivityTabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                mActivityTabSupplier.set(tab);
            }
        };
        mTopUiThemeColorProvider = new TopUiThemeColorProvider(mActivity, mActivityTabSupplier,
                mActivity::getActivityThemeColor, mActivity::isTablet);
    }

    // TODO(pnoland, crbug.com/865801): remove this in favor of wiring it directly.
    public ToolbarManager getToolbarManager() {
        return mToolbarManager;
    }

    // TODO(jinsukkim): remove this in favor of wiring it directly.
    /**
     * @return {@link ThemeColorProvider} for top UI.
     */
    public TopUiThemeColorProvider getTopUiThemeColorProvider() {
        return mTopUiThemeColorProvider;
    }

    @Override
    public void destroy() {
        // TODO(meiliang): Understand why we need to set most of the class member instances to null
        //  other than the mActivity. If the nulling calls are not necessary, we can remove them.
        mCallbackController.destroy();
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(this);

        mActivity.getLayoutManagerSupplier().removeObserver(mLayoutManagerSupplierCallback);

        if (mMessageDispatcher != null) {
            mMessageDispatcher.dismissAllMessages(DismissReason.ACTIVITY_DESTROYED);
            MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
            mMessageDispatcher = null;
        }

        if (mMessageQueueMediator != null) {
            mMessageQueueMediator.destroy();
            mMessageQueueMediator = null;
        }

        if (mMessageContainerCoordinator != null) {
            mMessageContainerCoordinator.destroy();
            mMessageContainerCoordinator = null;
        }

        if (mOverlayPanelManager != null) {
            mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
        }

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }

        if (mToolbarManager != null) {
            if (mMicStateObserver != null && mToolbarManager.getVoiceRecognitionHandler() != null) {
                mToolbarManager.getVoiceRecognitionHandler().removeObserver(mMicStateObserver);
            }
            mToolbarManager.destroy();
            mToolbarManager = null;
        }

        if (mAppMenuCoordinator != null) {
            mAppMenuCoordinator.unregisterAppMenuBlocker(this);
            mAppMenuCoordinator.unregisterAppMenuBlocker(mActivity);
            mAppMenuCoordinator.destroy();
        }

        if (mTopUiThemeColorProvider != null) {
            mTopUiThemeColorProvider.destroy();
            mTopUiThemeColorProvider = null;
        }

        mTabObserver.destroy();

        if (mFindToolbarManager != null) mFindToolbarManager.removeObserver(mFindToolbarObserver);

        if (mVrModeObserver != null) VrModuleProvider.unregisterVrModeObserver(mVrModeObserver);

        if (mModalDialogManagerObserver != null && mActivity.getModalDialogManager() != null) {
            mActivity.getModalDialogManager().removeObserver(mModalDialogManagerObserver);
        }

        if (mBottomSheetManager != null) mBottomSheetManager.destroy();
        if (mBottomSheetController != null) {
            if (mContextualSearchSuppressor != null) {
                mBottomSheetController.removeObserver(mContextualSearchSuppressor);
            }
            BottomSheetControllerFactory.detach(mBottomSheetController);
            mBottomSheetController.destroy();
        }

        if (mBrowserControlsManager != null) {
            mBrowserControlsManager.destroy();
            mBrowserControlsManager = null;
        }

        if (mButtonDataProviders != null) {
            for (ButtonDataProvider provider : mButtonDataProviders) {
                provider.destroy();
            }

            mButtonDataProviders = null;
        }

        if (mScrimCoordinator != null) mScrimCoordinator.destroy();
        mScrimCoordinator = null;

        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }

        mActivity = null;
    }

    @Override
    public void onPreInflationStartup() {
        initializeBottomSheetController();
    }

    @Override
    public void onInflationComplete() {
        // Allow the ChromePowerModeVoter instance to observe any touch events on the view
        // hierarchy, so that we can avoid power throttling while the user interacts with Java views
        // in the main Chrome activity.
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        ((CoordinatorLayoutForPointer) coordinator)
                .setTouchEventCallback(ChromePowerModeVoter.getInstance().getTouchEventCallback());

        mScrimCoordinator = buildScrimWidget();

        initFindToolbarManager();
        initializeToolbar();
    }

    @Override
    public void onPostInflationStartup() {
        initAppMenu();
        initDirectActionInitializer();
        initContextualSearchSuppressor();
        if (mAppMenuCoordinator != null) {
            mModalDialogManagerObserver = new ModalDialogManagerObserver() {
                @Override
                public void onDialogAdded(PropertyModel model) {
                    mAppMenuCoordinator.getAppMenuHandler().hideAppMenu();
                }
            };
            mActivity.getModalDialogManager().addObserver(mModalDialogManagerObserver);
        }
        mChromeActionModeHandler = new ChromeActionModeHandler(mActivity.getActivityTabProvider(),
                mToolbarManager::onActionBarVisibilityChanged, (searchText) -> {
                    if (mTabModelSelectorSupplier.get() == null) return;

                    String query = ActionModeCallbackHelper.sanitizeQuery(
                            searchText, ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
                    if (TextUtils.isEmpty(query)) return;

                    Tab tab = mActivity.getActivityTabProvider().get();
                    TrackerFactory
                            .getTrackerForProfile(Profile.fromWebContents(tab.getWebContents()))
                            .notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);

                    mTabModelSelectorSupplier.get().openNewTab(
                            generateUrlParamsForSearch(tab, query),
                            TabLaunchType.FROM_LONGPRESS_FOREGROUND, tab, tab.isIncognito());
                }, mShareDelegateSupplier);
        mVrModeObserver = new VrModeObserver() {
            @Override
            public void onEnterVr() {
                mFindToolbarManager.hideToolbar();
            }

            @Override
            public void onExitVr() {}
        };
        VrModuleProvider.registerVrModeObserver(mVrModeObserver);

        // Ensure the bottom sheet's container has been laid out at least once before hiding it.
        // TODO(1196804): This should be owned by the BottomSheetControllerImpl, but there are some
        //                complexities around the order of events resulting from waiting for layout.
        ViewGroup sheetContainer = mActivity.findViewById(R.id.sheet_container);
        if (!sheetContainer.isLaidOut()) {
            sheetContainer.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View view, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    sheetContainer.setVisibility(View.GONE);
                    sheetContainer.removeOnLayoutChangeListener(this);
                }
            });
        } else {
            sheetContainer.setVisibility(View.GONE);
        }
    }

    @Override
    @CallSuper
    public void onFinishNativeInitialization() {
        // TODO(crbug.com/1185887): Move feature flag and parameters into a separate class in
        // the Messages component.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE)) {
            MessageContainer container = mActivity.findViewById(R.id.message_container);
            mMessageContainerCoordinator =
                    new MessageContainerCoordinator(container, getBrowserControlsManager());
            long autodismissDurationMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                    "autodismiss_duration_ms", 10 * (int) DateUtils.SECOND_IN_MILLIS);
            long autodismissDurationWithA11yMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                    "autodismiss_duration_with_a11y_ms", 30 * (int) DateUtils.SECOND_IN_MILLIS);
            Supplier<Long> autodismissDurationSupplier = () -> {
                return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                        ? autodismissDurationWithA11yMs
                        : autodismissDurationMs;
            };
            mMessageDispatcher = MessagesFactory.createMessageDispatcher(container,
                    mMessageContainerCoordinator::getMessageMaxTranslation,
                    autodismissDurationSupplier,
                    mActivity.getWindowAndroid()::startAnimationOverContent);
            mMessageQueueMediator =
                    new ChromeMessageQueueMediator(mActivity.getBrowserControlsManager(),
                            mMessageContainerCoordinator, mActivity.getFullscreenManager(),
                            mActivityTabProvider, mLayoutStateProviderOneShotSupplier,
                            mActivity.getModalDialogManagerSupplier(), mMessageDispatcher);
            mMessageDispatcher.setDelegate(mMessageQueueMediator);
            MessagesFactory.attachMessageDispatcher(
                    mActivity.getWindowAndroid(), mMessageDispatcher);
        }
    }

    /**
     * Generate the LoadUrlParams necessary to load the specified search query.
     */
    private static LoadUrlParams generateUrlParamsForSearch(Tab tab, String query) {
        String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery(query);
        String headers = GeolocationHeader.getGeoHeader(url, tab);

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(headers);
        loadUrlParams.setTransitionType(PageTransition.GENERATED);
        return loadUrlParams;
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    @VisibleForTesting
    public void onShareMenuItemSelected(final boolean shareDirectly, final boolean isIncognito) {
        ShareDelegate shareDelegate = mShareDelegateSupplier.get();
        Tab tab = mActivityTabProvider.get();

        if (shareDelegate == null || tab == null) return;

        if (shareDirectly) {
            RecordUserAction.record("MobileMenuDirectShare");
        } else {
            RecordUserAction.record("MobileMenuShare");
        }
        shareDelegate.share(tab, shareDirectly, ShareOrigin.OVERFLOW_MENU);
    }

    // MenuOrKeyboardActionHandler implementation

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.show_menu && mAppMenuCoordinator != null) {
            mAppMenuCoordinator.showAppMenuForKeyboardEvent();
            return true;
        } else if (id == R.id.find_in_page_id) {
            if (mFindToolbarManager == null) return false;

            mFindToolbarManager.showToolbar();

            Tab tab = mActivityTabProvider.get();
            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
                new UkmRecorder.Bridge().recordEventWithBooleanMetric(
                        tab.getWebContents(), "MobileMenu.FindInPage", "HasOccurred");
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
            return true;
        } else if (id == R.id.share_menu_id || id == R.id.direct_share_menu_id) {
            onShareMenuItemSelected(id == R.id.direct_share_menu_id,
                    mTabModelSelectorSupplier.get().isIncognitoSelected());
        } else if (id == R.id.paint_preview_show_id) {
            DemoPaintPreview.showForTab(mActivityTabProvider.get());
        } else if (id == R.id.get_image_descriptions_id) {
            ImageDescriptionsController.getInstance().onImageDescriptionsMenuItemSelected(mActivity,
                    mActivity.getModalDialogManager(), mActivityTabProvider.get().getWebContents());
        }

        return false;
    }

    // AppMenuBlocker implementation

    @Override
    public boolean canShowAppMenu() {
        // TODO(https:crbug.com/931496): Eventually the ContextualSearchManager,
        // EphemeralTabCoordinator, and FindToolbarManager will all be owned by this class.

        // Do not show the menu if Contextual Search panel is opened.
        if (mContextualSearchManagerSupplier.get() != null
                && mContextualSearchManagerSupplier.get().isSearchPanelOpened()) {
            return false;
        }

        // Do not show the menu if we are in find in page view.
        if (mFindToolbarManager != null && mFindToolbarManager.isShowing()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            return false;
        }

        return true;
    }

    /**
     * Performs a direct action.
     *
     * @param actionId Name of the direct action to perform.
     * @param arguments Arguments for this action.
     * @param cancellationSignal Signal used to cancel a direct action from the caller.
     * @param callback Callback to run when the action is done.
     */
    public void onPerformDirectAction(String actionId, Bundle arguments,
            CancellationSignal cancellationSignal, Consumer<Bundle> callback) {
        if (mDirectActionInitializer == null) return;
        mDirectActionInitializer.onPerformDirectAction(
                actionId, arguments, cancellationSignal, callback);
    }

    /**
     * Lists direct actions supported.
     *
     * Returns a list of direct actions supported by the Activity associated with this
     * RootUiCoordinator.
     *
     * @param cancellationSignal Signal used to cancel a direct action from the caller.
     * @param callback Callback to run when the action is done.
     */
    public void onGetDirectActions(CancellationSignal cancellationSignal, Consumer<List> callback) {
        if (mDirectActionInitializer == null) return;
        mDirectActionInitializer.onGetDirectActions(cancellationSignal, callback);
    }

    // Protected class methods
    protected void onLayoutManagerAvailable(LayoutManagerImpl layoutManager) {
        mLayoutManager = layoutManager;
        if (mOverlayPanelManager != null) {
            mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
        }
        mOverlayPanelManager = layoutManager.getOverlayPanelManager();

        if (mOverlayPanelManagerObserver == null) {
            mOverlayPanelManagerObserver = new OverlayPanelManager.OverlayPanelManagerObserver() {
                @Override
                public void onOverlayPanelShown() {
                    if (mFindToolbarManager != null) {
                        mFindToolbarManager.hideToolbar(false);
                    }
                }

                @Override
                public void onOverlayPanelHidden() {}
            };
        }

        mOverlayPanelManager.addObserver(mOverlayPanelManagerObserver);
    }

    /**
     * Constructs {@link ToolbarManager} and the handler necessary for controlling the menu on the
     * {@link Toolbar}.
     */
    protected void initializeToolbar() {
        try (TraceEvent te = TraceEvent.scoped("RootUiCoordinator.initializeToolbar")) {
            final View controlContainer = mActivity.findViewById(R.id.control_container);
            assert controlContainer != null;
            ToolbarControlContainer toolbarContainer = (ToolbarControlContainer) controlContainer;
            Callback<Boolean> urlFocusChangedCallback = hasFocus -> {
                if (mOnOmniboxFocusChangedListener != null) {
                    mOnOmniboxFocusChangedListener.onResult(hasFocus);
                }
                mOmniboxFocusStateSupplier.set(hasFocus);
            };

            mIdentityDiscController = new IdentityDiscController(
                    mActivity, mActivity.getLifecycleDispatcher(), mProfileSupplier);
            ShareButtonController shareButtonController = new ShareButtonController(mActivity,
                    mActivityTabProvider, mShareDelegateSupplier, new ShareUtils(),
                    mActivity.getLifecycleDispatcher(), mActivity.getModalDialogManager(),
                    () -> mToolbarManager.setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS));
            VoiceToolbarButtonController.VoiceSearchDelegate voiceSearchDelegate =
                    new VoiceToolbarButtonController.VoiceSearchDelegate() {
                        @Override
                        public boolean isVoiceSearchEnabled() {
                            VoiceRecognitionHandler voiceRecognitionHandler =
                                    mToolbarManager.getVoiceRecognitionHandler();
                            if (voiceRecognitionHandler == null) return false;
                            return voiceRecognitionHandler.isVoiceSearchEnabled();
                        }

                        @Override
                        public void startVoiceRecognition() {
                            VoiceRecognitionHandler voiceRecognitionHandler =
                                    mToolbarManager.getVoiceRecognitionHandler();
                            if (voiceRecognitionHandler == null) return;
                            voiceRecognitionHandler.startVoiceRecognition(
                                    VoiceInteractionSource.TOOLBAR);
                        }
                    };
            VoiceToolbarButtonController voiceToolbarButtonController =
                    new VoiceToolbarButtonController(mActivity,
                            AppCompatResources.getDrawable(mActivity, R.drawable.btn_mic),
                            mActivityTabProvider, mActivity.getLifecycleDispatcher(),
                            mActivity.getModalDialogManager(), voiceSearchDelegate);
            if (AdaptiveToolbarFeatures.isEnabled()) {
                OptionalNewTabButtonController newTabButtonController =
                        new OptionalNewTabButtonController(mActivity,
                                mActivity.getLifecycleDispatcher(),
                                mActivity.getTabCreatorManagerSupplier(),
                                mTabModelSelectorSupplier);
                AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                        new AdaptiveToolbarButtonController();
                adaptiveToolbarButtonController.addButtonVariant(
                        AdaptiveToolbarButtonVariant.NEW_TAB, newTabButtonController);
                adaptiveToolbarButtonController.addButtonVariant(
                        AdaptiveToolbarButtonVariant.SHARE, shareButtonController);
                adaptiveToolbarButtonController.addButtonVariant(
                        AdaptiveToolbarButtonVariant.VOICE, voiceToolbarButtonController);
                mButtonDataProviders =
                        Arrays.asList(mIdentityDiscController, adaptiveToolbarButtonController);
            } else {
                mButtonDataProviders = Arrays.asList(mIdentityDiscController, shareButtonController,
                        voiceToolbarButtonController);
            }
            mToolbarManager = new ToolbarManager(mActivity, mActivity.getBrowserControlsManager(),
                    mActivity.getFullscreenManager(), toolbarContainer,
                    mActivity.getCompositorViewHolder(), urlFocusChangedCallback,
                    mTopUiThemeColorProvider, mTabObscuringHandler, mShareDelegateSupplier,
                    mIdentityDiscController, mButtonDataProviders, mActivityTabProvider,
                    mScrimCoordinator, mActionModeControllerCallback, mFindToolbarManager,
                    mProfileSupplier, mBookmarkBridgeSupplier, mCanAnimateBrowserControls,
                    mLayoutStateProviderOneShotSupplier, mAppMenuSupplier,
                    shouldShowMenuUpdateBadge(), mTabModelSelectorSupplier, mStartSurfaceSupplier,
                    mOmniboxFocusStateSupplier, mIntentMetadataOneshotSupplier,
                    mPromoShownOneshotSupplier, mActivity.getWindowAndroid(),
                    mActivity::isInOverviewMode, mActivity.getModalDialogManagerSupplier(),
                    mActivity.getStatusBarColorController(),
                    /* appMenuDelegate= */ mActivity, mActivity.getLifecycleDispatcher(),
                    mStartSurfaceParentTabSupplier);
            if (!mActivity.supportsAppMenu()) {
                mToolbarManager.getToolbar().disableMenuButton();
            }

            VoiceRecognitionHandler voiceRecognitionHandler =
                    mToolbarManager.getVoiceRecognitionHandler();
            if (voiceRecognitionHandler != null) {
                mMicStateObserver = voiceToolbarButtonController::updateMicButtonState;
                voiceRecognitionHandler.addObserver(mMicStateObserver);
            }
        }
    }

    /**
     * Gives concrete implementation of {@link ScrimCoordinator.SystemUiScrimDelegate} and
     * constructs {@link ScrimCoordinator}.
     */
    protected ScrimCoordinator buildScrimWidget() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        ScrimCoordinator.SystemUiScrimDelegate delegate =
                new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {
                        mActivity.getStatusBarColorController().setStatusBarScrimFraction(
                                scrimFraction);
                    }

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                };
        return new ScrimCoordinator(mActivity, delegate, coordinator,
                ApiCompatibilityUtils.getColor(coordinator.getResources(),
                        R.color.omnibox_focused_fading_background_color));
    }

    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        assert mLayoutStateProvider == null : "The LayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onStartedShowing(int layoutType, boolean showToolbar) {
                if (layoutType != LayoutType.BROWSING
                        && layoutType != LayoutType.SIMPLE_ANIMATION) {
                    // Hide contextual search.
                    if (mContextualSearchManagerSupplier.get() != null) {
                        mContextualSearchManagerSupplier.get().dismissContextualSearchBar();
                    }
                }

                if (layoutType == LayoutType.TAB_SWITCHER) {
                    // Hide find toolbar and app menu.
                    if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
                    hideAppMenu();
                }
            }

            @Override
            public void onFinishedShowing(int layoutType) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    // Ideally we wouldn't allow the app menu to show while animating the
                    // overview mode. This is hard to track, however, because in some
                    // instances #onOverviewModeStartedShowing is called after
                    // #onOverviewModeFinishedShowing (see https://crbug.com/969047).
                    // Once that bug is fixed, we can remove this call to hide in favor of
                    // disallowing app menu shows during animation. Alternatively, we
                    // could expose a way to query whether an animation is in progress.
                    hideAppMenu();
                }
            }

            @Override
            public void onStartedHiding(
                    int layoutType, boolean showToolbar, boolean delayAnimation) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    hideAppMenu();
                }
            }

            @Override
            public void onFinishedHiding(int layoutType) {
                if (layoutType != LayoutType.TAB_SWITCHER) {
                    hideAppMenu();
                }
            }
        };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void initAppMenu() {
        // TODO(https://crbug.com/931496): Revisit this as part of the broader
        // discussion around activity-specific UI customizations.
        if (mActivity.supportsAppMenu()) {
            mAppMenuCoordinator = AppMenuCoordinatorFactory.createAppMenuCoordinator(mActivity,
                    mActivity.getLifecycleDispatcher(), mToolbarManager, mActivity,
                    mActivity.getWindow().getDecorView(),
                    mActivity.getWindow().getDecorView().findViewById(R.id.menu_anchor_stub));

            mAppMenuCoordinator.registerAppMenuBlocker(this);
            mAppMenuCoordinator.registerAppMenuBlocker(mActivity);

            mAppMenuSupplier.set(mAppMenuCoordinator);
        }
    }

    private void hideAppMenu() {
        if (mAppMenuCoordinator != null) mAppMenuCoordinator.getAppMenuHandler().hideAppMenu();
    }

    private void initFindToolbarManager() {
        if (!mActivity.supportsFindInPage()) return;

        int stubId = R.id.find_toolbar_stub;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            stubId = R.id.find_toolbar_tablet_stub;
        }
        mFindToolbarManager = new FindToolbarManager(mActivity.findViewById(stubId),
                mTabModelSelectorSupplier.get(), mActivity.getWindowAndroid(),
                mActionModeControllerCallback);

        mFindToolbarObserver = new FindToolbarObserver() {
            @Override
            public void onFindToolbarShown() {
                RootUiCoordinator.this.onFindToolbarShown();
            }

            @Override
            public void onFindToolbarHidden() {}
        };

        mFindToolbarManager.addObserver(mFindToolbarObserver);
    }

    /**
     * Called when the find in page toolbar is shown. Sub-classes may override to manage
     * cross-feature interaction, e.g. hide other features when this feature is shown.
     */
    protected void onFindToolbarShown() {
        if (mContextualSearchManagerSupplier.get() != null) {
            mContextualSearchManagerSupplier.get().hideContextualSearch(
                    OverlayPanel.StateChangeReason.UNKNOWN);
        }
    }

    /**
     * @return Whether the "update available" badge should be displayed on menu button(s) in the
     * context of this coordinator's UI.
     **/
    protected boolean shouldShowMenuUpdateBadge() {
        return false;
    }

    /**
     * Initialize the {@link BottomSheetController}. The view for this component is not created
     * until content is requested in the sheet.
     */
    private void initializeBottomSheetController() {
        // TODO(1093999): Componentize SnackbarManager so BottomSheetController can own this.
        Callback<View> sheetInitializedCallback = (view) -> {
            mBottomSheetSnackbarManager = new SnackbarManager(mActivity,
                    view.findViewById(org.chromium.components.browser_ui.bottomsheet.R.id
                                              .bottom_sheet_snackbar_container),
                    mActivity.getWindowAndroid());
        };

        Supplier<OverlayPanelManager> panelManagerSupplier = ()
                -> mActivity.getCompositorViewHolder().getLayoutManager().getOverlayPanelManager();

        // TODO(1094000): Initialize after inflation so we don't need to pass in view suppliers.
        mBottomSheetController = BottomSheetControllerFactory.createBottomSheetController(
                () -> mScrimCoordinator, sheetInitializedCallback, mActivity.getWindow(),
                mActivity.getWindowAndroid().getKeyboardDelegate(),
                () -> mActivity.findViewById(R.id.sheet_container));
        BottomSheetControllerFactory.setExceptionReporter(
                (throwable)
                        -> PureJavaExceptionReporter.reportJavaException((Throwable) throwable));
        BottomSheetControllerFactory.attach(mActivity.getWindowAndroid(), mBottomSheetController);

        mBottomSheetManager = new BottomSheetManager(mBottomSheetController, mActivityTabProvider,
                mActivity.getBrowserControlsManager(), mActivity::getModalDialogManager,
                this::getBottomSheetSnackbarManager, mTabObscuringHandler,
                mOmniboxFocusStateSupplier, panelManagerSupplier, mStartSurfaceSupplier);
    }

    /**
     * TODO(jinsukkim): remove/hide this in favor of wiring it directly.
     * @return {@link TabObscuringHandler} object.
     */
    public TabObscuringHandler getTabObscuringHandler() {
        return mTabObscuringHandler;
    }

    /** @return The {@link BottomSheetController} for this activity. */
    public ManagedBottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    /**
     * Gets the browser controls manager, creates it unless already created.
     */
    @NonNull
    public BrowserControlsManager getBrowserControlsManager() {
        if (mBrowserControlsManager == null) {
            // When finish()ing, getBrowserControlsManager() is required to perform cleanup logic.
            // It should never be called when it results in creating a new manager though.
            if (mActivity.isActivityFinishingOrDestroyed()) {
                throw new IllegalStateException();
            }
            mBrowserControlsManager = createBrowserControlsManager();
            assert mBrowserControlsManager != null;
        }
        return mBrowserControlsManager;
    }

    /**
     * Create a browser controls manager to be used by ChromeActivity.
     * Note: This may be called before native code is initialized.
     * @return A {@link BrowserControlsManager} instance that's been created.
     */
    @NonNull
    protected BrowserControlsManager createBrowserControlsManager() {
        return new BrowserControlsManager(mActivity, BrowserControlsManager.ControlsPosition.TOP);
    }

    /** @return The {@link ScrimCoordinator} to control activity's primary scrim. */
    // TODO(crbug.com/1064140): This method is used to pass ScrimCoordinator to StartSurface. We
    // should be able to create StartSurface in this class so that we don't need to expose this
    // getter.
    public ScrimCoordinator getScrimCoordinator() {
        return mScrimCoordinator;
    }

    /** @return The {@link SnackbarManager} for the {@link BottomSheetController}. */
    public SnackbarManager getBottomSheetSnackbarManager() {
        return mBottomSheetSnackbarManager;
    }

    private void initDirectActionInitializer() {
        @ActivityType
        int activityType = mActivity.getActivityType();
        mDirectActionInitializer = new DirectActionInitializer(mActivity, activityType, mActivity,
                mActivity::onBackPressed, mTabModelSelectorSupplier.get(), mFindToolbarManager,
                getBottomSheetController(), mActivity.getBrowserControlsManager(),
                mActivity.getCompositorViewHolder(), mActivity.getActivityTabProvider());
        mActivity.getLifecycleDispatcher().register(mDirectActionInitializer);
    }

    /**
     * Initializes a glue logic that suppresses Contextual Search while a Bottom Sheet feature is
     * in action.
     */
    private void initContextualSearchSuppressor() {
        if (mBottomSheetController == null) return;
        mContextualSearchSuppressor = new EmptyBottomSheetObserver() {
            private boolean mOpened;

            @Override
            public void onSheetStateChanged(int newState) {
                switch (newState) {
                    case SheetState.PEEK:
                    case SheetState.HALF:
                    case SheetState.FULL:
                        if (!mOpened) {
                            mOpened = true;
                            ContextualSearchManager manager =
                                    mContextualSearchManagerSupplier.get();
                            if (manager != null) manager.onBottomSheetVisible(true);
                        }
                        break;
                    case SheetState.HIDDEN:
                        mOpened = false;
                        ContextualSearchManager manager = mContextualSearchManagerSupplier.get();
                        if (manager != null) manager.onBottomSheetVisible(false);
                        break;
                }
            }
        };
        mBottomSheetController.addObserver(mContextualSearchSuppressor);
    }

    // Testing methods

    @VisibleForTesting
    public AppMenuCoordinator getAppMenuCoordinatorForTesting() {
        return mAppMenuCoordinator;
    }

    @VisibleForTesting
    public ScrimCoordinator getScrimCoordinatorForTesting() {
        return mScrimCoordinator;
    }
}
