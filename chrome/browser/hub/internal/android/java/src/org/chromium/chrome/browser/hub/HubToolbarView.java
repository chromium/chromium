// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;


import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.getPaneColorBlendInterpolator;
import static org.chromium.ui.util.ColorBlendAnimationFactory.createMultiColorBlendAnimation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.support.annotation.Px;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;
import androidx.core.widget.TextViewCompat;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.animation.AnimationHandler;

import java.util.List;

/** Toolbar for the Hub. May contain a single or multiple rows, of which this view is the parent. */
@NullMarked
public class HubToolbarView extends LinearLayout {
    private Button mActionButton;
    private TabLayout mPaneSwitcher;
    private LinearLayout mMenuButtonContainer;
    private ImageButton mMenuButton;
    private View mSearchBoxLayout;
    private EditText mSearchBoxTextView;
    private ImageView mSearchLoupeView;
    private FrameLayout mPaneSwitcherCard;

    private Callback<Integer> mToolbarOverviewColorSetter;
    private @Nullable OnTabSelectedListener mOnTabSelectedListener;
    private boolean mBlockTabSelectionCallback;
    private boolean mApplyDelayForSearchBoxAnimation;
    private final AnimationHandler mHubSearchAnimatorHandler;
    private final Handler mHandler;

    /** Default {@link LinearLayout} constructor called by inflation. */
    public HubToolbarView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mHubSearchAnimatorHandler = new AnimationHandler();
        mHandler = new Handler();
        mToolbarOverviewColorSetter = (color) -> {};
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionButton = findViewById(R.id.toolbar_action_button);
        mPaneSwitcher = findViewById(R.id.pane_switcher);
        ViewGroup slidingTabIndicator = (ViewGroup) mPaneSwitcher.getChildAt(0);
        // Unclip children here to get unbounded ripple to work.
        slidingTabIndicator.setClipToPadding(false);
        slidingTabIndicator.setClipChildren(false);
        mMenuButtonContainer = findViewById(R.id.menu_button_container);
        mMenuButton = mMenuButtonContainer.findViewById(R.id.menu_button);
        mPaneSwitcherCard = findViewById(R.id.pane_switcher_card);

        // SearchBoxLayout is GONE by default, and enabled via the mediator.
        mSearchBoxLayout = findViewById(R.id.search_box);
        mSearchBoxTextView = findViewById(R.id.search_box_text);
        mSearchLoupeView = findViewById(R.id.search_loupe);

