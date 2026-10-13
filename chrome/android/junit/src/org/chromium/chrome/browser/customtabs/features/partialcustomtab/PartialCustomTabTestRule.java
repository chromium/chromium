// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.view.Display;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.RequiresApi;
import androidx.swiperefreshlayout.widget.CircularProgressDrawable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowDisplay;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsCoordinator;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarTestUtils;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TouchEventProvider;

import java.util.function.BooleanSupplier;

/**
 * A TestRule that sets up the mocks and contains helper methods for JUnit/Robolectric tests scoped
 * to the Partial Custom Tabs logic.
 */
public class PartialCustomTabTestRule implements TestRule {
    /**
     * Hands out a mocked WindowManager, since Robolectric can't report arbitrary window metrics or
     * system bar insets. Falls back to the real one until the mock is set, so that the window and
     * views can be inflated.
     */
    private static class TestActivity extends Activity {
        private WindowManager mMockWindowManager;
        private boolean mHasStatusBar;
        private Resources mResources;

        @Override
        public WindowManager getWindowManager() {
            return mMockWindowManager != null ? mMockWindowManager : super.getWindowManager();
        }

        @Override
        public Resources getResources() {
            if (mResources == null) {
                Resources base = super.getResources();
                mResources =
                        new Resources(
                                base.getAssets(),
                                base.getDisplayMetrics(),
                                base.getConfiguration()) {
                            @Override
                            public int getIdentifier(
                                    String name, String defType, String defPackage) {
                                if (!mHasStatusBar && "status_bar_height".equals(name)) {
                                    return 0;
                                }
                                return super.getIdentifier(name, defType, defPackage);
                            }
                        };
            }
            return mResources;
        }
    }

    // Pixel 3 XL metrics
    static final float DENSITY = 1.25f;
    static final int DEVICE_HEIGHT = 2960;
    static final int DEVICE_WIDTH = 1440;
    static final int DEVICE_HEIGHT_LANDSCAPE = DEVICE_WIDTH;
    static final int DEVICE_WIDTH_LANDSCAPE = DEVICE_HEIGHT;
    static final int NAVBAR_HEIGHT = 160;
    static final int STATUS_BAR_HEIGHT = 30;
    static final int FULL_HEIGHT = DEVICE_HEIGHT - NAVBAR_HEIGHT;
    static final int MULTIWINDOW_HEIGHT = FULL_HEIGHT / 2;

    static final int DEVICE_WIDTH_MEDIUM = 700;
    static final int DEVICE_WIDTH_COMPACT = 500;
    static final int DEVICE_HEIGHT_COMPACT = 300;
    static final int DEVICE_WIDTH_COMPACT_PORTRAIT = DEVICE_HEIGHT_COMPACT;
    static final int DEVICE_HEIGHT_COMPACT_PORTRAIT = DEVICE_WIDTH_COMPACT;

    private static final int DEFAULT_BG_COLOR = Color.LTGRAY;

    @Mock WindowManager mWindowManager;
    @Mock TouchEventProvider mTouchEventProvider;
    @Mock Tab mTab;
    @Mock BrowserServicesIntentDataProvider mIntentData;
    @Mock CustomTabHeightStrategy.OnResizedCallback mOnResizedCallback;
    @Mock CustomTabHeightStrategy.OnActivityLayoutCallback mOnActivityLayoutCallback;
    @Mock FullscreenManager mFullscreenManager;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock CircularProgressDrawable mSpinner;
    @Mock PartialCustomTabHandleStrategyFactory mHandleStrategyFactory;
    @Mock CustomTabToolbarButtonsCoordinator mToolbarButtonsCoordinator;

    Display mDisplay;
    ShadowDisplay mShadowDisplay;
    Configuration mConfiguration;
    Activity mActivity;
    ViewGroup mContentFrame;
    ViewGroup mCoordinatorLayout;
    View mHandleView;
    CustomTabDragBar mDragBar;
    View mDragHandlebar;
    ImageView mSpinnerView;
    CustomTabToolbar mToolbarView;
    View mToolbarCoordinator;

    Context mContext;

