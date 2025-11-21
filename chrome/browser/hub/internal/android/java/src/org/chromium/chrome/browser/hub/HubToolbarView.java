// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS;
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
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.interpolators.Interpolators;

import java.util.List;

/** Toolbar for the Hub. May contain a single or multiple rows, of which this view is the parent. */
@NullMarked
public class HubToolbarView extends LinearLayout {
    private TabLayout mPaneSwitcher;
    private LinearLayout mMenuButtonContainer;
    private ImageButton mMenuButton;
    private MenuButton mMenuButtonWrapper;
    private View mSearchBoxLayout;
    private EditText mSearchBoxTextView;
    private ImageView mSearchLoupeView;
    private ImageView mHairline;
    private ImageButton mBackButton;
    private @Nullable View mSpacer;
    private FrameLayout mPaneSwitcherCard;

    private Callback<Integer> mToolbarOverviewColorSetter;
    private @Nullable OnTabSelectedListener mOnTabSelectedListener;
    private boolean mBlockTabSelectionCallback;
    private boolean mApplyDelayForSearchBoxAnimation;
    private final AnimationHandler mHubSearchAnimatorHandler;
    private final Handler mHandler;
    private @Nullable ObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private @Nullable List<FullButtonData> mCachedButtonDataList;

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
        mPaneSwitcher = findViewById(R.id.pane_switcher);
        ViewGroup slidingTabIndicator = (ViewGroup) mPaneSwitcher.getChildAt(0);
        // Unclip children here to get unbounded ripple to work.
        slidingTabIndicator.setClipToPadding(false);
        slidingTabIndicator.setClipChildren(false);
        mMenuButtonContainer = findViewById(R.id.menu_button_container);
        mMenuButton = mMenuButtonContainer.findViewById(R.id.menu_button);
        mMenuButtonWrapper = mMenuButtonContainer.findViewById(R.id.menu_button_wrapper);
        mPaneSwitcherCard = findViewById(R.id.pane_switcher_card);

