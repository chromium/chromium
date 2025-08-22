// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.DATA_SHARING;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.DATA_SHARING_JOIN_ONLY;

import android.app.Activity;
import android.content.res.ColorStateList;
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

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link TabGridDialogViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures({DATA_SHARING, DATA_SHARING_JOIN_ONLY})
@Batch(Batch.PER_CLASS)
public class TabGridDialogViewBinderTest {
    private static final int CONTENT_TOP_MARGIN = 56;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PropertyModel mModel;
    private TabGridDialogToolbarView mToolbarView;
    private TabListRecyclerView mContentView;
    private TabGridDialogView mTabGridDialogView;
    private ChromeImageView mNewTabButton;
    private ChromeImageView mBackButton;
    private EditText mTitleTextView;
    private FrameLayout mColorIconContainer;
    private ImageView mColorIcon;
    private View mMainContent;
    private @Nullable View mShareButtonContainer;
    private @Nullable ButtonCompat mShareButton;
    private @Nullable View mImageTilesContainer;
    private @Nullable View mSendFeedbackButton;
    private ImageView mHairline;
    private ScrimManager mScrimManager;
    private GridLayoutManager mLayoutManager;
    private LinearLayoutManager mLinearLayoutManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private View.OnClickListener mOnClickListener;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout parentView = new FrameLayout(sActivity);
                    sActivity.setContentView(parentView);
                    mContentView =
                            (TabListRecyclerView)
                                    LayoutInflater.from(sActivity)
                                            .inflate(
                                                    R.layout.tab_list_recycler_view_layout,
                                                    parentView,
                                                    false);
                    mLayoutManager = spy(new GridLayoutManager(sActivity, 2));
                    mContentView.setLayoutManager(mLayoutManager);
                    boolean isDataSharingEnabled = ChromeFeatureList.isEnabled(DATA_SHARING);
                    @LayoutRes
                    int toolbar_res_id =
                            isDataSharingEnabled
                                    ? R.layout.tab_grid_dialog_toolbar_two_row
                                    : R.layout.tab_grid_dialog_toolbar;
                    mToolbarView =
                            (TabGridDialogToolbarView)
                                    LayoutInflater.from(sActivity)
                                            .inflate(toolbar_res_id, mContentView, false);
                    LayoutInflater.from(sActivity)
                            .inflate(R.layout.tab_grid_dialog_layout, parentView, true);
                    mTabGridDialogView = parentView.findViewById(R.id.dialog_parent_view);

                    mHairline = mTabGridDialogView.findViewById(R.id.tab_grid_dialog_hairline);
                    mSendFeedbackButton =
                            mTabGridDialogView.findViewById(R.id.send_feedback_button);
                    mBackButton = mToolbarView.findViewById(R.id.toolbar_back_button);
                    mNewTabButton = mToolbarView.findViewById(R.id.toolbar_new_tab_button);
                    mTitleTextView = mToolbarView.findViewById(R.id.title);
                    mColorIconContainer =
                            mToolbarView.findViewById(R.id.tab_group_color_icon_container);
                    mColorIcon = mToolbarView.findViewById(R.id.tab_group_color_icon);
                    mMainContent = mToolbarView.findViewById(R.id.main_content);
                    mShareButtonContainer = mToolbarView.findViewById(R.id.share_button_container);
                    mShareButton = mToolbarView.findViewById(R.id.share_button);
                    mImageTilesContainer = mToolbarView.findViewById(R.id.image_tiles_container);
                    if (isDataSharingEnabled) {
                        assertNotNull(mShareButtonContainer);
                        assertNotNull(mShareButton);
                        assertNotNull(mImageTilesContainer);
                    } else {
                        assertNull(mShareButtonContainer);
                        assertNull(mShareButton);
                        assertNull(mImageTilesContainer);
                    }
                    mScrimManager = new ScrimManager(sActivity, parentView, ScrimClient.NONE);
                    mTabGridDialogView.setupScrimManager(mScrimManager);

                    mModel =
                            new PropertyModel.Builder(TabGridDialogProperties.ALL_KEYS)
                                    .with(
                                            TabGridDialogProperties.BROWSER_CONTROLS_STATE_PROVIDER,
                                            mBrowserControlsStateProvider)
                                    .build();

