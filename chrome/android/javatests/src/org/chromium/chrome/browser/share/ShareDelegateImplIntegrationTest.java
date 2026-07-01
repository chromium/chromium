// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareSheetDelegate;
import org.chromium.chrome.browser.share.android_share_sheet.TabGroupSharingController;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Supplier;

/** Integration tests for the Share Menu handling. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareDelegateImplIntegrationTest {
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Test
    @SmallTest
    public void testShareVisibleUrl() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);

        // We expect to share the visible URL (httpsCanonicalUrl), NOT the canonical URL.
        verifyShareUrl(httpsCanonicalUrl, httpsCanonicalUrl);
    }

    private void verifyShareUrl(String pageUrl, String expectedShareUrl)
            throws IllegalArgumentException, TimeoutException {
        mActivityTestRule.loadUrl(pageUrl);
        ShareParams params = triggerShare();
        Assert.assertTrue(params.getTextAndUrl().contains(expectedShareUrl));
    }

    private ShareParams triggerShare() throws TimeoutException {
        final CallbackHelper helper = new CallbackHelper();
        final AtomicReference<ShareParams> paramsRef = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShareSheetDelegate delegate =
                            new ShareSheetDelegate() {
                                @Override
                                void share(
                                        ShareParams params,
                                        ChromeShareExtras chromeShareParams,
                                        BottomSheetController controller,
                                        ActivityLifecycleDispatcher lifecycleDispatcher,
                                        Supplier<Tab> tabProvider,
                                        Supplier<TabModelSelector> tabModelSelectorProvider,
                                        Profile profileSupplier,
                                        Callback<Tab> printCallback,
                                        TabGroupSharingController tabGroupSharingController,
                                        int shareOrigin,
                                        long shareStartTime,
                                        boolean sharingHubEnabled,
                                        SigninAndHistorySyncActivityLauncher
                                                signinAndHistorySyncActivityLauncher,
                                        ActivityResultTracker activityResultTracker,
                                        MonotonicObservableSupplier<ModalDialogManager>
                                                modalDialogManagerSupplier,
                                        SnackbarManager snackbarManager) {
                                    paramsRef.set(params);
                                    helper.notifyCalled();
                                }
                            };

                    new ShareDelegateImpl(
                                    mActivityTestRule.getActivity(),
                                    mActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting()
                                            .getBottomSheetController(),
                                    mActivityTestRule.getActivity().getLifecycleDispatcher(),
                                    mActivityTestRule.getActivity().getActivityTabProvider(),
                                    mActivityTestRule.getActivity().getTabModelSelectorSupplier(),
                                    () -> mActivityTestRule.getProfile(false),
                                    delegate,
                                    false,
                                    mActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting()
                                            .getDataSharingTabManager(),
                                    SigninAndHistorySyncActivityLauncherImpl.get(),
                                    mActivityTestRule.getActivity().getActivityResultTracker(),
                                    mActivityTestRule.getActivity().getModalDialogManagerSupplier(),
                                    mActivityTestRule.getActivity().getSnackbarManager())
                            .share(
                                    mActivityTestRule.getActivity().getActivityTab(),
                                    false,
                                    /* shareOrigin= */ 0);
                });
        helper.waitForCallback(0);
        return paramsRef.get();
    }
}
