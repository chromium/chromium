// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.Drawable;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Unit tests for {@link GlicActionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicActionCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Tab mIncognitoTab;
    @Mock private Tab mNtpTab;
    @Mock private GlicButtonDelegate mToggleGlicCallback;
    @Mock private Profile mProfile;
    private Activity mActivity;
    @Mock private Supplier<ChromeAndroidTask> mTaskSupplier;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private Supplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private ActorKeyedService mActorService;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;
    @Mock private UserEducationHelper mUserEducationHelper;

    private ActionRegistry mActionRegistry;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private GlicActionCoordinator mCoordinator;
    private PropertyModel mActionModel;

    @Before
    public void setUp() {
        ActorKeyedServiceFactory.setForTesting(mActorService);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        TrackerFactory.setTrackerForTests(mTracker);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        mActionRegistry = new ActionRegistry();
        mTabSupplier = ObservableSuppliers.createNullable();
        mActionModel = new PropertyModel.Builder(GlicActionProperties.ALL_KEYS).build();
        mActionRegistry.register(ActionId.GLIC, mActionModel);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getProfile()).thenReturn(mProfile);

        when(mIncognitoTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mIncognitoTab.isOffTheRecord()).thenReturn(true);
        when(mIncognitoTab.getProfile()).thenReturn(mProfile);

        when(mNtpTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mNtpTab.isOffTheRecord()).thenReturn(false);
        when(mNtpTab.getProfile()).thenReturn(mProfile);

        mCoordinator =
                new GlicActionCoordinator(
                        mActivity,
                        mActionRegistry,
                        mToggleGlicCallback,
                        mTabSupplier,
                        mTaskSupplier,
                        mBrowserControlsVisibilityManager,
                        mTabModelSelectorSupplier,
                        mSnackbarManager,
                        mUserEducationHelper);

        mTabSupplier.set(mTab);
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testInitialization_setsCallback() {
        assertNotNull(mActionModel.get(ActionProperties.ON_PRESS_CALLBACK));
    }

    @Test
    public void testClick_callsToggle() {
        Callback<View> callback = mActionModel.get(ActionProperties.ON_PRESS_CALLBACK);
        callback.onResult(null);
        verify(mToggleGlicCallback)
                .onClick(false, GlicKeyedService.GlicInvocationSource.TOOLBAR_BUTTON);
    }

    @Test
    public void testState_Default() {
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testState_Incognito() {
        mTabSupplier.set(mIncognitoTab);
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
        assertTrue(mActionModel.get(ActionProperties.ON_PRESS_CALLBACK) != null);
    }

    @Test
    public void testClick_Incognito_showsSnackbar() {
        mTabSupplier.set(mIncognitoTab);
        Callback<View> callback = mActionModel.get(ActionProperties.ON_PRESS_CALLBACK);
        assertTrue(callback != null);
        callback.onResult(null);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testState_Ntp() {
        mTabSupplier.set(mNtpTab);
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testState_NullTab() {
        mTabSupplier.set(null);
        assertEquals(ButtonState.UNCLICKABLE, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testTabTransition_updatesState() {
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
        Callback<View> normalCallback = mActionModel.get(ActionProperties.ON_PRESS_CALLBACK);
        assertTrue(normalCallback != null);

        mTabSupplier.set(mIncognitoTab);

        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
        Callback<View> incognitoCallback = mActionModel.get(ActionProperties.ON_PRESS_CALLBACK);
        assertTrue(incognitoCallback != null);
        assertTrue(normalCallback != incognitoCallback);
    }

    @Test
    public void testUrlUpdate_updatesState() {
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        observer.onUrlUpdated(mTab);

        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testOnStateChanged_updatesDrawable_allStates() {
        mCoordinator.onStateChanged(
                GlicButtonStateController.ButtonState.DEFAULT, /* isPanelOpen= */ false);
        Drawable defaultClosed = mActionModel.get(GlicActionProperties.GLIC_DRAWABLE);
        assertNotNull(defaultClosed);
        assertEquals(false, mActionModel.get(ActionProperties.IS_SELECTED));

        mCoordinator.onStateChanged(
                GlicButtonStateController.ButtonState.DEFAULT, /* isPanelOpen= */ true);
        Drawable defaultOpen = mActionModel.get(GlicActionProperties.GLIC_DRAWABLE);
        assertNotNull(defaultOpen);
        assertNotEquals(defaultClosed, defaultOpen);
        assertEquals(true, mActionModel.get(ActionProperties.IS_SELECTED));

        mCoordinator.onStateChanged(
                GlicButtonStateController.ButtonState.WORKING, /* isPanelOpen= */ true);
        Drawable working = mActionModel.get(GlicActionProperties.GLIC_DRAWABLE);
        assertNotNull(working);
        assertNotEquals(working, defaultOpen);

        mCoordinator.onStateChanged(
                GlicButtonStateController.ButtonState.NEEDS_REVIEW, /* isPanelOpen= */ true);
        Drawable needsReview = mActionModel.get(GlicActionProperties.GLIC_DRAWABLE);
        assertNotNull(needsReview);
        assertNotEquals(needsReview, working);

        mCoordinator.onStateChanged(
                GlicButtonStateController.ButtonState.DONE, /* isPanelOpen= */ true);
        Drawable done = mActionModel.get(GlicActionProperties.GLIC_DRAWABLE);
        assertNotNull(done);
        assertEquals(needsReview, done);
    }
}
