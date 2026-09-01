// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Build;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link SendTabToSelfOmniboxIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class SendTabToSelfOmniboxIphControllerTest {
    private static final String HTTP_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mBridgeMock;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;

    private Activity mActivity;
    private View mAnchorView;
    private @Nullable Tab mCurrentTab;
    private SendTabToSelfOmniboxIphController mController;

    @Before
    public void setUp() {
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mBridgeMock);
        TrackerFactory.setTrackerForTests(mTracker);

        doReturn(mProfile).when(mProfile).getOriginalProfile();

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mAnchorView = new View(mActivity);
        mCurrentTab = mTab;

        doReturn(mProfile).when(mTab).getProfile();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
        doAnswer(
                        invocation -> {
                            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
                            return null;
                        })
                .when(mTracker)
                .addOnInitializedCallback(any());

        mController =
                new SendTabToSelfOmniboxIphController(
                        mActivity, mProfile, () -> mCurrentTab, mAnchorView);
    }

    // --- Eligible & IPH Shown ---

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_eligible_modelAlreadyReady_showsIphImmediately() {
        doReturn(true).when(mBridgeMock).isModelReady(eq(mProfile));
        doReturn(EntryPointDisplayReason.OFFER_FEATURE)
                .when(mBridgeMock)
                .getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();

        // Model is already ready: no observer is registered, IPH is triggered immediately.
        verify(mBridgeMock, never()).addModelObserver(any(), any());
        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_eligible_modelNotReadyInitially_showsIphWhenReady() {
        doReturn(false).when(mBridgeMock).isModelReady(eq(mProfile));
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));
        doReturn(EntryPointDisplayReason.OFFER_FEATURE)
                .when(mBridgeMock)
                .getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();
        verify(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));

        // IPH is not triggered immediately before the model is ready.
        verify(mTracker, never()).shouldTriggerHelpUi(any());

        // Model becomes ready and triggers IPH.
        mController.onModelReady();
        verify(mBridgeMock).removeModelObserver(observerPtr);
        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
    }

    // --- Non-Eligible & Invalid State (No IPH Shown) ---

    @Test
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_notEligible_featureDisabled() {
        mController.maybeShowIph();
        verify(mBridgeMock, never()).addModelObserver(any(), any());
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_notEligible_invalidStateOrTab_noObserver() {
        mCurrentTab = null;
        mController.maybeShowIph();

        mCurrentTab = mTab;
        doReturn(true).when(mTab).isIncognito();
        mController.maybeShowIph();

        doReturn(false).when(mTab).isIncognito();
        doReturn(true).when(mTab).isDestroyed();
        mController.maybeShowIph();

        verify(mBridgeMock, never()).addModelObserver(any(), any());
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_notEligible_nullDisplayReason() {
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));
        doReturn(null).when(mBridgeMock).getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();
        mController.onModelReady();

        verify(mBridgeMock).removeModelObserver(observerPtr);
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_notEligible_offerSignIn() {
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));
        doReturn(EntryPointDisplayReason.OFFER_SIGN_IN)
                .when(mBridgeMock)
                .getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();
        mController.onModelReady();

        verify(mBridgeMock).removeModelObserver(observerPtr);
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testMaybeShowIph_notEligible_informNoTargetDevice() {
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));
        doReturn(EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE)
                .when(mBridgeMock)
                .getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();
        mController.onModelReady();

        verify(mBridgeMock).removeModelObserver(observerPtr);
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testOnModelReady_tabDestroyed_noIph() {
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));
        doReturn(EntryPointDisplayReason.OFFER_FEATURE)
                .when(mBridgeMock)
                .getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();

        // Tab is destroyed before model is ready.
        doReturn(true).when(mTab).isDestroyed();
        mController.onModelReady();

        verify(mBridgeMock).removeModelObserver(observerPtr);
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testOnModelReady_activityFinishing_noIph() {
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));
        doReturn(EntryPointDisplayReason.OFFER_FEATURE)
                .when(mBridgeMock)
                .getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));

        mController.maybeShowIph();

        // Activity is finishing before model is ready.
        mActivity.finish();
        mController.onModelReady();

        verify(mBridgeMock).removeModelObserver(observerPtr);
        verify(mTracker, never()).shouldTriggerHelpUi(any());
    }

    // --- Lifecycle / Cleanup ---

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_EXTRA_ENTRY_POINTS)
    public void testDestroy_removesModelObserver() {
        long observerPtr = 12345L;
        doReturn(observerPtr).when(mBridgeMock).addModelObserver(eq(mProfile), eq(mController));

        mController.maybeShowIph();
        mController.destroy();

        verify(mBridgeMock).removeModelObserver(observerPtr);
    }
}
