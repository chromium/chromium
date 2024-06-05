// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogView.VisibilityListener;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link TabGridDialogViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/344675717): Failing when batched, batch this again.
public class TabGridDialogViewBinderTest extends BlankUiTestActivityTestCase {
    private static final int CONTENT_TOP_MARGIN = 56;

    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;
    private TabGroupUiToolbarView mToolbarView;
    private RecyclerView mContentView;
    private TabGridDialogView mTabGridDialogView;
    private ChromeImageView mRightButton;
    private ChromeImageView mLeftButton;
    private EditText mTitleTextView;
    private FrameLayout mColorIconContainer;
    private ImageView mColorIcon;
    private View mMainContent;
    private ScrimCoordinator mScrimCoordinator;
    private GridLayoutManager mLayoutManager;
    private LinearLayoutManager mLinearLayoutManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private GradientDrawable mCardViewBackground;

    private Integer mBindingToken;

    @Before
    public void setUp() throws Exception {
        mBindingToken = 5;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout parentView = new FrameLayout(getActivity());
                    getActivity().setContentView(parentView);
                    mContentView =
                            (TabListRecyclerView)
                                    LayoutInflater.from(getActivity())
                                            .inflate(
                                                    R.layout.tab_list_recycler_view_layout,
                                                    parentView,
                                                    false);
                    mLayoutManager = spy(new GridLayoutManager(getActivity(), 2));
                    mContentView.setLayoutManager(mLayoutManager);
                    mToolbarView =
                            (TabGroupUiToolbarView)
                                    LayoutInflater.from(getActivity())
                                            .inflate(
                                                    R.layout.tab_group_ui_toolbar,
                                                    mContentView,
                                                    false);
                    LayoutInflater.from(getActivity())
                            .inflate(R.layout.tab_grid_dialog_layout, parentView, true);
                    mTabGridDialogView = parentView.findViewById(R.id.dialog_parent_view);
                    mLeftButton = mToolbarView.findViewById(R.id.toolbar_left_button);
                    mRightButton = mToolbarView.findViewById(R.id.toolbar_right_button);
                    mTitleTextView = mToolbarView.findViewById(R.id.title);
                    mColorIconContainer =
                            mToolbarView.findViewById(R.id.tab_group_color_icon_container);
                    mColorIcon = mToolbarView.findViewById(R.id.tab_group_color_icon);
                    mMainContent = mToolbarView.findViewById(R.id.main_content);
                    mScrimCoordinator =
                            new ScrimCoordinator(getActivity(), null, parentView, Color.RED);
                    mTabGridDialogView.setupScrimCoordinator(mScrimCoordinator);

                    mModel =
                            new PropertyModel.Builder(TabGridDialogProperties.ALL_KEYS)
                                    .with(
                                            TabGridDialogProperties.BROWSER_CONTROLS_STATE_PROVIDER,
                                            mBrowserControlsStateProvider)
                                    .build();
                    mModel.set(TabGridDialogProperties.BINDING_TOKEN, mBindingToken);

                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mModel,
                                    new TabGridDialogViewBinder.ViewHolder(
                                            mToolbarView, mContentView, mTabGridDialogView, null),
                                    TabGridDialogViewBinder::bind);
                });
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testBindingToken() {
        Assert.assertEquals(
                mTabGridDialogView.getBindingToken().intValue(), mBindingToken.intValue());

        mModel.set(TabGridDialogProperties.BINDING_TOKEN, null);
        Assert.assertNull(mTabGridDialogView.getBindingToken());

        String title = "1024 tabs";
        Assert.assertNotEquals(title, mTitleTextView.getText());
        mModel.set(TabGridDialogProperties.HEADER_TITLE, title);
        Assert.assertNotEquals(title, mTitleTextView.getText());

        mModel.set(TabGridDialogProperties.BINDING_TOKEN, 4);
        Assert.assertEquals(mTabGridDialogView.getBindingToken().intValue(), 4);
        Assert.assertEquals(title, mTitleTextView.getText().toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCollapseClickListener() {
        AtomicBoolean leftButtonClicked = new AtomicBoolean();
        leftButtonClicked.set(false);
        mLeftButton.performClick();
        Assert.assertFalse(leftButtonClicked.get());

        mModel.set(
                TabGridDialogProperties.COLLAPSE_CLICK_LISTENER,
                (View view) -> leftButtonClicked.set(true));

        mLeftButton.performClick();
        Assert.assertTrue(leftButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAddClickListener() {
        AtomicBoolean rightButtonClicked = new AtomicBoolean();
        rightButtonClicked.set(false);
        mRightButton.performClick();
        Assert.assertFalse(rightButtonClicked.get());

        mModel.set(
                TabGridDialogProperties.ADD_CLICK_LISTENER,
                (View view) -> rightButtonClicked.set(true));

        mRightButton.performClick();
        Assert.assertTrue(rightButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetHeaderTitle() {
        String title = "1024 tabs";
        Assert.assertNotEquals(title, mTitleTextView.getText());

        mModel.set(TabGridDialogProperties.HEADER_TITLE, title);

        Assert.assertEquals(title, mTitleTextView.getText().toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testContentTopMargin() {
        // Since setting content top margin is only used in sheet, we can assume that the parent is
        // a FrameLayout here.
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(0, 0);
        params.setMargins(0, 0, 0, 0);
        mContentView.setLayoutParams(new FrameLayout.LayoutParams(0, 0));
        Assert.assertEquals(
                0, ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);

        mModel.set(TabGridDialogProperties.CONTENT_TOP_MARGIN, CONTENT_TOP_MARGIN);

        Assert.assertEquals(
                CONTENT_TOP_MARGIN,
                ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetPrimaryColor() {
        int color = ContextCompat.getColor(getActivity(), R.color.baseline_primary_80);

        mModel.set(TabGridDialogProperties.PRIMARY_COLOR, color);

        Assert.assertEquals(color, ((ColorDrawable) mMainContent.getBackground()).getColor());
        Assert.assertEquals(color, ((ColorDrawable) mContentView.getBackground()).getColor());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testSetTint() {
        ColorStateList tint = ThemeUtils.getThemedToolbarIconTint(getActivity(), true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertNotEquals(tint, mLeftButton.getImageTintList());
            Assert.assertNotEquals(tint, mRightButton.getImageTintList());
        }
        Assert.assertNotEquals(tint, mTitleTextView.getTextColors());

        mModel.set(TabGridDialogProperties.TINT, tint);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(tint, mLeftButton.getImageTintList());
            Assert.assertEquals(tint, mRightButton.getImageTintList());
        }
        Assert.assertEquals(tint, mTitleTextView.getTextColors());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testSetScrimViewObserver() {
        AtomicBoolean scrimViewClicked = new AtomicBoolean();
        scrimViewClicked.set(false);
        Runnable scrimClickRunnable = () -> scrimViewClicked.set(true);

        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, scrimClickRunnable);
        // Open the dialog to show the ScrimView.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        View scrimView = mScrimCoordinator.getViewForTesting();
        scrimView.performClick();
        Assert.assertTrue(scrimViewClicked.get());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testSetDialogVisibility() {
        Assert.assertNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());

        // Setup basic dialog animation and a fake scrim view click runnable. These are always
        // initialized before the visibility of dialog is set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                    mTabGridDialogView.setScrimClickRunnable(() -> {});
                });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true));

        if (areAnimatorsEnabled()) {
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
        }
        Assert.assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false));

        if (areAnimatorsEnabled()) {
            Assert.assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
        }
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        Assert.assertEquals(View.GONE, mTabGridDialogView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testSetAnimationSourceView() {
        // When set animation source view as null, the show animation is set to be basic fade-in
        // which contains only one animation in animation set.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        Assert.assertEquals(
                1,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());

        // Create a placeholder source view to setup the dialog animation.
        ViewGroup sourceViewParent = new FrameLayout(getActivity());
        View sourceView = new View(getActivity());
        sourceViewParent.addView(sourceView);

        // When set with a specific animation source view, the show animation contains 6 child
        // animations.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, sourceView);
        Assert.assertEquals(
                6,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarStatus() {
        mModel.set(
                TabGridDialogProperties.UNGROUP_BAR_STATUS, TabGridDialogView.UngroupBarStatus.SHOW);
        Assert.assertEquals(
                TabGridDialogView.UngroupBarStatus.SHOW,
                mTabGridDialogView.getUngroupBarStatusForTesting());

        mModel.set(
                TabGridDialogProperties.UNGROUP_BAR_STATUS,
                TabGridDialogView.UngroupBarStatus.HOVERED);
        Assert.assertEquals(
                TabGridDialogView.UngroupBarStatus.HOVERED,
                mTabGridDialogView.getUngroupBarStatusForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testSetDialogBackgroundColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        getActivity(), R.color.incognito_tab_grid_dialog_background_color);

        mModel.set(TabGridDialogProperties.DIALOG_BACKGROUND_COLOR, incognitoColor);

        Assert.assertEquals(incognitoColor, mTabGridDialogView.getBackgroundColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarBackgroundColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        getActivity(), R.color.incognito_tab_grid_dialog_background_color);

        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR, incognitoColor);

        Assert.assertEquals(
                incognitoColor, mTabGridDialogView.getUngroupBarBackgroundColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarHoveredBackgroundColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        getActivity(),
                        R.color.incognito_tab_grid_dialog_ungroup_bar_bg_hovered_color);

        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR, incognitoColor);

        Assert.assertEquals(
                incognitoColor, mTabGridDialogView.getUngroupBarHoveredBackgroundColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testSetUngroupbarTextColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        getActivity(), R.color.incognito_tab_grid_dialog_ungroup_bar_text_color);

        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT_COLOR, incognitoColor);

        Assert.assertEquals(incognitoColor, mTabGridDialogView.getUngroupBarTextColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarHoveredTextColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        getActivity(),
                        R.color.incognito_tab_grid_dialog_ungroup_bar_text_hovered_color);

        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR, incognitoColor);

        Assert.assertEquals(
                incognitoColor, mTabGridDialogView.getUngroupBarHoveredTextColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetMainContentVisibility() {
        mContentView.setVisibility(View.INVISIBLE);
        Assert.assertEquals(View.INVISIBLE, mContentView.getVisibility());

        mModel.set(TabGridDialogProperties.IS_MAIN_CONTENT_VISIBLE, true);

        Assert.assertEquals(View.VISIBLE, mContentView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextWatcher() {
        String title = "cool tabs";
        AtomicBoolean titleTextUpdated = new AtomicBoolean();
        titleTextUpdated.set(false);

        TextWatcher textWatcher =
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable editable) {
                        titleTextUpdated.set(true);
                    }
                };
        mModel.set(TabGridDialogProperties.TITLE_TEXT_WATCHER, textWatcher);

        mTitleTextView.setText(title);
        Assert.assertTrue(titleTextUpdated.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextOnFocusListener() {
        AtomicBoolean textFocusChanged = new AtomicBoolean();
        textFocusChanged.set(false);
        Assert.assertFalse(mTitleTextView.isFocused());

        View.OnFocusChangeListener listener = (view, b) -> textFocusChanged.set(true);
        mModel.set(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER, listener);
        mTitleTextView.requestFocus();

        Assert.assertTrue(mTitleTextView.isFocused());
        Assert.assertTrue(textFocusChanged.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCursorVisibility() {
        mTitleTextView.setCursorVisible(false);

        mModel.set(TabGridDialogProperties.TITLE_CURSOR_VISIBILITY, true);

        Assert.assertTrue(mTitleTextView.isCursorVisible());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetIsTitleTextFocused() {
        Assert.assertFalse(mTitleTextView.isFocused());

        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, true);

        Assert.assertTrue(mTitleTextView.isFocused());

        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);

        Assert.assertFalse(mTitleTextView.isFocused());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetVisibilityListener() {
        mModel.set(
                TabGridDialogProperties.VISIBILITY_LISTENER,
                new VisibilityListener() {
                    @Override
                    public void finishedHidingDialogView() {}
                });

        Assert.assertNotNull(mTabGridDialogView.getVisibilityListenerForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetInitialScrollIndex() {
        mContentView.layout(0, 0, 100, 500);

        mModel.set(TabGridDialogProperties.INITIAL_SCROLL_INDEX, 5);

        verify(mLayoutManager, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .scrollToPositionWithOffset(
                        eq(5),
                        intThat(allOf(lessThan(mContentView.getHeight() / 2), greaterThan(0))));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetInitialScrollIndex_Linear() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLinearLayoutManager = spy(new LinearLayoutManager(getActivity()));
                    mContentView.setLayoutManager(mLinearLayoutManager);
                });
        mContentView.layout(0, 0, 100, 500);

        mModel.set(TabGridDialogProperties.INITIAL_SCROLL_INDEX, 5);

        verify(mLinearLayoutManager, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .scrollToPositionWithOffset(eq(5), eq(0));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testSetTabGroupColorIdAndIncognito() {
        int color = TabGroupColorId.GREY;

        mModel.set(TabGridDialogProperties.IS_INCOGNITO, false);
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, color);

        GradientDrawable drawable = (GradientDrawable) mColorIcon.getBackground();
        Assert.assertEquals(
                ColorStateList.valueOf(
                        ColorPickerUtils.getTabGroupColorPickerItemColor(
                                getActivity(), color, false)),
                drawable.getColor());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testSetColorIconClickListener() {
        AtomicBoolean colorIconClicked = new AtomicBoolean();
        colorIconClicked.set(false);
        mColorIconContainer.performClick();
        Assert.assertFalse(colorIconClicked.get());

        mModel.set(
                TabGridDialogProperties.COLOR_ICON_CLICK_LISTENER,
                (View view) -> colorIconClicked.set(true));

        mColorIconContainer.performClick();
        Assert.assertTrue(colorIconClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.FORCE_LIST_TAB_SWITCHER)
    public void testSetAnimationBackgroundColor() {
        int color = ContextCompat.getColor(getActivity(), R.color.baseline_primary_80);

        View cardView = mTabGridDialogView.findViewById(R.id.card_view);
        cardView.setBackground(mCardViewBackground);

        mModel.set(TabGridDialogProperties.ANIMATION_BACKGROUND_COLOR, color);

        verify(mCardViewBackground).setTint(color);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
    }
}
