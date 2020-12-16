// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.media.AudioManager;
import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.document.ChromeIntentUtil;
import org.chromium.chrome.browser.document.DocumentWebContentsDelegate;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.media.PictureInPicture;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/**
 * {@link WebContentsDelegateAndroid} that interacts with {@link ChromeActivity} and those
 * of the lifetime of the activity to process requests from underlying {@link WebContents}
 * for a given {@link Tab}.
 */
public class ActivityTabWebContentsDelegateAndroid extends TabWebContentsDelegateAndroid {
    private static final String TAG = "ActivityTabWCDA";

    private final ArrayMap<WebContents, GURL> mWebContentsUrlMapping = new ArrayMap<>();

    private final Tab mTab;

    @Nullable
    private ChromeActivity mActivity;

    public ActivityTabWebContentsDelegateAndroid(Tab tab, ChromeActivity activity) {
        mTab = tab;
        mActivity = activity;
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window == null) mActivity = null;
            }

            @Override
            public void onDestroyed(Tab tab) {
                tab.removeObserver(this);
            }
        });
    }

    private TabCreator getTabCreator() {
        return mActivity != null ? mActivity.getTabCreator(mTab.isIncognito()) : null;
    }

    private BrowserControlsStateProvider getBrowserControlsStateProvider() {
        return mActivity != null && !mActivity.isActivityFinishingOrDestroyed()
                ? mActivity.getBrowserControlsManager()
                : null;
    }

    private FullscreenManager getFullscreenManager() {
        return mActivity != null && !mActivity.isActivityFinishingOrDestroyed()
                ? mActivity.getFullscreenManager()
                : null;
    }

    @Override
    public int getDisplayMode() {
        return WebDisplayMode.BROWSER;
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
    public void webContentsCreated(WebContents sourceWebContents, long openerRenderProcessId,
            long openerRenderFrameId, String frameName, GURL targetUrl,
            WebContents newWebContents) {
        // The URL can't be taken from the WebContents if it's paused.  Save it for later.
        assert !mWebContentsUrlMapping.containsKey(newWebContents);
        mWebContentsUrlMapping.put(newWebContents, targetUrl);

        // TODO(dfalcantara): Re-remove this once crbug.com/508366 is fixed.
        TabCreator tabCreator = getTabCreator();

        if (tabCreator != null && tabCreator.createsTabsAsynchronously()) {
            DocumentWebContentsDelegate.getInstance().attachDelegate(newWebContents);
        }
    }

    @Override
    public boolean isFullscreenForTabOrPending() {
        FullscreenManager manager = getFullscreenManager();
        return manager != null ? manager.getPersistentFullscreenMode() : false;
    }

    @Override
    protected boolean shouldResumeRequestsForCreatedWindow() {
        // Pause the WebContents if an Activity has to be created for it first.
        TabCreator tabCreator = getTabCreator();
        assert tabCreator != null;
        return !tabCreator.createsTabsAsynchronously();
    }

    @Override
    protected boolean addNewContents(WebContents sourceWebContents, WebContents webContents,
            int disposition, Rect initialPosition, boolean userGesture) {
        assert mWebContentsUrlMapping.containsKey(webContents);

        TabCreator tabCreator = getTabCreator();
        assert tabCreator != null;

        // Grab the URL, which might not be available via the Tab.
        GURL url = mWebContentsUrlMapping.remove(webContents);

        // Skip opening a new Tab if it doesn't make sense.
        if (mTab.isClosing()) return false;

        // Creating new Tabs asynchronously requires starting a new Activity to create the Tab,
        // so the Tab returned will always be null.  There's no way to know synchronously
        // whether the Tab is created, so assume it's always successful.
        boolean createdSuccessfully = tabCreator.createTabWithWebContents(
                mTab, webContents, TabLaunchType.FROM_LONGPRESS_FOREGROUND, url);
        boolean success = tabCreator.createsTabsAsynchronously() || createdSuccessfully;

        if (success) {
            if (disposition == WindowOpenDisposition.NEW_FOREGROUND_TAB) {
                if (mActivity.getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter()
                                .getRelatedTabList(mTab.getId())
                                .size()
                        == 2) {
                    RecordUserAction.record("TabGroup.Created.DeveloperRequestedNewTab");
                }
            } else if (disposition == WindowOpenDisposition.NEW_POPUP) {
                PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
                auditor.notifyAuditEvent(ContextUtils.getApplicationContext(),
                        AuditEvent.OPEN_POPUP_URL_SUCCESS, url.getSpec(), "");
            }
        }

        return success;
    }

    @Override
    public void activateContents() {
        if (mActivity == null) {
            Log.e(TAG, "Activity not set activateContents().  Bailing out.");
            return;
        }

        if (mActivity.isActivityFinishingOrDestroyed()) {
            Log.e(TAG, "Activity destroyed before calling activateContents().  Bailing out.");
            return;
        }
        if (!mTab.isInitialized()) {
            Log.e(TAG, "Tab not initialized before calling activateContents().  Bailing out.");
            return;
        }

        // Do nothing if the tab can currently be interacted with by the user.
        if (mTab.isUserInteractable()) return;

        TabModel model = mActivity.getTabModelSelector().getModel(mTab.isIncognito());
        int index = model.indexOf(mTab);
        if (index == TabModel.INVALID_TAB_INDEX) return;
        TabModelUtils.setIndex(model, index);

        // Do nothing if the mActivity is visible (STOPPED is the only valid invisible state as we
        // explicitly check isActivityFinishingOrDestroyed above).
        if (ApplicationStatus.getStateForActivity(mActivity) == ActivityState.STOPPED) {
            bringActivityToForeground();
        }
    }

    /**
     * Brings chrome's Activity to foreground, if it is not so.
     */
    protected void bringActivityToForeground() {
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
        Intent newIntent = ChromeIntentUtil.createBringTabToFrontIntent(mTab.getId());
        if (newIntent != null) {
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(newIntent);
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
                        (AudioManager) ContextUtils.getApplicationContext().getSystemService(
                                Context.AUDIO_SERVICE);
                am.dispatchMediaKeyEvent(e);
                break;
            default:
                break;
        }
    }

    @Override
    protected void setOverlayMode(boolean useOverlayMode) {
        mActivity.setOverlayMode(useOverlayMode);
    }

    @Override
    public int getTopControlsHeight() {
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null ? provider.getTopControlsHeight() : 0;
    }

    @Override
    public int getTopControlsMinHeight() {
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null ? provider.getTopControlsMinHeight() : 0;
    }

    @Override
    public int getBottomControlsHeight() {
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null ? provider.getBottomControlsHeight() : 0;
    }

    @Override
    public int getBottomControlsMinHeight() {
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null ? provider.getBottomControlsMinHeight() : 0;
    }

    @Override
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null && provider.shouldAnimateBrowserControlsHeightChanges();
    }

    @Override
    public void enterFullscreenModeForTab(boolean prefersNavigationBar) {
        FullscreenManager manager = getFullscreenManager();
        if (manager != null) {
            manager.onEnterFullscreen(mTab, new FullscreenOptions(prefersNavigationBar));
        }
    }

    @Override
    public void exitFullscreenModeForTab() {
        FullscreenManager manager = getFullscreenManager();
        if (manager != null) manager.onExitFullscreen(mTab);
    }

    @Override
    protected boolean isPictureInPictureEnabled() {
        return mActivity != null ? PictureInPicture.isEnabled(mActivity.getApplicationContext())
                                 : false;
    }

    @Override
    protected boolean isNightModeEnabled() {
        return mActivity != null ? ColorUtils.inNightMode(mActivity) : false;
    }

    @Override
    protected boolean isCustomTab() {
        return mActivity != null && mActivity.isCustomTab();
    }

    private void showRepostFormWarningTabModalDialog() {
        // As a rule, showRepostFormWarningDialog should only be called on active tabs, as it's
        // called right after WebContents::Activate. But in various corner cases, that
        // activation may fail.
        if (mActivity == null || !mTab.isUserInteractable()) {
            mTab.getWebContents().getNavigationController().cancelPendingReload();
            return;
        }

        ModalDialogManager modalDialogManager = mActivity.getModalDialogManager();
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(modalDialogManager, (Integer dismissalCause) -> {
                    if (!mTab.isInitialized()) return;
                    switch (dismissalCause) {
                        case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                            mTab.getWebContents().getNavigationController().continuePendingReload();
                            break;
                        case DialogDismissalCause.ACTIVITY_DESTROYED:
                        case DialogDismissalCause.TAB_DESTROYED:
                            // Intentionally ignored as the tab object is gone.
                            break;
                        default:
                            mTab.getWebContents().getNavigationController().cancelPendingReload();
                            break;
                    }
                });

        Resources resources = mActivity.getResources();
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, resources,
                                R.string.http_post_warning_title)
                        .with(ModalDialogProperties.MESSAGE, resources, R.string.http_post_warning)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.http_post_warning_resend)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        modalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.TAB, true);
    }
}