                    PropertyModelChangeProcessor.create(
                            mModel,
                            new TabGridDialogViewBinder.ViewHolder(
                                    mToolbarView, mContentView, mTabGridDialogView),
                            TabGridDialogViewBinder::bind);
                });
    }

    @After
    public void tearDown() {
        // The verification that a mock is broken or misused is by default on the next verify which
        // might escape the current testcase causing other tests in the suite to fail. By putting
        // this here any flakes should be contained to the testcase in which they are caused.
        validateMockitoUsage();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCollapseClickListener() {
        AtomicBoolean leftButtonClicked = new AtomicBoolean();
        leftButtonClicked.set(false);
        mBackButton.performClick();
        assertFalse(leftButtonClicked.get());

        mModel.set(
                TabGridDialogProperties.COLLAPSE_CLICK_LISTENER,
                (View view) -> leftButtonClicked.set(true));

        mBackButton.performClick();
        assertTrue(leftButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAddClickListener() {
        AtomicBoolean rightButtonClicked = new AtomicBoolean();
        rightButtonClicked.set(false);
        mNewTabButton.performClick();
        assertFalse(rightButtonClicked.get());

        mModel.set(
                TabGridDialogProperties.ADD_CLICK_LISTENER,
                (View view) -> rightButtonClicked.set(true));

        mNewTabButton.performClick();
        assertTrue(rightButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetHeaderTitle() {
        String title = "1024 tabs";
        assertNotEquals(title, mTitleTextView.getText().toString());

        mModel.set(TabGridDialogProperties.HEADER_TITLE, title);

        assertEquals(title, mTitleTextView.getText().toString());
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
        assertEquals(0, ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);

        mModel.set(TabGridDialogProperties.CONTENT_TOP_MARGIN, CONTENT_TOP_MARGIN);

        assertEquals(
                CONTENT_TOP_MARGIN,
                ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetPrimaryColor() {
        int color = ContextCompat.getColor(sActivity, R.color.baseline_primary_80);

        mModel.set(TabGridDialogProperties.PRIMARY_COLOR, color);

        assertEquals(color, ((ColorDrawable) mMainContent.getBackground()).getColor());
        assertEquals(color, ((ColorDrawable) mContentView.getBackground()).getColor());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTint() {
        ColorStateList tint = ThemeUtils.getThemedToolbarIconTint(sActivity, true);
        assertNotEquals(tint, mBackButton.getImageTintList());
        assertNotEquals(tint, mNewTabButton.getImageTintList());
        assertNotEquals(tint, mTitleTextView.getTextColors());

        mModel.set(TabGridDialogProperties.TINT, tint);

        assertEquals(tint, mBackButton.getImageTintList());
        assertEquals(tint, mNewTabButton.getImageTintList());
        assertEquals(tint, mTitleTextView.getTextColors());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetScrimViewObserver() {
        AtomicBoolean scrimViewClicked = new AtomicBoolean();
        scrimViewClicked.set(false);
        Runnable scrimClickRunnable = () -> scrimViewClicked.set(true);

        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, scrimClickRunnable);
        // Open the dialog to show the ScrimView.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        View scrimView = mScrimManager.getViewForTesting();
        scrimView.performClick();
        assertTrue(scrimViewClicked.get());
    }

    @Test
    @SmallTest
    public void testSetDialogVisibility() {
        assertNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());

        // Setup basic dialog animation and a fake scrim view click runnable. These are always
        // initialized before the visibility of dialog is set.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                    mTabGridDialogView.setScrimClickRunnable(() -> {});
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true));

        if (!AccessibilityState.prefersReducedMotion()) {
            assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
        }
        assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false));

        if (!AccessibilityState.prefersReducedMotion()) {
            assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
        }
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        assertEquals(View.GONE, mTabGridDialogView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAnimationSourceView() {
        // When set animation source view as null, the show animation is set to be basic fade-in
        // which contains only one animation in animation set.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        assertEquals(
                1,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());

        // Create a placeholder source view to setup the dialog animation.
        ViewGroup sourceViewParent = new FrameLayout(sActivity);
        View sourceView = new View(sActivity);
        sourceViewParent.addView(sourceView);

        // When set with a specific animation source view, the show animation contains 6 child
        // animations.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, sourceView);
        assertEquals(
                7,
                mTabGridDialogView.getShowDialogAnimationForTesting().getChildAnimations().size());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarStatus() {
        mModel.set(
                TabGridDialogProperties.UNGROUP_BAR_STATUS,
                TabGridDialogView.UngroupBarStatus.SHOW);
        assertEquals(
                TabGridDialogView.UngroupBarStatus.SHOW,
                mTabGridDialogView.getUngroupBarStatusForTesting());

        mModel.set(
                TabGridDialogProperties.UNGROUP_BAR_STATUS,
                TabGridDialogView.UngroupBarStatus.HOVERED);
        assertEquals(
                TabGridDialogView.UngroupBarStatus.HOVERED,
                mTabGridDialogView.getUngroupBarStatusForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetDialogBackgroundColor() {
        int incognitoColor =
                ContextCompat.getColor(sActivity, R.color.gm3_baseline_surface_container_low_dark);

        mModel.set(TabGridDialogProperties.DIALOG_BACKGROUND_COLOR, incognitoColor);

        assertEquals(incognitoColor, mTabGridDialogView.getBackgroundColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarBackgroundColor() {
        int incognitoColor =
                ContextCompat.getColor(sActivity, R.color.gm3_baseline_surface_container_low_dark);

        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR, incognitoColor);

        assertEquals(incognitoColor, mTabGridDialogView.getUngroupBarBackgroundColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarHoveredBackgroundColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        sActivity, R.color.incognito_tab_grid_dialog_ungroup_bar_bg_hovered_color);

        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR,
                incognitoColor);

        assertEquals(
                incognitoColor, mTabGridDialogView.getUngroupBarHoveredBackgroundColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarTextColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        sActivity, R.color.incognito_tab_grid_dialog_ungroup_bar_text_color);

        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT_COLOR, incognitoColor);

        assertEquals(incognitoColor, mTabGridDialogView.getUngroupBarTextColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarHoveredTextColor() {
        int incognitoColor =
                ContextCompat.getColor(
                        sActivity,
                        R.color.incognito_tab_grid_dialog_ungroup_bar_text_hovered_color);

        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR, incognitoColor);

        assertEquals(incognitoColor, mTabGridDialogView.getUngroupBarHoveredTextColorForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetMainContentVisibility() {
        mContentView.setVisibility(View.INVISIBLE);
        assertEquals(View.INVISIBLE, mContentView.getVisibility());

        mModel.set(TabGridDialogProperties.IS_MAIN_CONTENT_VISIBLE, true);

        assertEquals(View.VISIBLE, mContentView.getVisibility());
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
        assertTrue(titleTextUpdated.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextOnFocusListener() {
        AtomicBoolean textFocusChanged = new AtomicBoolean();
        textFocusChanged.set(false);
        assertFalse(mTitleTextView.isFocused());

        View.OnFocusChangeListener listener = (view, b) -> textFocusChanged.set(true);
        mModel.set(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER, listener);
        mTitleTextView.requestFocus();

        assertTrue(mTitleTextView.isFocused());
        assertTrue(textFocusChanged.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCursorVisibility() {
        mTitleTextView.setCursorVisible(false);

        mModel.set(TabGridDialogProperties.TITLE_CURSOR_VISIBILITY, true);

        assertTrue(mTitleTextView.isCursorVisible());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetIsTitleTextFocused() {
        assertFalse(mTitleTextView.isFocused());

        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, true);

        assertTrue(mTitleTextView.isFocused());

        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);

        assertFalse(mTitleTextView.isFocused());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetVisibilityListener() {
        mModel.set(TabGridDialogProperties.VISIBILITY_LISTENER, () -> {});
        assertNotNull(mTabGridDialogView.getVisibilityListenerForTesting());

        mModel.set(TabGridDialogProperties.VISIBILITY_LISTENER, null);
        assertNull(mTabGridDialogView.getVisibilityListenerForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @RequiresRestart(
            "Changing the layout size must remain scoped to this testcase otherwise "
                    + "other tests in the suite may break. See https://crbug.com/363298801.")
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
    @EnableFeatures(DATA_SHARING)
    public void testShareButton() {
        mModel.set(
                TabGridDialogProperties.SHARE_BUTTON_STRING_RES,
                R.string.tab_grid_share_button_text);
        mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, false);
        mModel.set(TabGridDialogProperties.SHARE_BUTTON_CLICK_LISTENER, mOnClickListener);

        assertEquals(View.GONE, mShareButtonContainer.getVisibility());
        assertEquals("Share", mShareButton.getText());

        mModel.set(
                TabGridDialogProperties.SHARE_BUTTON_STRING_RES,
                R.string.tab_grid_manage_button_text);
        mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, true);
        assertEquals(View.VISIBLE, mShareButtonContainer.getVisibility());
        assertEquals(View.VISIBLE, mShareButton.getVisibility());
        assertEquals("Manage", mShareButton.getText());

        mShareButton.performClick();

        verify(mOnClickListener).onClick(any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(DATA_SHARING)
    public void testImageTiles_NonIncognito() {
        mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, false);
        mModel.set(TabGridDialogProperties.SHARE_IMAGE_TILES_CLICK_LISTENER, mOnClickListener);

        assertEquals(View.GONE, mImageTilesContainer.getVisibility());

        mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, true);
        assertEquals(View.VISIBLE, mImageTilesContainer.getVisibility());

        mImageTilesContainer.performClick();

        verify(mOnClickListener).onClick(any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @RequiresRestart(
            "Changing the LayoutManager and size must remain scoped to this testcase otherwise "
                    + "other tests in the suite may break. See https://crbug.com/363298801.")
    public void testSetInitialScrollIndex_Linear() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLinearLayoutManager = spy(new LinearLayoutManager(sActivity));
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
    public void testHairline() {
        mContentView.layout(0, 0, 100, 500);

        assertEquals(View.GONE, mHairline.getVisibility());

        mModel.set(TabGridDialogProperties.HAIRLINE_VISIBILITY, true);
        assertEquals(View.VISIBLE, mHairline.getVisibility());

        mModel.set(TabGridDialogProperties.HAIRLINE_VISIBILITY, false);
        assertEquals(View.GONE, mHairline.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTabGroupColorIdAndIncognito() {
        int color = TabGroupColorId.GREY;

        mModel.set(TabGridDialogProperties.IS_INCOGNITO, false);
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, color);

        GradientDrawable drawable = (GradientDrawable) mColorIcon.getBackground();
        assertEquals(
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                                sActivity, color, false)),
                drawable.getColor());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetColorIconClickListener() {
        AtomicBoolean colorIconClicked = new AtomicBoolean();
        colorIconClicked.set(false);
        mColorIconContainer.performClick();
        assertFalse(colorIconClicked.get());

        mModel.set(
                TabGridDialogProperties.COLOR_ICON_CLICK_LISTENER,
                (View view) -> colorIconClicked.set(true));

        mColorIconContainer.performClick();
        assertTrue(colorIconClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAppHeaderHeight() {
        int appHeaderHeight = 10;
        mModel.set(TabGridDialogProperties.APP_HEADER_HEIGHT, appHeaderHeight);
        assertEquals(appHeaderHeight, mTabGridDialogView.getAppHeaderHeightForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testSetIsContentSensitive() {
        mModel.set(TabGridDialogProperties.IS_CONTENT_SENSITIVE, true);
        assertEquals(
                View.CONTENT_SENSITIVITY_SENSITIVE, mTabGridDialogView.getContentSensitivity());
        mModel.set(TabGridDialogProperties.IS_CONTENT_SENSITIVE, false);
        assertEquals(
                View.CONTENT_SENSITIVITY_NOT_SENSITIVE, mTabGridDialogView.getContentSensitivity());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSendFeedback() {
        // Default state should be "safe".
        assertEquals(View.GONE, mSendFeedbackButton.getVisibility());
        mSendFeedbackButton.callOnClick();

        // Test visibility.
        mModel.set(TabGridDialogProperties.SHOW_SEND_FEEDBACK, true);
        assertEquals(View.VISIBLE, mSendFeedbackButton.getVisibility());

        mModel.set(TabGridDialogProperties.SHOW_SEND_FEEDBACK, false);
        assertEquals(View.GONE, mSendFeedbackButton.getVisibility());

        // Test click listener.
        Runnable r = mock(Runnable.class);
        mModel.set(TabGridDialogProperties.SEND_FEEDBACK_RUNNABLE, r);
        mSendFeedbackButton.callOnClick();
        verify(r).run();

        // Null should not crash.
        mModel.set(TabGridDialogProperties.SEND_FEEDBACK_RUNNABLE, null);
        mSendFeedbackButton.callOnClick();
    }
}
