// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Answers.CALLS_REAL_METHODS;
import static org.mockito.Mockito.doReturn;

import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.components.browser_ui.widget.loading.LoadingFullscreenCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link TabbedRootUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedRootUiCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock(answer = CALLS_REAL_METHODS)
    private TabbedRootUiCoordinator mCoordinator;

    @Mock private ScrimManager mScrimManager;

    /**
     * The loading fullscreen coordinator is created lazily on the first call to {@link
     * TabbedRootUiCoordinator#getLoadingFullscreenCoordinator()}. Its only client, the
     * collaboration controller delegate factory, can run after the coordinator was destroyed, and
     * the coordinator must not be resurrected then: the activity is gone and the loading view stub
     * was already consumed by the first inflation.
     *
     * <p>After {@link TabbedRootUiCoordinator#onDestroy()}, {@code mLoadingFullscreenCoordinator}
     * and {@code mActivity} (nulled by {@link RootUiCoordinator#onDestroy()}) are both null. The
     * real constructor requires the full activity graph, so those post-destroy field states are
     * simulated via reflection.
     */
    @Test
    public void testGetLoadingFullscreenCoordinatorAfterDestroy() {
        ReflectionHelpers.setField(mCoordinator, "mActivity", null);
        ReflectionHelpers.setField(mCoordinator, "mLoadingFullscreenCoordinator", null);
        assertNull(mCoordinator.getLoadingFullscreenCoordinator());
    }

    /**
     * The loading fullscreen coordinator is created at most once: the loading view stub is consumed
     * by the first inflation, so every later call must hand back the existing instance instead of
     * inflating the stub a second time.
     */
    @Test
    public void testGetLoadingFullscreenCoordinator_cachesResultAfterFirstCreation() {
        AppCompatActivity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        FrameLayout root = new FrameLayout(activity);
        ViewStub loadingStub = new ViewStub(activity);
        loadingStub.setId(R.id.loading_stub);
        AtomicInteger inflateCount = new AtomicInteger();
        loadingStub.setOnInflateListener((stub, inflated) -> inflateCount.incrementAndGet());
        root.addView(loadingStub);
        activity.setContentView(root);

        ReflectionHelpers.setField(mCoordinator, "mActivity", activity);
        ReflectionHelpers.setField(mCoordinator, "mLoadingFullscreenCoordinator", null);
        doReturn(mScrimManager).when(mCoordinator).getScrimManager();

        LoadingFullscreenCoordinator first = mCoordinator.getLoadingFullscreenCoordinator();
        LoadingFullscreenCoordinator second = mCoordinator.getLoadingFullscreenCoordinator();

        assertNotNull(first);
        assertSame(first, second);
        assertEquals(1, inflateCount.get());
    }
}
