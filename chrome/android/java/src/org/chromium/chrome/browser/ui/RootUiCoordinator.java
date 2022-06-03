// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.app.Fragment;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActionModeHandler;
import org.chromium.chrome.browser.ChromePowerModeVoter;
import org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.directactions.DirectActionInitializer;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.messages.ChromeMessageAutodismissDurationProvider;
import org.chromium.chrome.browser.messages.ChromeMessageQueueMediator;
import org.chromium.chrome.browser.messages.MessageContainerCoordinator;
import org.chromium.chrome.browser.messages.MessagesResourceMapperInitializer;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialog;
import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
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
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;

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
        implements DestroyObserver, InflationObserver, NativeInitObserver,
                   MenuOrKeyboardActionController.MenuOrKeyboardActionHandler {
    private final UnownedUserDataSupplier<TabObscuringHandler> mTabObscuringHandlerSupplier =
            new TabObscuringHandlerSupplier();
    private final JankTracker mJankTracker;

    protected AppCompatActivity mActivity;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    protected final ActivityWindowAndroid mWindowAndroid;

    protected final ActivityTabProvider mActivityTabProvider;
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    private Callback<LayoutManagerImpl> mLayoutManagerSupplierCallback;
    private OverlayPanelManager mOverlayPanelManager;
    private OverlayPanelManager.OverlayPanelManagerObserver mOverlayPanelManagerObserver;

    protected LayoutStateProvider mLayoutStateProvider;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /**
     * A controller which is used to show an Incognito re-auth dialog when the feature is
     * available.
     */
    private @Nullable IncognitoReauthController mIncognitoReauthController;

    /** A means of providing the theme color to different features. */
    private TopUiThemeColorProvider mTopUiThemeColorProvider;

    @Nullable
    private Callback<Boolean> mOnOmniboxFocusChangedListener;
    protected Supplier<Boolean> mCanAnimateBrowserControls;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    private BottomSheetManager mBottomSheetManager;
    private ManagedBottomSheetController mBottomSheetController;
    private SnackbarManager mBottomSheetSnackbarManager;

    private ScrimCoordinator mScrimCoordinator;
    private DirectActionInitializer mDirectActionInitializer;
    private ChromeActionModeHandler mChromeActionModeHandler;
    private ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<MerchantTrustSignalsCoordinator>
            mMerchantTrustSignalsCoordinatorSupplier = new ObservableSupplierImpl<>();
    protected final ObservableSupplier<Profile> mProfileSupplier;
    private BottomSheetObserver mContextualSearchSuppressor;
    private final Supplier<ContextualSearchManager> mContextualSearchManagerSupplier;
    protected final CallbackController mCallbackController;
    protected final BrowserControlsManager mBrowserControlsManager;
    protected ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    @Nullable
    protected ManagedMessageDispatcher mMessageDispatcher;
    @Nullable
    private MessageContainerCoordinator mMessageContainerCoordinator;
    @Nullable
    private ChromeMessageQueueMediator mMessageQueueMediator;
    // This supplier only ever updated when feature TOOLBAR_IPH_ANDROID is enabled.
    protected OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier = new OneshotSupplierImpl<>();
    protected Supplier<Tab> mStartSurfaceParentTabSupplier;
    private MediaCaptureOverlayController mCaptureController;
    private @Nullable ScrollCaptureManager mScrollCaptureManager;
    protected final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    protected final ObservableSupplier<LayoutManagerImpl> mLayoutManagerImplSupplier;
    protected final ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier;
    protected final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final BooleanSupplier mSupportsAppMenuSupplier;
    protected final BooleanSupplier mSupportsFindInPageSupplier;
    protected final Supplier<TabCreatorManager> mTabCreatorManagerSupplier;
    protected final FullscreenManager mFullscreenManager;
    protected final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    protected final StatusBarColorController mStatusBarColorController;
    protected final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    protected final @ActivityType int mActivityType;
    protected final Supplier<Boolean> mIsInOverviewModeSupplier;
    private final Supplier<Boolean> mIsWarmOnResumeSupplier;
    private final StatusBarColorProvider mStatusBarColorProvider;
    private final Supplier<TabContentManager> mTabContentManagerSupplier;
    private final IntentRequestTracker mIntentRequestTracker;
    private final OneshotSupplier<TabReparentingController> mTabReparentingControllerSupplier;
    private final boolean mInitializeUiWithIncognitoColors;

    /**
     * Create a new {@link RootUiCoordinator} for the given activity.
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param onOmniboxFocusChangedListener callback to invoke when Omnibox focus
     *         changes.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param contextualSearchManagerSupplier Supplier of the {@link ContextualSearchManager}.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param startSurfaceParentTabSupplier Supplies the parent tab for the StartSurface.
     * @param browserControlsManager Manages the browser controls.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param jankTracker Tracks the jank in the app.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param layoutManagerImplSupplier Supplies the {@link LayoutManager}.
     * @param menuOrKeyboardActionController Controls the menu or keyboard action controller.
     * @param activityThemeColorSupplier Supplies the activity color theme.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param supportsAppMenuSupplier Supplies the support state for the app menu.
     * @param supportsFindInPage Supplies the support state for find in page.
     * @param tabCreatorManagerSupplier Supplies the {@link TabCreatorManager}.
     * @param fullscreenManager Manages the fullscreen state.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param tabContentManagerSupplier Supplies the {@link TabContentManager}.
     * @param snackbarManagerSupplier Supplies the {@link SnackbarManager}.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param isWarmOnResumeSupplier Supplies whether the app was warm on resume.
     * @param statusBarColorProvider Provides the status bar color.
     * @param intentRequestTracker Tracks intent requests.
     * @param tabReparentingControllerSupplier Supplier of the {@link TabReparentingController}.
     * @param initializeUiWithIncognitoColors Whether to initialize the UI with incognito colors.
     */
    public RootUiCoordinator(@NonNull AppCompatActivity activity,
            @Nullable Callback<Boolean> onOmniboxFocusChangedListener,
            @NonNull ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @NonNull ActivityTabProvider tabProvider,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull Supplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull Supplier<Tab> startSurfaceParentTabSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ActivityWindowAndroid windowAndroid, @NonNull JankTracker jankTracker,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull ObservableSupplier<LayoutManagerImpl> layoutManagerSupplier,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<Integer> activityThemeColorSupplier,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull BooleanSupplier supportsAppMenuSupplier,
            @NonNull BooleanSupplier supportsFindInPage,
            @NonNull Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            @NonNull FullscreenManager fullscreenManager,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<TabContentManager> tabContentManagerSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @ActivityType int activityType, @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull OneshotSupplier<TabReparentingController> tabReparentingControllerSupplier,
            boolean initializeUiWithIncognitoColors) {
        mJankTracker = jankTracker;
        mCallbackController = new CallbackController();
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        setupUnownedUserDataSuppliers();
        mOnOmniboxFocusChangedListener = onOmniboxFocusChangedListener;
        mBrowserControlsManager = browserControlsManager;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mSupportsAppMenuSupplier = supportsAppMenuSupplier;
        mSupportsFindInPageSupplier = supportsFindInPage;
        mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
        mFullscreenManager = fullscreenManager;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mActivityType = activityType;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mIsWarmOnResumeSupplier = isWarmOnResumeSupplier;
        mStatusBarColorProvider = statusBarColorProvider;
        mIntentRequestTracker = intentRequestTracker;
        mTabReparentingControllerSupplier = tabReparentingControllerSupplier;
        mInitializeUiWithIncognitoColors = initializeUiWithIncognitoColors;

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(this);
        mActivityTabProvider = tabProvider;

        // This little bit of arithmetic is necessary because of Java doesn't like accepting
        // Supplier<BaseImpl> where Supplier<Base> is expected. We should remove the need for
        // LayoutManagerImpl in this class so we can simply use Supplier<LayoutManager>.
        mLayoutManagerSupplier = new ObservableSupplierImpl<>();
        mLayoutManagerSupplierCallback = (layoutManager) -> {
            onLayoutManagerAvailable(layoutManager);
            mLayoutManagerSupplier.set(layoutManager);
        };
        mLayoutManagerImplSupplier = layoutManagerSupplier;
        mLayoutManagerImplSupplier.addObserver(mLayoutManagerSupplierCallback);

        mShareDelegateSupplier = shareDelegateSupplier;
        mTabObscuringHandlerSupplier.set(new TabObscuringHandler());
        mProfileSupplier = profileSupplier;
        mContextualSearchManagerSupplier = contextualSearchManagerSupplier;

        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        mOmniboxFocusStateSupplier.set(false);

        mStartSurfaceParentTabSupplier = startSurfaceParentTabSupplier;

        mTopUiThemeColorProvider = new TopUiThemeColorProvider(mActivity, mActivityTabProvider,
                activityThemeColorSupplier,
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity),
                shouldAllowThemingInNightMode(), shouldAllowBrightThemeColors());

        mStatusBarColorController = new StatusBarColorController(mActivity.getWindow(),
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(/* Context */ mActivity),
                mActivity, mStatusBarColorProvider, mLayoutManagerSupplier,
                mActivityLifecycleDispatcher, mActivityTabProvider, mTopUiThemeColorProvider);
    }

    public StatusBarColorController getStatusBarColorController() {
        return mStatusBarColorController;
    }

    // TODO(jinsukkim): remove this in favor of wiring it directly.
    /**
     * @return {@link ThemeColorProvider} for top UI.
     */
    public TopUiThemeColorProvider getTopUiThemeColorProvider() {
        return mTopUiThemeColorProvider;
    }

    public void onAttachFragment(Fragment fragment) {
        if (fragment instanceof QrCodeDialog) {
            QrCodeDialog qrCodeDialog = (QrCodeDialog) fragment;
            qrCodeDialog.setWindowAndroid(mWindowAndroid);
        }
    }

    @Override
    public void onDestroy() {
        // TODO(meiliang): Understand why we need to set most of the class member instances to null
        //  other than the mActivity. If the nulling calls are not necessary, we can remove them.
        mCallbackController.destroy();
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(this);

        destroyUnownedUserDataSuppliers();
        mActivityLifecycleDispatcher.unregister(this);

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

        if (mTopUiThemeColorProvider != null) {
            mTopUiThemeColorProvider.destroy();
            mTopUiThemeColorProvider = null;
        }

        if (mModalDialogManagerObserver != null && mModalDialogManagerSupplier.hasValue()) {
            mModalDialogManagerSupplier.get().removeObserver(mModalDialogManagerObserver);
        }

        if (mBottomSheetManager != null) mBottomSheetManager.onDestroy();
        if (mBottomSheetController != null) {
            if (mContextualSearchSuppressor != null) {
                mBottomSheetController.removeObserver(mContextualSearchSuppressor);
            }
            BottomSheetControllerFactory.detach(mBottomSheetController);
            mBottomSheetController.destroy();
        }

        if (mScrimCoordinator != null) mScrimCoordinator.destroy();
        mScrimCoordinator = null;

        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }

        if (mCaptureController != null) {
            mCaptureController.destroy();
            mCaptureController = null;
        }

        if (mMerchantTrustSignalsCoordinatorSupplier.hasValue()) {
            mMerchantTrustSignalsCoordinatorSupplier.get().destroy();
            mMerchantTrustSignalsCoordinatorSupplier.set(null);
        }

        if (mScrollCaptureManager != null) {
            mScrollCaptureManager.destroy();
            mScrollCaptureManager = null;
        }

        if (mIncognitoReauthController != null) {
            mIncognitoReauthController.destroy();
        }

        mActivity = null;
    }

    private void setupUnownedUserDataSuppliers() {
        mTabObscuringHandlerSupplier.attach(mWindowAndroid.getUnownedUserDataHost());
    }

    private void destroyUnownedUserDataSuppliers() {
        // TabObscuringHandler doesn't have a destroy method.
        mTabObscuringHandlerSupplier.destroy();
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
    }

    @Override
    public void onPostInflationStartup() {
        initAppMenu();
        initDirectActionInitializer();
        initContextualSearchSuppressor();
        mChromeActionModeHandler = new ChromeActionModeHandler(mActivityTabProvider,
                aBoolean -> {}, (searchText) -> {
            if (mTabModelSelectorSupplier.get() == null) return;

            String query = ActionModeCallbackHelper.sanitizeQuery(
                    searchText, ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
            if (TextUtils.isEmpty(query)) return;

            Tab tab = mActivityTabProvider.get();
            TrackerFactory
                    .getTrackerForProfile(Profile.fromWebContents(tab.getWebContents()))
                    .notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);

            mTabModelSelectorSupplier.get().openNewTab(
                    generateUrlParamsForSearch(tab, query),
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND, tab, tab.isIncognito());
        }, mShareDelegateSupplier);

        mCaptureController = new MediaCaptureOverlayController(
                mWindowAndroid, mActivity.findViewById(R.id.capture_overlay));

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
            MessagesResourceMapperInitializer.init();
            MessageContainer container = mActivity.findViewById(R.id.message_container);
            mMessageContainerCoordinator =
                    new MessageContainerCoordinator(container, mBrowserControlsManager);
            mMessageDispatcher = MessagesFactory.createMessageDispatcher(container,
                    mMessageContainerCoordinator::getMessageMaxTranslation,
                    new ChromeMessageAutodismissDurationProvider(),
                    mWindowAndroid::startAnimationOverContent, mWindowAndroid);
            mMessageQueueMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                    mMessageContainerCoordinator, mActivityTabProvider,
                    mModalDialogManagerSupplier,
                    mActivityLifecycleDispatcher, mMessageDispatcher);
            mMessageDispatcher.setDelegate(mMessageQueueMediator);
            MessagesFactory.attachMessageDispatcher(mWindowAndroid, mMessageDispatcher);
        }

        initMerchantTrustSignals();
        initScrollCapture();

        if (IncognitoReauthManager.isIncognitoReauthFeatureAvailable()) {
            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            IncognitoReauthCoordinatorFactory incognitoReauthCoordinatorFactory =
                    new IncognitoReauthCoordinatorFactory(mActivity, tabModelSelector,
                            mModalDialogManagerSupplier.get());
            mIncognitoReauthController = new IncognitoReauthController(tabModelSelector,
                    mActivityLifecycleDispatcher,
                    mProfileSupplier, incognitoReauthCoordinatorFactory);
        }
    }

    private void initMerchantTrustSignals() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER)
                && shouldInitializeMerchantTrustSignals()) {
            MerchantTrustSignalsCoordinator merchantTrustSignalsCoordinator =
                    new MerchantTrustSignalsCoordinator(mActivity, mWindowAndroid,
                            getBottomSheetController(), mActivity.getWindow().getDecorView(),
                            MessageDispatcherProvider.from(mWindowAndroid), mActivityTabProvider,
                            mProfileSupplier, new MerchantTrustMetrics(), mIntentRequestTracker);
            mMerchantTrustSignalsCoordinatorSupplier.set(merchantTrustSignalsCoordinator);
        }
    }

    private void initScrollCapture() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S
                || DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) {
            return;
        }

        mScrollCaptureManager = new ScrollCaptureManager(mActivityTabProvider);
    }

    /**
     * @return Whether the {@link MerchantTrustSignalsCoordinator} should be initialized in the
     * context of this coordinator's UI.
     **/
    protected boolean shouldInitializeMerchantTrustSignals() {
        return false;
    }

    /**
     * Returns the supplier of {@link MerchantTrustSignalsCoordinator}.
     */
    @NonNull
    public Supplier<MerchantTrustSignalsCoordinator> getMerchantTrustSignalsCoordinatorSupplier() {
        return mMerchantTrustSignalsCoordinatorSupplier;
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
            new UkmRecorder.Bridge().recordEventWithBooleanMetric(
                    tab.getWebContents(), "MobileMenu.DirectShare", "HasOccurred");
        } else {
            RecordUserAction.record("MobileMenuShare");
            new UkmRecorder.Bridge().recordEventWithBooleanMetric(
                    tab.getWebContents(), "MobileMenu.Share", "HasOccurred");
        }
        shareDelegate.share(tab, shareDirectly, ShareOrigin.OVERFLOW_MENU);
    }

    // MenuOrKeyboardActionHandler implementation

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        return false;
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
        if (mOverlayPanelManager != null) {
            mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
        }
        mOverlayPanelManager = layoutManager.getOverlayPanelManager();

        if (mOverlayPanelManagerObserver == null) {
            mOverlayPanelManagerObserver = new OverlayPanelManager.OverlayPanelManagerObserver() {
                @Override
                public void onOverlayPanelShown() {
                }

                @Override
                public void onOverlayPanelHidden() {}
            };
        }

        mOverlayPanelManager.addObserver(mOverlayPanelManagerObserver);
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
                        mStatusBarColorController.setStatusBarScrimFraction(scrimFraction);
                    }

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                };
        return new ScrimCoordinator(mActivity, delegate, coordinator, Color.BLACK);
    }

    protected void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
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

    }

    private void hideAppMenu() {
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
     * Whether the top toolbar theme color provider should allow using the web pages theme if the
     * device is in night mode.
     */
    protected boolean shouldAllowThemingInNightMode() {
        return false;
    }

    /**
     * Whether the top toolbar theme color provider should allow bright theme colors.
     */
    protected boolean shouldAllowBrightThemeColors() {
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
                    mWindowAndroid);
        };

        Supplier<OverlayPanelManager> panelManagerSupplier = ()
                -> mCompositorViewHolderSupplier.get().getLayoutManager().getOverlayPanelManager();

        // TODO(1094000): Initialize after inflation so we don't need to pass in view suppliers.
        mBottomSheetController = BottomSheetControllerFactory.createBottomSheetController(
                ()
                        -> mScrimCoordinator,
                sheetInitializedCallback, mActivity.getWindow(),
                mWindowAndroid.getKeyboardDelegate(),
                () -> mActivity.findViewById(R.id.sheet_container));
        BottomSheetControllerFactory.setExceptionReporter(
                (throwable)
                        -> ChromePureJavaExceptionReporter.reportJavaException(
                                (Throwable) throwable));
        BottomSheetControllerFactory.attach(mWindowAndroid, mBottomSheetController);

        mBottomSheetManager = new BottomSheetManager(mBottomSheetController, mActivityTabProvider,
                mBrowserControlsManager, mModalDialogManagerSupplier,
                this::getBottomSheetSnackbarManager, mTabObscuringHandlerSupplier.get(),
                mOmniboxFocusStateSupplier, panelManagerSupplier);
    }

    /**
     * TODO(jinsukkim): remove/hide this in favor of wiring it directly.
     * @return {@link TabObscuringHandler} object.
     */
    public TabObscuringHandler getTabObscuringHandler() {
        return mTabObscuringHandlerSupplier.get();
    }

    /** @return The {@link BottomSheetController} for this activity. */
    public ManagedBottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    /**
     * Gets the browser controls manager, creates it unless already created.
     * @deprecated Instead, inject this directly to your constructor. If that's not possible, then
     *         use {@link BrowserControlsManagerSupplier}.
     */
    @NonNull
    @Deprecated
    public BrowserControlsManager getBrowserControlsManager() {
        return mBrowserControlsManager;
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
        mDirectActionInitializer = new DirectActionInitializer(mActivity, mActivityType,
                mMenuOrKeyboardActionController, mActivity::onBackPressed,
                mTabModelSelectorSupplier.get(), getBottomSheetController(),
                mBrowserControlsManager, mCompositorViewHolderSupplier.get(), mActivityTabProvider);
        mActivityLifecycleDispatcher.register(mDirectActionInitializer);
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
            public void onSheetStateChanged(int newState, int reason) {
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

}
