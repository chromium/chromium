// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests for {@link LocaleManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocaleManagerTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public @Rule TestRule mProcessor = new Features.JUnitProcessor();

    private @Mock TemplateUrl mMockTemplateUrl;

    @BeforeClass
    public static void setUpClass() throws ExecutionException {
        // Launch any activity as an Activity ref is required to attempt to show the activity.
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        sActivityTestRule.waitForDeferredStartup();
    }

    @After
    public void tearDown() {
        sActivityTestRule.getActivity().getModalDialogManager().dismissAllDialogs(
                DialogDismissalCause.UNKNOWN);
    }

    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    @SmallTest
    @Test
    public void testShowSearchEnginePromoDseDisabled() throws Exception {
        final CallbackHelper getShowTypeCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
                    @Override
                    public int getSearchEnginePromoShowType() {
                        getShowTypeCallback.notifyCalled();
                        return SearchEnginePromoType.DONT_SHOW;
                    }
                }));

        final CallbackHelper searchEnginesFinalizedCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                                sActivityTestRule.getActivity(), result -> {
                                    Assert.assertTrue(result);
                                    searchEnginesFinalizedCallback.notifyCalled();
                                }));
        searchEnginesFinalizedCallback.waitForCallback(0);
        Assert.assertEquals(0, getShowTypeCallback.getCallCount());
    }

    @Test
    @MediumTest
    public void testShowSearchEnginePromoIfNeeded_ForExisting() throws ExecutionException {
        final CallbackHelper searchEnginesFinalizedCallback = new CallbackHelper();
        final List<TemplateUrl> fakeTemplateUrls = List.of(mMockTemplateUrl);

        // Override the LocaleManagerDelegate to bypass the logic determining which type of promo
        // to show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
                    @Override
                    public int getSearchEnginePromoShowType() {
                        return SearchEnginePromoType.SHOW_EXISTING;
                    }

                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(
                            @SearchEnginePromoType int promoType) {
                        assertEquals(promoType, SearchEnginePromoType.SHOW_EXISTING);
                        return fakeTemplateUrls;
                    }
                }));

        // Trigger the dialog.
        DefaultSearchEnginePromoDialog dialog = TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                    sActivityTestRule.getActivity(),
                    unused -> searchEnginesFinalizedCallback.notifyCalled());
            return DefaultSearchEnginePromoDialog.getCurrentDialog();
        });
        CriteriaHelper.pollUiThread(dialog::isShowing);

        // searchEnginesFinalizedCallback should not have been called yet
        assertEquals(0, searchEnginesFinalizedCallback.getCallCount());

        // Act on the dialog and verify that it propagates to searchEnginesFinalizedCallback.
        TestThreadUtils.runOnUiThreadBlocking(dialog::dismiss);

        // We are waiting for the dialog to close and for the closure to propagate to the helper,
        // hence checking for when the currentDialog becomes null instead of just dialog dismissal.
        // This allows to make sure the callback has a chance to be called.
        CriteriaHelper.pollUiThread(
                () -> DefaultSearchEnginePromoDialog.getCurrentDialog() == null);

        assertEquals(1, searchEnginesFinalizedCallback.getCallCount());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.SEARCH_ENGINE_CHOICE})
    public void testShowSearchEnginePromoIfNeeded_ForWaffle() throws Exception {
        final CallbackHelper searchEnginesFinalizedCallback = new CallbackHelper();
        final List<TemplateUrl> fakeTemplateUrls = List.of(mMockTemplateUrl);

        // Override the LocaleManagerDelegate to bypass the logic determining which type of promo
        // to show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
                    @Override
                    public int getSearchEnginePromoShowType() {
                        return SearchEnginePromoType.SHOW_WAFFLE;
                    }

                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(
                            @SearchEnginePromoType int promoType) {
                        assertEquals(promoType, SearchEnginePromoType.SHOW_WAFFLE);
                        return fakeTemplateUrls;
                    }
                }));

        // Trigger the dialog.
        ModalDialogManager modalDialogManager = TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().getModalDialogManager());
        assertNotNull(modalDialogManager);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                                sActivityTestRule.getActivity(),
                                unused -> searchEnginesFinalizedCallback.notifyCalled()));
        CriteriaHelper.pollUiThread(modalDialogManager::isShowing);

        // searchEnginesFinalizedCallback should not have been called yet
        assertEquals(0, searchEnginesFinalizedCallback.getCallCount());

        // Act on the dialog and verify that it propagates to searchEnginesFinalizedCallback.
        // TODO(b/280753530): Update with the actual UI and use espresso to click the buttons.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> modalDialogManager.dismissAllDialogs(
                                DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE));
        assertEquals(1, searchEnginesFinalizedCallback.getCallCount());
    }
}
