// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import android.content.Context;
import android.graphics.Color;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetTrigger;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Tab;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator.SheetVisibilityDelegate;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.test.util.modelutil.FakeViewProvider;

/** Controller tests for the keyboard accessory bottom sheet component. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            CustomShadowAsyncTask.class,
            AccessorySheetControllerTest.ShadowSemanticColorUtils.class
        })
public class AccessorySheetControllerTest {
    @Implements(SemanticColorUtils.class)
    static class ShadowSemanticColorUtils {
        @Implementation
        public static int getDefaultBgColor(Context context) {
            return DEFAULT_BG_COLOR;
        }
    }

    private static final int DEFAULT_BG_COLOR = Color.LTGRAY;

    @Mock private PropertyObservable.PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock private ListObservable.ListObserver<Void> mTabListObserver;
    @Mock private AccessorySheetView mMockView;
    @Mock private RecyclerView mMockRecyclerView;
    @Mock private SheetVisibilityDelegate mSheetVisibilityDelegate;
    @Mock private AccessorySheetVisualStateProvider.Observer mVisualObserver;

    private final Tab[] mTabs =
            new Tab[] {
                new Tab("Passwords", null, null, 0, 0, null),
                new Tab("Passwords", null, null, 0, 0, null),
                new Tab("Passwords", null, null, 0, 0, null),
                new Tab("Passwords", null, null, 0, 0, null)
            };

    private AccessorySheetCoordinator mCoordinator;
    private AccessorySheetMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockView.getLayoutParams()).thenReturn(new ViewGroup.LayoutParams(0, 0));
        mCoordinator =
                new AccessorySheetCoordinator(
                        null, new FakeViewProvider<>(mMockView), mSheetVisibilityDelegate);
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
        mMediator.addObserver(mVisualObserver);
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
    }

    @Test
    public void testModelNotifiesAboutVisibilityOncePerChange() {
        mModel.addObserver(mMockPropertyObserver);
        // The visual observer is notified of the current state when it is added.
        verify(mVisualObserver).onAccessorySheetStateChanged(false, DEFAULT_BG_COLOR);

        // Calling show on the mediator should make model propagate that it's visible.
        mMediator.show();
        verify(mMockPropertyObserver).onPropertyChanged(mModel, VISIBLE);
        assertThat(mModel.get(VISIBLE), is(true));
        verify(mVisualObserver).onAccessorySheetStateChanged(true, DEFAULT_BG_COLOR);

        // Calling show again does nothing.
        mMediator.show();
        verify(mMockPropertyObserver) // Still the same call and no new one added.
                .onPropertyChanged(mModel, VISIBLE);
        verify(mVisualObserver) // Still the same call and no new one added.
                .onAccessorySheetStateChanged(true, DEFAULT_BG_COLOR);

        // Calling hide on the mediator should make model propagate that it's invisible.
        mMediator.hide();
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, VISIBLE);
        verify(mVisualObserver, times(2)).onAccessorySheetStateChanged(false, DEFAULT_BG_COLOR);

        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testModelNotifiesHeightChanges() {
        mModel.addObserver(mMockPropertyObserver);

        // Setting height triggers the observer and changes the model.
        mCoordinator.setHeight(123);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, HEIGHT);
        assertThat(mModel.get(HEIGHT), is(123));

        // Setting the same height doesn't trigger anything.
        mCoordinator.setHeight(123);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, HEIGHT); // No 2nd call.

        // Setting a different height triggers again.
        mCoordinator.setHeight(234);
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, HEIGHT);

        assertThat(mModel.get(HEIGHT), is(234));
    }

    @Test
    public void testModelNotifiesChangesForNewSheet() {
        mModel.addObserver(mMockPropertyObserver);
        mModel.get(TABS).addObserver(mTabListObserver);

        assertThat(mModel.get(TABS).size(), is(0));
        mCoordinator.setTabs(new Tab[] {mTabs[0]});
        verify(mTabListObserver).onItemRangeInserted(mModel.get(TABS), 0, 1);
        assertThat(mModel.get(TABS).size(), is(1));
    }

    @Test
    public void testDeletingFirstAndOnlyTabInvalidatesActiveTab() {
        mCoordinator.setTabs(new Tab[] {mTabs[0]});
        mCoordinator.setTabs(new Tab[0]);

        assertThat(mModel.get(TABS).size(), is(0));
        assertThat(mModel.get(ACTIVE_TAB_INDEX), is(NO_ACTIVE_TAB));
    }

    @Test
    public void testDoesntChangePositionOfActiveTabForDeletedSuccessors() {
        mCoordinator.setTabs(mTabs);
        mModel.set(ACTIVE_TAB_INDEX, 2);

        mCoordinator.setTabs(new Tab[] {mTabs[0], mTabs[1], mTabs[2]});

        assertThat(mModel.get(TABS).size(), is(3));
        assertThat(mModel.get(ACTIVE_TAB_INDEX), is(2));
    }

    @Test
    public void testScrollingChangesShadowVisibility() {
        assertThat(mModel.get(TOP_SHADOW_VISIBLE), is(false));

        // If the list was scrolled far enough that the top is hidden, show the top shadow.
        when(mMockRecyclerView.canScrollVertically(eq(-1))).thenReturn(true);
        mCoordinator.getScrollListener().onScrolled(mMockRecyclerView, 0, 10);
        assertThat(mModel.get(TOP_SHADOW_VISIBLE), is(true));

        // If the list was scrolled back to the top, hide the shadow again.
        when(mMockRecyclerView.canScrollVertically(eq(-1))).thenReturn(false);
        mCoordinator.getScrollListener().onScrolled(mMockRecyclerView, 0, -10);
        assertThat(mModel.get(TOP_SHADOW_VISIBLE), is(false));
    }

    @Test
    public void testRecordsSheetClosure() {
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        ManualFillingMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED),
                is(0));

        // Although sheets must be opened manually as of now, don't assume that every opened sheet
        // in the future will be manually opened. Closing is the only thing to be tested here.
        mCoordinator.show();
        mCoordinator.hide();
        assertThat(getTriggerMetricsCount(AccessorySheetTrigger.ANY_CLOSE), is(1));

        // Log closing every time it happens.
        mCoordinator.show();
        mCoordinator.hide();
        assertThat(getTriggerMetricsCount(AccessorySheetTrigger.ANY_CLOSE), is(2));
    }

    @Test
    public void testKeyboardRequest() {
        mCoordinator.setTabs(mTabs);
        mModel.set(ACTIVE_TAB_INDEX, NO_ACTIVE_TAB);
        Runnable keyboardCallback = mModel.get(SHOW_KEYBOARD_CALLBACK);

        assertThat(keyboardCallback, is(notNullValue()));
        verifyNoMoreInteractions(mSheetVisibilityDelegate);

        keyboardCallback.run();

        verifyNoMoreInteractions(mSheetVisibilityDelegate);

        mModel.set(ACTIVE_TAB_INDEX, 0);

        keyboardCallback.run();
        verify(mSheetVisibilityDelegate, times(1)).onCloseAccessorySheet();
    }

    private int getTriggerMetricsCount(@AccessorySheetTrigger int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                ManualFillingMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED, bucket);
    }
}