        // SearchBoxLayout is GONE by default, and enabled via the mediator.
        mSearchBoxLayout = findViewById(R.id.search_box);
        mSearchBoxTextView = findViewById(R.id.search_box_text);
        mSearchLoupeView = findViewById(R.id.search_loupe);
        mBackButton = findViewById(R.id.toolbar_back_button);
        mSpacer = findViewById(R.id.margin_spacer);
        mHairline = findViewById(R.id.toolbar_bottom_hairline);
        updateSpacerVisibility();
    }

    void setMenuButtonVisible(boolean visible) {
        if (OmniboxFeatures.sAndroidHubSearchTabGroups.isEnabled()
                && OmniboxFeatures.sAndroidHubSearchEnableOnTabGroupsPane.getValue()) {
            mMenuButtonWrapper.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        } else {
            mMenuButtonContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        }
    }

    void setPaneSwitcherButtonData(
            @Nullable List<FullButtonData> buttonDataList, int selectedIndex) {
        if (canPerformPaneSwitcherUpdate(buttonDataList)) {
            updatePaneSwitcherButtonList(assumeNonNull(buttonDataList), selectedIndex);
        } else {
            buildPaneSwitcherButtonList(buttonDataList, selectedIndex);
        }

        mCachedButtonDataList = buttonDataList;
    }

    private void updatePaneSwitcherButtonData(FullButtonData buttonData, int index) {
        // Currently, we only support in-place updates for panel switchers. Therefore, the index
        // should remain valid.
        assert index < mPaneSwitcher.getTabCount();

        Tab tab = mPaneSwitcher.getTabAt(index);
        assertNonNull(tab);

        Context context = getContext();
        Drawable drawable = buttonData.resolveIcon(context);
        tab.setIcon(drawable);
        tab.setContentDescription(buttonData.resolveContentDescription(context));
    }

    /**
     * Determines if we can perform an update of the existing pane switcher button list instead of
     * rebuilding it. We can only do this if the new list is non-null, the same size as the existing
     * list, and has more than one button.
     */
    private boolean canPerformPaneSwitcherUpdate(@Nullable List<FullButtonData> newList) {
        if (mCachedButtonDataList == null || newList == null) return false;

        // Currently, we only support updating the pane switcher when the button count remains
        // unchanged. If there exist more pane addition/creation cases, we should revisit this.
        if (mCachedButtonDataList.size() != newList.size()) return false;

        return newList.size() > 1;
    }

    private void updatePaneSwitcherButtonList(
            List<FullButtonData> buttonDataList, int selectedIndex) {
        for (int i = 0; i < buttonDataList.size(); i++) {
            FullButtonData newButtonData = buttonDataList.get(i);
            FullButtonData oldButtonData = assumeNonNull(mCachedButtonDataList).get(i);

            if (!oldButtonData.buttonDataEquals(newButtonData)) {
                updatePaneSwitcherButtonData(newButtonData, i);
            }
        }

        setPaneSwitcherTabSelectedListenerAndSetIndex(buttonDataList, selectedIndex);
    }

    private void buildPaneSwitcherButtonList(
            @Nullable List<FullButtonData> buttonDataList, int selectedIndex) {
        // Null can safely be passed here.
        mPaneSwitcher.removeAllTabs();

        if (buttonDataList == null || buttonDataList.size() <= 1) {
            mPaneSwitcher.setVisibility(View.GONE);
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

        setPaneSwitcherTabSelectedListenerAndSetIndex(buttonDataList, selectedIndex);
    }

    private void setPaneSwitcherTabSelectedListenerAndSetIndex(
            @Nullable List<FullButtonData> buttonDataList, int selectedIndex) {
        if (mOnTabSelectedListener != null) {
            mPaneSwitcher.removeOnTabSelectedListener(mOnTabSelectedListener);
        }
        if (buttonDataList == null || buttonDataList.size() <= 1) {
            mOnTabSelectedListener = null;
            return;
        }

        mOnTabSelectedListener = makeTabSelectedListener(buttonDataList);
        mPaneSwitcher.addOnTabSelectedListener(mOnTabSelectedListener);

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
        registerSearchBoxColorBlends(mixer);
    }

    private void registerColorBlends(HubColorMixer mixer) {
        Context context = getContext();
        boolean isGtsUpdateEnabled = HubUtils.isGtsUpdateEnabled();

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> getBackgroundColor(context, colorScheme),
                        this::setBackgroundColor));

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

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getHairlineColor(context, colorScheme),
                        this::setHairlineColor));

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
                    animation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
                    return animation;
                };
        mixer.registerBlend(multiColorBlend);

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getIconColor(context, colorScheme),
                        interpolatedColor -> {
                            ColorStateList menuButtonColor =
                                    ColorStateList.valueOf(interpolatedColor);
                            ImageViewCompat.setImageTintList(mMenuButton, menuButtonColor);
                        }));

        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getIconColor(context, colorScheme),
                        interpolatedColor -> {
                            ColorStateList backButtonColor =
                                    HubColors.getButtonColorStateList(context, interpolatedColor);
                            ImageViewCompat.setImageTintList(mBackButton, backButtonColor);
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

    void setHairlineVisibility(boolean visible) {
        mHairline.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setHairlineColor(@ColorInt int hairlineColor) {
        mHairline.setImageTintList(ColorStateList.valueOf(hairlineColor));
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

    /**
     * In the event there is no back button and the GTS update is enabled, we need to show a spacer
     * view so that that new tab button is vertically aligned with the tabs in the tab switcher.
     * Once the back button launches we can remove this spacer.
     */
    private void updateSpacerVisibility() {
        if (mSpacer == null || !HubUtils.isGtsUpdateEnabled()) return;

        boolean shouldShowSpacer = mBackButton.getVisibility() == View.GONE;
        mSpacer.setVisibility(shouldShowSpacer ? View.VISIBLE : View.GONE);
    }

    void setBackButtonVisible(boolean visible) {
        if (!ChromeFeatureList.sHubBackButton.isEnabled()) {
            updateSpacerVisibility();
            return;
        }

        mBackButton.setVisibility(visible ? View.VISIBLE : View.GONE);
        updateSpacerVisibility();
    }

    void setBackButtonEnabled(boolean enabled) {
        mBackButton.setEnabled(enabled);
    }

    void setBackButtonListener(Runnable backButtonListener) {
        mBackButton.setOnClickListener(v -> backButtonListener.run());
    }

    void setToolbarColorOverviewListener(Callback<Integer> colorSetter) {
        mToolbarOverviewColorSetter = colorSetter;
    }

    void updateIncognitoElements(boolean isIncognito) {
        updateSearchBoxElements(isIncognito);
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
        int regularEmptyHintRes =
                OmniboxFeatures.sAndroidHubSearchEnableTabGroupStrings.getValue()
                        ? R.string.hub_search_empty_hint_with_tab_groups
                        : R.string.hub_search_empty_hint;
        @StringRes
        int emptyHintRes =
                isIncognito ? R.string.hub_search_empty_hint_incognito : regularEmptyHintRes;

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

    AnimatorSet getHubSearchBoxTransitionAnimation(boolean visible) {
        boolean isSquishAnimationEnabled =
                ChromeFeatureList.sAndroidPinnedTabs.isEnabled()
                        && ChromeFeatureList.sAndroidPinnedTabsSearchBoxSquishAnimation.getValue();

        AnimatorSet transitionAnimator = new AnimatorSet();

        float fadeAlphaFrom = visible ? 0 : 1;
        float fadeAlphaTo = visible ? 1 : 0;
        Animator fade =
                ObjectAnimator.ofFloat(mSearchBoxLayout, View.ALPHA, fadeAlphaFrom, fadeAlphaTo);

        Animator primaryAnimator;
        if (isSquishAnimationEnabled) {
            primaryAnimator = createSquishAnimation(visible);
        } else {
            primaryAnimator = createSlideAnimation(visible);
        }

        transitionAnimator.play(primaryAnimator).with(fade);
        transitionAnimator.setDuration(PANE_FADE_ANIMATION_DURATION_MS);

        return transitionAnimator;
    }

    private Animator createSquishAnimation(boolean visible) {
        mSearchBoxLayout.setPivotY(0);
        float scaleYFrom = visible ? 0f : 1f;
        float scaleYTo = visible ? 1f : 0f;
        return ObjectAnimator.ofFloat(mSearchBoxLayout, View.SCALE_Y, scaleYFrom, scaleYTo);
    }

    private Animator createSlideAnimation(boolean visible) {
        float slideTransitionY = visible ? 0 : -mSearchBoxLayout.getHeight();
        return ObjectAnimator.ofFloat(mSearchBoxLayout, View.TRANSLATION_Y, slideTransitionY);
    }

    private GradientDrawable buildBackgroundDrawableForTab() {
        int radius = getResources().getDimensionPixelSize(R.dimen.hub_pane_switcher_tab_radius);
        GradientDrawable hoverDrawable = new GradientDrawable();

        hoverDrawable.setShape(GradientDrawable.RECTANGLE);
        hoverDrawable.setCornerRadius(radius);
        return hoverDrawable;
    }

    private @ColorInt int getBackgroundColor(Context context, @HubColorScheme int colorScheme) {
        boolean isXrFullSpaceMode =
                mXrSpaceModeObservableSupplier != null && mXrSpaceModeObservableSupplier.get();
        return HubColors.getBackgroundColor(context, colorScheme, isXrFullSpaceMode);
    }

    public void setXrSpaceModeObservableSupplier(
            @Nullable ObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
        HubColors.setXrSpaceModeObservableSupplier(xrSpaceModeObservableSupplier);
    }
}
