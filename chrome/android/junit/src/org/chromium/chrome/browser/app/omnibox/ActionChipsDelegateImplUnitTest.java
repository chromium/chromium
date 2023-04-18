// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.Nullable;

import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.HistoryClustersAction;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionType;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.components.omnibox.action.OmniboxPedalType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Set;

/**
 * Tests for {@link ActionChipsDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowLog.class, ShadowLooper.class,
                ActionChipsDelegateImplUnitTest.ShadowPasswordManagerLauncher.class})
public class ActionChipsDelegateImplUnitTest {
    /** Shadow PasswordManagerLauncher, used to capture Password Manager launch events. */
    @Implements(PasswordManagerLauncher.class)
    public static class ShadowPasswordManagerLauncher {
        public static boolean sPasswordSettingsRequested;

        @Implementation
        public static void showPasswordSettings(Activity activity, int referrer,
                ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
                boolean managePasskeys) {
            sPasswordSettingsRequested = true;
        }

        public static void reset() {
            sPasswordSettingsRequested = false;
        }
    }

    /** List of all supported OmniboxPedalTypes. */
    public static final Set<Integer> SUPPORTED_PEDALS = ImmutableSet.of(
            OmniboxPedalType.CLEAR_BROWSING_DATA, OmniboxPedalType.MANAGE_PASSWORDS,
            OmniboxPedalType.UPDATE_CREDIT_CARD, OmniboxPedalType.LAUNCH_INCOGNITO,
            OmniboxPedalType.RUN_CHROME_SAFETY_CHECK, OmniboxPedalType.MANAGE_SITE_SETTINGS,
            OmniboxPedalType.MANAGE_CHROME_SETTINGS, OmniboxPedalType.VIEW_CHROME_HISTORY,
            OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY, OmniboxPedalType.PLAY_CHROME_DINO_GAME);

    /** List of all supported OmniboxActionTypes. */
    public static final Set<Integer> SUPPORTED_ACTIONS =
            ImmutableSet.of(OmniboxActionType.PEDAL, OmniboxActionType.HISTORY_CLUSTERS);

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock HistoryClustersCoordinator mHistoryClustersCoordinator;
    private @Mock Tab mTab;

    private ShadowActivity mShadowActivity;
    private ShadowLooper mShadowLooper;
    private OneshotSupplierImpl<HistoryClustersCoordinator> mHistoryClustersCoordinatorSupplier;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ActionChipsDelegate mDelegate;

    @Before
    public void setUp() {
        var activity = Robolectric.buildActivity(Activity.class).get();
        mShadowActivity = shadowOf(activity);
        mShadowLooper = ShadowLooper.shadowMainLooper();

        mHistoryClustersCoordinatorSupplier = new OneshotSupplierImpl<>();
        mTabSupplier = new ObservableSupplierImpl<>();

        mDelegate = new ActionChipsDelegateImpl(
                activity, mHistoryClustersCoordinatorSupplier, null, mTabSupplier);
    }

    @After
    public void cleanUp() {
        // Other than tests that verify this value (and reset it to its original state) no other
        // tests should ever trigger PasswordManager.
        assertFalse(ShadowPasswordManagerLauncher.sPasswordSettingsRequested);
        // Other than tests that verify an intent being started (and remove it from the queue) no
        // other tests should ever invoke intents.
        assertNull(mShadowActivity.getNextStartedActivity());
        // Other than tests that interact with mHistoryClustersCoordinator (and confirm appropriate
        // calls to be made) no other tests should interact with this instance.
        verifyNoMoreInteractions(mHistoryClustersCoordinator);
    }

    /**
     * Confirm that an intent has been emitted to start a particular Fragment of the Search
     * activity.
     *
     * @param fragmentClass When specified, expect particular settings fragment to be requested.
     */
    private void checkSettingsActivityFragmentStarted(@Nullable Class fragmentClass) {
        var intent = mShadowActivity.getNextStartedActivity();
        assertEquals(SettingsActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(fragmentClass == null ? null : fragmentClass.getName(),
                intent.getStringExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT));
    }

    /**
     * Verify that a histogram recording the use of particular type of OmniboxPedal has been
     * recorded.
     *
     * @param type The type of Pedal to check for.
     */
    private void checkOmniboxPedalUsageRecorded(@OmniboxPedalType int type) {
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestionUsed.Pedal", type));
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting("Omnibox.SuggestionUsed.Pedal"));
    }

    /**
     * Create Omnibox Pedal action of a given type.
     */
    private OmniboxAction buildPedal(@OmniboxPedalType int type) {
        return new OmniboxPedal("hint", type);
    }

    /**
     * Create HistoryCluster Action with a supplied query.
     */
    private OmniboxAction buildHistoryClustersAction(String query) {
        return new HistoryClustersAction("hint", query);
    }

    @Test
    public void executePedal_manageChromeSettings() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_CHROME_SETTINGS));
        checkSettingsActivityFragmentStarted(null);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_CHROME_SETTINGS);
    }

    @Test
    public void executePedal_clearBrowsingData() {
        mDelegate.execute(buildPedal(OmniboxPedalType.CLEAR_BROWSING_DATA));
        checkSettingsActivityFragmentStarted(ClearBrowsingDataTabsFragment.class);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.CLEAR_BROWSING_DATA);
    }

    @Test
    public void executePedal_managePasswords() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_PASSWORDS));
        assertTrue(ShadowPasswordManagerLauncher.sPasswordSettingsRequested);
        ShadowPasswordManagerLauncher.reset();
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_PASSWORDS);
    }

    @Test
    public void executePedal_updateCreditCard() {
        mDelegate.execute(buildPedal(OmniboxPedalType.UPDATE_CREDIT_CARD));
        checkSettingsActivityFragmentStarted(AutofillPaymentMethodsFragment.class);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.UPDATE_CREDIT_CARD);
    }

    @Test
    public void executePedal_runChromeSafetyCheck() {
        mDelegate.execute(buildPedal(OmniboxPedalType.RUN_CHROME_SAFETY_CHECK));
        checkSettingsActivityFragmentStarted(SafetyCheckSettingsFragment.class);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.RUN_CHROME_SAFETY_CHECK);
    }

    @Test
    public void executePedal_manageSiteSettings() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_SITE_SETTINGS));
        checkSettingsActivityFragmentStarted(SiteSettings.class);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_SITE_SETTINGS);
    }

    @Test
    public void executePedal_manageChromeAccessibility() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY));
        checkSettingsActivityFragmentStarted(AccessibilitySettings.class);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY);
    }

    @Test
    public void executePedal_launchIncognito_fromCustomActivity() {
        mDelegate.execute(buildPedal(OmniboxPedalType.LAUNCH_INCOGNITO));

        var intent = mShadowActivity.getNextStartedActivity();
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertTrue(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));

        checkOmniboxPedalUsageRecorded(OmniboxPedalType.LAUNCH_INCOGNITO);
    }

    @Test
    public void executePedal_viewChromeHistory_fromCustomActivity() {
        mDelegate.execute(buildPedal(OmniboxPedalType.VIEW_CHROME_HISTORY));

        var intent = mShadowActivity.getNextStartedActivity();
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.HISTORY_URL, intent.getDataString());
        assertTrue(
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.VIEW_CHROME_HISTORY);
    }

    @Test
    public void executePedal_viewChromeHistory_fromTabbedActivity() {
        mTabSupplier.set(mTab);
        mDelegate.execute(buildPedal(OmniboxPedalType.VIEW_CHROME_HISTORY));

        var loadParamsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab, times(1)).loadUrl(loadParamsCaptor.capture());

        var loadUrlParams = loadParamsCaptor.getValue();
        assertNotNull(loadUrlParams);
        assertEquals(UrlConstants.HISTORY_URL, loadUrlParams.getUrl());
    }

    @Test
    public void executePedal_playChromeDinoGame_fromCustomActivity() {
        mDelegate.execute(buildPedal(OmniboxPedalType.PLAY_CHROME_DINO_GAME));

        var intent = mShadowActivity.getNextStartedActivity();
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.CHROME_DINO_URL, intent.getDataString());
        assertTrue(
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));

        checkOmniboxPedalUsageRecorded(OmniboxPedalType.PLAY_CHROME_DINO_GAME);
    }

    @Test
    public void executeNonPedal_historyClusters_noCoordinator() {
        String testJourneyName = "example journey name";
        mDelegate.execute(buildHistoryClustersAction(testJourneyName));
        verifyNoMoreInteractions(mHistoryClustersCoordinator);
    }

    @Test
    public void executeNonPedal_historyClusters_withCoordinator() {
        String testJourneyName = "example journey name";

        mHistoryClustersCoordinatorSupplier.set(mHistoryClustersCoordinator);
        mShadowLooper.runToEndOfTasks();

        mDelegate.execute(buildHistoryClustersAction(testJourneyName));
        verify(mHistoryClustersCoordinator).openHistoryClustersUi(testJourneyName);
    }
}