        setTouchDelegate(getToolbarActionButtonDelegate());
    }

    void setMenuButtonVisible(boolean visible) {
        mMenuButtonContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    void setActionButton(@Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, mActionButton);
        mActionButton.setText(null);
        mActionButton.setCompoundDrawablePadding(0);

        if (HubUtils.isGtsUpdateEnabled()) {
            int paddingLR =
                    getResources()
                            .getDimensionPixelSize(R.dimen.hub_toolbar_action_button_padding_lr);
            mActionButton.setPadding(paddingLR, 0, paddingLR, 0);

            int buttonSize =
                    getResources().getDimensionPixelSize(R.dimen.hub_toolbar_action_button_size);
            FrameLayout.LayoutParams params =
                    (FrameLayout.LayoutParams) mActionButton.getLayoutParams();
            params.leftMargin =
                    getResources()
                            .getDimensionPixelSize(R.dimen.hub_toolbar_action_button_left_margin);
            params.width = buttonSize;
            params.height = buttonSize;
            params.gravity = Gravity.START | Gravity.CENTER_VERTICAL;
            mActionButton.setLayoutParams(params);
        }
    }

    void setPaneSwitcherButtonData(
            @Nullable List<FullButtonData> buttonDataList, int selectedIndex) {
        // Null can safely be passed here.
        mPaneSwitcher.removeOnTabSelectedListener(assumeNonNull(mOnTabSelectedListener));
        mPaneSwitcher.removeAllTabs();

        if (buttonDataList == null || buttonDataList.size() <= 1) {
            mPaneSwitcher.setVisibility(View.GONE);
            mOnTabSelectedListener = null;
        } else {
            Context context = getContext();
            Resources resources = getResources();
            @Px
            int tabItemPadding =
                    resources.getDimensionPixelSize(R.dimen.hub_pane_switcher_tab_item_padding);
            @Px
            int tabItemMargin =
                    resources.getDimensionPixelSize(
                            R.dimen.hub_pane_switcher_tab_item_horizontal_margin);
            @Px
            int tabItemHeight = resources.getDimensionPixelSize(R.dimen.hub_pane_switcher_tab_size);
            for (FullButtonData buttonData : buttonDataList) {
                Tab tab = mPaneSwitcher.newTab();

                // TODO(crbug.com/40286849): Conditionally use text instead.
                Drawable drawable = buttonData.resolveIcon(context);
                tab.setIcon(drawable);
                tab.setContentDescription(buttonData.resolveContentDescription(context));
                // Unclip children here to get unbounded ripple to work.
                tab.view.setClipChildren(false);
                tab.view.setClipToPadding(false);
                mPaneSwitcher.addTab(tab);

                if (HubUtils.isGtsUpdateEnabled()) {
                    LinearLayout.LayoutParams tabLayoutParams =
                            (LinearLayout.LayoutParams) tab.view.getLayoutParams();

                    tabLayoutParams.height = tabItemHeight;
                    tabLayoutParams.leftMargin = tabItemMargin;
                    tabLayoutParams.rightMargin = tabItemMargin;
                    tabLayoutParams.gravity = Gravity.CENTER;
                    tab.view.setLayoutParams(tabLayoutParams);
                    tab.view.setPadding(
                            tabItemPadding, tabItemPadding, tabItemPadding, tabItemPadding);
                    tab.view.setBackground(buildBackgroundDrawableForTab());
                }
            }
            mPaneSwitcher.setVisibility(View.VISIBLE);
            mOnTabSelectedListener = makeTabSelectedListener(buttonDataList);
            mPaneSwitcher.addOnTabSelectedListener(mOnTabSelectedListener);

            if (HubUtils.isGtsUpdateEnabled()) {
                @Px
                int paneSwitcherHorizontalPadding =
                        resources.getDimensionPixelSize(
                                R.dimen.hub_pane_switcher_horizontal_padding);
                @Px
                int paneSwitcherVerticalPadding =
                        resources.getDimensionPixelSize(R.dimen.hub_pane_switcher_vertical_padding);
                mPaneSwitcherCard.setPaddingRelative(
                        paneSwitcherHorizontalPadding,
                        paneSwitcherVerticalPadding,
                        paneSwitcherHorizontalPadding,
                        paneSwitcherVerticalPadding);

                // Pane switcher in new design needs to be center aligned in the toolbar.
                FrameLayout.LayoutParams params =
                        (FrameLayout.LayoutParams) mPaneSwitcherCard.getLayoutParams();
                params.gravity = Gravity.CENTER;
                params.height =
                        resources.getDimensionPixelSize(R.dimen.hub_pane_switcher_card_height);
                mPaneSwitcherCard.setLayoutParams(params);

                mPaneSwitcher.setTabIndicatorAnimationMode(
                        TabLayout.INDICATOR_ANIMATION_MODE_LINEAR);
                mPaneSwitcher.setSelectedTabIndicatorGravity(TabLayout.INDICATOR_GRAVITY_CENTER);
                mPaneSwitcher.setSelectedTabIndicator(
                        AppCompatResources.getDrawable(
                                context, R.drawable.hub_pane_switcher_item_selector));
            }
        }

        setPaneSwitcherIndex(selectedIndex);
    }

    void setPaneSwitcherIndex(int index) {
        @Nullable Tab tab = mPaneSwitcher.getTabAt(index);
        if (tab == null) return;

        // Setting the selected tab should never trigger the callback.
        mBlockTabSelectionCallback = true;
        tab.select();
        mBlockTabSelectionCallback = false;
    }

    void setColorMixer(HubColorMixer mixer) {
        registerColorBlends(mixer);
        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            registerSearchBoxColorBlends(mixer);
        }
    }

    private void registerColorBlends(HubColorMixer mixer) {
        Context context = getContext();
        boolean isGtsUpdateEnabled = HubUtils.isGtsUpdateEnabled();

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        this::setBackgroundColor));

        if (isGtsUpdateEnabled) {
            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme ->
                                    HubColors.getToolbarActionButtonIconColor(context, colorScheme),
                            color -> updateActionButtonIconColorInternal(context, color)));

            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme ->
                                    HubColors.getToolbarActionButtonBackgroundColor(
                                            context, colorScheme),
                            this::updateActionButtonColorInternal));
        }

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> {
                            if (isGtsUpdateEnabled) {
                                return HubColors.geTabItemSelectorColor(context, colorScheme);
                            } else {
                                return HubColors.getSelectedIconColor(
                                        context, colorScheme, /* isGtsUpdateEnabled= */ false);
                            }
                        },
                        mPaneSwitcher::setSelectedTabIndicatorColor));

        HubViewColorBlend multiColorBlend =
                (prevColorScheme, newColorScheme) -> {
                    @ColorInt int newIconColor = HubColors.getIconColor(context, newColorScheme);
                    @ColorInt
                    int newSelectedIconColor =
                            HubColors.getSelectedIconColor(
                                    context, newColorScheme, isGtsUpdateEnabled);
                    @ColorInt int prevIconColor = HubColors.getIconColor(context, prevColorScheme);
                    @ColorInt
                    int prevSelectedIconColor =
                            HubColors.getSelectedIconColor(
                                    context, prevColorScheme, isGtsUpdateEnabled);
                    Animator animation =
                            createMultiColorBlendAnimation(
                                    PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                                    new int[] {prevIconColor, prevSelectedIconColor},
                                    new int[] {newIconColor, newSelectedIconColor},
                                    colorList -> {
                                        @ColorInt int interpolatedIconColor = colorList[0];
                                        @ColorInt int interpolatedSelectedIconColor = colorList[1];
                                        updateTabIconTintInternal(
                                                interpolatedIconColor,
                                                interpolatedSelectedIconColor);
                                    });
                    animation.setInterpolator(getPaneColorBlendInterpolator());
                    return animation;
                };
        mixer.registerBlend(multiColorBlend);

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getIconColor(context, colorScheme),
                        interpolatedColor -> {
                            if (!isGtsUpdateEnabled) {
                                updateActionButtonIconColorInternal(context, interpolatedColor);
                            }
                            ColorStateList menuButtonColor =
                                    ColorStateList.valueOf(interpolatedColor);
                            ImageViewCompat.setImageTintList(mMenuButton, menuButtonColor);
                        }));

        // We don't want to pass a method reference. Lambdas will ensure we run the most recent
        // setter.
        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        color -> mToolbarOverviewColorSetter.onResult(color)));

        if (isGtsUpdateEnabled) {
            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme ->
                                    HubColors.getPaneSwitcherBackgroundColor(context, colorScheme),
                            color -> {
                                ColorFilter filter =
                                        new PorterDuffColorFilter(color, PorterDuff.Mode.SRC);
                                mPaneSwitcherCard.getBackground().setColorFilter(filter);
                            }));

            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme ->
                                    HubColors.getPaneSwitcherTabItemHoverColor(
                                            context, colorScheme),
                            color -> updateTabItemBackgroundColor(context, color)));
        }
    }

    private void registerSearchBoxColorBlends(HubColorMixer mixer) {
        Context context = getContext();

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getSearchBoxHintTextColor(context, colorScheme),
                        mSearchBoxTextView::setHintTextColor));

        GradientDrawable backgroundDrawable =
                (GradientDrawable) mSearchBoxLayout.getBackground().mutate();
        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getSearchBoxBgColor(context, colorScheme),
                        backgroundDrawable::setColor));

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getIconColor(context, colorScheme),
                        this::updateSearchLoupeColor));
    }

    private void updateTabIconTintInternal(
            @ColorInt int iconColor, @ColorInt int selectedIconColor) {
        ColorStateList selectableIconList =
                HubColors.getSelectableIconList(selectedIconColor, iconColor);
        mPaneSwitcher.setTabIconTint(selectableIconList);
    }

    private void updateActionButtonIconColorInternal(Context context, @ColorInt int color) {
        ColorStateList actionButtonColor = HubColors.getActionButtonColor(context, color);
        TextViewCompat.setCompoundDrawableTintList(mActionButton, actionButtonColor);
    }

    private void updateActionButtonColorInternal(@ColorInt int color) {
        mActionButton.setBackgroundTintList(ColorStateList.valueOf(color));
    }

    private void updateSearchLoupeColor(@ColorInt int color) {
        ColorStateList colorStateList = ColorStateList.valueOf(color);
        mSearchLoupeView.setImageTintList(colorStateList);
    }

    private void updateTabItemBackgroundColor(Context context, @ColorInt int color) {
        ColorStateList colorStateList =
                HubColors.generateHoveredStateColorStateList(context, color);
        for (int i = 0; i < mPaneSwitcher.getTabCount(); i++) {
            View tabView = getButtonView(i);
            if (tabView != null) {
                GradientDrawable background = (GradientDrawable) tabView.getBackground();
                background.setColor(colorStateList);
            }
        }
    }

    void setButtonLookupConsumer(Callback<PaneButtonLookup> lookupConsumer) {
        lookupConsumer.onResult(this::getButtonView);
    }

    void setSearchBoxVisible(boolean visible) {
        AnimatorSet hubSearchTransitionAnimation = getHubSearchBoxTransitionAnimation(visible);
        AnimatorListenerAdapter animationListener =
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        if (visible) {
                            mSearchBoxLayout.setVisibility(View.VISIBLE);
                        }
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (!visible) {
                            mSearchBoxLayout.setVisibility(View.GONE);
                        }
                    }
                };
        hubSearchTransitionAnimation.addListener(animationListener);
        mHubSearchAnimatorHandler.startAnimation(hubSearchTransitionAnimation);
    }

    void setHubSearchEnabledState(boolean enabled) {
        mSearchBoxLayout.setEnabled(enabled);
        mSearchBoxTextView.setEnabled(enabled);
        mSearchLoupeView.setEnabled(enabled);
    }

    void setApplyDelayForSearchBoxAnimation(boolean applyDelay) {
        mApplyDelayForSearchBoxAnimation = applyDelay;
    }

    public void setSearchLoupeVisible(boolean visible) {
        mSearchLoupeView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setSearchListener(Runnable searchBarListener) {
        mSearchBoxLayout.setOnClickListener(v -> searchBarListener.run());
        mSearchBoxTextView.setOnClickListener(v -> searchBarListener.run());
        mSearchLoupeView.setOnClickListener(v -> searchBarListener.run());
    }

    void setToolbarColorOverviewListener(Callback<Integer> colorSetter) {
        mToolbarOverviewColorSetter = colorSetter;
    }

    void updateIncognitoElements(boolean isIncognito) {
        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            updateSearchBoxElements(isIncognito);
        }
    }

    private @Nullable View getButtonView(int index) {
        Tab tab = mPaneSwitcher.getTabAt(index);
        return tab == null ? null : tab.view;
    }

    private OnTabSelectedListener makeTabSelectedListener(List<FullButtonData> buttonDataList) {
        return new OnTabSelectedListener() {
            @Override
            public void onTabSelected(Tab tab) {
                if (!mBlockTabSelectionCallback) {
                    assumeNonNull(buttonDataList.get(tab.getPosition()).getOnPressRunnable()).run();
                }
            }

            @Override
            public void onTabUnselected(Tab tab) {}

            @Override
            public void onTabReselected(Tab tab) {}
        };
    }

    private void updateSearchBoxElements(boolean isIncognito) {
        Context context = getContext();
        @StringRes
        int emptyHintRes =
                isIncognito
                        ? R.string.hub_search_empty_hint_incognito
                        : R.string.hub_search_empty_hint;

        // Delay the text from changing until the hub search animation is finished to prevent the
        // incorrect text from showing too early on pane toggles.
        if (mApplyDelayForSearchBoxAnimation) {
            mHandler.postDelayed(
                    () -> {
                        mSearchBoxTextView.setHint(context.getString(emptyHintRes));
                    },
                    PANE_FADE_ANIMATION_DURATION_MS);
        } else {
            mSearchBoxTextView.setHint(context.getString(emptyHintRes));
        }
    }

    private AnimatorSet getHubSearchBoxTransitionAnimation(boolean visible) {
        float fadeAlphaFrom = visible ? 0 : 1;
        float fadeAlphaTo = visible ? 1 : 0;
        float slideTransitionY = visible ? 0 : -mSearchBoxLayout.getHeight();
        Animator fade =
                ObjectAnimator.ofFloat(mSearchBoxLayout, View.ALPHA, fadeAlphaFrom, fadeAlphaTo);
        Animator slide =
                ObjectAnimator.ofFloat(mSearchBoxLayout, View.TRANSLATION_Y, slideTransitionY);
        AnimatorSet slideFadeHubSearchBoxAnimator = new AnimatorSet();
        slideFadeHubSearchBoxAnimator.play(slide).with(fade);
        slideFadeHubSearchBoxAnimator.setDuration(PANE_FADE_ANIMATION_DURATION_MS);
        return slideFadeHubSearchBoxAnimator;
    }

    private TouchDelegate getToolbarActionButtonDelegate() {
        Rect rect = new Rect();
        mActionButton.getHitRect(rect);

        int touchSize =
                mActionButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.min_touch_target_size);
        int halfWidthDelta = Math.max((touchSize - mActionButton.getWidth()) / 2, 0);
        int halfHeightDelta = Math.max((touchSize - mActionButton.getHeight()) / 2, 0);

        rect.left -= halfWidthDelta;
        rect.right += halfWidthDelta;
        rect.top -= halfHeightDelta;
        rect.bottom += halfHeightDelta;

        return new TouchDelegate(rect, mActionButton);
    }

    private GradientDrawable buildBackgroundDrawableForTab() {
        int radius = getResources().getDimensionPixelSize(R.dimen.hub_pane_switcher_tab_radius);
        GradientDrawable hoverDrawable = new GradientDrawable();

        hoverDrawable.setShape(GradientDrawable.RECTANGLE);
        hoverDrawable.setCornerRadius(radius);
        return hoverDrawable;
    }
}
