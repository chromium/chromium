// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TABS;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

/**
 * Controller tests for the keyboard accessory tab layout component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class KeyboardAccessoryTabLayoutControllerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver<Void> mMockTabListObserver;
    @Mock
    private KeyboardAccessoryTabLayoutCoordinator.AccessoryTabObserver mMockAccessoryTabObserver;
    @Mock
    private KeyboardAccessoryTabLayoutView mMockView;

    private final KeyboardAccessoryData.Tab mTestTab =
            new KeyboardAccessoryData.Tab("Passwords", null, null, 0, 0, null);

    private KeyboardAccessoryTabLayoutCoordinator mCoordinator;
    private PropertyModel mModel;
    private KeyboardAccessoryTabLayoutMediator mMediator;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);

        mCoordinator = new KeyboardAccessoryTabLayoutCoordinator();
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mCoordinator.getModelForTesting();
        mCoordinator.assignNewView(mMockView);
        mCoordinator.setTabObserver(mMockAccessoryTabObserver);
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
    }

    @Test
    public void testChangingTabsNotifiesTabObserver() {
        mModel.get(TABS).addObserver(mMockTabListObserver);

        // Calling addTab on the coordinator should make the model propagate that it has a new tab.
        mCoordinator.getTabSwitchingDelegate().addTab(mTestTab);
        verify(mMockTabListObserver).onItemRangeInserted(mModel.get(TABS), 0, 1);
        assertThat(mModel.get(TABS).size(), is(1));
        assertThat(mModel.get(TABS).get(0), is(mTestTab));

        // Calling hide on the coordinator should make the model propagate that it's invisible.
        mCoordinator.getTabSwitchingDelegate().removeTab(mTestTab);
        verify(mMockTabListObserver).onItemRangeRemoved(mModel.get(TABS), 0, 1);
        assertThat(mModel.get(TABS).size(), is(0));
    }

    @Test
    public void testModelDoesntNotifyUnchangedActiveTab() {
        mModel.addObserver(mMockPropertyObserver);

        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        mModel.set(ACTIVE_TAB, null);
        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        verify(mMockPropertyObserver, never()).onPropertyChanged(mModel, ACTIVE_TAB);

        mModel.set(ACTIVE_TAB, 0);
        assertThat(mModel.get(ACTIVE_TAB), is(0));
        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);

        mModel.set(ACTIVE_TAB, 0);
        assertThat(mModel.get(ACTIVE_TAB), is(0));
        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);
    }

    @Test
    public void testClosingTabDismissesOpenSheet() {
        mModel.set(ACTIVE_TAB, 0);
        mModel.addObserver(mMockPropertyObserver);
        assertThat(mModel.get(ACTIVE_TAB), is(0));

        // Closing the active tab should reset the tab which should trigger the visibility delegate.
        mCoordinator.getTabSwitchingDelegate().closeActiveTab();
        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);
        verify(mMockAccessoryTabObserver).onActiveTabChanged(null);
    }

    @Test
    public void testConvertsInvalidTabPositionToNull() {
        // Asserts that the helper used to validate tab positions converts the invalid position to
        // null. It would be better to call onTabSelected() with a tab having this position but Tab
        // is final and has a private constructor, so this works only in mockito V2 or newer.
        assertThat(mMediator.validateActiveTab(TabLayout.Tab.INVALID_POSITION), is(nullValue()));

        mMediator.setTabs(new KeyboardAccessoryData.Tab[0]);
        // Simulate a call when a removed tab was marked selected before the view picked it up:
        assertThat(mMediator.validateActiveTab(0), is(nullValue()));
    }

    @Test
    public void testClosingTabIsNoOpForAlreadyClosedTab() {
        mModel.set(ACTIVE_TAB, null);
        mModel.addObserver(mMockPropertyObserver);

        mCoordinator.getTabSwitchingDelegate().closeActiveTab();
        verifyNoMoreInteractions(
                mMockPropertyObserver, mMockTabListObserver, mMockAccessoryTabObserver);
    }

    @Test
    public void testSetActiveTab() {
        mModel.addObserver(mMockPropertyObserver);
        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        mCoordinator.getTabSwitchingDelegate().addTab(mTestTab);

        // Set the active tab type to 0 which is the recording_type of |mTestTab|.
        mCoordinator.getTabSwitchingDelegate().setActiveTab(0);

        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);
        assertThat(mModel.get(ACTIVE_TAB), is(0));
    }

    @Test(expected = AssertionError.class)
    public void testSetActiveTab_tabTypeNotFound_throwsException() {
        mModel.addObserver(mMockPropertyObserver);
        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        mCoordinator.getTabSwitchingDelegate().addTab(mTestTab);

        // Set the active tab type to 1 which is different from the recording_type of |mTestTab|.
        mCoordinator.getTabSwitchingDelegate().setActiveTab(1);
    }
}
