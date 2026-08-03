// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Lightweight Activity used to host {@link SettingsHostFragment} in tests when {@link
 * SettingsInTab} is enabled. Extends {@link ChromeBaseAppCompatActivity} directly without
 * referencing {@link SettingsActivity}, and uses {@link SettingsPageFragmentDelegateImpl} to
 * provide a realistic simulation of settings in a tab.
 */
@NullMarked
public class SettingsInTabTestActivity extends ChromeBaseAppCompatActivity
        implements SnackbarManageable,
                SettingsActivityInterface,
                ActivityLifecycleDispatcherProvider {
    private static final int TAB_ID = 123;
    private Profile mProfile;
    private ScrimManager mScrimManager;
    private ManagedBottomSheetController mManagedBottomSheetController;
    private IntentRequestTracker mIntentRequestTracker;
    private SettingsPageFragmentDelegateImpl mFragmentDelegate;

    private final TestActivityLifecycleDispatcher mLifecycleDispatcher =
            new TestActivityLifecycleDispatcher(this);
    private final OneshotSupplierImpl<WindowAndroid> mWindowAndroidSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<SnackbarManager> mSnackbarManagerSupplier =
            new OneshotSupplierImpl<>();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        assert SettingsInTab.isEnabled();

        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        mProfile = ProfileManager.getLastUsedRegularProfile();

        super.onCreate(savedInstanceState);

        FrameLayout contentView = new FrameLayout(this);
        setContentView(contentView);

        FrameLayout sheetContainer = new FrameLayout(this);
        sheetContainer.setId(R.id.sheet_container);
        contentView.addView(
                sheetContainer,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        initBottomSheet();

        mSnackbarManagerSupplier.set(
                new SnackbarManager(this, contentView, null, null, getModalDialogManager()));

        mIntentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroidSupplier.set(
                new ActivityWindowAndroid(
                        this,
                        /* listenToActivityState= */ true,
                        mIntentRequestTracker,
                        getInsetObserver(),
                        /* occlusionTrackingAllowed= */ true));

        mFragmentDelegate =
                new SettingsPageFragmentDelegateImpl(
                        this,
                        mProfile,
                        assumeNonNull(mWindowAndroidSupplier.get()),
                        getActivityResultTracker(),
                        assumeNonNull(mSnackbarManagerSupplier.get()),
                        assumeNonNull(mBottomSheetControllerSupplier.get()),
                        assumeNonNull(getModalDialogManager()),
                        new MockTab(TAB_ID, mProfile));
        mFragmentDelegate.initSettings(contentView, "");

        // Ensure bottom sheets are above the settings views.
        contentView.bringChildToFront(sheetContainer);
    }

    private void initBottomSheet() {
        ViewGroup sheetContainer = findViewById(R.id.sheet_container);
        mScrimManager =
                new ScrimManager(
                        this,
                        (ViewGroup) sheetContainer.getParent(),
                        ScrimClient.SETTINGS_ACTIVITY);
        mManagedBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrimManager,
                        getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0,
                        /* desktopWindowStateManager= */ (DesktopWindowStateManager) null,
                        getInsetObserver(),
                        /* enableLargeFormFactorUi= */ false);
        mBottomSheetControllerSupplier.set(mManagedBottomSheetController);
    }

    @Override
    protected @Nullable ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
        assert snackbarManager != null;
        return snackbarManager;
    }

    @Override
    public @Nullable Fragment getMainFragment() {
        SettingsHostFragment host = SettingsHostFragment.get(this);
        return host != null ? host.getMainFragment() : null;
    }

    @Override
    public void finishCurrentSettings(Fragment fragment) {
        SettingsHostFragment host = SettingsHostFragment.get(this);
        if (host != null) {
            host.finishCurrentSettings(fragment);
        }
    }

    @Override
    public @Nullable MultiColumnSettings getMultiColumnSettings() {
        SettingsHostFragment host = SettingsHostFragment.get(this);
        return host != null ? host.getMultiColumnSettings() : null;
    }

    @Override
    public ActivityLifecycleDispatcher getLifecycleDispatcher() {
        return mLifecycleDispatcher;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mLifecycleDispatcher.dispatchOnSaveInstanceState(outState);
    }

    @Override
    protected void onDestroy() {
        mFragmentDelegate.destroySettings();
        mScrimManager.destroy();
        assumeNonNull(mSnackbarManagerSupplier.get()).destroy();
        assumeNonNull(mWindowAndroidSupplier.get()).destroy();
        super.onDestroy();
    }

    /**
     * Minimal implementation of {@link ActivityLifecycleDispatcher} for tests, supporting
     * registration of lifecycle observers and dispatching {@code onSaveInstanceState}.
     */
    private static class TestActivityLifecycleDispatcher implements ActivityLifecycleDispatcher {
        private final Activity mActivity;
        private final ObserverList<LifecycleObserver> mObservers = new ObserverList<>();

        public TestActivityLifecycleDispatcher(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void register(LifecycleObserver observer) {
            mObservers.addObserver(observer);
        }

        @Override
        public void unregister(LifecycleObserver observer) {
            mObservers.removeObserver(observer);
        }

        @Override
        public int getCurrentActivityState() {
            return ActivityState.CREATED_WITH_NATIVE;
        }

        @Override
        public boolean isNativeInitializationFinished() {
            return true;
        }

        @Override
        public boolean isActivityFinishingOrDestroyed() {
            return mActivity.isFinishing() || mActivity.isDestroyed();
        }

        public void dispatchOnSaveInstanceState(Bundle outState) {
            for (LifecycleObserver observer : mObservers) {
                if (observer instanceof SaveInstanceStateObserver saveObserver) {
                    saveObserver.onSaveInstanceState(outState);
                }
            }
        }
    }
}
