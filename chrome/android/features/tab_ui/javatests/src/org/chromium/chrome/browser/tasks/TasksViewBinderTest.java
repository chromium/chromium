// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_MANAGER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_LEARN_MORE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.LENS_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;
import android.widget.ListView;
import android.widget.ScrollView;

import androidx.core.widget.NestedScrollView;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.ScrollToAction;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link TasksViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TasksViewBinderTest extends DummyUiActivityTestCase {
    private TasksView mTasksView;
    private PropertyModel mTasksViewPropertyModel;
    private AtomicBoolean mViewClicked = new AtomicBoolean();
    private View.OnClickListener mViewOnClickListener = (v) -> mViewClicked.set(true);
    @Mock
    private IncognitoCookieControlsManager mCookieControlsManager;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksView = (TasksView) getActivity().getLayoutInflater().inflate(
                    R.layout.tasks_view_layout, null);
            getActivity().setContentView(mTasksView);

            mTasksViewPropertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(
                    mTasksViewPropertyModel, mTasksView, TasksViewBinder::bind);
        });
    }

    private boolean isViewVisible(int viewId) {
        return mTasksView.findViewById(viewId).getVisibility() == View.VISIBLE;
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTabCarouselMode() {
        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, true);
        assertTrue(isViewVisible(R.id.carousel_tab_switcher_container));
        assertTrue(isViewVisible(R.id.tab_switcher_title));

        mTasksViewPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, false);
        assertFalse(isViewVisible(R.id.carousel_tab_switcher_container));
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
        onView(withId(R.id.search_box_text)).perform(click());
        assertFalse(mViewClicked.get());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(FAKE_SEARCH_BOX_CLICK_LISTENER, mViewOnClickListener);
        });
        onView(withId(R.id.search_box_text)).perform(click());
        assertTrue(mViewClicked.get());

        textChanged.set(false);
        onView(withId(R.id.search_box_text)).perform(replaceText("test"));
        assertFalse(textChanged.get());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(FAKE_SEARCH_BOX_TEXT_WATCHER, textWatcher));
        onView(withId(R.id.search_box_text)).perform(replaceText("test2"));
        assertTrue(textChanged.get());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, false));
        assertFalse(isViewVisible(R.id.search_box));
    }

    @Test
    @SmallTest
    @DisabledTest
    public void testSetVoiceSearchButtonVisibilityAndClickListener() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
            mTasksViewPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, true);
        });
        assertTrue(isViewVisible(R.id.voice_search_button));

        mViewClicked.set(false);
        onView(withId(R.id.voice_search_button)).perform(click());
        assertFalse(mViewClicked.get());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(VOICE_SEARCH_BUTTON_CLICK_LISTENER, mViewOnClickListener);
        });
        onView(withId(R.id.voice_search_button)).perform(click());
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
        assertTrue(isViewVisible(R.id.lens_camera_button_end));

        mViewClicked.set(false);
        onView(withId(R.id.lens_camera_button_end)).perform(click());
        assertFalse(mViewClicked.get());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTasksViewPropertyModel.set(LENS_BUTTON_CLICK_LISTENER, mViewOnClickListener);
        });
        onView(withId(R.id.lens_camera_button_end)).perform(click());
        assertTrue(mViewClicked.get());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTasksViewPropertyModel.set(IS_LENS_BUTTON_VISIBLE, false));
        assertFalse(isViewVisible(R.id.lens_camera_button_end));
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetMVTilesVisibility() {
        mTasksViewPropertyModel.set(MV_TILES_VISIBLE, true);
        assertTrue(isViewVisible(R.id.mv_tiles_container));

        mTasksViewPropertyModel.set(MV_TILES_VISIBLE, false);
        assertFalse(isViewVisible(R.id.mv_tiles_container));
    }

    @Test
    @UiThreadTest
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
    @UiThreadTest
    @SmallTest
    public void testSetIncognitoMode() {
        mTasksViewPropertyModel.set(IS_INCOGNITO, true);
        Resources resources = getActivity().getResources();
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(resources, true);
        ColorDrawable viewColor = (ColorDrawable) mTasksView.getBackground();
        assertEquals(backgroundColor, viewColor.getColor());

        mTasksViewPropertyModel.set(IS_INCOGNITO, false);
        backgroundColor = ChromeColors.getPrimaryBackgroundColor(resources, false);
        viewColor = (ColorDrawable) mTasksView.getBackground();
        assertEquals(backgroundColor, viewColor.getColor());
    }

    @Test
    @SmallTest
    public void
    testSetIncognitoDescriptionVisibilityAndClickListener() {
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

        mViewClicked.set(false);
        // Default scrollTo() cannot be used for NestedScrollView. Add a customized scrollTo for
        // scrolling to learn_more button.
        ViewAction customizedScrollTo = new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.allOf(
                        ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE),
                        ViewMatchers.isDescendantOfA(
                                Matchers.anyOf(ViewMatchers.isAssignableFrom(ScrollView.class),
                                        ViewMatchers.isAssignableFrom(HorizontalScrollView.class),
                                        ViewMatchers.isAssignableFrom(ListView.class),
                                        ViewMatchers.isAssignableFrom(NestedScrollView.class))));
            }

            @Override
            public String getDescription() {
                return "scroll to";
            }

            @Override
            public void perform(UiController uiController, View view) {
                new ScrollToAction().perform(uiController, view);
            }
        };
        onView(withId(R.id.learn_more)).perform(customizedScrollTo, click());
        assertTrue(mViewClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTasksSurfaceBodyTopMargin() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.getBodyViewContainer().getLayoutParams();
        assertEquals(0, params.topMargin);

        mTasksViewPropertyModel.set(TASKS_SURFACE_BODY_TOP_MARGIN, 16);

        assertEquals(16, params.topMargin);
    }

    @Test
    @UiThreadTest
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
    @UiThreadTest
    @SmallTest
    public void testSetTabSwitcherTitleTopMargin() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mTasksView.findViewById(R.id.tab_switcher_title)
                        .getLayoutParams();
        assertEquals(0, params.topMargin);

        mTasksViewPropertyModel.set(TAB_SWITCHER_TITLE_TOP_MARGIN, 16);

        assertEquals(16, params.topMargin);
    }
}