    // The layout params of |mToolbarCoordinator|.
    FrameLayout.LayoutParams mLayoutParams =
            new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);

    private FrameLayout mSpinnerHost;

    private void setUp() {
        // MockitoRule is not processed recursively in JUnit 4, and these are
        // TestRule or TestWatcher implementations. Manual initialization is
        // required.
        MockitoAnnotations.initMocks(this);
        SemanticColorUtils.setDividerLineBgColorForTesting(Color.LTGRAY);
        SemanticColorUtils.setDefaultBgColorForTesting(DEFAULT_BG_COLOR);

        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);

        TestActivity activity = setUpActivity();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            setUpModernAndroidMocks();
        }

        setUpHandleStrategyFactory();

        activity.mMockWindowManager = mWindowManager;
    }

    private TestActivity setUpActivity() {
        ActivityController<TestActivity> controller = Robolectric.buildActivity(TestActivity.class);
        TestActivity activity = controller.get();
        mActivity = activity;
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        controller.create();
        setUpResources();
        setUpDisplayMocks();

        // Mirrors the bottom-sheet layout, with the handle view already inflated.
        activity.setContentView(R.layout.custom_tabs_handle_view);
        mCoordinatorLayout = new FrameLayout(activity);
        mCoordinatorLayout.setId(R.id.coordinator);
        FrameLayout compositorViewHolder = new FrameLayout(activity);
        compositorViewHolder.setId(R.id.compositor_view_holder);
        mCoordinatorLayout.addView(compositorViewHolder);
        activity.addContentView(
                mCoordinatorLayout, new FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        mContentFrame = activity.findViewById(android.R.id.content);
        setContentFrameHeight(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        mHandleView = activity.findViewById(R.id.custom_tabs_handle_view);
        mDragBar = activity.findViewById(R.id.drag_bar);
        mDragHandlebar = activity.findViewById(R.id.drag_handle);

        // Start with dimming enabled so that verifyWindowFlagsSet() can check that it is cleared.
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        mToolbarView = CustomTabToolbarTestUtils.inflateToolbar(activity);
        mToolbarCoordinator = new View(activity);
        mToolbarCoordinator.setLayoutParams(mLayoutParams);

        // The spinner is attached to its own window so that its fade animations run.
        mSpinnerView = new ImageView(mContext);
        mSpinnerView.setVisibility(View.GONE);
        mSpinnerHost = new FrameLayout(mContext);
        mSpinnerHost.addView(
                mSpinnerView, new FrameLayout.LayoutParams(MATCH_PARENT, /* height= */ 0));
        mContext.getSystemService(WindowManager.class)
                .addView(mSpinnerHost, new WindowManager.LayoutParams());
        RobolectricUtil.runAllBackgroundAndUi();
        return activity;
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private void setUpModernAndroidMocks() {
        doAnswer(invocation -> getCurrentWindowMetrics())
                .when(mWindowManager)
                .getCurrentWindowMetrics();
        doAnswer(invocation -> getCurrentWindowMetrics())
                .when(mWindowManager)
                .getMaximumWindowMetrics();
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private WindowMetrics getCurrentWindowMetrics() {
        Point realSize = new Point();
        mDisplay.getRealSize(realSize);
        WindowInsets windowInsets =
                new WindowInsets.Builder()
                        .setInsets(
                                WindowInsets.Type.navigationBars(),
                                Insets.of(0, 0, 0, getNavigationBarHeight()))
                        .build();
        return new WindowMetrics(new Rect(0, 0, realSize.x, realSize.y), windowInsets);
    }

    private int getNavigationBarHeight() {
        boolean isMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
        if (mConfiguration.orientation == Configuration.ORIENTATION_PORTRAIT && !isMultiWindow) {
            return NAVBAR_HEIGHT;
        }
        return 0;
    }

    private void setUpResources() {
        Resources resources = mActivity.getResources();
        mConfiguration = resources.getConfiguration();
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        resources.getDisplayMetrics().density = DENSITY;
    }

    private void setUpHandleStrategyFactory() {
        when(mHandleStrategyFactory.create(
                        anyInt(),
                        any(Context.class),
                        any(BooleanSupplier.class),
                        any(),
                        any(PartialCustomTabHandleStrategy.DragEventCallback.class)))
                .thenReturn(null);
    }

    private void setUpDisplayMocks() {
        mDisplay = ShadowDisplay.getDefaultDisplay();
        mShadowDisplay = shadowOf(mDisplay);
        when(mWindowManager.getDefaultDisplay()).thenReturn(mDisplay);
        mShadowDisplay.setRealWidth(DEVICE_WIDTH);
        mShadowDisplay.setRealHeight(DEVICE_HEIGHT);
        mShadowDisplay.setDensity(DENSITY);
        mShadowDisplay.setWidth(DEVICE_WIDTH);
        mShadowDisplay.setHeight(DEVICE_HEIGHT - NAVBAR_HEIGHT);
    }

    private void commonTearDown() {
        // Reset the multi-window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
        mContext.getSystemService(WindowManager.class).removeViewImmediate(mSpinnerHost);
    }

    public static void waitForAnimationToFinish() {
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    public void configPortraitMode() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mShadowDisplay.setRealWidth(DEVICE_WIDTH);
        mShadowDisplay.setRealHeight(DEVICE_HEIGHT);
        mShadowDisplay.setDensity(DENSITY);
        mShadowDisplay.setWidth(DEVICE_WIDTH);
        mShadowDisplay.setHeight(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        setContentFrameHeight(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        mShadowDisplay.setRotation(Surface.ROTATION_90);
    }

    public void configLandscapeMode() {
        configLandscapeMode(Surface.ROTATION_90);
    }

    public void configLandscapeMode(int direction) {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mShadowDisplay.setRealWidth(DEVICE_HEIGHT);
        mShadowDisplay.setRealHeight(DEVICE_WIDTH);
        mShadowDisplay.setDensity(DENSITY);
        mShadowDisplay.setWidth(DEVICE_HEIGHT);
        mShadowDisplay.setHeight(DEVICE_WIDTH);
        setContentFrameHeight(DEVICE_WIDTH);
        mShadowDisplay.setRotation(direction);
    }

    public void configDeviceWidthMedium() {
        mShadowDisplay.setRealWidth(DEVICE_WIDTH_MEDIUM);
        mShadowDisplay.setWidth(DEVICE_WIDTH_MEDIUM);
        mShadowDisplay.setHeight(DEVICE_HEIGHT - NAVBAR_HEIGHT);
    }

    public void configCompactDevice() {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mShadowDisplay.setRealWidth(DEVICE_WIDTH_COMPACT);
        mShadowDisplay.setRealHeight(DEVICE_HEIGHT_COMPACT);
        mShadowDisplay.setWidth(DEVICE_WIDTH_COMPACT);
        mShadowDisplay.setHeight(DEVICE_HEIGHT_COMPACT);
        setContentFrameHeight(DEVICE_WIDTH_COMPACT);
    }

    public void configCompactDevice_Portrait() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mShadowDisplay.setRealWidth(DEVICE_WIDTH_COMPACT_PORTRAIT);
        mShadowDisplay.setRealHeight(DEVICE_HEIGHT_COMPACT_PORTRAIT);
        mShadowDisplay.setWidth(DEVICE_WIDTH_COMPACT_PORTRAIT);
        mShadowDisplay.setHeight(DEVICE_HEIGHT_COMPACT_PORTRAIT);
        setContentFrameHeight(DEVICE_HEIGHT_COMPACT_PORTRAIT);
    }

    public void verifyWindowFlagsSet() {
        assertTrue(hasWindowFlag(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL));
        assertFalse(hasWindowFlag(WindowManager.LayoutParams.FLAG_DIM_BEHIND));
    }

    public boolean hasWindowFlag(int flag) {
        return (getWindowAttributes().flags & flag) != 0;
    }

    /** Returns the window's live attributes (not a snapshot). */
    public WindowManager.LayoutParams getWindowAttributes() {
        return mActivity.getWindow().getAttributes();
    }

    public float getDisplayDensity() {
        return mActivity.getResources().getDisplayMetrics().density;
    }

    public void setupDisplayMetricsInMultiWindowMode() {
        mShadowDisplay.setRealHeight(MULTIWINDOW_HEIGHT);
        mShadowDisplay.setHeight(MULTIWINDOW_HEIGHT);
        setContentFrameHeight(MULTIWINDOW_HEIGHT);
    }

    public void configureStatusBarHeightForQ() {
        ((TestActivity) mActivity).mHasStatusBar = true;
    }

    /** Asserts that the coordinator background was wrapped to reset the divider line insets. */
    public void assertCoordinatorInsetsReset(Drawable previousBackground) {
        var background = (InsetDrawable) mCoordinatorLayout.getBackground();
        assertSame(previousBackground, background.getDrawable());
    }

    /** Sets the height of the (never laid out) content frame, read by pre-R display logic. */
    public void setContentFrameHeight(int height) {
        mContentFrame.setBottom(mContentFrame.getTop() + height);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                try {
                    statement.evaluate();
                } finally {
                    commonTearDown();
                }
            }
        };
    }
}
