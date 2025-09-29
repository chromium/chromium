// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewStub;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.ui.resources.ResourceFactory;
import org.chromium.ui.resources.ResourceFactoryJni;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for the bookmark bar feature. */
@Batch(Batch.PER_CLASS)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@RunWith(ParameterizedRunner.class)
public class BookmarkBarRenderTest {

    @ClassParameter
    private static final List<ParameterSet> sClassParameters =
            List.of(
                    new ParameterSet().value(true).name("NightModeEnabled"),
                    new ParameterSet().value(false).name("NightModeDisabled"));

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    @Mock private BookmarkBarSceneLayer.Natives mBookmarkBarSceneLayerJniMock;
    @Mock private ResourceFactory.Natives mResourceFactoryJniMock;

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private LayoutManager mLayoutManager;
    @Mock private ResourceManager mResourceManager;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private DynamicResourceLoader mDynamicResourceLoader;
    @Mock private BrowserControlsManager mBrowserControlsManager;
    @Mock private Tab mCurrentTab;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;

    private BookmarkBarCoordinator mCoordinator;
    private BookmarkBar mView;

    public BookmarkBarRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        final Activity activity = mActivityTestRule.launchActivity(null);
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        BookmarkBarSceneLayerJni.setInstanceForTesting(mBookmarkBarSceneLayerJniMock);
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryJniMock);
        when(mResourceManager.getBitmapDynamicResourceLoader()).thenReturn(mDynamicResourceLoader);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final var contentView = new CoordinatorLayoutForPointer(activity, null);
                    activity.setContentView(contentView);

                    final var viewStub = new ViewStub(activity, R.layout.bookmark_bar);
                    viewStub.setOnInflateListener((stub, view) -> mView = (BookmarkBar) view);
                    contentView.addView(viewStub, new LayoutParams(MATCH_PARENT, WRAP_CONTENT));

                    mCoordinator =
                            new BookmarkBarCoordinator(
                                    activity,
                                    mActivityLifecycleDispatcher,
                                    mLayoutManager,
                                    /* requestUpdate= */ () -> {},
                                    mFullscreenManager,
                                    mResourceManager,
                                    mBrowserControlsManager,
                                    /* heightChangeCallback= */ result -> {},
                                    /* profileSupplier= */ new ObservableSupplierImpl<>(),
                                    viewStub,
                                    mCurrentTab,
                                    mBookmarkOpener,
                                    new ObservableSupplierImpl<>(mBookmarkManagerOpener),
                                    mTopControlsStacker,
                                    mCurrentTabSupplier,
                                    mTopUiThemeColorProvider);

                    assertNotNull(mView);
                    ChromeRenderTestRule.sanitize(mView);
                });
    }

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"RenderTest"})
    public void testEmptyState() throws IOException {
        mRenderTestRule.render(mView, "EmptyState");
    }
}
