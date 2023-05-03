// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.EntityInfoProto;
import org.chromium.components.omnibox.action.HistoryClustersAction;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionInSuggest;
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
        public static void showPasswordSettings(Context context, int referrer,
                Supplier<ModalDialogManager> modalDialogManagerSupplier, boolean managePasskeys) {
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
    private @Mock Context mMockContext;
    private ArgumentCaptor<Intent> mIntentCaptor = ArgumentCaptor.forClass(Intent.class);

    private ShadowLooper mShadowLooper;
    private OneshotSupplierImpl<HistoryClustersCoordinator> mHistoryClustersCoordinatorSupplier;
    private ActionChipsDelegate mDelegate;

    @Before
    public void setUp() {
        mShadowLooper = ShadowLooper.shadowMainLooper();

        mHistoryClustersCoordinatorSupplier = new OneshotSupplierImpl<>();

        mDelegate = new ActionChipsDelegateImpl(
                mMockContext, mHistoryClustersCoordinatorSupplier, () -> mTab);

        doReturn(ContextUtils.getApplicationContext()).when(mMockContext).getApplicationContext();
        doReturn(ContextUtils.getApplicationContext().getPackageName())
                .when(mMockContext)
                .getPackageName();
        doReturn(true).when(mTab).isUserInteractable();
    }

    @After
    public void cleanUp() {
        // Other than tests that verify this value (and reset it to its original state) no other
        // tests should ever trigger PasswordManager.
        assertFalse(ShadowPasswordManagerLauncher.sPasswordSettingsRequested);
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
        verify(mMockContext, times(1)).startActivity(mIntentCaptor.capture(), any());

        var intent = mIntentCaptor.getValue();
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

    /**
     * Create Action in Suggest with a supplied definition.
     */
    private OmniboxAction buildActionInSuggest(
            EntityInfoProto.ActionInfo.ActionType type, Intent intent) {
        var uri = intent.toUri(Intent.URI_INTENT_SCHEME);
        var action = EntityInfoProto.ActionInfo.newBuilder()
                             .setActionType(type)
                             .setActionUri(uri)
                             .build();

        return new OmniboxActionInSuggest("wink", action);
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
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.LAUNCH_INCOGNITO));

        verify(mMockContext, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertTrue(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));

        checkOmniboxPedalUsageRecorded(OmniboxPedalType.LAUNCH_INCOGNITO);
    }

    @Test
    public void executePedal_viewChromeHistory_fromCustomActivity() {
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.VIEW_CHROME_HISTORY));

        verify(mMockContext, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.HISTORY_URL, intent.getDataString());
        assertTrue(
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.VIEW_CHROME_HISTORY);
    }

    @Test
    public void executePedal_viewChromeHistory_fromTabbedActivity() {
        doReturn(true).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.VIEW_CHROME_HISTORY));

        var loadParamsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab, times(1)).loadUrl(loadParamsCaptor.capture());

        var loadUrlParams = loadParamsCaptor.getValue();
        assertNotNull(loadUrlParams);
        assertEquals(UrlConstants.HISTORY_URL, loadUrlParams.getUrl());
    }

    @Test
    public void executePedal_playChromeDinoGame_fromCustomActivity() {
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.PLAY_CHROME_DINO_GAME));

        verify(mMockContext, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.CHROME_DINO_URL, intent.getDataString());
        assertTrue(
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));

        checkOmniboxPedalUsageRecorded(OmniboxPedalType.PLAY_CHROME_DINO_GAME);
    }

    @Test
    public void executeHistoryClusters_noCoordinator() {
        String testJourneyName = "example journey name";
        mDelegate.execute(buildHistoryClustersAction(testJourneyName));
        verifyNoMoreInteractions(mHistoryClustersCoordinator);
    }

    @Test
    public void executeHistoryClusters_withCoordinator() {
        String testJourneyName = "example journey name";

        mHistoryClustersCoordinatorSupplier.set(mHistoryClustersCoordinator);
        mShadowLooper.runToEndOfTasks();

        mDelegate.execute(buildHistoryClustersAction(testJourneyName));
        verify(mHistoryClustersCoordinator).openHistoryClustersUi(testJourneyName);
    }

    @Test
    public void executeActionInSuggest_executeDirectionsWithMaps() {
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mTab).isIncognito();

        mDelegate.execute(buildActionInSuggest(EntityInfoProto.ActionInfo.ActionType.DIRECTIONS,
                new Intent("Magic Intent Action")));

        verify(mTab, times(1)).isIncognito();
        verify(mMockContext, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();

        assertEquals("Magic Intent Action", intent.getAction());

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        SuggestionsMetrics.ActionInSuggestIntentResult.SUCCESS));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void executeActionInSuggest_executeDirectionsInBrowserForIncognitoMode() {
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isIncognito();

        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        mDelegate.execute(
                buildActionInSuggest(EntityInfoProto.ActionInfo.ActionType.DIRECTIONS, intent));

        verify(mTab, times(1)).isUserInteractable();
        verify(mTab, times(1)).isIncognito();

        // Should not be recorded.
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult"));

        var loadParamsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab, times(1)).loadUrl(loadParamsCaptor.capture());

        var loadUrlParams = loadParamsCaptor.getValue();
        assertNotNull(loadUrlParams);
        assertEquals(UrlConstants.CHROME_DINO_URL, loadUrlParams.getUrl());
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void executeActionInSuggest_redirectDirectionsActionToLocalTabIfAvailable() {
        doReturn(true).when(mTab).isUserInteractable();

        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        // NOTE: the intent is serialized and deserialized. Can't directly check if instance is
        // same.
        doThrow(new ActivityNotFoundException()).when(mMockContext).startActivity(any());
        mDelegate.execute(
                buildActionInSuggest(EntityInfoProto.ActionInfo.ActionType.DIRECTIONS, intent));
        verify(mTab, times(1)).isUserInteractable();
        verify(mTab, times(1)).isIncognito();

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        SuggestionsMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));

        var loadParamsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab, times(1)).loadUrl(loadParamsCaptor.capture());

        var loadUrlParams = loadParamsCaptor.getValue();
        assertNotNull(loadUrlParams);
        assertEquals(UrlConstants.CHROME_DINO_URL, loadUrlParams.getUrl());
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void executeActionInSuggest_redirectDirectionsActionToRemoteTab() {
        doReturn(false).when(mTab).isUserInteractable();

        var intent = new Intent(Intent.ACTION_DIAL);
        intent.setClassName("no.such.package", ".");
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        // First intent is to DIAL with "no.such.package".
        doThrow(new ActivityNotFoundException())
                // Confirm the second intent asks to load webpage in Chrome.
                .doAnswer(inv -> {
                    Intent newIntent = inv.getArgument(0);
                    assertEquals(Intent.ACTION_VIEW, newIntent.getAction());
                    assertEquals(mMockContext.getPackageName(),
                            newIntent.getComponent().getPackageName());
                    assertEquals(UrlConstants.CHROME_DINO_URL, newIntent.getDataString());
                    return 0;
                })
                .when(mMockContext)
                .startActivity(any());

        mDelegate.execute(
                buildActionInSuggest(EntityInfoProto.ActionInfo.ActionType.DIRECTIONS, intent));

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        SuggestionsMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));
    }

    @Test
    public void executeActionInSuggest_executeCallActionWithDialer() {
        doReturn(true).when(mTab).isUserInteractable();
        mDelegate.execute(buildActionInSuggest(
                EntityInfoProto.ActionInfo.ActionType.CALL, new Intent(Intent.ACTION_CALL)));

        verify(mMockContext, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();
        // OBSERVE: We rewrite ACTION_CALL with ACTION_DIAL, which does not carry high permission
        // requirements.
        assertEquals(Intent.ACTION_DIAL, intent.getAction());

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        SuggestionsMetrics.ActionInSuggestIntentResult.SUCCESS));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void executeActionInSuggest_dontRedirectCallActionToLocalTab() {
        doReturn(true).when(mTab).isUserInteractable();

        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        // NOTE: the intent is serialized and deserialized. Can't directly check if instance is
        // same.
        doThrow(new ActivityNotFoundException()).when(mMockContext).startActivity(any());
        mDelegate.execute(buildActionInSuggest(EntityInfoProto.ActionInfo.ActionType.CALL, intent));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        SuggestionsMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void executeActionInSuggest_dontRedirectCallActionToRemoteTab() {
        doReturn(false).when(mTab).isUserInteractable();

        var intent = new Intent(Intent.ACTION_DIAL);
        intent.setClassName("no.such.package", ".");
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        // Keep throwing. Test should fail if we attempt to invoke intent to self.
        doThrow(new ActivityNotFoundException()).when(mMockContext).startActivity(any());
        mDelegate.execute(buildActionInSuggest(EntityInfoProto.ActionInfo.ActionType.CALL, intent));

        verifyNoMoreInteractions(mTab);

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        SuggestionsMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));
    }
}
