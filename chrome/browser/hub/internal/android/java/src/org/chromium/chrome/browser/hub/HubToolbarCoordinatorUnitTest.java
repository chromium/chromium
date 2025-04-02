// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link HubToolbarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubToolbarCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private HubToolbarCoordinator mCoordinator;
    private HubToolbarView mHubToolbarView;
    private MenuButton mMenuButton;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier = new ObservableSupplierImpl<>();

    @Mock private PaneManager mPaneManager;
    @Mock private PaneOrderController mPaneOrderController;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private Tracker mTracker;
    @Mock private SearchActivityClient mSearchActivityClient;
    @Mock private HubColorMixer mHubColorMixer;

    @Captor private ArgumentCaptor<View.OnKeyListener> mKeyListenerCaptor;

    @Before
    public void setUp() {
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        when(mPaneManager.getPaneOrderController()).thenReturn(mPaneOrderController);
        when(mPaneOrderController.getPaneOrder()).thenReturn(ImmutableSet.of());

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        View rootView = LayoutInflater.from(activity).inflate(R.layout.hub_layout, null);
        activity.setContentView(rootView);
        mHubToolbarView = spy(rootView.findViewById(R.id.hub_toolbar));
        mMenuButton = spy(mHubToolbarView.findViewById(R.id.menu_button_wrapper));
        when(mHubToolbarView.findViewById(R.id.menu_button_wrapper)).thenReturn(mMenuButton);
        mCoordinator =
                new HubToolbarCoordinator(
                        activity,
                        mHubToolbarView,
                        mPaneManager,
                        mMenuButtonCoordinator,
                        mTracker,
                        mSearchActivityClient,
                        mHubColorMixer);
        verify(mMenuButton).setOnKeyListener(mKeyListenerCaptor.capture());
    }

    @Test
    public void enterOpensMenu() {
        mKeyListenerCaptor
                .getValue()
                .onKey(
                        mMenuButton,
                        KeyEvent.KEYCODE_ENTER,
                        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));

        verify(mMenuButtonCoordinator).onEnterKeyPress();
    }
}
