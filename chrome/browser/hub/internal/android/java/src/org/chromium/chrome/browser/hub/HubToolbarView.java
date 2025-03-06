// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.getPaneColorBlendInterpolator;
import static org.chromium.ui.util.ColorBlendAnimationFactory.createMultiColorBlendAnimation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.widget.ImageViewCompat;
import androidx.core.widget.TextViewCompat;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.base.ViewUtils;

import java.util.List;

/** Toolbar for the Hub. May contain a single or multiple rows, of which this view is the parent. */
public class HubToolbarView extends LinearLayout {

    private Button mActionButton;
    private TabLayout mPaneSwitcher;
    private LinearLayout mMenuButtonContainer;
    private ImageButton mMenuButton;
    private View mSearchBoxLayout;
    private EditText mSearchBoxTextView;
    private ImageView mSearchLoupeView;

    public Callback<Integer> mToolbarOverviewColorSetter;
    private OnTabSelectedListener mOnTabSelectedListener;
    private boolean mBlockTabSelectionCallback;
    private boolean mApplyDelayForSearchBoxAnimation;
    private final AnimationHandler mColorBlendAnimatorHandler;
    private final AnimationHandler mHubSearchAnimatorHandler;
    private final HubColorBlendAnimatorSetHelper mAnimatorSetBuilder;
    private final Handler mHandler;

    /** Default {@link LinearLayout} constructor called by inflation. */
    public HubToolbarView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mColorBlendAnimatorHandler = new AnimationHandler();
        mHubSearchAnimatorHandler = new AnimationHandler();
        mHandler = new Handler();
        mAnimatorSetBuilder = new HubColorBlendAnimatorSetHelper();
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

        // SearchBoxLayout is GONE by default, and enabled via the mediator.
        mSearchBoxLayout = findViewById(R.id.search_box);
        mSearchBoxTextView = findViewById(R.id.search_box_text);
        mSearchLoupeView = findViewById(R.id.search_loupe);

        registerColorBlends();

        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            registerSearchBoxColorBlends();
        }
    }

    void setMenuButtonVisible(boolean visible) {
        mMenuButtonContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    void setActionButton(@Nullable FullButtonData buttonData, boolean showText) {
        ApplyButtonData.apply(buttonData, mActionButton);
        if (!showText) {
            mActionButton.setCompoundDrawablePadding(0);
            mActionButton.setText(null);
        } else {
            mActionButton.setCompoundDrawablePadding(ViewUtils.dpToPx(getContext(), 16));
        }
    }

    void setPaneSwitcherButtonData(
            @Nullable List<FullButtonData> buttonDataList, int selectedIndex) {
        mPaneSwitcher.removeOnTabSelectedListener(mOnTabSelectedListener);
        mPaneSwitcher.removeAllTabs();

        if (buttonDataList == null || buttonDataList.size() <= 1) {
            mPaneSwitcher.setVisibility(View.GONE);
            mOnTabSelectedListener = null;
        } else {
            Context context = getContext();
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
            }
            mPaneSwitcher.setVisibility(View.VISIBLE);
            mOnTabSelectedListener = makeTabSelectedListener(buttonDataList);
            mPaneSwitcher.addOnTabSelectedListener(mOnTabSelectedListener);
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

    void setColorScheme(HubColorSchemeUpdate colorSchemeUpdate) {
        @HubColorScheme int newColorScheme = colorSchemeUpdate.newColorScheme;
        @HubColorScheme int prevColorScheme = colorSchemeUpdate.previousColorScheme;

        AnimatorSet animatorSet =
                mAnimatorSetBuilder
                        .setNewColorScheme(newColorScheme)
                        .setPreviousColorScheme(prevColorScheme)
                        .setIsImmediate(!colorSchemeUpdate.animate)
                        .build();
        mColorBlendAnimatorHandler.startAnimation(animatorSet);

        // TODO(crbug.com/40948541): Updating the app menu color here is more correct and
        // should be done for code health.
    }

    private void registerColorBlends() {
        Context context = getContext();

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        this::setBackgroundColor));

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getSelectedIconColor(context, colorScheme),
                        mPaneSwitcher::setSelectedTabIndicatorColor));

        HubViewColorBlend multiColorBlend =
                (prevColorScheme, newColorScheme) -> {
                    @ColorInt int newIconColor = HubColors.getIconColor(context, newColorScheme);
                    @ColorInt
                    int newSelectedIconColor =
                            HubColors.getSelectedIconColor(context, newColorScheme);
                    @ColorInt int prevIconColor = HubColors.getIconColor(context, prevColorScheme);
                    @ColorInt
                    int prevSelectedIconColor =
                            HubColors.getSelectedIconColor(context, prevColorScheme);
                    Animator animation =
                            createMultiColorBlendAnimation(
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
        mAnimatorSetBuilder.registerBlend(multiColorBlend);

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getIconColor(context, colorScheme),
                        interpolatedColor -> {
                            updateActionButtonColorInternal(context, interpolatedColor);
                            ColorStateList menuButtonColor =
                                    ColorStateList.valueOf(interpolatedColor);
                            ImageViewCompat.setImageTintList(mMenuButton, menuButtonColor);
                        }));

        // We don't want to pass a method reference. Lambdas will ensure we run the most recent
        // setter.
        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        color -> mToolbarOverviewColorSetter.onResult(color)));

        // TODO(crbug.com/40948541): Updating the app menu color here is more correct and
        // should be done for code health. Menu Button Color is also set by
        // HubToolbarCoordinator.
    }

    private void registerSearchBoxColorBlends() {
        Context context = getContext();

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getSearchBoxHintTextColor(context, colorScheme),
                        mSearchBoxTextView::setHintTextColor));

        GradientDrawable backgroundDrawable =
                (GradientDrawable) mSearchBoxLayout.getBackground().mutate();
        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getSearchBoxBgColor(context, colorScheme),
                        backgroundDrawable::setColor));

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getIconColor(context, colorScheme),
                        this::updateSearchLoupeColor));
    }

    private void updateTabIconTintInternal(
            @ColorInt int iconColor, @ColorInt int selectedIconColor) {
        ColorStateList selectableIconList =
                HubColors.getSelectableIconList(selectedIconColor, iconColor);
        mPaneSwitcher.setTabIconTint(selectableIconList);
    }

    private void updateActionButtonColorInternal(Context context, @ColorInt int color) {
        ColorStateList actionButtonColor = HubColors.getActionButtonColor(context, color);
        TextViewCompat.setCompoundDrawableTintList(mActionButton, actionButtonColor);
        mActionButton.setTextColor(actionButtonColor);
    }

    private void updateSearchLoupeColor(@ColorInt int color) {
        ColorStateList colorStateList = ColorStateList.valueOf(color);
        mSearchLoupeView.setImageTintList(colorStateList);
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

    private View getButtonView(int index) {
        @Nullable Tab tab = mPaneSwitcher.getTabAt(index);
        return tab == null ? null : tab.view;
    }

    private OnTabSelectedListener makeTabSelectedListener(
            @NonNull List<FullButtonData> buttonDataList) {
        return new OnTabSelectedListener() {
            @Override
            public void onTabSelected(Tab tab) {
                if (!mBlockTabSelectionCallback) {
                    buttonDataList.get(tab.getPosition()).getOnPressRunnable().run();
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
}
