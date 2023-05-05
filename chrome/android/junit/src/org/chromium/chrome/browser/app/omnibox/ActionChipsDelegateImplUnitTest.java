// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.EntityInfoProto;
import org.chromium.components.omnibox.action.HistoryClustersAction;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionInSuggest;
import org.chromium.components.omnibox.action.OmniboxActionType;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.components.omnibox.action.OmniboxPedalType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.Set;
import java.util.function.Consumer;

/**
 * Tests for {@link ActionChipsDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsDelegateImplUnitTest {
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
    private @Mock Consumer<String> mMockOpenUrl;
    private @Mock Consumer<String> mMockOpenHistoryClustersUi;
    private @Mock Runnable mMockOpenIncognitoPage;
    private @Mock Runnable mMockOpenPasswordSettings;
    private @Mock SettingsLauncher mMockSettingsLauncher;
    private @Mock Tab mTab;
    private @Mock Context mMockContext;
    private ArgumentCaptor<Intent> mIntentCaptor = ArgumentCaptor.forClass(Intent.class);

    private ShadowLooper mShadowLooper;
    private ActionChipsDelegate mDelegate;

    @Before
    public void setUp() {
        mShadowLooper = ShadowLooper.shadowMainLooper();

        mDelegate = new ActionChipsDelegateImpl(mMockContext,
                ()
                        -> mTab,
                mMockSettingsLauncher, mMockOpenUrl, mMockOpenIncognitoPage,
                mMockOpenPasswordSettings, mMockOpenHistoryClustersUi);

        doReturn(ContextUtils.getApplicationContext()).when(mMockContext).getApplicationContext();
        doReturn(ContextUtils.getApplicationContext().getPackageName())
                .when(mMockContext)
                .getPackageName();
        doReturn(true).when(mTab).isUserInteractable();
    }

    @After
    public void cleanUp() {
        verifyNoMoreInteractions(mMockOpenIncognitoPage);
        verifyNoMoreInteractions(mMockOpenPasswordSettings);
        verifyNoMoreInteractions(mMockOpenHistoryClustersUi);
        verifyNoMoreInteractions(mMockOpenUrl);
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
        verify(mMockSettingsLauncher).launchSettingsActivity(any(), eq(SettingsFragment.MAIN));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_CHROME_SETTINGS);
    }

    @Test
    public void executePedal_clearBrowsingData() {
        mDelegate.execute(buildPedal(OmniboxPedalType.CLEAR_BROWSING_DATA));
        verify(mMockSettingsLauncher)
                .launchSettingsActivity(any(), eq(SettingsFragment.CLEAR_BROWSING_DATA));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.CLEAR_BROWSING_DATA);
    }

    @Test
    public void executePedal_managePasswords() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_PASSWORDS));
        verify(mMockOpenPasswordSettings).run();
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_PASSWORDS);
    }

    @Test
    public void executePedal_updateCreditCard() {
        mDelegate.execute(buildPedal(OmniboxPedalType.UPDATE_CREDIT_CARD));
        verify(mMockSettingsLauncher)
                .launchSettingsActivity(any(), eq(SettingsFragment.PAYMENT_METHODS));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.UPDATE_CREDIT_CARD);
    }

    @Test
    public void executePedal_runChromeSafetyCheck() {
        mDelegate.execute(buildPedal(OmniboxPedalType.RUN_CHROME_SAFETY_CHECK));
        verify(mMockSettingsLauncher)
                .launchSettingsActivity(any(), eq(SettingsFragment.SAFETY_CHECK));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.RUN_CHROME_SAFETY_CHECK);
    }

    @Test
    public void executePedal_manageSiteSettings() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_SITE_SETTINGS));
        verify(mMockSettingsLauncher).launchSettingsActivity(any(), eq(SettingsFragment.SITE));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_SITE_SETTINGS);
    }

    @Test
    public void executePedal_manageChromeAccessibility() {
        mDelegate.execute(buildPedal(OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY));
        verify(mMockSettingsLauncher)
                .launchSettingsActivity(any(), eq(SettingsFragment.ACCESSIBILITY));
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY);
    }

    @Test
    public void executePedal_launchIncognito() {
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.LAUNCH_INCOGNITO));
        verify(mMockOpenIncognitoPage).run();
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.LAUNCH_INCOGNITO);
    }

    @Test
    public void executePedal_viewChromeHistory_nonInteractable() {
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.VIEW_CHROME_HISTORY));

        verify(mMockOpenUrl).accept(UrlConstants.HISTORY_URL);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.VIEW_CHROME_HISTORY);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.VIEW_CHROME_HISTORY);
    }

    @Test
    public void executePedal_viewChromeHistory() {
        doReturn(true).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.VIEW_CHROME_HISTORY));

        var loadParamsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab, times(1)).loadUrl(loadParamsCaptor.capture());

        var loadUrlParams = loadParamsCaptor.getValue();
        assertNotNull(loadUrlParams);
        assertEquals(UrlConstants.HISTORY_URL, loadUrlParams.getUrl());
    }

    @Test
    public void executePedal_playChromeDinoGame_nonInteractable() {
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.execute(buildPedal(OmniboxPedalType.PLAY_CHROME_DINO_GAME));
        verify(mMockOpenUrl).accept(UrlConstants.CHROME_DINO_URL);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.PLAY_CHROME_DINO_GAME);
    }

    @Test
    public void executeHistoryClusters() {
        String testJourneyName = "example journey name";
        mDelegate.execute(buildHistoryClustersAction(testJourneyName));
        verify(mMockOpenHistoryClustersUi).accept(testJourneyName);
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

        verify(mMockOpenUrl).accept(UrlConstants.CHROME_DINO_URL);

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
