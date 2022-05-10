// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantSnackbarFactory;
import org.chromium.components.autofill_assistant.AssistantTabChangeObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implementation of {@link AssistantDependencies} for Chrome.
 */
public class AssistantDependenciesChrome
        extends AssistantStaticDependenciesChrome implements AssistantDependencies {
    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private BrowserControlsStateProvider mBrowserControls;
    private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private ApplicationViewportInsetSupplier mBottomInsetProvider;
    private ActivityTabProvider mActivityTabProvider;
    private View mRootView;
    private AssistantSnackbarFactory mSnackbarFactory;

    public AssistantDependenciesChrome(Activity activity) {
        maybeUpdateDependencies(activity);
    }

    @Override
    public boolean maybeUpdateDependencies(WebContents webContents) {
        @Nullable
        Activity activity = ActivityUtils.getActivityFromWebContents(webContents);
        if (activity == null) return false;
        return maybeUpdateDependencies(activity);
    }

    /**
     * Updates dependencies that are tied to the activity.
     * @return Whether a new activity could be found.
     */
    private boolean maybeUpdateDependencies(Activity activity) {
        if (activity == mActivity) return true;
        if (!(activity instanceof ChromeActivity)) return false;
        ChromeActivity chromeActivity = (ChromeActivity) activity;

        Supplier<View> rootView = chromeActivity.getCompositorViewHolderSupplier();

        mActivity = chromeActivity;
        mWindowAndroid = chromeActivity.getWindowAndroid();
        mBottomSheetController = BottomSheetControllerProvider.from(mWindowAndroid);
        mBrowserControls = chromeActivity.getBrowserControlsManager();
        mKeyboardVisibilityDelegate = mWindowAndroid.getKeyboardDelegate();
        mBottomInsetProvider = mWindowAndroid.getApplicationBottomInsetProvider();
        mActivityTabProvider = chromeActivity.getActivityTabProvider();
        mRootView = rootView.get();
        mSnackbarFactory =
                new AssistantSnackbarFactoryChrome(mActivity, chromeActivity.getSnackbarManager());
        return true;
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @Override
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @Override
    public KeyboardVisibilityDelegate getKeyboardVisibilityDelegate() {
        return mKeyboardVisibilityDelegate;
    }

    @Override
    public ApplicationViewportInsetSupplier getBottomInsetProvider() {
        return mBottomInsetProvider;
    }

    @Override
    public View getRootView() {
        return mRootView;
    }

    @Override
    public ViewGroup getRootViewGroup() {
        return (ViewGroup) mActivity.findViewById(R.id.coordinator);
    }

    @Override
    public AssistantSnackbarFactory getSnackbarFactory() {
        return mSnackbarFactory;
    }

    @Override
    public AssistantBrowserControlsFactory createBrowserControlsFactory() {
        return () -> new AssistantBrowserControlsChrome(mBrowserControls);
    }

    @Override
    public Destroyable observeTabChanges(AssistantTabChangeObserver tabChangeObserver) {
        return new AssistantTabChangeObserverChrome(mActivityTabProvider, tabChangeObserver);
    }
}
