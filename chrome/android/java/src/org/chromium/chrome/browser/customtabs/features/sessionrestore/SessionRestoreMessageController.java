// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import android.app.Activity;
import android.content.res.Resources;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingDelegateFactory;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Used to display a message for user to restore the previous session when applicable.
 */
@ActivityScope
public class SessionRestoreMessageController implements NativeInitObserver {
    private final Activity mActivity;
    private final ActivityWindowAndroid mWindowAndroid;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabsConnection mConnection;
    private MessageDispatcher mMessageDispatcher;
    private final CustomTabActivityTabFactory mTabFactory;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;
    private final Lazy<CustomTabDelegateFactory> mCustomTabDelegateFactory;
    private int mHiddenTabId;

    @Inject
    public SessionRestoreMessageController(Activity activity, ActivityWindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider, CustomTabsConnection connection,
            Lazy<CustomTabDelegateFactory> customTabDelegateFactory,
            CustomTabActivityTabFactory tabFactory,
            Lazy<CompositorViewHolder> compositorViewHolder) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mIntentDataProvider = intentDataProvider;
        mConnection = connection;
        mCustomTabDelegateFactory = customTabDelegateFactory;
        mTabFactory = tabFactory;
        mCompositorViewHolder = compositorViewHolder;
        if (ChromeFeatureList.sCctRetainableStateInMemory.isEnabled() && isRestorable()) {
            if (lifecycleDispatcher.isNativeInitializationFinished()) {
                showMessage();
            } else {
                lifecycleDispatcher.register(this);
            }
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        showMessage();
    }

    private void showMessage() {
        mMessageDispatcher = MessageDispatcherProvider.from(mWindowAndroid);

        Resources resources = mActivity.getResources();

        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.RESTORE_CUSTOM_TAB)
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(R.string.restore_custom_tab_title))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.infobar_chrome)
                        .with(MessageBannerProperties.DESCRIPTION,
                                resources.getString(R.string.restore_custom_tab_description))
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.restore_custom_tab_button_text))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::onMessageAccepted)
                        .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                        .build();

        mMessageDispatcher.enqueueWindowScopedMessage(message, false);
    }

    private void restoreTabOnPrimaryAction() {
        SessionRestoreManager sessionRestoreManager = mConnection.getSessionRestoreManager();
        assert sessionRestoreManager != null;
        setHiddenTabId(mTabFactory.getTabModelSelector().getCurrentTabId());

        Tab tab = sessionRestoreManager.restoreTab();
        if (tab == null) return;

        // TODO(crbug.com/1401058): Optimize with correct LaunchType and LoadIfNeededCaller
        Runnable callback = () -> {
            mTabFactory.getTabModelSelector().getCurrentModel().addTab(
                    tab, 0, tab.getLaunchType(), TabCreationState.LIVE_IN_FOREGROUND);
            tab.show(TabSelectionType.FROM_USER, TabUtils.LoadIfNeededCaller.OTHER);
            showUndoMessage();
        };
        ReparentingTask.from(tab).finish(ReparentingDelegateFactory.createReparentingTaskDelegate(
                                                 mCompositorViewHolder.get(), mWindowAndroid,
                                                 mCustomTabDelegateFactory.get()),
                callback);
    }

    private boolean isRestorable() {
        SessionRestoreManager sessionRestoreManager = mConnection.getSessionRestoreManager();
        assert sessionRestoreManager != null;

        int taskId = mActivity.getTaskId();
        String clientPackage = mIntentDataProvider.getClientPackageName();
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        String urlToLoad = mIntentDataProvider.getUrlToLoad();
        String referrer = CustomTabActivityLifecycleUmaTracker.getReferrerUriString(mActivity);
        return sessionRestoreManager.canRestoreTab()
                && SessionRestoreUtils.canRestoreSession(
                        taskId, urlToLoad, preferences, clientPackage, referrer);
    }

    @VisibleForTesting
    void showUndoMessage() {
        mMessageDispatcher = MessageDispatcherProvider.from(mWindowAndroid);

        Resources resources = mActivity.getResources();

        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.UNDO_CUSTOM_TAB_RESTORATION)
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(R.string.undo_restoration_title))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.infobar_chrome)
                        .with(MessageBannerProperties.DESCRIPTION,
                                resources.getString(R.string.restore_custom_tab_description))
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.undo_restoration_button_text))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                this::onUndoMessageAccepted)
                        .with(MessageBannerProperties.ON_DISMISSED, this::onUndoMessageDismissed)
                        .build();

        mMessageDispatcher.enqueueWindowScopedMessage(message, true);
    }

    @VisibleForTesting
    @PrimaryActionClickBehavior
    int onMessageAccepted() {
        restoreTabOnPrimaryAction();

        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    @VisibleForTesting
    int onUndoMessageAccepted() {
        Tab tab = mTabFactory.getTabModelSelector().getCurrentTab();
        assert tab != null;

        mTabFactory.getTabModelSelector().closeTab(tab);

        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    @VisibleForTesting
    void onMessageDismissed(@DismissReason int dismissReason) {
        if (dismissReason != DismissReason.PRIMARY_ACTION) {
            mConnection.getSessionRestoreManager().clearCache();
        }
    }

    @VisibleForTesting
    void onUndoMessageDismissed(@DismissReason int dismissReason) {
        if (dismissReason == DismissReason.PRIMARY_ACTION) return;

        int hiddenTabId = getHiddenTabId();
        Tab hiddenTab = mTabFactory.getTabModelSelector().getTabById(hiddenTabId);
        mTabFactory.getTabModelSelector().closeTab(hiddenTab);
    }

    private void setHiddenTabId(int id) {
        mHiddenTabId = id;
    }

    private int getHiddenTabId() {
        return mHiddenTabId;
    }
}
