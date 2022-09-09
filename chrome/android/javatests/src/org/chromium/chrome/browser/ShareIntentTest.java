// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.view.Window;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.jank_tracker.DummyJankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;

/**
 * Instrumentation tests for Share intents.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareIntentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TAG = "ShareIntentTest";

    /**
     * Mock activity class that overrides the startActivity and checks if the file passed in the
     * intent can be opened.
     *
     * This class is a wrapper around the actual activity of the test, while it also inherits from
     * activity and redirects the calls to the methods to the actual activity.
     */
    private static class MockChromeActivity extends ChromeTabbedActivity {
        private final Object mLock = new Object();
        private boolean mCheckCompleted;
        private ChromeActivity mActivity;

        public MockChromeActivity(ChromeActivity activity) {
            mActivity = activity;
            mCheckCompleted = false;
        }

        @Override
        public String getPackageName() {
            return mActivity.getPackageName();
        }

        @Override
        public Tab getActivityTab() {
            return mActivity.getActivityTab();
        }

        @Override
        public PackageManager getPackageManager() {
            return mActivity.getPackageManager();
        }

        @Override
        public Window getWindow() {
            return mActivity.getWindow();
        }

        @Override
        public ActivityTabProvider getActivityTabProvider() {
            return mActivity.getActivityTabProvider();
        }

        @Override
        public TabModelSelector getTabModelSelector() {
            // TabModelSelector remains uninitialized for this test. Return a mock instead.
            return new MockTabModelSelector(1, 0, null);
        }

        @Override
        public ObservableSupplier<ShareDelegate> getShareDelegateSupplier() {
            return mActivity.getShareDelegateSupplier();
        }

        @Override
        public Object getSystemService(String name) {
            // Prevents a scenario where InputMethodManager#hideSoftInput()
            // gets called before Activity#onCreate() gets called in this test.
            return name.equals(Context.INPUT_SERVICE) ? null : mActivity.getSystemService(name);
        }

        @Override
        public String getSystemServiceName(Class<?> serviceClass) {
            return mActivity.getSystemServiceName(serviceClass);
        }

        @Override
        public Resources getResources() {
            return mActivity.getResources();
        }

        @Override
        public Resources.Theme getTheme() {
            return mActivity.getTheme();
        }
    }

    @Test
    @LargeTest
    public void testShareIntent() throws ExecutionException, InterruptedException {
        MockChromeActivity mockActivity = TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Sets a test component as last shared and "shareDirectly" option is set so that
            // the share selector menu is not opened. The start activity is overridden, so the
            // package and class names do not matter.
            return new MockChromeActivity(mActivityTestRule.getActivity());
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowserControlsManager browserControlsManager = new BrowserControlsManager(
                    mockActivity, BrowserControlsManager.ControlsPosition.TOP);
            RootUiCoordinator rootUiCoordinator = new RootUiCoordinator(mockActivity, null,
                    mockActivity.getShareDelegateSupplier(), mockActivity.getActivityTabProvider(),
                    null, null, null, null, null, new OneshotSupplierImpl<>(),
                    new OneshotSupplierImpl<>(), new OneshotSupplierImpl<>(),
                    new OneshotSupplierImpl<>(),
                    ()
                            -> null,
                    browserControlsManager, mActivityTestRule.getActivity().getWindowAndroid(),
                    new DummyJankTracker(), mockActivity.getLifecycleDispatcher(),
                    mockActivity.getLayoutManagerSupplier(),
                    /* menuOrKeyboardActionController= */ mockActivity,
                    mockActivity::getActivityThemeColor,
                    mockActivity.getModalDialogManagerSupplier(),
                    /* appMenuBlocker= */ mockActivity, mockActivity::supportsAppMenu,
                    mockActivity::supportsFindInPage, mockActivity.getTabCreatorManagerSupplier(),
                    browserControlsManager.getFullscreenManager(),
                    mockActivity.getCompositorViewHolderSupplier(),
                    mockActivity.getTabContentManagerSupplier(), mockActivity::getSnackbarManager,
                    mockActivity.getActivityType(), mockActivity::isInOverviewMode,
                    mockActivity::isWarmOnResume,
                    /* appMenuDelegate= */ mockActivity,
                    /* statusBarColorProvider= */ mockActivity,
                    mockActivity.getIntentRequestTracker(), new OneshotSupplierImpl<>(),
                    new ObservableSupplierImpl<>(), false, null);

            ShareHelper.setLastShareComponentName(
                    null, new ComponentName("test.package", "test.activity"));

            WindowAndroid window = new WindowAndroid(mActivityTestRule.getActivity()) {
                @Override
                public WeakReference<Activity> getActivity() {
                    return new WeakReference<>(mockActivity);
                }
            };
            mockActivity.getActivityTab().updateAttachment(window, null);
            rootUiCoordinator.onShareMenuItemSelected(
                    true /* shareDirectly */, false /* isIncognito */);

            ShareHelper.setLastShareComponentName(null, new ComponentName("", ""));
            mockActivity.getActivityTab().updateAttachment(null, null);
            window.destroy();
        });
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
