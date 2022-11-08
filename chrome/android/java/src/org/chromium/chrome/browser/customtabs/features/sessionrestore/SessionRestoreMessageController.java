// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import android.app.Activity;
import android.content.res.Resources;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import javax.inject.Inject;

/**
 * Used to display a message for user to restore the previous session when applicable.
 */
@ActivityScope
public class SessionRestoreMessageController implements NativeInitObserver {
    private final Activity mActivity;
    private final ActivityWindowAndroid mWindowAndroid;
    private MessageDispatcher mMessageDispatcher;

    @Inject
    public SessionRestoreMessageController(Activity activity, ActivityWindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        if (ChromeFeatureList.sCctRetainableStateInMemory.isEnabled()) {
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
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(
                                        org.chromium.chrome.R.string.restore_custom_tab_title))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID,
                                org.chromium.chrome.R.drawable.infobar_chrome)
                        .with(MessageBannerProperties.DESCRIPTION,
                                resources.getString(org.chromium.chrome.R.string
                                                            .restore_custom_tab_description))
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(org.chromium.chrome.R.string
                                                            .restore_custom_tab_button_text))
                        .build();

        mMessageDispatcher.enqueueWindowScopedMessage(message, false);
    }
}
