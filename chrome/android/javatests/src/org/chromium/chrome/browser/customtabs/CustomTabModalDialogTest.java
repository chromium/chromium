// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.is;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.CCT_TAB_MODAL_DIALOG)
public class CustomTabModalDialogTest {

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (getActivity() == null) return;
                    AppMenuCoordinator coordinator =
                            mCustomTabActivityTestRule.getAppMenuCoordinator();
                    // CCT doesn't always have a menu (ex. in the media viewer).
                    if (coordinator == null) return;
                    AppMenuHandler handler = coordinator.getAppMenuHandler();
                    if (handler != null) handler.hideAppMenu();
                });
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1511082")
    public void testShowAndDismissTabModalDialog() throws InterruptedException {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        var visibilityDelegate =
                mCustomTabActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getAppBrowserControlsVisibilityDelegate();

        ModalDialogManager dialogManager =
                mCustomTabActivityTestRule.getActivity().getModalDialogManagerSupplier().get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel dialog =
                            new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                    .with(ModalDialogProperties.TITLE, "test")
                                    .with(
                                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                            context.getString(
                                                    org.chromium.chrome.test.R.string.delete))
                                    .with(
                                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                            context.getString(
                                                    org.chromium.chrome.test.R.string.cancel))
                                    .with(
                                            ModalDialogProperties.CONTROLLER,
                                            new ModalDialogProperties.Controller() {
                                                @Override
                                                public void onClick(
                                                        PropertyModel model, int buttonType) {}

                                                @Override
                                                public void onDismiss(
                                                        PropertyModel model, int dismissalCause) {}
                                            })
                                    .build();

                    dialogManager.showDialog(dialog, ModalDialogManager.ModalDialogType.TAB);
                });

        Assert.assertNotNull(visibilityDelegate.get());
        Assert.assertEquals(
                "Browser Control should be SHOWN when dialog is being displayed.",
                BrowserControlsState.SHOWN,
                (int) visibilityDelegate.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> dialogManager.dismissAllDialogs(DialogDismissalCause.DISMISSED_BY_NATIVE));

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertEquals(
                "Browser Control State should be BOTH when dialog becomes hidden.",
                BrowserControlsState.BOTH,
                (int) visibilityDelegate.get());
    }

    @Test
    @SmallTest
    public void testNavigationDismissTabModalDialog() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

        ModalDialogManager dialogManager =
                mCustomTabActivityTestRule.getActivity().getModalDialogManagerSupplier().get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel dialog =
                            new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                    .with(ModalDialogProperties.TITLE, "test")
                                    .with(
                                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                            context.getString(
                                                    org.chromium.chrome.test.R.string.delete))
                                    .with(
                                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                            context.getString(
                                                    org.chromium.chrome.test.R.string.cancel))
                                    .with(
                                            ModalDialogProperties.CONTROLLER,
                                            new ModalDialogProperties.Controller() {
                                                @Override
                                                public void onClick(
                                                        PropertyModel model, int buttonType) {}

                                                @Override
                                                public void onDismiss(
                                                        PropertyModel model, int dismissalCause) {}
                                            })
                                    .build();

                    dialogManager.showDialog(dialog, ModalDialogManager.ModalDialogType.TAB);
                });

        CriteriaHelper.pollUiThread(() -> dialogManager.isShowing());

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        Assert.assertTrue(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        CriteriaHelper.pollUiThread(() -> !dialogManager.isShowing());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressDismissTabModalDialog() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

        ModalDialogManager dialogManager =
                mCustomTabActivityTestRule.getActivity().getModalDialogManagerSupplier().get();

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel dialog =
                            new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                    .with(ModalDialogProperties.TITLE, "test")
                                    .with(
                                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                            context.getString(
                                                    org.chromium.chrome.test.R.string.delete))
                                    .with(
                                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                            context.getString(
                                                    org.chromium.chrome.test.R.string.cancel))
                                    .with(
                                            ModalDialogProperties.CONTROLLER,
                                            new ModalDialogProperties.Controller() {
                                                @Override
                                                public void onClick(
                                                        PropertyModel model, int buttonType) {}

                                                @Override
                                                public void onDismiss(
                                                        PropertyModel model, int dismissalCause) {}
                                            })
                                    .build();

                    dialogManager.showDialog(dialog, ModalDialogManager.ModalDialogType.TAB);
                });
        CriteriaHelper.pollUiThread(() -> dialogManager.isShowing(), "Dialog should be displayed");

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.Intercept",
                        BackPressManager.getHistogramValue(
                                BackPressHandler.Type.TAB_MODAL_HANDLER));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCustomTabActivityTestRule
                            .getActivity()
                            .getOnBackPressedDispatcher()
                            .onBackPressed();
                });

        Assert.assertTrue("Should be able to navigate back after navigation", tab.canGoBack());
        Assert.assertFalse("Should be unable to navigate forward", tab.canGoForward());
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tab should not be navigated when dialog is dismissed",
                            ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                            is(mTestPage2));
                });

        histogramWatcher.assertExpected("Dialog should be dismissed by back press");
        CriteriaHelper.pollUiThread(
                () -> !dialogManager.isShowing(), "Dialog should be dismissed by back press");
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressDismissTabModalDialog_BackGestureRefactor() {
        testBackPressDismissTabModalDialog();
    }
}
