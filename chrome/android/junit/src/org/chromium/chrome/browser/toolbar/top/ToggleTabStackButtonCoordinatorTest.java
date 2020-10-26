// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.intent.IntentMetadata;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for ToggleTabStackButtonCoordinator.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ToggleTabStackButtonCoordinatorTest.ShadowChromeFeatureList.class})
public class ToggleTabStackButtonCoordinatorTest {
    private static final IntentMetadata DEFAULT_INTENT_METADATA =
            new IntentMetadata(/*isMainIntent*/ true, /*isIntentWithEffect*/ false);

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static Map<String, String> sParamMap;
        @Implementation
        public static String getFieldTrialParamByFeature(String featureName, String paramName) {
            Assert.assertEquals("Wrong feature name", FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
                    featureName);
            if (sParamMap.containsKey(paramName)) return sParamMap.get(paramName);
            return "";
        }
    }

    @Mock
    private Context mContext;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    private ToggleTabStackButton mToggleTabStackButton;
    @Mock
    private android.content.res.Resources mResources;
    @Mock
    private UserEducationHelper mUserEducationHelper;
    @Mock
    private Callback<Boolean> mSetNewTabButtonHighlightCallback;

    @Captor
    private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private boolean mIsIncognito;
    private boolean mOverviewOpen;
    private final OneshotSupplierImpl<IntentMetadata> mIntentMetadataOneshotSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier =
            new OneshotSupplierImpl<>();
    private Set<OverviewModeBehavior.OverviewModeObserver> mOverviewModeObserverSet;

    private OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeBehaviorOneshotSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mContext.getResources()).thenReturn(mResources);
        doAnswer(invocation -> mOverviewOpen).when(mOverviewModeBehavior).overviewVisible();
        doAnswer(invocation -> {
            mOverviewModeObserverSet.add(invocation.getArgument(0));
            return null;
        })
                .when(mOverviewModeBehavior)
                .addOverviewModeObserver(any());
        doAnswer(invocation -> {
            mOverviewModeObserverSet.remove(invocation.getArgument(0));
            return null;
        })
                .when(mOverviewModeBehavior)
                .removeOverviewModeObserver(any());

        mOverviewModeObserverSet = new HashSet<>();
        mOverviewModeBehaviorOneshotSupplier = new OneshotSupplierImpl<>();

        // Defaults most test cases expect, can be overridden by each test though.
        when(mToggleTabStackButton.isShown()).thenReturn(true);
        ShadowChromeFeatureList.sParamMap = new HashMap<>();
        mIsIncognito = false;
    }

    private ToggleTabStackButtonCoordinator newToggleTabStackButtonCoordinator(
            ToggleTabStackButton toggleTabStackButton) {
        return new ToggleTabStackButtonCoordinator(mContext, toggleTabStackButton,
                mActivityTabProvider, mUserEducationHelper,
                ()
                        -> mIsIncognito,
                mIntentMetadataOneshotSupplier, mPromoShownOneshotSupplier,
                mOverviewModeBehaviorOneshotSupplier, mSetNewTabButtonHighlightCallback);
    }

    private void showOverviewMode() {
        mOverviewOpen = true;
        for (OverviewModeBehavior.OverviewModeObserver observer : mOverviewModeObserverSet) {
            observer.onOverviewModeStartedShowing(/*showToolbar*/ false);
        }
        for (OverviewModeBehavior.OverviewModeObserver observer : mOverviewModeObserverSet) {
            observer.onOverviewModeFinishedShowing();
        }
    }

    private void hideOverviewMode() {
        mOverviewOpen = false;
        for (OverviewModeBehavior.OverviewModeObserver observer : mOverviewModeObserverSet) {
            observer.onOverviewModeStartedHiding(/*showToolbar*/ false, /*delayAnimation*/ false);
        }
        for (OverviewModeBehavior.OverviewModeObserver observer : mOverviewModeObserverSet) {
            observer.onOverviewModeFinishedHiding();
        }
    }

    private IPHCommand verifyIphShown() {
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());
        reset(mUserEducationHelper);
        return mIPHCommandCaptor.getValue();
    }

    private void verifyIphNotShown() {
        verify(mUserEducationHelper, never()).requestShowIPH(any());
        reset(mUserEducationHelper);
    }

    private void verifyTabButtonHighlightChanged(boolean expectedHighlight) {
        verify(mToggleTabStackButton).setHighlightDrawable(expectedHighlight);
        reset(mToggleTabStackButton);
    }

    private void verifyTabButtonHighlightNotChanged() {
        verify(mToggleTabStackButton, never()).setHighlightDrawable(anyBoolean());
        reset(mToggleTabStackButton);
    }

    private void verifyNtpButtonHighlightChanged(boolean expectedHighlight) {
        verify(mSetNewTabButtonHighlightCallback).onResult(expectedHighlight);
        reset(mSetNewTabButtonHighlightCallback);
    }

    private void verifyNtpButtonHighlightNotChanged() {
        verify(mSetNewTabButtonHighlightCallback, never()).onResult(any());
        reset(mSetNewTabButtonHighlightCallback);
    }

    @Test
    public void testOverviewBehaviorAvailableDuringConstruction() {
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        Assert.assertEquals("Should have 1 overview observer", 1, mOverviewModeObserverSet.size());

        toggleTabStackButtonCoordinator.destroy();
        Assert.assertTrue("Should have no overview observers", mOverviewModeObserverSet.isEmpty());
    }

    @Test
    public void testOverviewBehaviorAvailableAfterDestroy() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        toggleTabStackButtonCoordinator.destroy();

        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        Assert.assertTrue("Should have no overview observers", mOverviewModeObserverSet.isEmpty());
    }

    @Test
    public void testDestroyDuringIph() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        Assert.assertEquals("Should have 1 overview observer", 1, mOverviewModeObserverSet.size());

        toggleTabStackButtonCoordinator.destroy();
        Assert.assertTrue("Should have no overview observers", mOverviewModeObserverSet.isEmpty());
    }

    @Test
    public void testIphAndOverviewHighlight() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();

        verifyTabButtonHighlightChanged(true);
        verifyNtpButtonHighlightNotChanged();

        showOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightChanged(true);

        iphCommand.onDismissCallback.run();
        verifyTabButtonHighlightChanged(false);
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightChanged(false);
    }

    @Test
    public void testDismissIphBeforeOverview() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        verifyTabButtonHighlightChanged(true);
        verifyNtpButtonHighlightNotChanged();

        iphCommand.onDismissCallback.run();
        verifyTabButtonHighlightChanged(false);
        verifyNtpButtonHighlightNotChanged();

        showOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightNotChanged();
    }

    @Test
    public void testOverviewModeEventsWithoutIph() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        showOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightNotChanged();
    }

    @Test
    public void testIphWithNoPageLoad() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        verifyIphNotShown();
    }

    @Test
    public void testIphWithNoViewButton() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ null);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();
    }

    @Test
    public void testIphWithNoOverviewModeBehavior() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        verifyTabButtonHighlightChanged(true);
        verifyNtpButtonHighlightNotChanged();

        showOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightNotChanged();

        iphCommand.onDismissCallback.run();
        verifyTabButtonHighlightChanged(false);
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        verifyTabButtonHighlightNotChanged();
        verifyNtpButtonHighlightNotChanged();
    }

    @Test
    public void testIphIncognito() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        mIsIncognito = true;
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mIsIncognito = false;
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphIsShown() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        when(mToggleTabStackButton.isShown()).thenReturn(false);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        when(mToggleTabStackButton.isShown()).thenReturn(true);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphMainIntentFalse() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(
                new IntentMetadata(/*isMainIntent*/ false, /*isIntentWithEffect*/ false));
        mPromoShownOneshotSupplier.set(false);

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME, "");
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME, "false");
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME, "true");
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();
    }

    @Test
    public void testIphIntentWithEffectTrue() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(
                new IntentMetadata(/*isMainIntent*/ true, /*isIntentWithEffect*/ true));
        mPromoShownOneshotSupplier.set(false);

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.INTENT_WITH_EFFECT_PARAM_NAME, "");
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.INTENT_WITH_EFFECT_PARAM_NAME, "false");
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.INTENT_WITH_EFFECT_PARAM_NAME, "true");
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphShowedPromo() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(true);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();
    }

    @Test
    public void testIphDelayedIntentMetadata() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphDelayedPromoShown() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/*view*/ mToggleTabStackButton);
        mOverviewModeBehaviorOneshotSupplier.set(mOverviewModeBehavior);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mPromoShownOneshotSupplier.set(false);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }
}
