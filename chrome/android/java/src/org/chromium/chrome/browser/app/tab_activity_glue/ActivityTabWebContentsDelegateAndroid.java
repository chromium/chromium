// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.media.AudioManager;
import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.media.PictureInPicture;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Objects;

/**
 * {@link WebContentsDelegateAndroid} that interacts with {@link Activity} and those of the lifetime
 * of the activity to process requests from underlying {@link WebContents} for a given {@link Tab}.
 */
public class ActivityTabWebContentsDelegateAndroid extends TabWebContentsDelegateAndroid {
    private static final String TAG = "ActivityTabWCDA";

    private final ArrayMap<WebContents, GURL> mWebContentsUrlMapping = new ArrayMap<>();

    private final Tab mTab;

    @Nullable private Activity mActivity;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final boolean mIsCustomTab;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final TabObserver mTabObserver;

    public ActivityTabWebContentsDelegateAndroid(
            Tab tab,
            Activity activity,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            boolean isCustomTab,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager,
            TabCreatorManager tabCreatorManager,
            @NonNull Supplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mTab = tab;
        mActivity = activity;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mIsCustomTab = isCustomTab;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mFullscreenManager = fullscreenManager;
        mTabCreatorManager = tabCreatorManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (window == null) mActivity = null;
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        tab.removeObserver(this);
                    }
                };
        tab.addObserver(mTabObserver);
    }

    @Override
    public void openNewTab(
            GURL url,
            String extraHeaders,
            ResourceRequestBody postData,
            int disposition,
            boolean isRendererInitiated) {
        // New tabs are handled by the tab model (see
        // TabWebContentsDelegateAndroid::OpenURLFromTab().
        assert false;
    }

    @Override
    public int getDisplayMode() {
        return DisplayMode.BROWSER;
    }

    @Override
    public void showRepostFormWarningDialog() {
        // When the dialog is visible, keeping the refresh animation active
        // in the background is distracting and unnecessary (and likely to
        // jank when the dialog is shown).
        SwipeRefreshHandler handler = SwipeRefreshHandler.get(mTab);
        if (handler != null) handler.reset();

        showRepostFormWarningTabModalDialog();
    }

    @Override
    public void webContentsCreated(
            WebContents sourceWebContents,
            long openerRenderProcessId,
            long openerRenderFrameId,
            String frameName,
            GURL targetUrl,
            WebContents newWebContents) {
        // The URL can't be taken from the WebContents if it's paused.  Save it for later.
        assert !mWebContentsUrlMapping.containsKey(newWebContents);
        mWebContentsUrlMapping.put(newWebContents, targetUrl);
    }

    @Override
    public boolean isFullscreenForTabOrPending() {
        return mFullscreenManager != null
                ? mFullscreenManager.getPersistentFullscreenMode()
                : false;
    }

    @Override
    protected boolean shouldResumeRequestsForCreatedWindow() {
        return true;
    }

    @Override
    protected boolean addNewContents(
            WebContents sourceWebContents,
            WebContents webContents,
            int disposition,
            WindowFeatures windowFeatures,
            boolean userGesture) {
        assert mWebContentsUrlMapping.containsKey(webContents);

        TabCreator tabCreator = mTabCreatorManager.getTabCreator(mTab.isIncognito());
        assert tabCreator != null;

        // Grab the URL, which might not be available via the Tab.
        GURL url = mWebContentsUrlMapping.remove(webContents);

        // Skip opening a new Tab if it doesn't make sense.
        if (mTab.isClosing()) return false;

        boolean openingPopup =
                PopupCreator.arePopupsEnabled(mActivity)
                        && (disposition == WindowOpenDisposition.NEW_POPUP);

        // Auxiliary navigations starting in a PWA will always cause a tab reparenting, we
        // want to prevent UI effects caused by adding the Tab to the TabModel.
        // This check is done before the tab is even created and the Tab where navigation started
        // will be used to extract some information. The destination WebContents is provided to
        // extract the missing features of this navigation that cannot be extracted from this
        // InterceptNavigationDelegateImpl instance.
        // TODO(crbug.com/404767741): enable early navigation capturing to address captured
        // navigations UI jank.
        var navigationTabHelper = InterceptNavigationDelegateTabHelper.getFromTab(mTab);
        boolean willReparentTab =
                navigationTabHelper != null
                        && navigationTabHelper
                                .getInterceptNavigationDelegate()
                                .shouldReparentTab(webContents);

        Tab tab =
                tabCreator.createTabWithWebContents(
                        mTab,
                        webContents,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                        url,
                        !openingPopup && !willReparentTab);
        if (tab == null) return false;

        if (openingPopup) {
            PopupCreator.moveTabToNewPopup(
                    tab, windowFeatures, mTab.getWindowAndroid().getDisplay());
        }

        if (disposition == WindowOpenDisposition.NEW_FOREGROUND_TAB) {
            RecordUserAction.record("LinkNavigationOpenedInForegroundTab");
        } else if (disposition == WindowOpenDisposition.NEW_POPUP) {
            PolicyAuditor auditor = PolicyAuditor.maybeCreate();
            if (auditor != null) {
                auditor.notifyAuditEvent(
                        ContextUtils.getApplicationContext(),
                        AuditEvent.OPEN_POPUP_URL_SUCCESS,
                        url.getSpec(),
                        "");
            }
        }

        Tab sourceTab = fromWebContents(sourceWebContents);
        if (sourceTab == null
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.GROUP_NEW_TAB_WITH_PARENT)) {
            return true;
        }

        if (disposition != WindowOpenDisposition.NEW_FOREGROUND_TAB
                && disposition != WindowOpenDisposition.NEW_BACKGROUND_TAB) {
            return true;
        }

        Tab newTab = fromWebContents(webContents);
        if (newTab == null || newTab.getParentId() != sourceTab.getId()) {
            return true;
        }

        // If the new tab is in a different TabModel from the parent tab, don't group them.
        if (TabWindowManagerSingleton.getInstance().getTabModelForTab(sourceTab)
                == TabWindowManagerSingleton.getInstance().getTabModelForTab(newTab)) {
            TabGroupModelFilter tabGroupModelFilter = getTabGroupModelFilter(sourceTab);
            // Set notify to false so snackbar to undo the grouping will not be shown.
            if (tabGroupModelFilter != null
                    && tabGroupModelFilter.isTabInTabGroup(sourceTab)
                    && tabGroupModelFilter.isTabModelRestored()) {
                tabGroupModelFilter.mergeListOfTabsToGroup(
                        Arrays.asList(newTab), sourceTab, /* notify= */ false);
                if (mChromeActivityNativeDelegate != null) {
                    assert newTab.getRootId() == sourceTab.getRootId();
                    assert Objects.equals(newTab.getTabGroupId(), sourceTab.getTabGroupId());
                    assert tabGroupModelFilter
                            .getTabsInGroup(newTab.getTabGroupId())
                            .contains(sourceTab);
                    assert tabGroupModelFilter
                            .getTabsInGroup(sourceTab.getTabGroupId())
                            .contains(newTab);
                }
            }
        }

        return true;
    }

    @Override
    protected void setContentsBounds(WebContents source, Rect bounds) {
        // Do nothing.
    }

    @Override
    public void activateContents() {
        if (mActivity == null) {
            Log.e(TAG, "Activity not set activateContents().  Bailing out.");
            return;
        }

        if (mChromeActivityNativeDelegate.isActivityFinishingOrDestroyed()) {
            Log.e(TAG, "Activity destroyed before calling activateContents().  Bailing out.");
            return;
        }
        if (!mTab.isInitialized()) {
            Log.e(TAG, "Tab not initialized before calling activateContents().  Bailing out.");
            return;
        }

        // If the tab can currently be interacted with by the user and it's not in multi-window
        // mode, then it is already focused so we can drop the call.
        if (!mActivity.isInMultiWindowMode() && mTab.isUserInteractable()) {
            return;
        }

        TabModel model = mTabModelSelectorSupplier.get().getModel(mTab.isIncognito());
        int index = model.indexOf(mTab);
        if (index == TabModel.INVALID_TAB_INDEX) return;
        TabModelUtils.setIndex(model, index);

        WindowAndroid hostWindow = mTab.getWindowAndroid();

        // If the activity is the top resumed activity, then it is already focused so we can drop
        // the call.
        if (hostWindow.isActivityTopResumedSupported() && hostWindow.isTopResumedActivity()) {
            return;
        }

        // If the activity is visible in fullscreen windowing mode (STOPPED is the only valid
        // invisible state in fullscreen windowing mode as we explicitly check
        // isActivityFinishingOrDestroyed above), then it is already focused so we can drop the
        // call.
        if (!hostWindow.isActivityTopResumedSupported()
                && !mActivity.isInMultiWindowMode()
                && ApplicationStatus.getStateForActivity(mActivity) != ActivityState.STOPPED) {
            return;
        }

        bringActivityToForeground();
    }

    /** Brings chrome's Activity to foreground, if it is not so. */
    protected void bringActivityToForeground() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.USE_ACTIVITY_MANAGER_FOR_TAB_ACTIVATION)) {
            ((ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE))
                    .moveTaskToFront(mActivity.getTaskId(), 0);
        } else {
            // This intent is sent in order to get the activity back to the foreground if it was
            // not already. The previous call will activate the right tab in the context of the
            // TabModel but will only show the tab to the user if Chrome was already in the
            // foreground.
            // The intent is getting the tabId mostly because it does not cost much to do so.
            // When receiving the intent, the tab associated with the tabId should already be
            // active.
            // Note that calling only the intent in order to activate the tab is slightly slower
            // because it will change the tab when the intent is handled, which happens after
            // Chrome gets back to the foreground.
            Intent newIntent =
                    IntentHandler.createTrustedBringTabToFrontIntent(
                            mTab.getId(), IntentHandler.BringToFrontSource.ACTIVATE_TAB);
            if (newIntent != null) {
                newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                ContextUtils.getApplicationContext().startActivity(newIntent);
            }
        }
    }

    @Override
    public boolean takeFocus(boolean reverse) {
        if (mActivity == null) return false;
        if (reverse) {
            View menuButton = mActivity.findViewById(R.id.menu_button);
            if (menuButton != null && menuButton.isShown()) {
                return menuButton.requestFocus();
            }

            View tabSwitcherButton = mActivity.findViewById(R.id.tab_switcher_button);
            if (tabSwitcherButton != null && tabSwitcherButton.isShown()) {
                return tabSwitcherButton.requestFocus();
            }
        } else {
            View urlBar = mActivity.findViewById(R.id.url_bar);
            if (urlBar != null) return urlBar.requestFocus();
        }
        return false;
    }

    @Override
    public void handleKeyboardEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN && mActivity != null) {
            if (mActivity.onKeyDown(event.getKeyCode(), event)) return;

            // Handle the Escape key here (instead of in KeyboardShortcuts.java), so it doesn't
            // interfere with other parts of the activity (e.g. the URL bar).
            if (event.getKeyCode() == KeyEvent.KEYCODE_ESCAPE && event.hasNoModifiers()) {
                WebContents wc = mTab.getWebContents();
                if (wc != null) wc.stop();
                return;
            }
        }
        handleMediaKey(event);
    }

    /**
     * Redispatches unhandled media keys. This allows bluetooth headphones with play/pause or
     * other buttons to function correctly.
     */
    private void handleMediaKey(KeyEvent e) {
        switch (e.getKeyCode()) {
            case KeyEvent.KEYCODE_MUTE:
            case KeyEvent.KEYCODE_HEADSETHOOK:
            case KeyEvent.KEYCODE_MEDIA_PLAY:
            case KeyEvent.KEYCODE_MEDIA_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_STOP:
            case KeyEvent.KEYCODE_MEDIA_NEXT:
            case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
            case KeyEvent.KEYCODE_MEDIA_REWIND:
            case KeyEvent.KEYCODE_MEDIA_RECORD:
            case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
            case KeyEvent.KEYCODE_MEDIA_CLOSE:
            case KeyEvent.KEYCODE_MEDIA_EJECT:
            case KeyEvent.KEYCODE_MEDIA_AUDIO_TRACK:
                AudioManager am =
                        (AudioManager)
                                ContextUtils.getApplicationContext()
                                        .getSystemService(Context.AUDIO_SERVICE);
                am.dispatchMediaKeyEvent(e);
                break;
            default:
                break;
        }
    }

    @Override
    protected void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorViewHolderSupplier.hasValue()) {
            mCompositorViewHolderSupplier.get().setOverlayMode(useOverlayMode);
        }
    }

    @Override
    public int getTopControlsHeight() {
        return mBrowserControlsStateProvider != null
                ? mBrowserControlsStateProvider.getTopControlsHeight()
                : 0;
    }

    @Override
    public int getTopControlsMinHeight() {
        return mBrowserControlsStateProvider != null
                ? mBrowserControlsStateProvider.getTopControlsMinHeight()
                : 0;
    }

    @Override
    public int getBottomControlsHeight() {
        return mBrowserControlsStateProvider != null
                ? mBrowserControlsStateProvider.getBottomControlsHeight()
                : 0;
    }

    @Override
    public int getBottomControlsMinHeight() {
        return mBrowserControlsStateProvider != null
                ? mBrowserControlsStateProvider.getBottomControlsMinHeight()
                : 0;
    }

    @Override
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return mBrowserControlsStateProvider != null
                && mBrowserControlsStateProvider.shouldAnimateBrowserControlsHeightChanges();
    }

    @Override
    public boolean controlsResizeView() {
        return mCompositorViewHolderSupplier.hasValue()
                && mCompositorViewHolderSupplier.get().controlsResizeView();
    }

    @Override
    public int getVirtualKeyboardHeight() {
        if (mActivity == null) return 0;

        View rootView = mActivity.getWindow().getDecorView().getRootView();
        return mTab.getWindowAndroid().getKeyboardDelegate().calculateTotalKeyboardHeight(rootView);
    }

    @Override
    public void enterFullscreenModeForTab(boolean prefersNavigationBar, boolean prefersStatusBar) {
        if (mFullscreenManager != null) {
            mFullscreenManager.onEnterFullscreen(
                    mTab, new FullscreenOptions(prefersNavigationBar, prefersStatusBar));
        }
    }

    @Override
    public void fullscreenStateChangedForTab(
            boolean prefersNavigationBar, boolean prefersStatusBar) {
        // State-only changes are useful for recursive fullscreen activation. Early out if
        // fullscreen mode is not on.
        if (mFullscreenManager == null || !mFullscreenManager.getPersistentFullscreenMode()) return;
        mFullscreenManager.onEnterFullscreen(
                mTab, new FullscreenOptions(prefersNavigationBar, prefersStatusBar));
    }

    @Override
    public void exitFullscreenModeForTab() {
        if (mFullscreenManager != null) mFullscreenManager.onExitFullscreen(mTab);
    }

    @Override
    protected boolean isPictureInPictureEnabled() {
        return mActivity != null
                ? PictureInPicture.isEnabled(mActivity.getApplicationContext())
                : false;
    }

    @Override
    protected boolean isNightModeEnabled() {
        return mActivity != null ? ColorUtils.inNightMode(mActivity) : false;
    }

    @Override
    protected boolean isForceDarkWebContentEnabled() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)) {
            return true;
        }
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            return false;
        }
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) {
            return false;
        }
        Profile profile = mTab.getProfile();
        return isNightModeEnabled()
                && WebContentsDarkModeController.isEnabledForUrl(
                        profile, webContents.getVisibleUrl());
    }

    @Override
    protected boolean isCustomTab() {
        return mIsCustomTab;
    }

    private void showRepostFormWarningTabModalDialog() {
        // As a rule, showRepostFormWarningDialog should only be called on active tabs, as it's
        // called right after WebContents::Activate. But in various corner cases, that
        // activation may fail.
        if (mActivity == null || !mTab.isUserInteractable()) {
            mTab.getWebContents().getNavigationController().cancelPendingReload();
            return;
        }

        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(
                        modalDialogManager,
                        (Integer dismissalCause) -> {
                            if (!mTab.isInitialized()) return;
                            switch (dismissalCause) {
                                case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                                    mTab.getWebContents()
                                            .getNavigationController()
                                            .continuePendingReload();
                                    break;
                                case DialogDismissalCause.ACTIVITY_DESTROYED:
                                case DialogDismissalCause.TAB_DESTROYED:
                                    // Intentionally ignored as the tab object is gone.
                                    break;
                                default:
                                    mTab.getWebContents()
                                            .getNavigationController()
                                            .cancelPendingReload();
                                    break;
                            }
                        });

        Resources resources = mActivity.getResources();
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.http_post_warning_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(R.string.http_post_warning))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.http_post_warning_resend)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        modalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.TAB, true);
    }

    @Override
    protected boolean isModalContextMenu() {
        return !ContextMenuUtils.isPopupSupported(mActivity);
    }

    @Override
    protected boolean isDynamicSafeAreaInsetsEnabled() {
        return EdgeToEdgeControllerFactory.isSupportedConfiguration(mActivity);
    }

    protected TabGroupModelFilter getTabGroupModelFilter(Tab tab) {
        return TabModelUtils.getTabGroupModelFilterByTab(tab);
    }

    protected Tab fromWebContents(WebContents webContents) {
        return TabUtils.fromWebContents(webContents);
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mTabObserver);
    }
}
