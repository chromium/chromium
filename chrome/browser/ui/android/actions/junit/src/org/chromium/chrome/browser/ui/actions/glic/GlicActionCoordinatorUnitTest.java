// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
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
    @Mock private Activity mActivity;
    @Mock private Supplier<ChromeAndroidTask> mTaskSupplier;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private Supplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private ActorKeyedService mActorService;
    @Mock private GlicKeyedService mGlicKeyedService;

    private ActionRegistry mActionRegistry;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private GlicActionCoordinator mCoordinator;
    private PropertyModel mActionModel;

    @Before
    public void setUp() {
        ActorKeyedServiceFactory.setForTesting(mActorService);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

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
                        mTabModelSelectorSupplier);

        mTabSupplier.set(mTab);
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testInitialization_setsCallback() {
        assertNotNull(mActionModel.get(ActionProperties.ON_PRESS_CALLBACK));
    }

    @Test
    public void testClick_callsToggle() {
        Callback<android.view.View> callback = mActionModel.get(ActionProperties.ON_PRESS_CALLBACK);
        callback.onResult(null);
        verify(mToggleGlicCallback).onClick(false);
    }

    @Test
    public void testState_Default() {
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testState_Incognito() {
        mTabSupplier.set(mIncognitoTab);
        assertEquals(ButtonState.UNCLICKABLE, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testState_Ntp() {
        mTabSupplier.set(mNtpTab);
        assertEquals(ButtonState.UNCLICKABLE, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testTabTransition_updatesState() {
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));

        mTabSupplier.set(mIncognitoTab);

        assertEquals(ButtonState.UNCLICKABLE, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testUrlUpdate_updatesState() {
        assertEquals(ButtonState.DEFAULT, mActionModel.get(ActionProperties.BUTTON_STATE));

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        observer.onUrlUpdated(mTab);

        assertEquals(ButtonState.UNCLICKABLE, mActionModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testOnStateChanged_updatesModel() {
        mCoordinator.onStateChanged(
                GlicButtonStateController.ButtonState.WORKING, /* isPanelOpen= */ false);

        assertEquals(
                GlicButtonStateController.ButtonState.WORKING,
                mActionModel.get(GlicActionProperties.GLIC_STATE));
    }
}
