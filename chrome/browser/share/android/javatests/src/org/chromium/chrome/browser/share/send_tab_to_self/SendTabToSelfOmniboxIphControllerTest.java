// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Build;
import android.view.View;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link SendTabToSelfOmniboxIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class SendTabToSelfOmniboxIphControllerTest {
    private static final String HTTP_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private static final int TAB_ID = 1;
    private static final int OTHER_TAB_ID = 2;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mBridgeMock;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private Tab mOtherTab;
    @Mock private NavigationHandle mNavigation;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private Activity mActivity;
    private View mAnchorView;
    private SendTabToSelfOmniboxIphController mController;

    @Before
    public void setUp() {
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mBridgeMock);
        // Boxed Integer: an unstubbed call would otherwise return 0 (OFFER_FEATURE).
        doReturn(null).when(mBridgeMock).getEntryPointDisplayReason(any(), any());
        TrackerFactory.setTrackerForTests(mTracker);
        doReturn(true).when(mTracker).wouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);

        doReturn(mProfile).when(mProfile).getOriginalProfile();

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mAnchorView = new View(mActivity);
        mActivity.setContentView(mAnchorView);
        doReturn(TAB_ID).when(mTab).getId();
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
        doReturn(true).when(mTab).isUserInteractable();

        doReturn(OTHER_TAB_ID).when(mOtherTab).getId();
        doReturn(mProfile).when(mOtherTab).getProfile();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mOtherTab).getUrl();
        doReturn(true).when(mOtherTab).isUserInteractable();

        mTabSupplier.set(mTab);

        doAnswer(
                        invocation -> {
                            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
                            return null;
                        })
                .when(mTracker)
                .addOnInitializedCallback(any());

        mController = createController(mTabSupplier);
    }

    private SendTabToSelfOmniboxIphController createController(
            SettableNullableObservableSupplier<Tab> tabSupplier) {
        SendTabToSelfOmniboxIphController controller =
                SendTabToSelfOmniboxIphController.create(
                        mActivity, mProfile, tabSupplier, mAnchorView);
        assertNotNull(controller);
        return controller;
    }

    private void setDisplayReason(@Nullable Integer reason) {
        doReturn(reason).when(mBridgeMock).getEntryPointDisplayReason(eq(mProfile), eq(HTTP_URL));
    }

    private TabObserver captureObserver(Tab tab) {
        ArgumentCaptor<TabObserver> observer = ArgumentCaptor.forClass(TabObserver.class);
        verify(tab).addObserver(observer.capture());
        return observer.getValue();
    }

    private void assertNoIphOnTabSwitch() {
        mTabSupplier.set(mOtherTab);

        verify(mTracker, never()).shouldTriggerHelpUi(any());
        verify(mOtherTab, never()).removeObserver(any());
    }

    // --- Creation ---

    @Test
    public void testCreate_wouldNotTrigger_returnsNull() {
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);

        assertNull(
                SendTabToSelfOmniboxIphController.create(
                        mActivity, mProfile, mTabSupplier, mAnchorView));
    }

    // --- Display reason ---

    @Test
    public void testTabSwitch_nullDisplayReason_noIph() {
        setDisplayReason(null);
        assertNoIphOnTabSwitch();
    }

    @Test
    public void testTabSwitch_offerSignIn_noIph() {
        setDisplayReason(EntryPointDisplayReason.OFFER_SIGN_IN);
        assertNoIphOnTabSwitch();
    }

    @Test
    public void testTabSwitch_informNoTargetDevice_noIph() {
        setDisplayReason(EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE);
        assertNoIphOnTabSwitch();
    }

    // --- Tab switch triggers ---

    @Test
    public void testColdStart_noIph() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);

        // Initial interactable tab delivered at construction does not show IPH.
        mController.destroy();
        SendTabToSelfOmniboxIphController controller = createController(mTabSupplier);
        ShadowLooper.idleMainLooper();
        verify(mTracker, never()).shouldTriggerHelpUi(any());
        controller.destroy();

        // Starting with no tab (e.g. in the hub), the first tab to become interactable is recorded
        // without IPH, and a later switch shows it.
        Tab nonInteractableTab = mock(Tab.class);
        doReturn(3).when(nonInteractableTab).getId();
        doReturn(false).when(nonInteractableTab).isUserInteractable();

        SettableNullableObservableSupplier<Tab> supplier = ObservableSuppliers.createNullable();
        controller = createController(supplier);
        supplier.set(nonInteractableTab);
        TabObserver observer = captureObserver(nonInteractableTab);

        doReturn(true).when(nonInteractableTab).isUserInteractable();
        observer.onInteractabilityChanged(nonInteractableTab, true);
        verify(mTracker, never()).shouldTriggerHelpUi(any());

        supplier.set(mOtherTab);
        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
    }

    @Test
    public void testTabSwitch_showsIphAndStopsObserving() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);

        mTabSupplier.set(mOtherTab);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
        TabObserver observer = captureObserver(mOtherTab);
        verify(mOtherTab).removeObserver(observer);
    }

    @Test
    public void testTabSwitch_ineligibleTab_keepsObservingUntilEligibleTab() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        doReturn(JUnitTestGURLs.NTP_URL).when(mOtherTab).getUrl();

        // Switch to an ineligible tab (e.g. NTP with visible location bar on tablet).
        assertNoIphOnTabSwitch();

        // Switching back to an eligible tab shows IPH and stops observing.
        clearInvocations(mTab);
        mTabSupplier.set(mTab);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
        TabObserver observer = captureObserver(mTab);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testTabSwitch_fromHub_waitsForInteractableTab() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);

        // Enter the Hub: current tab is not interactable, supplier sets null.
        doReturn(false).when(mTab).isUserInteractable();
        mTabSupplier.set(null);

        // Select another tab in the Hub: tab is not interactable while the Hub is hiding.
        doReturn(false).when(mOtherTab).isUserInteractable();
        mTabSupplier.set(mOtherTab);
        TabObserver otherObserver = captureObserver(mOtherTab);

        verify(mTracker, never()).shouldTriggerHelpUi(any());
        verify(mOtherTab, never()).removeObserver(any());

        // Hub finishes hiding: tab becomes interactable.
        doReturn(true).when(mOtherTab).isUserInteractable();
        otherObserver.onInteractabilityChanged(mOtherTab, true);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
        verify(mOtherTab).removeObserver(otherObserver);
    }

    @Test
    public void testTabSwitch_fromHub_sameTab_noIph() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        TabObserver observer = captureObserver(mTab);

        // Enter the Hub (which detaches TabSupplierObserver from mTab) and return to the same tab.
        doReturn(false).when(mTab).isUserInteractable();
        mTabSupplier.set(null);
        clearInvocations(mTab);

        mTabSupplier.set(mTab);
        doReturn(true).when(mTab).isUserInteractable();
        observer.onInteractabilityChanged(mTab, true);

        verify(mTracker, never()).shouldTriggerHelpUi(any());
        verify(mTab).addObserver(observer);
        verify(mTab, never()).removeObserver(any());
    }

    // --- Navigation triggers ---

    @Test
    public void testUserNavigation_omnibox_showsIphAndStopsObserving() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        doReturn(true).when(mNavigation).hasCommitted();
        doReturn(false).when(mNavigation).isRendererInitiated();
        doReturn(false).when(mNavigation).hasUserGesture();
        doReturn(PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR)
                .when(mNavigation)
                .pageTransition();
        TabObserver observer = captureObserver(mTab);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testUserNavigation_linkWithGesture_showsIphAndStopsObserving() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        doReturn(true).when(mNavigation).hasCommitted();
        doReturn(true).when(mNavigation).isRendererInitiated();
        doReturn(true).when(mNavigation).hasUserGesture();
        doReturn(PageTransition.LINK).when(mNavigation).pageTransition();
        TabObserver observer = captureObserver(mTab);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testUserNavigation_browserInitiatedBookmark_showsIphAndStopsObserving() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        // NTP tiles, bookmarks and history: browser-initiated without a gesture.
        doReturn(true).when(mNavigation).hasCommitted();
        doReturn(false).when(mNavigation).isRendererInitiated();
        doReturn(false).when(mNavigation).hasUserGesture();
        doReturn(PageTransition.AUTO_BOOKMARK).when(mNavigation).pageTransition();
        TabObserver observer = captureObserver(mTab);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testNonUserNavigation_noIph() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        TabObserver observer = captureObserver(mTab);

        // Uncommitted navigation.
        doReturn(false).when(mNavigation).hasCommitted();
        doReturn(PageTransition.LINK).when(mNavigation).pageTransition();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        // Same-document navigation.
        doReturn(true).when(mNavigation).hasCommitted();
        doReturn(true).when(mNavigation).isSameDocument();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);
        doReturn(false).when(mNavigation).isSameDocument();

        // Reload keeping the original transition (e.g. NetErrorAutoReloader).
        doReturn(true).when(mNavigation).isReload();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);
        doReturn(false).when(mNavigation).isReload();

        // Killed-renderer reload or session restore of the startup tab.
        doReturn(PageTransition.RELOAD | PageTransition.FORWARD_BACK)
                .when(mNavigation)
                .pageTransition();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        // Cold-start load (launcher shortcut, widget, startup URLs).
        doReturn(PageTransition.AUTO_TOPLEVEL | PageTransition.CHAIN_START)
                .when(mNavigation)
                .pageTransition();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        // Intent load with FROM_API.
        doReturn(PageTransition.LINK | PageTransition.FROM_API).when(mNavigation).pageTransition();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        // Renderer-initiated without a gesture (e.g. client redirect).
        doReturn(PageTransition.LINK).when(mNavigation).pageTransition();
        doReturn(true).when(mNavigation).isRendererInitiated();
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        verify(mTracker, never()).shouldTriggerHelpUi(any());
        verify(mTab, never()).removeObserver(any());
    }

    @Test
    public void testUserNavigation_hiddenAnchor_noIph() {
        setDisplayReason(EntryPointDisplayReason.OFFER_FEATURE);
        doReturn(true).when(mNavigation).hasCommitted();
        TabObserver observer = captureObserver(mTab);
        mAnchorView.setVisibility(View.INVISIBLE);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);

        verify(mTracker, never()).shouldTriggerHelpUi(any());
        verify(mTab, never()).removeObserver(any());
    }

    // --- Lifecycle / Cleanup ---

    @Test
    public void testDestroy_stopsObservingTabs() {
        TabObserver observer = captureObserver(mTab);

        mController.destroy();
        mTabSupplier.set(mOtherTab);

        verify(mTab).removeObserver(observer);
        verify(mOtherTab, never()).addObserver(any());
    }
}
