// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.test.core.app.ActivityScenario.ActivityAction;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link BookmarkBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarCoordinatorTest {

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsManager mBrowserControlsManager;
    @Mock private Callback<Integer> mHeightChangeCallback;
    @Mock private Callback<Integer> mHeightSupplierObserver;

    private BookmarkBarCoordinator mCoordinator;
    private BookmarkBar mView;

    @Before
    public void setUp() {
        onActivity(this::createCoordinator);
    }

    private void createCoordinator(@NonNull Activity activity) {
        final var viewStub = new ViewStub(activity, R.layout.bookmark_bar);
        viewStub.setOnInflateListener((stub, view) -> mView = (BookmarkBar) view);

        // NOTE: `viewStub` must be attached to a `viewGroup` in order to inflate.
        final var viewGroup = new CoordinatorLayoutForPointer(activity, /* attrs= */ null);
        viewGroup.addView(viewStub);

        // NOTE: `viewStub` inflation occurs during coordinator construction.
        mView = null;
        mCoordinator =
                new BookmarkBarCoordinator(
                        activity,
                        mBrowserControlsManager,
                        mHeightChangeCallback,
                        /* profileSupplier= */ new ObservableSupplierImpl<>(),
                        viewStub);
        assertNotNull("Verify view stub inflation during construction.", mView);
    }

    private void onActivity(@NonNull ActivityAction<TestActivity> callback) {
        mActivityScenarioRule.getScenario().onActivity(callback);
    }

    @Test
    @SmallTest
    public void testConstructorWhenTopControlOffsetIsNonZero() {
        testConstructor(/* topControlOffset= */ -1);
    }

    @Test
    @SmallTest
    public void testConstructorWhenTopControlOffsetIsZero() {
        testConstructor(/* topControlOffset= */ 0);
    }

    private void testConstructor(int topControlOffset) {
        // Initialize browser controls manager.
        final int topControlsHeight = 1;
        when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(topControlsHeight);
        when(mBrowserControlsManager.getTopControlOffset()).thenReturn(topControlOffset);

        // Invoke constructor.
        onActivity(this::createCoordinator);

        assertEquals(
                "Verify view top margin.",
                topControlsHeight - mView.getHeight(),
                ((MarginLayoutParams) mView.getLayoutParams()).topMargin);

        assertEquals(
                "Verify view visibility.",
                topControlOffset == 0 ? View.VISIBLE : View.GONE,
                mView.getVisibility());
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarHeightChanged() {
        // Verify initial state.
        ObservableSupplier<Integer> heightSupplier = mCoordinator.getHeightSupplier();
        assertEquals("Verify initial state.", 0, heightSupplier.get().intValue());

        // NOTE: the `mHeightChangeCallback` is expected to have been registered for observation
        // during `mCoordinator` construction and notified of initial height via posted task.
        onActivity(
                activity -> {
                    verify(mHeightChangeCallback).onResult(0);
                    verifyNoMoreInteractions(mHeightSupplierObserver);
                });

        // Register another observer explicitly.
        heightSupplier.addObserver(mHeightSupplierObserver);

        // Verify state after height-changing layout.
        final var rect = new Rect(1, 2, 3, 4);
        mView.layout(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(
                "Verify state after height-changing layout.",
                rect.height(),
                heightSupplier.get().intValue());
        verify(mHeightChangeCallback).onResult(rect.height());
        verify(mHeightSupplierObserver).onResult(rect.height());

        // Verify state after height-consistent layout.
        rect.top += 1;
        rect.bottom += 1;
        mView.layout(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(
                "Verify state after height-consistent layout.",
                rect.height(),
                heightSupplier.get().intValue());
        verifyNoMoreInteractions(mHeightChangeCallback);
        verifyNoMoreInteractions(mHeightSupplierObserver);

        // Clean up.
        heightSupplier.removeObserver(mHeightSupplierObserver);
    }

    @Test
    @SmallTest
    public void testOnTopControlsHeightChanged() {
        // Initialize browser controls manager.
        final int topControlsHeight = 1;
        when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(topControlsHeight);

        // Simulate top controls height changed.
        final var obs = ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsManager).addObserver(obs.capture());
        obs.getValue()
                .onTopControlsHeightChanged(
                        mBrowserControlsManager.getTopControlsHeight(),
                        mBrowserControlsManager.getTopControlsMinHeight());

        assertEquals(
                "Verify view top margin.",
                topControlsHeight - mView.getHeight(),
                ((MarginLayoutParams) mView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    public void testOnTopControlsOffsetChanged() {
        // Initialize browser controls manager.
        final var topControlOffset = new AtomicInteger(-1);
        when(mBrowserControlsManager.getTopControlOffset())
                .thenAnswer(invocation -> topControlOffset.get());

        // Simulate top controls offset changed to non-zero value.
        final var obs = ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsManager).addObserver(obs.capture());
        obs.getValue()
                .onControlsOffsetChanged(
                        mBrowserControlsManager.getTopControlOffset(),
                        mBrowserControlsManager.getTopControlsMinHeightOffset(),
                        /* topControlsMinHeightChanged= */ false,
                        mBrowserControlsManager.getBottomControlOffset(),
                        mBrowserControlsManager.getBottomControlsMinHeightOffset(),
                        /* bottomControlsMinHeightChanged= */ false,
                        /* requestNewFrame= */ false,
                        /* isVisibilityForced= */ false);

        assertEquals(
                "Verify view visibility after top controls offset changed to non-zero value.",
                View.GONE,
                mView.getVisibility());

        // Simulate top controls offset changed to zero value.
        topControlOffset.set(0);
        obs.getValue()
                .onControlsOffsetChanged(
                        mBrowserControlsManager.getTopControlOffset(),
                        mBrowserControlsManager.getTopControlsMinHeightOffset(),
                        /* topControlsMinHeightChanged= */ false,
                        mBrowserControlsManager.getBottomControlOffset(),
                        mBrowserControlsManager.getBottomControlsMinHeightOffset(),
                        /* bottomControlsMinHeightChanged= */ false,
                        /* requestNewFrame= */ false,
                        /* isVisibilityForced= */ false);

        assertEquals(
                "Verify view visibility after top controls offset changed to zero value.",
                View.VISIBLE,
                mView.getVisibility());
    }
}
