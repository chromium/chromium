// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_MANAGER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_LEARN_MORE_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.LENS_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.SINGLE_TAB_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TOP_TOOLBAR_PLACEHOLDER_HEIGHT;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link TasksViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TasksViewBinderUnitTest {
    private final AtomicBoolean mViewClicked = new AtomicBoolean();
    private final View.OnClickListener mViewOnClickListener = (v) -> mViewClicked.set(true);
    private Activity mActivity;
    private TasksView mTasksView;
    private PropertyModel mTasksViewPropertyModel;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private IncognitoCookieControlsManager mCookieControlsManager;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        MockitoAnnotations.initMocks(this);

        mTasksView =
                (TasksView) mActivity.getLayoutInflater().inflate(R.layout.tasks_view_layout, null);
        mActivity.setContentView(mTasksView);

        mTasksViewPropertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mTasksViewPropertyModel, mTasksView, TasksViewBinder::bind);
    }

    private boolean isViewVisible(int viewId) {
        return mTasksView.findViewById(viewId).getVisibility() == View.VISIBLE;
    }

    @Test
    @SmallTest
    public void testSetTabCarouselMode() {
        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, true);
        assertTrue(isViewVisible(R.id.carousel_tab_switcher_container));

        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, false);
        assertFalse(isViewVisible(R.id.carousel_tab_switcher_container));
    }

    @Test
    @SmallTest
    public void testSetTabCarouselTitle() {
        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_TITLE_VISIBLE, true);
        assertTrue(isViewVisible(R.id.tab_switcher_title));

        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_TITLE_VISIBLE, false);
        assertFalse(isViewVisible(R.id.tab_switcher_title));
    }

    @Test
    @SmallTest
    public void testSetFakeboxVisibilityClickListenerAndTextWatcher() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true));
        assertTrue(isViewVisible(R.id.search_box));

        AtomicBoolean textChanged = new AtomicBoolean();
        TextWatcher textWatcher = new TextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {
                // do nothing.
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                // do nothing.
            }
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                textChanged.set(true);
            }
        };

        mViewClicked.set(false);
        mTasksView.findViewById(R.id.search_box_text).performClick();
        assertFalse(mViewClicked.get());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(FAKE_SEARCH_BOX_CLICK_LISTENER, mViewOnClickListener);
        });
        mTasksView.findViewById(R.id.search_box_text).performClick();
        assertTrue(mViewClicked.get());

        textChanged.set(false);
        EditText searchBoxText = mTasksView.findViewById(R.id.search_box_text);
        searchBoxText.setText("test");
        searchBoxText.performClick();
        assertFalse(textChanged.get());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(FAKE_SEARCH_BOX_TEXT_WATCHER, textWatcher));
        searchBoxText.setText("test2");
        searchBoxText.performClick();
        assertTrue(textChanged.get());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, false));
        assertFalse(isViewVisible(R.id.search_box));
    }

    @Test
    @SmallTest
    public void testSetVoiceSearchButtonVisibilityAndClickListener() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
            mTasksViewPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, true);
        });
        assertTrue(isViewVisible(R.id.voice_search_button));

        mViewClicked.set(false);
        mTasksView.findViewById(R.id.voice_search_button).performClick();
        assertFalse(mViewClicked.get());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(VOICE_SEARCH_BUTTON_CLICK_LISTENER, mViewOnClickListener);
        });
        mTasksView.findViewById(R.id.voice_search_button).performClick();
        assertTrue(mViewClicked.get());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false));
        assertFalse(isViewVisible(R.id.voice_search_button));
    }

    @Test
    @SmallTest
    public void testSetLensButtonVisibilityAndClickListener() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
            mTasksViewPropertyModel.set(IS_LENS_BUTTON_VISIBLE, true);
        });
        assertTrue(isViewVisible(R.id.lens_camera_button));

        mViewClicked.set(false);
        mTasksView.findViewById(R.id.lens_camera_button).performClick();
        assertFalse(mViewClicked.get());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(LENS_BUTTON_CLICK_LISTENER, mViewOnClickListener);
        });
        mTasksView.findViewById(R.id.lens_camera_button).performClick();
        assertTrue(mViewClicked.get());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(IS_LENS_BUTTON_VISIBLE, false));
        assertFalse(isViewVisible(R.id.lens_camera_button));
    }

    @Test
    @SmallTest
    public void testSetMVTilesVisibility() {
        mTasksViewPropertyModel.set(MV_TILES_VISIBLE, true);
        assertTrue(isViewVisible(R.id.mv_tiles_container));

        mTasksViewPropertyModel.set(MV_TILES_VISIBLE, false);
        assertFalse(isViewVisible(R.id.mv_tiles_container));
    }

    @Test
    @SmallTest
    public void testSetMoreTabsClickListener() {
        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, true);

        mViewClicked.set(false);
        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO (crbug.com/1186752): Investigate whether this would be a problem for real users.
        mTasksView.findViewById(R.id.more_tabs).performClick();
        assertFalse(mViewClicked.get());
        mTasksViewPropertyModel.set(MORE_TABS_CLICK_LISTENER, mViewOnClickListener);
        mTasksView.findViewById(R.id.more_tabs).performClick();
        assertTrue(mViewClicked.get());
    }

    @Test
    @SmallTest
    public void testSetIncognitoMode() {
        mTasksViewPropertyModel.set(IS_INCOGNITO, true);
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(mActivity, true);
        ColorDrawable viewColor = (ColorDrawable) mTasksView.getBackground();
        assertEquals(backgroundColor, viewColor.getColor());

        mTasksViewPropertyModel.set(IS_INCOGNITO, false);
        backgroundColor = ChromeColors.getPrimaryBackgroundColor(mActivity, false);
        viewColor = (ColorDrawable) mTasksView.getBackground();
        assertEquals(backgroundColor, viewColor.getColor());
    }

    @Test
    @SmallTest
    public void testSetIncognitoDescriptionVisibilityAndClickListener() {
        assertFalse(isViewVisible(R.id.incognito_description_container_layout_stub));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(INCOGNITO_LEARN_MORE_CLICK_LISTENER, mViewOnClickListener);
        });
        assertFalse(isViewVisible(R.id.incognito_description_container_layout_stub));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(INCOGNITO_COOKIE_CONTROLS_MANAGER, mCookieControlsManager);
            mTasksViewPropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
            mTasksViewPropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, true);
        });
        assertTrue(isViewVisible(R.id.new_tab_incognito_container));
    }

    @Test
    @SmallTest
    public void testSetTasksSurfaceBodyTopMargin() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.getBodyViewContainer().getLayoutParams();
        assertEquals(0, params.topMargin);

        mTasksViewPropertyModel.set(TASKS_SURFACE_BODY_TOP_MARGIN, 16);

        assertEquals(16, params.topMargin);
    }

    @Test
    @SmallTest
    public void testSetMVTilesContainerTopMargin() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.findViewById(R.id.mv_tiles_container)
                        .getLayoutParams();
        assertEquals(0, params.topMargin);

        mTasksViewPropertyModel.set(MV_TILES_CONTAINER_TOP_MARGIN, 16);

        assertEquals(16, params.topMargin);
    }

    @Test
    @SmallTest
    public void testSetMVTilesContainerLeftAndRightMargin() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.findViewById(R.id.mv_tiles_container)
                        .getLayoutParams();
        assertEquals(0, params.leftMargin);
        assertEquals(0, params.rightMargin);

        mTasksViewPropertyModel.set(MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN, 16);

        assertEquals(16, params.leftMargin);
        assertEquals(16, params.rightMargin);
    }

    @Test
    @SmallTest
    public void testSetSingleTabTopMargin() {
        SingleTabView singleTabView = (SingleTabView) mActivity.getLayoutInflater().inflate(
                R.layout.single_tab_view_layout, mTasksView.getCarouselTabSwitcherContainer(),
                false);
        mTasksView.getCarouselTabSwitcherContainer().addView(singleTabView);

        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.findViewById(R.id.single_tab_view)
                        .getLayoutParams();
        // The initial top margin of single_tab_view_layout is 24.
        assertEquals(24, params.topMargin);

        mTasksViewPropertyModel.set(SINGLE_TAB_TOP_MARGIN, 16);

        assertEquals(16, params.topMargin);
    }

    @Test
    @SmallTest
    public void testSetTabSwitcherTitleTopMargin() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.findViewById(R.id.tab_switcher_title)
                        .getLayoutParams();
        assertEquals(0, params.topMargin);

        mTasksViewPropertyModel.set(TAB_SWITCHER_TITLE_TOP_MARGIN, 16);

        assertEquals(16, params.topMargin);
    }

    @Test
    @SmallTest
    public void testSetTopToolbarLayoutHeight() {
        ViewGroup.LayoutParams params =
                mTasksView.findViewById(R.id.top_toolbar_placeholder).getLayoutParams();
        assertEquals(LayoutParams.WRAP_CONTENT, params.height);

        mTasksViewPropertyModel.set(TOP_TOOLBAR_PLACEHOLDER_HEIGHT, 16);

        assertEquals(16, params.height);
    }
}
