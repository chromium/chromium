// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.TabFeatureUtilities;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.IncognitoToggleTabLayout;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.OptimizedFrameLayout;

/** The tab switcher mode top toolbar shown on phones. */
public class TabSwitcherModeTTPhone extends OptimizedFrameLayout
        implements View.OnClickListener, IncognitoStateProvider.IncognitoStateObserver {
    private View.OnClickListener mNewTabListener;

    private TabCountProvider mTabCountProvider;
    private TabModelSelector mTabModelSelector;
    private IncognitoStateProvider mIncognitoStateProvider;

    private @Nullable IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    // The following view is used as a variation for mNewTabImageButton. When this view is showing
    // as the button for creating new tab, the incognito toggle is hidden.
    private @Nullable View mNewTabViewButton;

    // The following three buttons are not used when Duet is enabled.
    private @Nullable NewTabButton mNewTabImageButton;
    private @Nullable MenuButton mMenuButton;
    private @Nullable ToggleTabStackButton mToggleTabStackButton;

    private int mPrimaryColor;
    private boolean mUseLightIcons;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    private boolean mIsIncognito;

    private ObjectAnimator mVisiblityAnimator;

    public TabSwitcherModeTTPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mNewTabImageButton = findViewById(R.id.new_tab_button);
        mNewTabViewButton = findViewById(R.id.new_tab_view);
        mMenuButton = findViewById(R.id.menu_button_wrapper);
        mToggleTabStackButton = findViewById(R.id.tab_switcher_mode_tab_switcher_button);

        boolean isBottomToolbarEnabled = FeatureUtilities.isBottomToolbarEnabled();

        if (isBottomToolbarEnabled) {
            UiUtils.removeViewFromParent(mNewTabImageButton);
            mNewTabImageButton.destroy();
            mNewTabImageButton = null;

            UiUtils.removeViewFromParent(mMenuButton);
            mMenuButton.destroy();
            mMenuButton = null;

            UiUtils.removeViewFromParent(mToggleTabStackButton);
            mToggleTabStackButton.destroy();
            mToggleTabStackButton = null;

            UiUtils.removeViewFromParent(mNewTabViewButton);
            mNewTabViewButton = null;
        } else {
            // TODO(twellington): Try to make NewTabButton responsible for handling its own clicks.
            //                    TabSwitcherBottomToolbarCoordinator also uses NewTabButton and
            //                    sets an onClickListener directly on NewTabButton rather than
            //                    acting as the click listener itself so the behavior between this
            //                    class and the bottom toolbar will need to be unified.
            mNewTabImageButton.setOnClickListener(this);
            mNewTabViewButton.setOnClickListener(this);
        }

        if ((usingHorizontalTabSwitcher() || FeatureUtilities.isGridTabSwitcherEnabled())
                && IncognitoUtils.isIncognitoModeEnabled()) {
            updateTabSwitchingElements(true);
        }
    }

    @Override
    public void onClick(View v) {
        if (mNewTabImageButton == v || mNewTabViewButton == v) {
            v.setEnabled(false);
            if (mNewTabListener != null) mNewTabListener.onClick(v);
        }
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mNewTabImageButton != null) {
            mNewTabImageButton.destroy();
            mNewTabImageButton = null;
        }
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.destroy();
            mToggleTabStackButton = null;
        }
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.destroy();
            mIncognitoToggleTabLayout = null;
        }
        if (mMenuButton != null) {
            mMenuButton.destroy();
            mMenuButton = null;
        }
    }

    /**
     * Called when tab switcher mode is entered or exited.
     * @param inTabSwitcherMode Whether or not tab switcher mode should be shown or hidden.
     */
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        if (mVisiblityAnimator != null) mVisiblityAnimator.cancel();

        setVisibility(View.VISIBLE);
        // TODO(twellington): Handle interrupted animations to avoid jumps to 1.0 or 0.f.
        setAlpha(inTabSwitcherMode ? 0.0f : 1.0f);

        boolean showZoomingAnimation = FeatureUtilities.isGridTabSwitcherEnabled()
                && TabFeatureUtilities.isTabToGtsAnimationEnabled();
        long duration = showZoomingAnimation
                ? TopToolbarCoordinator.TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS
                : TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS;

        mVisiblityAnimator =
                ObjectAnimator.ofFloat(this, View.ALPHA, inTabSwitcherMode ? 1.0f : 0.0f);
        mVisiblityAnimator.setDuration(duration);
        if (showZoomingAnimation && inTabSwitcherMode) {
            mVisiblityAnimator.setStartDelay(duration);
        }
        mVisiblityAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // TODO(https://crbug.com/914868): Use consistent logic here for setting clickable/enabled
        // on mIncognitoToggleTabLayout & mNewTabButton?
        if (!inTabSwitcherMode) {
            if (mIncognitoToggleTabLayout != null) mIncognitoToggleTabLayout.setClickable(false);
        } else {
            if (mNewTabImageButton != null) mNewTabImageButton.setEnabled(true);
            if (mNewTabViewButton != null) mNewTabViewButton.setEnabled(true);
        }

        mVisiblityAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animation) {
                setAlpha(1.0f);

                if (!inTabSwitcherMode) {
                    setVisibility(View.GONE);
                }

                if (mIncognitoToggleTabLayout != null) {
                    mIncognitoToggleTabLayout.setClickable(true);
                }

                mVisiblityAnimator = null;
            }
        });

        mVisiblityAnimator.start();

        if (DeviceClassManager.enableAccessibilityLayout()) mVisiblityAnimator.end();
    }

    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        if (mMenuButton == null) return;

        mMenuButton.getImageButton().setOnTouchListener(appMenuButtonHelper);
        mMenuButton.getImageButton().setAccessibilityDelegate(
                appMenuButtonHelper.getAccessibilityDelegate());
    }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(View.OnClickListener listener) {
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setOnTabSwitcherClickHandler(listener);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabListener = listener;
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} used to observe the number of tabs in
     *                         the current model.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setTabCountProvider(tabCountProvider);
        }
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(selector);
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);

        if (mNewTabImageButton != null) {
            mNewTabImageButton.setIncognitoStateProvider(mIncognitoStateProvider);
        }
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        mIsIncognito = isIncognito;
        updatePrimaryColorAndTint();
        updateIncognitoToggleTabsVisibility();
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        if (mNewTabImageButton != null) mNewTabImageButton.onAccessibilityStatusChanged();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                && IncognitoUtils.isIncognitoModeEnabled()) {
            updateTabSwitchingElements(!enabled);
        }

        updatePrimaryColorAndTint();
    }

    private void updatePrimaryColorAndTint() {
        int primaryColor = getToolbarColorForCurrentState();
        if (mPrimaryColor != primaryColor) {
            mPrimaryColor = primaryColor;
            setBackgroundColor(primaryColor);
        }

        boolean useLightIcons;
        if (primaryColor == Color.TRANSPARENT) {
            // If the toolbar is transparent, the icon tint will depend on the background color of
            // the tab switcher, which is the standard mode background. Note that horizontal tab
            // switcher is an exception, which uses the correspond background color for standard
            // and incognito mode.
            int backgroundColor = ChromeColors.getPrimaryBackgroundColor(
                    getResources(), usingHorizontalTabSwitcher() && mIsIncognito);
            useLightIcons = ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
        } else {
            useLightIcons = ColorUtils.shouldUseLightForegroundOnBackground(primaryColor);
        }

        if (mUseLightIcons == useLightIcons) return;

        mUseLightIcons = useLightIcons;

        if (mLightIconTint == null) {
            mLightIconTint =
                    AppCompatResources.getColorStateList(getContext(), R.color.tint_on_dark_bg);
            mDarkIconTint =
                    AppCompatResources.getColorStateList(getContext(), R.color.standard_mode_tint);
        }

        ColorStateList tintList = useLightIcons ? mLightIconTint : mDarkIconTint;
        if (mMenuButton != null) {
            ApiCompatibilityUtils.setImageTintList(mMenuButton.getImageButton(), tintList);
        }

        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setUseLightDrawables(useLightIcons);
        }
    }

    private int getToolbarColorForCurrentState() {
        // TODO(huayinz): Split tab switcher background color from primary background color.
        if (DeviceClassManager.enableAccessibilityLayout()
                || FeatureUtilities.isGridTabSwitcherEnabled()) {
            return ChromeColors.getPrimaryBackgroundColor(getResources(), mIsIncognito);
        }

        return Color.TRANSPARENT;
    }

    private boolean usingHorizontalTabSwitcher() {
        // The horizontal tab switcher flag does not affect the accessibility switcher. We do the
        // enableAccessibilityLayout() check first here to avoid logging an experiment exposure for
        // these users.
        return !DeviceClassManager.enableAccessibilityLayout()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
    }

    private void inflateIncognitoToggle() {
        ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
        mIncognitoToggleTabLayout = (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();

        if (mTabCountProvider != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(mTabCountProvider);
        }

        if (mTabModelSelector != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(mTabModelSelector);
        }
    }

    private void setIncognitoToggleVisibility(boolean showIncognitoToggle) {
        // If StartSurface is enabled, the incognito switch is shown and handled
        // by the IncognitoSwitchCoordinator in the
        // TabSwitcherModeTTCoordinatorPhone.
        if (FeatureUtilities.isStartSurfaceEnabled()) return;

        if (mIncognitoToggleTabLayout == null) {
            if (showIncognitoToggle) inflateIncognitoToggle();
        } else {
            mIncognitoToggleTabLayout.setVisibility(showIncognitoToggle ? View.VISIBLE : View.GONE);
        }
    }

    private void setToggleTabStackButtonVisibility(boolean showToggleTabStackButton) {
        if (mToggleTabStackButton == null) return;
        mToggleTabStackButton.setVisibility(showToggleTabStackButton ? View.VISIBLE : View.GONE);
    }

    private void updateTabSwitchingElements(boolean showIncognitoToggle) {
        setIncognitoToggleVisibility(showIncognitoToggle);
        setToggleTabStackButtonVisibility(!showIncognitoToggle);
    }

    private void updateIncognitoToggleTabsVisibility() {
        // TODO(yuezhanggg): Add a regression test for this "New Tab" variation. (crbug: 977546)
        if (!FeatureUtilities.isGridTabSwitcherEnabled() || !ChromeFeatureList.isInitialized()
                || !ChromeFeatureList
                            .getFieldTrialParamByFeature(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                    "tab_grid_layout_android_new_tab")
                            .equals("NewTabVariation")
                || FeatureUtilities.isBottomToolbarEnabled() || mIncognitoToggleTabLayout == null) {
            return;
        }
        boolean hasIncognitoTabs = hasIncognitoTabs();
        mIncognitoToggleTabLayout.setVisibility(hasIncognitoTabs ? VISIBLE : GONE);
        if (mNewTabImageButton != null && mNewTabViewButton != null) {
            // Only show one new tab variation at a time.
            mNewTabImageButton.setVisibility(hasIncognitoTabs ? VISIBLE : GONE);
            mNewTabViewButton.setVisibility(hasIncognitoTabs ? GONE : VISIBLE);
        }
    }

    private boolean hasIncognitoTabs() {
        // Check if there is no incognito tab, or all the incognito tabs are being closed.
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        for (int i = 0; i < incognitoTabModel.getCount(); i++) {
            if (!incognitoTabModel.getTabAt(i).isClosing()) return true;
        }
        return false;
    }
}
