// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.PointF;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.vr.keyboard.VrInputMethodManagerWrapper;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.VirtualDisplayAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.widget.UiWidgetFactory;

import java.util.ArrayList;
import java.util.function.Function;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr")
public class VrShell extends GvrLayout
        implements SurfaceHolder.Callback, VrInputMethodManagerWrapper.BrowserKeyboardInterface,
                   EmptySniffingVrViewContainer.EmptyListener, VrDialogManager, VrToastManager {
    private static final String TAG = "VrShellImpl";
    private static final float INCHES_TO_METERS = 0.0254f;

    private final Activity mActivity;
    private final VrCompositorSurfaceManager mVrCompositorSurfaceManager;
    private final VrShellDelegate mDelegate;
    private final VirtualDisplayAndroid mContentVirtualDisplay;
    private final TabObserver mTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final View.OnTouchListener mTouchListener;
    private final boolean mVrBrowsingEnabled;
    private final TabModelSelector mTabModelSelector;
    private final ToolbarManager mToolbarManager;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<Tab> mCurrentTabSupplier;
    private final BrowserControlsManager mBrowserControlsManager;
    private final TabCreatorManager mTabCreatorManager;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<Boolean> mIsActivityFinishingOrDestroyedSupplier;
    private final FullscreenManager mFullscreenManager;
    private final Function<Tab, Boolean> mBackShouldCloseTabFunc;
    private final Supplier<Boolean> mIsInOverviewModeSupplier;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final CompositorView mCompositorView;

    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private long mNativeVrShell;

    private View mPresentationView;

    // The tab that holds the main WebContents.
    private Tab mTab;
    private ViewEventSink mViewEventSink;
    private Boolean mCanGoBack;
    private Boolean mCanGoForward;

    private VrWindowAndroid mContentVrWindowAndroid;

    private boolean mReprojectedRendering;

    private UiWidgetFactory mNonVrUiWidgetFactory;

    private float mLastContentWidth;
    private float mLastContentHeight;
    private float mLastContentDpr;
    private Boolean mPaused;

    private boolean mPendingVSyncPause;

    private AndroidUiGestureTarget mAndroidUiGestureTarget;
    private AndroidUiGestureTarget mAndroidDialogGestureTarget;

    private OnDispatchTouchEventCallback mOnDispatchTouchEventForTesting;
    private Runnable mOnVSyncPausedForTesting;

    private Surface mContentSurface;
    private EmptySniffingVrViewContainer mNonVrViews;
    private VrViewContainer mVrUiViewContainer;
    private FrameLayout mUiView;
    private ModalDialogManager mNonVrModalDialogManager;
    private ModalDialogManager mVrModalDialogManager;
    private VrModalPresenter mVrModalPresenter;
    private Runnable mVrDialogDismissHandler;

    private VrInputMethodManagerWrapper mInputMethodManagerWrapper;

    private ArrayList<Integer> mUiOperationResults;
    private ArrayList<Runnable> mUiOperationResultCallbacks;

    /**
     * A struct-like object for registering UI operations during tests.
     */
    @VisibleForTesting
    public static class UiOperationData {
        // The UiTestOperationType of this operation.
        public int actionType;
        // The callback to run when the operation completes.
        public Runnable resultCallback;
        // The timeout of the operation.
        public int timeoutMs;
        // The UserFriendlyElementName to perform the operation on.
        public int elementName;
        // The desired visibility status of the element.
        public boolean visibility;
    }

    public VrShell(Activity activity, VrShellDelegate delegate, TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager, Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<Tab> currentTabSupplier, BrowserControlsManager browserControlsManager,
            TabCreatorManager tabCreatorManager, WindowAndroid windowAndroid,
            Supplier<Boolean> isActivityFinishingOrDestroyedSupplier,
            FullscreenManager fullscreenManager, Function<Tab, Boolean> backShouldCloseTabFunc,
            Supplier<Boolean> isInOverviewModeSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController) {
        super(activity);
        mActivity = activity;
        mDelegate = delegate;
        mTabModelSelector = tabModelSelector;
        mVrBrowsingEnabled = mDelegate.isVrBrowsingEnabled();
        mToolbarManager = toolbarManager;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mCurrentTabSupplier = currentTabSupplier;
        mBrowserControlsManager = browserControlsManager;
        mTabCreatorManager = tabCreatorManager;
        mWindowAndroid = windowAndroid;
        mIsActivityFinishingOrDestroyedSupplier = isActivityFinishingOrDestroyedSupplier;
        mFullscreenManager = fullscreenManager;
        mBackShouldCloseTabFunc = backShouldCloseTabFunc;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;

        mReprojectedRendering = setAsyncReprojectionEnabled(true);
        if (mReprojectedRendering) {
            // No need render to a Surface if we're reprojected. We'll be rendering with surfaceless
            // EGL.
            mPresentationView = new FrameLayout(mActivity);

            // This can show up behind popups on standalone devices, so make sure it's black.
            mPresentationView.setBackgroundColor(Color.BLACK);

            // Only enable sustained performance mode when Async reprojection decouples the app
            // framerate from the display framerate.
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        } else {
            if (VrShellDelegate.isDaydreamCurrentViewer()) {
                // We need Async Reprojection on when entering VR browsing, because otherwise our
                // GL context will be lost every time we're hidden, like when we go to the dashboard
                // and come back.
                // TODO(mthiesse): Supporting context loss turned out to be hard. We should consider
                // spending more effort on supporting this in the future if it turns out to be
                // important.
                Log.e(TAG, "Could not turn async reprojection on for Daydream headset.");
                throw new VrShellDelegate.VrUnsupportedException();
            }
            SurfaceView surfaceView = new SurfaceView(mActivity);
            surfaceView.getHolder().addCallback(this);
            mPresentationView = surfaceView;
        }

        DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(activity);
        mContentVirtualDisplay = VirtualDisplayAndroid.createVirtualDisplay();
        mContentVirtualDisplay.setTo(primaryDisplay);

        mContentVrWindowAndroid =
                new VrWindowAndroid(mActivity, mContentVirtualDisplay, mModalDialogManagerSupplier);
        reparentAllTabs(mContentVrWindowAndroid);

        mCompositorView = mCompositorViewHolderSupplier.get().getCompositorView();
        mVrCompositorSurfaceManager = new VrCompositorSurfaceManager(mCompositorView);
        mCompositorView.replaceSurfaceManagerForVr(
                mVrCompositorSurfaceManager, mContentVrWindowAndroid);

        if (mVrBrowsingEnabled) {
            injectVrRootView();
        }

        // This overrides the default intent created by GVR to return to Chrome when the DON flow
        // is triggered by resuming the GvrLayout, which is the usual way Daydream apps enter VR.
        // See VrShellDelegate#getEnterVrPendingIntent for why we need to do this.
        setReentryIntent(VrShellDelegate.getEnterVrPendingIntent(activity));

        setPresentationView(mPresentationView);

        getUiLayout().setCloseButtonListener(mDelegate.getVrCloseButtonListener());
        getUiLayout().setSettingsButtonListener(mDelegate.getVrSettingsButtonListener());

        if (mVrBrowsingEnabled) injectVrHostedUiView();

        // This has to happen after VrModalDialogManager is created.
        mNonVrUiWidgetFactory = UiWidgetFactory.getInstance();
        UiWidgetFactory.setInstance(new VrUiWidgetFactory(this, mModalDialogManagerSupplier.get()));

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                // Restore proper focus on the old content.
                if (mViewEventSink != null) mViewEventSink.onWindowFocusChanged(true);
                mViewEventSink = null;
                if (mNativeVrShell == 0) return;
                if (mLastContentWidth != 0) {
                    setContentCssSize(mLastContentWidth, mLastContentHeight, mLastContentDpr);
                }
                if (tab != null && tab.getContentView() != null && tab.getWebContents() != null) {
                    tab.getContentView().requestFocus();
                    // We need the content layer to think it has Window Focus so it doesn't blur
                    // the page, even though we're drawing VR layouts over top of it.
                    mViewEventSink = ViewEventSink.from(tab.getWebContents());
                    if (mViewEventSink != null) mViewEventSink.onWindowFocusChanged(true);
                }
                VrShellJni.get().swapContents(mNativeVrShell, VrShell.this, tab);
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                onContentChanged(tab);
                // It is not needed to restore IME for old web contents as it is going away and
                // replaced by the new web contents.
                configWebContentsImeForVr(tab.getWebContents());
            }

            @Override
            public void onLoadProgressChanged(Tab tab, float progress) {
                if (mNativeVrShell == 0) return;
                VrShellJni.get().onLoadProgressChanged(mNativeVrShell, VrShell.this, progress);
            }

            @Override
            public void onCrash(Tab tab) {
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                updateHistoryButtonsVisibility();
            }
        };

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onChange() {
                swapToForegroundTab();
            }

            @Override
            public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                if (mNativeVrShell == 0) return;
                VrShellJni.get().onTabUpdated(mNativeVrShell, VrShell.this, tab.isIncognito(),
                        tab.getId(), tab.getTitle());
            }
        };

        mTouchListener = new View.OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    VrShellJni.get().onTriggerEvent(mNativeVrShell, VrShell.this, true);
                    return true;
                } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                        || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    VrShellJni.get().onTriggerEvent(mNativeVrShell, VrShell.this, false);
                    return true;
                }
                return false;
            }
        };
    }

    private void injectVrRootView() {
        // Inject a view into the hierarchy above R.id.content so that the rest of Chrome can
        // remain unaware/uncaring of its existence. This view is used to draw the view hierarchy
        // into a texture when browsing in VR. See https://crbug.com/793430.
        View content = mActivity.getWindow().findViewById(android.R.id.content);
        ViewGroup parent = (ViewGroup) content.getParent();
        mNonVrViews = new EmptySniffingVrViewContainer(mActivity, this);
        parent.removeView(content);
        parent.addView(mNonVrViews);
        // Some views in Clank are just added next to the content view, like the 2D tab switcher.
        // We need to create a parent to contain the content view and all of its siblings so that
        // the VrViewContainer can inject input into the parent and not care about how to do its own
        // input targeting.
        FrameLayout childHolder = new FrameLayout(mActivity);
        mNonVrViews.addView(childHolder);
        childHolder.addView(content);
    }

    private void injectVrHostedUiView() {
        mNonVrModalDialogManager = mModalDialogManagerSupplier.get();
        mNonVrModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
        mVrModalPresenter = new VrModalPresenter(mActivity, this);
        mVrModalDialogManager =
                new ModalDialogManager(mVrModalPresenter, ModalDialogManager.ModalDialogType.APP);
        setModalDialogManager(mVrModalDialogManager);

        ViewGroup decor = (ViewGroup) mActivity.getWindow().getDecorView();
        mUiView = new FrameLayout(decor.getContext());
        LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        decor.addView(mUiView, params);
        mVrUiViewContainer = new VrViewContainer(mActivity);
        mUiView.addView(mVrUiViewContainer);
    }

    private void setModalDialogManager(ModalDialogManager modalDialogManager) {
        ((ObservableSupplierImpl) mModalDialogManagerSupplier).set(mVrModalDialogManager);
    }

    private void removeVrRootView() {
        ViewGroup contentViewParent = (ViewGroup) mNonVrViews.getParent();
        assert mNonVrViews.getChildCount() == 1;
        ViewGroup childHolder = (ViewGroup) mNonVrViews.getChildAt(0);
        mNonVrViews.removeAllViews();
        contentViewParent.removeView(mNonVrViews);
        int children = childHolder.getChildCount();
        assert children >= 1;
        for (int i = 0; i < children; ++i) {
            View child = childHolder.getChildAt(0);
            childHolder.removeView(child);
            contentViewParent.addView(child);
        }
        // Ensure the omnibox doesn't get initial focus (as it would when re-attaching the views
        // to a window), and immediately bring up the keyboard.
        if (mCompositorViewHolderSupplier.hasValue()) {
            mCompositorViewHolderSupplier.get().requestFocus();
        }
    }

    @RequiresApi(Build.VERSION_CODES.N)
    public void initializeNative(boolean forWebVr, boolean isStandaloneVrDevice) {
        Tab tab = mCurrentTabSupplier.get();
        if (mIsInOverviewModeSupplier.get() || tab == null) {
            openNewTab(false /*incognito*/);
            tab = mCurrentTabSupplier.get();
        }

        // Start with content rendering paused if the renderer-drawn controls are visible, as this
        // would cause the in-content omnibox to be shown to users.
        boolean pauseContent = mBrowserControlsManager.getContentOffset() > 0;
        DisplayAndroid display = tab.getWindowAndroid().getDisplay();
        int widthPixels = display.getDisplayWidth();
        int heightPixels = display.getDisplayHeight();
        float densityDpi = mActivity.getResources().getConfiguration().densityDpi;

        // We're supposed to be in landscape at this point, but it's possible for us to get here
        // before the change has fully propagated. In this case, the width and height are swapped,
        // which causes an incorrect display size to be used, and the page to appear zoomed in.
        if (widthPixels < heightPixels) {
            int tempWidth = heightPixels;
            heightPixels = widthPixels;
            widthPixels = tempWidth;
            // In the case where we're still in portrait, keep the black overlay visible until the
            // GvrLayout is in the correct orientation.
        } else {
            VrModuleProvider.getDelegate().removeBlackOverlayView(mActivity, false /* animate */);
        }
        float displayWidthMeters = (widthPixels / densityDpi) * INCHES_TO_METERS;
        float displayHeightMeters = (heightPixels / densityDpi) * INCHES_TO_METERS;

        // Semi-arbitrary resolution cutoff that determines how much we scale our default buffer
        // size in VR. This is so we can make the right performance/quality tradeoff for both the
        // relatively low-res Pixel, and higher-res Pixel XL and other devices.
        boolean lowDensity = densityDpi <= DisplayMetrics.DENSITY_XXHIGH;

        boolean hasOrCanRequestRecordAudioPermission =
                hasRecordAudioPermission() || canRequestRecordAudioPermission();
        boolean supportsRecognition = VoiceRecognitionUtil.isRecognitionIntentPresent(false);
        mNativeVrShell = VrShellJni.get().init(VrShell.this, mDelegate, forWebVr,
                !mVrBrowsingEnabled, hasOrCanRequestRecordAudioPermission && supportsRecognition,
                getGvrApi().getNativeGvrContext(), mReprojectedRendering, displayWidthMeters,
                displayHeightMeters, widthPixels, heightPixels, pauseContent, lowDensity,
                isStandaloneVrDevice);

        swapToTab(tab);
        createTabList();
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        attachTabModelSelectorTabObserver();
        updateHistoryButtonsVisibility();

        mPresentationView.setOnTouchListener(mTouchListener);

        if (mVrBrowsingEnabled) {
            mAndroidUiGestureTarget = new AndroidUiGestureTarget(mNonVrViews.getInputTarget(),
                    mContentVrWindowAndroid.getDisplay().getDipScale(), getNativePageScrollRatio(),
                    getTouchSlop());
            VrShellJni.get().setAndroidGestureTarget(
                    mNativeVrShell, VrShell.this, mAndroidUiGestureTarget);
        }
    }

    private void createTabList() {
        assert mNativeVrShell != 0;
        TabModel main = mTabModelSelector.getModel(false);
        int count = main.getCount();
        Tab[] mainTabs = new Tab[count];
        for (int i = 0; i < count; ++i) {
            mainTabs[i] = main.getTabAt(i);
        }
        TabModel incognito = mTabModelSelector.getModel(true);
        count = incognito.getCount();
        Tab[] incognitoTabs = new Tab[count];
        for (int i = 0; i < count; ++i) {
            incognitoTabs[i] = incognito.getTabAt(i);
        }
        VrShellJni.get().onTabListCreated(mNativeVrShell, VrShell.this, mainTabs, incognitoTabs);
    }

    private void swapToForegroundTab() {
        Tab tab = mCurrentTabSupplier.get();
        if (tab == mTab) return;
        swapToTab(tab);
    }

    private void swapToTab(Tab tab) {
        if (mTab != null) {
            mTab.removeObserver(mTabObserver);
            restoreTabFromVR();
        }

        mTab = tab;
        if (mTab != null) {
            initializeTabForVR();
            mTab.addObserver(mTabObserver);
            TabBrowserControlsConstraintsHelper.update(mTab, BrowserControlsState.HIDDEN, false);
        }
        mTabObserver.onContentChanged(mTab);
    }

    private void configWebContentsImeForVr(WebContents webContents) {
        if (webContents == null) return;

        ImeAdapter imeAdapter = ImeAdapter.fromWebContents(webContents);
        if (imeAdapter == null) return;

        mInputMethodManagerWrapper = new VrInputMethodManagerWrapper(mActivity, this);
        imeAdapter.setInputMethodManagerWrapper(mInputMethodManagerWrapper);
    }

    private void restoreWebContentsImeFromVr(WebContents webContents) {
        if (webContents == null) return;

        ImeAdapter imeAdapter = ImeAdapter.fromWebContents(webContents);
        if (imeAdapter == null) return;

        // Use application context here to avoid leaking the activity context.
        imeAdapter.setInputMethodManagerWrapper(ImeAdapter.createDefaultInputMethodManagerWrapper(
                mActivity.getApplicationContext(), mContentVrWindowAndroid, null));
        mInputMethodManagerWrapper = null;
    }

    private void initializeTabForVR() {
        if (mTab == null) return;
        assert mTab.getWindowAndroid() == mContentVrWindowAndroid;
        configWebContentsImeForVr(mTab.getWebContents());
    }

    private void restoreTabFromVR() {
        if (mTab == null) return;
        restoreWebContentsImeFromVr(mTab.getWebContents());
    }

    private void reparentAllTabs(WindowAndroid window) {
        // Ensure new tabs are created with the correct window.
        boolean[] values = {true, false};
        for (boolean incognito : values) {
            TabCreator tabCreator = mTabCreatorManager.getTabCreator(incognito);
            if (tabCreator instanceof ChromeTabCreator) {
                ((ChromeTabCreator) tabCreator).setWindowAndroid(window);
            }
        }

        // Reparent all existing tabs.
        for (TabModel model : mTabModelSelector.getModels()) {
            for (int i = 0; i < model.getCount(); ++i) {
                model.getTabAt(i).updateAttachment(window, null);
            }
        }
    }

    // Returns true if Chrome has permission to use audio input.
    @CalledByNative
    public boolean hasRecordAudioPermission() {
        return mDelegate.hasRecordAudioPermission();
    }

    // Returns true if Chrome has not been permanently denied audio input permission.
    @CalledByNative
    public boolean canRequestRecordAudioPermission() {
        return mDelegate.canRequestRecordAudioPermission();
    }

    // Exits VR, telling the user to remove their headset, and returning to Chromium.
    @CalledByNative
    public void forceExitVr() {
        mDelegate.showDoff(false);
    }

    // Called when the user clicks on the security icon in the URL bar.
    @CalledByNative
    public void showPageInfo() {
        Tab tab = mCurrentTabSupplier.get();
        if (tab == null) return;
        new ChromePageInfo(mModalDialogManagerSupplier, null, OpenedFromSource.VR,
                /*storeInfoActionHandlerSupplier=*/null, /*ephemeralTabCoordinatorSupplier=*/null)
                .show(tab, ChromePageInfoHighlight.noHighlight());
    }

    // Called because showing audio permission dialog isn't supported in VR. This happens when
    // the user wants to do a voice search.
    @CalledByNative
    public void onUnhandledPermissionPrompt() {
        VrShellDelegate.requestToExitVr(new OnExitVrRequestListener() {
            @Override
            public void onSucceeded() {
                PermissionCallback callback = new PermissionCallback() {
                    @Override
                    public void onRequestPermissionsResult(
                            String[] permissions, int[] grantResults) {
                        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                            @Override
                            public void run() {
                                VrShellDelegate.enterVrIfNecessary();

                                // In SVR, the native VR UI is destroyed when
                                // exiting VR (mNativeVrShell == 0), so
                                // permission changes will be detected when the
                                // VR UI is reconstructed. For AIO devices this
                                // doesn't happen, so we need to notify native
                                // UI of the permission change immediately.
                                if (mNativeVrShell != 0) {
                                    VrShellJni.get().requestRecordAudioPermissionResult(
                                            mNativeVrShell, VrShell.this,
                                            grantResults[0] == PackageManager.PERMISSION_GRANTED);
                                }
                            }
                        });
                    }
                };
                String[] permissionArray = new String[1];
                permissionArray[0] = android.Manifest.permission.RECORD_AUDIO;
                mWindowAndroid.requestPermissions(permissionArray, callback);
            }

            @Override
            public void onDenied() {}
        }, UiUnsupportedMode.VOICE_SEARCH_NEEDS_RECORD_AUDIO_OS_PERMISSION);
    }

    // Called when the user has an older GVR Keyboard installed on their device and we need them to
    // have a newer one.
    @CalledByNative
    public void onNeedsKeyboardUpdate() {
        VrShellDelegate.requestToExitVr(new OnExitVrRequestListener() {
            @Override
            public void onSucceeded() {
                mDelegate.promptForKeyboardUpdate();
            }

            @Override
            public void onDenied() {}
        }, UiUnsupportedMode.NEEDS_KEYBOARD_UPDATE);
    }

    // Close the current hosted Dialog in VR
    @CalledByNative
    public void closeCurrentDialog() {
        mVrModalPresenter.closeCurrentDialog();
        if (mVrDialogDismissHandler != null) {
            mVrDialogDismissHandler.run();
            mVrDialogDismissHandler = null;
        }
    }

    /**
     * @param vrDialogDismissHandler the mVrDialogDismissHandler to set
     */
    @Override
    public void setVrDialogDismissHandler(Runnable vrDialogDismissHandler) {
        mVrDialogDismissHandler = vrDialogDismissHandler;
    }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        // This handles the case where we open 2D popups in 2D-in-VR. We lose window focus, but stay
        // resumed, so we have to listen for focus gain to know when the popup was closed. However,
        // we pause VrShellImpl so that we don't react to input from the controller nor do any
        // rendering. This also handles the case where we're launched via intent and turn VR mode on
        // with a popup open. We'll lose window focus when the popup 'gets shown' and know to turn
        // VR mode off.
        // TODO(asimjour): Focus is a bad signal. We should listen for windows being created and
        // destroyed if possible.
        if (VrModuleProvider.getDelegate().bootsToVr()) {
            if (focused) {
                resume();
            } else {
                pause();
            }
            VrShellDelegate.setVrModeEnabled(mActivity, focused);
            setVisibility(focused ? View.VISIBLE : View.INVISIBLE);
        }
    }

    @CalledByNative
    public void setContentCssSize(float width, float height, float dpr) {
        ThreadUtils.assertOnUiThread();
        boolean surfaceUninitialized = mLastContentWidth == 0;
        mLastContentWidth = width;
        mLastContentHeight = height;
        mLastContentDpr = dpr;

        // Java views don't listen to our DPR changes, so to get them to render at the correct
        // size we need to make them larger.
        DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(mActivity);
        float dip = primaryDisplay.getDipScale();

        int contentWidth = (int) Math.ceil(width * dpr);
        int contentHeight = (int) Math.ceil(height * dpr);

        int overlayWidth = (int) Math.ceil(width * dip);
        int overlayHeight = (int) Math.ceil(height * dip);

        VrShellJni.get().bufferBoundsChanged(mNativeVrShell, VrShell.this, contentWidth,
                contentHeight, overlayWidth, overlayHeight);
        if (mContentSurface != null) {
            if (surfaceUninitialized) {
                mVrCompositorSurfaceManager.setSurface(
                        mContentSurface, PixelFormat.OPAQUE, contentWidth, contentHeight);
            } else {
                mVrCompositorSurfaceManager.surfaceResized(contentWidth, contentHeight);
            }
        }
        Point size = new Point(contentWidth, contentHeight);
        mContentVirtualDisplay.update(
                size, dpr, dip / dpr, null, null, null, null, null, null, null, null);
        if (mTab != null && mTab.getWebContents() != null) {
            mTab.getWebContents().setSize(contentWidth, contentHeight);
        }
        if (mVrBrowsingEnabled) mNonVrViews.resize(overlayWidth, overlayHeight);
    }

    @CalledByNative
    public void contentSurfaceCreated(Surface surface) {
        mContentSurface = surface;
        if (mLastContentWidth == 0) return;
        int width = (int) Math.ceil(mLastContentWidth * mLastContentDpr);
        int height = (int) Math.ceil(mLastContentHeight * mLastContentDpr);
        mVrCompositorSurfaceManager.setSurface(mContentSurface, PixelFormat.OPAQUE, width, height);
    }

    @CalledByNative
    public void contentOverlaySurfaceCreated(Surface surface) {
        if (mVrBrowsingEnabled) mNonVrViews.setSurface(surface);
    }

    @CalledByNative
    public void dialogSurfaceCreated(Surface surface) {
        if (mVrBrowsingEnabled && mVrUiViewContainer != null) {
            mVrUiViewContainer.setSurface(surface);
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        boolean parentConsumed = super.dispatchTouchEvent(event);
        if (mOnDispatchTouchEventForTesting != null) {
            mOnDispatchTouchEventForTesting.onDispatchTouchEvent(parentConsumed);
        }
        return parentConsumed;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mTab != null && mTab.getWebContents() != null
                && mTab.getWebContents().getEventForwarder().dispatchKeyEvent(event)) {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (mTab != null && mTab.getWebContents() != null
                && mTab.getWebContents().getEventForwarder().onGenericMotionEvent(event)) {
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public void onResume() {
        if (mPaused != null && !mPaused) return;
        mPaused = false;
        super.onResume();
        if (mNativeVrShell != 0) {
            // Refreshing the viewer profile may accesses disk under some circumstances outside of
            // our control.
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                VrShellJni.get().onResume(mNativeVrShell, VrShell.this);
            }
        }
    }

    @Override
    public void onPause() {
        if (mPaused != null && mPaused) return;
        mPaused = true;
        super.onPause();
        if (mNativeVrShell != 0) VrShellJni.get().onPause(mNativeVrShell, VrShell.this);
    }

    public void destroyWindowAndroid() {
        reparentAllTabs(mWindowAndroid);
        mCompositorView.onExitVr(mWindowAndroid);
        mContentVrWindowAndroid.destroy();
    }

    @Override
    public void shutdown() {
        if (mVrBrowsingEnabled) {
            if (mVrModalDialogManager != null) {
                mVrModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                setModalDialogManager(mNonVrModalDialogManager);
                mVrModalDialogManager = null;
            }
            mNonVrViews.destroy();
            if (mVrUiViewContainer != null) mVrUiViewContainer.destroy();
            removeVrRootView();
        }

        if (!mIsActivityFinishingOrDestroyedSupplier.get()) {
            mFullscreenManager.exitPersistentFullscreenMode();
        }
        reparentAllTabs(mWindowAndroid);
        if (mNativeVrShell != 0) {
            VrShellJni.get().destroy(mNativeVrShell, VrShell.this);
            mNativeVrShell = 0;
        }
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mTabModelSelectorTabObserver.destroy();
        if (mTab != null) {
            mTab.removeObserver(mTabObserver);
            restoreTabFromVR();
            restoreWebContentsImeFromVr(mTab.getWebContents());
            if (mTab.getWebContents() != null && mTab.getContentView() != null) {
                View parent = mTab.getContentView();
                mTab.getWebContents().setSize(parent.getWidth(), parent.getHeight());
            }
            TabBrowserControlsConstraintsHelper.update(mTab, BrowserControlsState.SHOWN, false);
        }

        mContentVirtualDisplay.destroy();

        mCompositorView.onExitVr(mWindowAndroid);
        mContentVrWindowAndroid.destroy();

        if (mNonVrUiWidgetFactory != null) UiWidgetFactory.setInstance(mNonVrUiWidgetFactory);

        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        decor.removeView(mUiView);
        super.shutdown();
    }

    public void pause() {
        onPause();
    }

    public void resume() {
        onResume();
    }

    public void teardown() {
        shutdown();
    }

    public boolean hasUiFinishedLoading() {
        return VrShellJni.get().hasUiFinishedLoading(mNativeVrShell, VrShell.this);
    }

    /**
     * Set View for the Dialog that should show up on top of the main content.
     */
    @Override
    public void setDialogView(View view) {
        if (view == null) return;
        assert mVrUiViewContainer.getChildCount() == 0;
        mVrUiViewContainer.addView(view);
    }

    /**
     * Close the popup Dialog in VR.
     */
    @Override
    public void closeVrDialog() {
        VrShellJni.get().closeAlertDialog(mNativeVrShell, VrShell.this);
        mVrUiViewContainer.removeAllViews();
        mVrDialogDismissHandler = null;
    }

    /**
     * Set size of the Dialog in VR.
     */
    @Override
    public void setDialogSize(int width, int height) {
        VrShellJni.get().setDialogBufferSize(mNativeVrShell, VrShell.this, width, height);
        VrShellJni.get().setAlertDialogSize(mNativeVrShell, VrShell.this, width, height);
    }

    /**
     * Set size of the Dialog location in VR.
     */
    @Override
    public void setDialogLocation(int x, int y) {
        if (getWebVrModeEnabled()) return;
        float dipScale = DisplayAndroid.getNonMultiDisplay(mActivity).getDipScale();
        float w = mLastContentWidth * dipScale;
        float h = mLastContentHeight * dipScale;
        float scale = mContentVrWindowAndroid.getDisplay().getAndroidUIScaling();
        VrShellJni.get().setDialogLocation(
                mNativeVrShell, VrShell.this, x * scale / w, y * scale / h);
    }

    @Override
    public void setDialogFloating(boolean floating) {
        VrShellJni.get().setDialogFloating(mNativeVrShell, VrShell.this, floating);
    }

    /**
     * Initialize the Dialog in VR.
     */
    @Override
    public void initVrDialog(int width, int height) {
        VrShellJni.get().setAlertDialog(mNativeVrShell, VrShell.this, width, height);
        mAndroidDialogGestureTarget =
                new AndroidUiGestureTarget(mVrUiViewContainer.getInputTarget(), 1.0f,
                        getNativePageScrollRatio(), getTouchSlop());
        VrShellJni.get().setDialogGestureTarget(
                mNativeVrShell, VrShell.this, mAndroidDialogGestureTarget);
    }

    /**
     * Show a text only Toast.
     */
    @Override
    public void showToast(CharSequence text) {
        VrShellJni.get().showToast(mNativeVrShell, VrShell.this, text.toString());
    }

    /**
     * Cancel a Toast.
     */
    @Override
    public void cancelToast() {
        VrShellJni.get().cancelToast(mNativeVrShell, VrShell.this);
    }

    public void setWebVrModeEnabled(boolean enabled) {
        if (mNativeVrShell != 0) {
            VrShellJni.get().setWebVrMode(mNativeVrShell, VrShell.this, enabled);
        }
        if (!enabled) {
            mContentVrWindowAndroid.setVSyncPaused(false);
            mPendingVSyncPause = false;
            return;
        }
        // Wait for the compositor to produce a frame to allow the omnibox to start hiding
        // before we pause VSync. Control heights may not be correct as the omnibox might
        // animate, but this is handled when exiting VR.
        mPendingVSyncPause = true;
        mCompositorView.surfaceRedrawNeededAsync(() -> {
            if (mPendingVSyncPause) {
                mContentVrWindowAndroid.setVSyncPaused(true);
                mPendingVSyncPause = false;
                if (mOnVSyncPausedForTesting != null) {
                    mOnVSyncPausedForTesting.run();
                }
            }
        });
    }

    public boolean getWebVrModeEnabled() {
        if (mNativeVrShell == 0) return false;
        return VrShellJni.get().getWebVrMode(mNativeVrShell, VrShell.this);
    }

    public boolean isDisplayingUrlForTesting() {
        assert mNativeVrShell != 0;
        return PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            return VrShellJni.get().isDisplayingUrlForTesting(mNativeVrShell, VrShell.this);
        });
    }

    @VisibleForTesting
    public VrInputConnection getVrInputConnectionForTesting() {
        assert mNativeVrShell != 0;
        return VrShellJni.get().getVrInputConnectionForTesting(mNativeVrShell, VrShell.this);
    }

    public FrameLayout getContainer() {
        return this;
    }

    public void rawTopContentOffsetChanged(float topContentOffset) {
        if (topContentOffset != 0) return;
        // Wait until a new frame is definitely available.
        mCompositorView.surfaceRedrawNeededAsync(() -> {
            if (mNativeVrShell != 0) {
                VrShellJni.get().resumeContentRendering(mNativeVrShell, VrShell.this);
            }
        });
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (mNativeVrShell == 0) return;
        VrShellJni.get().setSurface(mNativeVrShell, VrShell.this, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // No need to do anything here, we don't care about surface width/height.
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mVrCompositorSurfaceManager.surfaceDestroyed();
        VrShellDelegate.forceExitVrImmediately();
    }

    /** Creates and attaches a TabModelSelectorTabObserver to the tab model selector. */
    private void attachTabModelSelectorTabObserver() {
        assert mTabModelSelectorTabObserver == null;
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onTitleUpdated(Tab tab) {
                if (mNativeVrShell == 0) return;
                VrShellJni.get().onTabUpdated(mNativeVrShell, VrShell.this, tab.isIncognito(),
                        tab.getId(), tab.getTitle());
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (mNativeVrShell == 0) return;
                if (closing) {
                    VrShellJni.get().onTabRemoved(
                            mNativeVrShell, VrShell.this, tab.isIncognito(), tab.getId());
                } else {
                    VrShellJni.get().onTabUpdated(mNativeVrShell, VrShell.this, tab.isIncognito(),
                            tab.getId(), tab.getTitle());
                }
            }

            @Override
            public void onDestroyed(Tab tab) {
                if (mNativeVrShell == 0) return;
                VrShellJni.get().onTabRemoved(
                        mNativeVrShell, VrShell.this, tab.isIncognito(), tab.getId());
            }
        };
    }

    @CalledByNative
    public boolean hasDaydreamSupport() {
        return VrCoreInstallUtils.hasDaydreamSupport();
    }

    public void requestToExitVr(@UiUnsupportedMode int reason, boolean showExitPromptBeforeDoff) {
        if (mNativeVrShell == 0) return;
        if (showExitPromptBeforeDoff) {
            VrShellJni.get().requestToExitVr(mNativeVrShell, VrShell.this, reason);
        } else {
            mDelegate.onExitVrRequestResult(true);
        }
    }

    @CalledByNative
    private void onExitVrRequestResult(boolean shouldExit) {
        mDelegate.onExitVrRequestResult(shouldExit);
    }

    @CalledByNative
    private void loadUrl(String url) {
        if (mTab == null) {
            mTabCreatorManager.getTabCreator(mTabModelSelector.isIncognitoSelected())
                    .createNewTab(new LoadUrlParams(url), TabLaunchType.FROM_CHROME_UI, null);
        } else {
            mTab.loadUrl(new LoadUrlParams(url));
        }
    }

    @VisibleForTesting
    @CalledByNative
    public void navigateForward() {
        if (!mCanGoForward) return;
        mToolbarManager.forward();
        updateHistoryButtonsVisibility();
    }

    @VisibleForTesting
    @CalledByNative
    public void navigateBack() {
        if (!mCanGoBack) return;
        if (mActivity instanceof ChromeTabbedActivity) {
            // TODO(mthiesse): We should do this for custom tabs as well, as back for custom tabs
            // is also expected to close tabs.
            ((ChromeTabbedActivity) mActivity).handleBackPressed();
        } else {
            mToolbarManager.back();
        }
        updateHistoryButtonsVisibility();
    }

    @CalledByNative
    public void reloadTab() {
        mTab.reload();
    }

    @CalledByNative
    public void openNewTab(boolean incognito) {
        mTabCreatorManager.getTabCreator(incognito).launchNTP();
    }

    @CalledByNative
    public void openBookmarks() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.all_bookmarks_menu_id, true);
    }

    @CalledByNative
    public void openRecentTabs() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.recent_tabs_menu_id, true);
    }

    @CalledByNative
    public void openHistory() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.open_history_menu_id, true);
    }

    @CalledByNative
    public void openDownloads() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.downloads_menu_id, true);
    }

    @CalledByNative
    public void openShare() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.share_menu_id, true);
    }

    @CalledByNative
    public void openSettings() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.preferences_id, true);
    }

    @CalledByNative
    public void closeAllIncognitoTabs() {
        mTabModelSelector.getModel(true).closeAllTabs();
        if (mTabModelSelector.getTotalTabCount() == 0) openNewTab(false);
    }

    @CalledByNative
    public void openFeedback() {
        mMenuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.help_id, true);
    }

    private void updateHistoryButtonsVisibility() {
        if (mNativeVrShell == 0) return;
        if (mTab == null) {
            mCanGoBack = false;
            mCanGoForward = false;
            VrShellJni.get().setHistoryButtonsEnabled(
                    mNativeVrShell, VrShell.this, mCanGoBack, mCanGoForward);
            return;
        }
        boolean willCloseTab = false;
        if (mActivity instanceof ChromeTabbedActivity) {
            // If hitting back would minimize Chrome, disable the back button.
            // See ChromeTabbedActivity#handleBackPressed().
            willCloseTab = mBackShouldCloseTabFunc.apply(mTab)
                    && !TabAssociatedApp.isOpenedFromExternalApp(mTab);
        }
        boolean canGoBack = mTab.canGoBack() || willCloseTab;
        boolean canGoForward = mTab.canGoForward();
        if ((mCanGoBack != null && canGoBack == mCanGoBack)
                && (mCanGoForward != null && canGoForward == mCanGoForward)) {
            return;
        }
        mCanGoBack = canGoBack;
        mCanGoForward = canGoForward;
        VrShellJni.get().setHistoryButtonsEnabled(
                mNativeVrShell, VrShell.this, mCanGoBack, mCanGoForward);
    }

    private float getNativePageScrollRatio() {
        return mWindowAndroid.getDisplay().getDipScale()
                / mContentVrWindowAndroid.getDisplay().getDipScale();
    }

    private int getTouchSlop() {
        ViewConfiguration vc = ViewConfiguration.get(mActivity);
        return vc.getScaledTouchSlop();
    }

    @Override
    public void onVrViewEmpty() {
        if (mNativeVrShell != 0) {
            VrShellJni.get().onOverlayTextureEmptyChanged(mNativeVrShell, VrShell.this, true);
        }
    }

    @Override
    public void onVrViewNonEmpty() {
        if (mNativeVrShell != 0) {
            VrShellJni.get().onOverlayTextureEmptyChanged(mNativeVrShell, VrShell.this, false);
        }
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        if (width > height) {
            VrModuleProvider.getDelegate().removeBlackOverlayView(mActivity, true /* animate */);
        }
    }

    /**
     * Sets the callback that will be run when VrShellImpl's dispatchTouchEvent
     * is run and the parent consumed the event.
     * @param callback The Callback to be run.
     */
    @VisibleForTesting
    public void setOnDispatchTouchEventForTesting(OnDispatchTouchEventCallback callback) {
        mOnDispatchTouchEventForTesting = callback;
    }

    /**
     * Sets that callback that will be run when VrShellImpl has issued the request to pause the
     * Android Window's VSyncs.
     * @param callback The Runnable to be run.
     */
    @VisibleForTesting
    public void setOnVSyncPausedForTesting(Runnable callback) {
        mOnVSyncPausedForTesting = callback;
    }

    @VisibleForTesting
    public Boolean isBackButtonEnabled() {
        return mCanGoBack;
    }

    @VisibleForTesting
    public Boolean isForwardButtonEnabled() {
        return mCanGoForward;
    }

    @VisibleForTesting
    public float getContentWidthForTesting() {
        return mLastContentWidth;
    }

    @VisibleForTesting
    public float getContentHeightForTesting() {
        return mLastContentHeight;
    }

    @VisibleForTesting
    public View getPresentationViewForTesting() {
        return mPresentationView;
    }

    @VisibleForTesting
    public boolean isDisplayingDialogView() {
        return mVrUiViewContainer.getChildCount() > 0;
    }

    @VisibleForTesting
    public VrViewContainer getVrViewContainerForTesting() {
        return mVrUiViewContainer;
    }

    @Override
    public void showSoftInput(boolean show) {
        assert mNativeVrShell != 0;
        VrShellJni.get().showSoftInput(mNativeVrShell, VrShell.this, show);
    }

    @Override
    public void updateIndices(
            int selectionStart, int selectionEnd, int compositionStart, int compositionEnd) {
        assert mNativeVrShell != 0;
        VrShellJni.get().updateWebInputIndices(mNativeVrShell, VrShell.this, selectionStart,
                selectionEnd, compositionStart, compositionEnd);
    }

    @VisibleForTesting
    public VrInputMethodManagerWrapper getInputMethodManagerWrapperForTesting() {
        return mInputMethodManagerWrapper;
    }

    public void acceptDoffPromptForTesting() {
        VrShellJni.get().acceptDoffPromptForTesting(mNativeVrShell, VrShell.this);
    }

    public void performControllerActionForTesting(
            int elementName, int actionType, PointF position) {
        VrShellJni.get().performControllerActionForTesting(
                mNativeVrShell, VrShell.this, elementName, actionType, position.x, position.y);
    }

    public void performKeyboardInputForTesting(int inputType, String inputString) {
        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            VrShellJni.get().performKeyboardInputForTesting(
                    mNativeVrShell, VrShell.this, inputType, inputString);
        });
    }

    public void registerUiOperationCallbackForTesting(UiOperationData operationData) {
        int actionType = operationData.actionType;
        assert actionType < UiTestOperationType.NUM_UI_TEST_OPERATION_TYPES;
        // Fill the ArrayLists if this is the first time the method has been called.
        if (mUiOperationResults == null) {
            mUiOperationResults =
                    new ArrayList<Integer>(UiTestOperationType.NUM_UI_TEST_OPERATION_TYPES);
            mUiOperationResultCallbacks =
                    new ArrayList<Runnable>(UiTestOperationType.NUM_UI_TEST_OPERATION_TYPES);
            for (int i = 0; i < UiTestOperationType.NUM_UI_TEST_OPERATION_TYPES; i++) {
                mUiOperationResults.add(null);
                mUiOperationResultCallbacks.add(null);
            }
        }
        mUiOperationResults.set(actionType, UiTestOperationResult.UNREPORTED);
        mUiOperationResultCallbacks.set(actionType, operationData.resultCallback);

        // In the case of the UI activity quiescence callback type, we need to let the native UI
        // know how long to wait before timing out.
        if (actionType == UiTestOperationType.UI_ACTIVITY_RESULT) {
            VrShellJni.get().setUiExpectingActivityForTesting(
                    mNativeVrShell, VrShell.this, operationData.timeoutMs);
        } else if (actionType == UiTestOperationType.ELEMENT_VISIBILITY_STATUS) {
            VrShellJni.get().watchElementForVisibilityStatusForTesting(mNativeVrShell, VrShell.this,
                    operationData.elementName, operationData.timeoutMs, operationData.visibility);
        }
    }

    public void saveNextFrameBufferToDiskForTesting(String filepathBase) {
        VrShellJni.get().saveNextFrameBufferToDiskForTesting(
                mNativeVrShell, VrShell.this, filepathBase);
    }

    public int getLastUiOperationResultForTesting(int actionType) {
        return mUiOperationResults.get(actionType).intValue();
    }

    @CalledByNative
    public void reportUiOperationResultForTesting(int actionType, int result) {
        mUiOperationResults.set(actionType, result);
        mUiOperationResultCallbacks.get(actionType).run();
        mUiOperationResultCallbacks.set(actionType, null);
    }

    @NativeMethods
    interface Natives {
        long init(VrShell caller, VrShellDelegate delegate, boolean forWebVR,
                boolean browsingDisabled, boolean hasOrCanRequestRecordAudioPermission, long gvrApi,
                boolean reprojectedRendering, float displayWidthMeters, float displayHeightMeters,
                int displayWidthPixels, int displayHeightPixels, boolean pauseContent,
                boolean lowDensity, boolean isStandaloneVrDevice);
        boolean hasUiFinishedLoading(long nativeVrShell, VrShell caller);
        void setSurface(long nativeVrShell, VrShell caller, Surface surface);
        void swapContents(long nativeVrShell, VrShell caller, Tab tab);
        void setAndroidGestureTarget(
                long nativeVrShell, VrShell caller, AndroidUiGestureTarget androidUiGestureTarget);
        void setDialogGestureTarget(
                long nativeVrShell, VrShell caller, AndroidUiGestureTarget dialogGestureTarget);
        void destroy(long nativeVrShell, VrShell caller);
        void onTriggerEvent(long nativeVrShell, VrShell caller, boolean touched);
        void onPause(long nativeVrShell, VrShell caller);
        void onResume(long nativeVrShell, VrShell caller);
        void onLoadProgressChanged(long nativeVrShell, VrShell caller, double progress);
        void bufferBoundsChanged(long nativeVrShell, VrShell caller, int contentWidth,
                int contentHeight, int overlayWidth, int overlayHeight);
        void setWebVrMode(long nativeVrShell, VrShell caller, boolean enabled);
        boolean getWebVrMode(long nativeVrShell, VrShell caller);
        boolean isDisplayingUrlForTesting(long nativeVrShell, VrShell caller);
        void onTabListCreated(
                long nativeVrShell, VrShell caller, Tab[] mainTabs, Tab[] incognitoTabs);
        void onTabUpdated(
                long nativeVrShell, VrShell caller, boolean incognito, int id, String title);
        void onTabRemoved(long nativeVrShell, VrShell caller, boolean incognito, int id);
        void closeAlertDialog(long nativeVrShell, VrShell caller);
        void setAlertDialog(long nativeVrShell, VrShell caller, float width, float height);
        void setDialogBufferSize(long nativeVrShell, VrShell caller, int width, int height);
        void setAlertDialogSize(long nativeVrShell, VrShell caller, float width, float height);
        void setDialogLocation(long nativeVrShell, VrShell caller, float x, float y);
        void setDialogFloating(long nativeVrShell, VrShell caller, boolean floating);
        void showToast(long nativeVrShell, VrShell caller, String text);
        void cancelToast(long nativeVrShell, VrShell caller);
        void setHistoryButtonsEnabled(
                long nativeVrShell, VrShell caller, boolean canGoBack, boolean canGoForward);
        void requestToExitVr(long nativeVrShell, VrShell caller, @UiUnsupportedMode int reason);
        void showSoftInput(long nativeVrShell, VrShell caller, boolean show);
        void updateWebInputIndices(long nativeVrShell, VrShell caller, int selectionStart,
                int selectionEnd, int compositionStart, int compositionEnd);
        VrInputConnection getVrInputConnectionForTesting(long nativeVrShell, VrShell caller);
        void acceptDoffPromptForTesting(long nativeVrShell, VrShell caller);
        void performControllerActionForTesting(long nativeVrShell, VrShell caller, int elementName,
                int actionType, float x, float y);
        void performKeyboardInputForTesting(
                long nativeVrShell, VrShell caller, int inputType, String inputString);
        void setUiExpectingActivityForTesting(
                long nativeVrShell, VrShell caller, int quiescenceTimeoutMs);
        void saveNextFrameBufferToDiskForTesting(
                long nativeVrShell, VrShell caller, String filepathBase);
        void watchElementForVisibilityStatusForTesting(long nativeVrShell, VrShell caller,
                int elementName, int timeoutMs, boolean visibility);
        void resumeContentRendering(long nativeVrShell, VrShell caller);
        void onOverlayTextureEmptyChanged(long nativeVrShell, VrShell caller, boolean empty);
        void requestRecordAudioPermissionResult(
                long nativeVrShell, VrShell caller, boolean canRecordAudio);
    }
}
