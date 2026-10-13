// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.View;
import android.view.View.OnDragListener;

import androidx.annotation.ColorInt;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.ntp.NewTabPageManager;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxFeatureUtils;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

@NullMarked
class SearchBoxMediator implements DestroyObserver {
    private static final long PLUS_ROTATION_DURATION_MS = 750;
    private static final long INITIAL_HINT_DELAY_MS = 300;
    private static final long HINT_HOLD_DURATION_MS = 2500;
    private static final long FADE_DURATION_MS = 150;

    private final Context mContext;
    private final PropertyModel mModel;
    private final SearchBoxContainerView mView;
    private final NewTabPageManager mNewTabPageManager;
    private final boolean mIsIncognito;
    private final WindowAndroid mWindowAndroid;
    private final TemplateUrlService mTemplateUrlService;
    private final float mTransitionEndOffset;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Profile mProfile;
    private final TemplateUrlServiceObserver mTemplateUrlServiceObserver =
            this::onTemplateURLServiceChanged;
    private @Nullable StatusIconResource mSearchEngineIcon;
    private @Nullable AnimatorSet mHintAnimator;

    SearchBoxMediator(
            Context context,
            PropertyModel model,
            SearchBoxContainerView view,
            boolean isLff,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            NewTabPageManager newTabPageManager,
            boolean isIncognito,
            WindowAndroid windowAndroid,
            Profile profile) {
        mContext = context;
        mModel = model;
        mView = view;
        mNewTabPageManager = newTabPageManager;
        mIsIncognito = isIncognito;
        mWindowAndroid = windowAndroid;
        mProfile = profile;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(mTemplateUrlServiceObserver);
        PropertyModelChangeProcessor.create(mModel, mView, new SearchBoxViewBinder());

        mTransitionEndOffset =
                !isLff
                        ? context.getResources()
                                .getDimensionPixelSize(R.dimen.ntp_search_box_transition_end_offset)
                        : 0;

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, this::onSearchBoxClick);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, this::onVoiceSearchClick);
        mModel.set(SearchBoxProperties.PLUS_BUTTON_CLICK_CALLBACK, this::onPlusButtonClick);
        mModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, this::onLensClick);
        mModel.set(SearchBoxProperties.AI_CHIP_CLICK_CALLBACK, this::onAiChipClick);
        mModel.set(SearchBoxProperties.APPLY_ELEVATION, OmniboxCapabilities.isDesktopPlatform());
        mModel.set(SearchBoxProperties.PLUS_BUTTON_ROTATION, 0f);
        mModel.set(SearchBoxProperties.HINT_TEXT_ALPHA, 1f);
        mModel.set(
                SearchBoxProperties.PLUS_BUTTON_TINT,
                context.getColorStateList(R.color.default_icon_color_tint_list));

        updateAiChip();
        updateStartIcon();
    }

    @Override
    public void onDestroy() {
        mActivityLifecycleDispatcher.unregister(this);

        cancelHintAnimation();
        mModel.set(SearchBoxProperties.AI_CHIP_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.PLUS_BUTTON_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, null);
        mModel.set(SearchBoxProperties.DSE_ICON_DRAWABLE, null);
        mModel.set(SearchBoxProperties.DSE_ICON_TINT, null);
        mSearchEngineIcon = null;

        mTemplateUrlService.removeObserver(mTemplateUrlServiceObserver);
    }

    private void onSearchBoxClick(View v) {
        mNewTabPageManager.focusSearchBox(
                /* beginVoiceSearch= */ false,
                AutocompleteRequestType.SEARCH,
                OmniboxFocusReason.FAKE_BOX_TAP,
                /* pastedText= */ null);
    }

    private void onVoiceSearchClick(View v) {
        mNewTabPageManager.focusSearchBox(
                /* beginVoiceSearch= */ true,
                AutocompleteRequestType.SEARCH,
                OmniboxFocusReason.FAKE_BOX_TAP,
                /* pastedText= */ null);
    }

    private void onPlusButtonClick(View v) {
        mNewTabPageManager.focusSearchBox(
                /* beginVoiceSearch= */ false,
                AutocompleteRequestType.SEARCH,
                OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP,
                /* pastedText= */ null);
    }

    private void onLensClick(View v) {
        LensMetrics.recordClicked(LensEntryPoint.NEW_TAB_PAGE);
        LensIntentParams lensIntentParams =
                new LensIntentParams.Builder(LensEntryPoint.NEW_TAB_PAGE, mIsIncognito).build();
        LensController.getInstance().startLens(mWindowAndroid, lensIntentParams);
    }

    void setSearchEngineIcon(@Nullable StatusIconResource newIcon) {
        // AL desktop environments with Google as DSE uses the search loop icon instead of the
        // engine specific icon.
        mSearchEngineIcon = isGoogleDseOnDesktop() ? null : newIcon;
        updateDseIconTint();

        if (mSearchEngineIcon == null) {
            mModel.set(
                    SearchBoxProperties.DSE_ICON_DRAWABLE,
                    mContext.getDrawable(R.drawable.ic_search_24dp));
            return;
        }

        // When DSE is Google, setSearchEngineIcon() is called before
        // NewTabPageLayout#setSearchProviderInfo(). Thus, we check the icon's resource id to change
        // the icon to be R.drawable.ic_logo_googleg_24dp which doesn't have a padding.
        if (mSearchEngineIcon.getIconRes() == R.drawable.ic_logo_googleg_20dp) {
            mModel.set(
                    SearchBoxProperties.DSE_ICON_DRAWABLE,
                    mContext.getDrawable(R.drawable.ic_logo_googleg_24dp));
            return;
        }

        mModel.set(SearchBoxProperties.DSE_ICON_DRAWABLE, mSearchEngineIcon.getDrawable(mContext));
    }

    private void updateDseIconTint() {
        if (mSearchEngineIcon == null) {
            mModel.set(SearchBoxProperties.DSE_ICON_TINT, getIconTint());
            return;
        }
        mModel.set(SearchBoxProperties.DSE_ICON_TINT, null);
    }

    private @Nullable ColorStateList getIconTint() {
        return ComposeplateUtils.getSearchBoxIconColorTint(
                mContext, mModel.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND_AND_SHADOW));
    }

    private boolean isGoogleDseOnDesktop() {
        return mTemplateUrlService.isDefaultSearchEngineGoogle()
                && OmniboxCapabilities.isDesktopPlatform();
    }

    private void updateStartIcon() {
        mModel.set(
                SearchBoxProperties.PLUS_BUTTON_VISIBILITY,
                FuseboxFeatureUtils.shouldShowNtpPlusButton(
                        mContext, mProfile, mTemplateUrlService));
    }

    private void onTemplateURLServiceChanged() {
        cancelHintAnimation();
        updateStartIcon();
        updateAiChip();
    }

    /** Called to set a drag listener for the search box. */
    void setSearchBoxDragListener(OnDragListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, listener);
    }

    /**
     * Check whether the Lens is enabled for an entry point.
     *
     * @param lensEntryPoint A {@link LensEntryPoint}.
     * @param isIncognito Whether the request is from a Incognito tab.
     * @param isLff Whether the request is from a large form factor device.
     * @return Whether the Lens is currently enabled.
     */
    boolean isLensEnabled(@LensEntryPoint int lensEntryPoint, boolean isIncognito, boolean isLff) {
        if (BrowserUiUtils.isAndroidAutoProjected(mContext)) {
            return false;
        }
        return LensController.getInstance()
                .isLensEnabled(
                        new LensQueryParams.Builder(lensEntryPoint, isIncognito, isLff).build());
    }

    void setHeight(int height) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_HEIGHT, height);
    }

    void setTopMargin(int topMargin) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TOP_MARGIN, topMargin);
    }

    void setEndPadding(int endPadding) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_END_PADDING, endPadding);
    }

    void setSearchBoxTextAppearance(@StyleRes int resId) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
    }

    void enableSearchBoxEditText(boolean enabled) {
        mModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, enabled);
    }

    void setSearchBoxHintText(@Nullable String hint) {
        cancelHintAnimation();
        mModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, hint);
    }

    void applyWhiteBackground(boolean apply) {
        cancelHintAnimation();
        ColorStateList colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(mContext, apply);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST, colorStateList);
        mModel.set(SearchBoxProperties.PLUS_BUTTON_TINT, colorStateList);

        mModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND_AND_SHADOW, apply);

        @StyleRes int resId = getFakeSearchBoxTextStyle(apply);
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
        updateDseIconTint();
    }

    /**
     * Returns the text appearance style resource ID for the fake search box hint text.
     *
     * <p>When NTP Aurora is enabled ({@link NewTabPageUtils#isNtpAuroraEnabled()}), returns the new
     * style TextAppearance_FakeSearchBoxTextNewStyle.
     *
     * <p>When NTP Aurora is disabled, falls back to legacy typography based on {@code apply}.
     *
     * @param apply Whether a white background is applied to the fake search box.
     * @return The text appearance style resource ID to apply to the fake search box.
     */
    @VisibleForTesting
    static @StyleRes int getFakeSearchBoxTextStyle(boolean apply) {
        if (NewTabPageUtils.isNtpAuroraEnabled()) {
            return R.style.TextAppearance_FakeSearchBoxTextNewStyle;
        }
        return apply
                ? R.style.TextAppearance_FakeSearchBoxTextMediumDark
                : R.style.TextAppearance_FakeSearchBoxTextMedium;
    }

    /**
     * Calculates the percentage (between 0 and 1) of the transition from the search box to the
     * omnibox at the top of the New Tab Page, which is determined by the amount of scrolling and
     * the position of the search box.
     *
     * @return the transition percentage
     */
    float getToolbarTransitionPercentage(
            FeedSurfaceScrollDelegate scrollDelegate,
            @Nullable Supplier<Integer> tabStripHeightSupplier,
            int currentNtpFakeSearchBoxTransitionStartOffset) {
        // During startup the view may not be fully initialized.
        if (!scrollDelegate.isScrollViewInitialized() || mView == null) return 0f;

        if (isSearchBoxOffscreen(scrollDelegate)) {
            // getVerticalScrollOffset is valid only for the scroll view if the first item is
            // visible. If the search box view is offscreen, we must have scrolled quite far and we
            // know the toolbar transition should be 100%. This might be the initial scroll position
            // due to the scroll restore feature, so the search box will not have been laid out yet.
            return 1f;
        }

        // During startup the view may not be fully initialized, so we only calculate the current
        // percentage if some basic view properties (position of the search box) are sane.
        int searchBoxTop = mView.getTop();
        if (searchBoxTop == 0) return 0f;

        // For all other calculations, add the search box padding, because it defines where the
        // visible "border" of the search box is.
        searchBoxTop += mView.getPaddingTop();

        final int scrollY = scrollDelegate.getVerticalScrollOffset();
        // Use int pixel size instead of float dimension to avoid precision error on the percentage.
        final float transitionLength =
                currentNtpFakeSearchBoxTransitionStartOffset + mTransitionEndOffset;

        // Tab strip height is zero on phones, and may vary on tablets.
        Integer tabStripHeightValue =
                tabStripHeightSupplier != null ? tabStripHeightSupplier.get() : null;
        int tabStripHeight = tabStripHeightValue != null ? tabStripHeightValue : 0;

        // When scrollY equals searchBoxTop + tabStripHeight -transitionStartOffset, it marks the
        // start point of the transition. When scrollY equals searchBoxTop plus transitionEndOffset
        // plus tabStripHeight, it marks the end point of the transition.
        return MathUtils.clamp(
                (scrollY
                                - (searchBoxTop + mTransitionEndOffset)
                                + tabStripHeight
                                + transitionLength)
                        / transitionLength,
                0f,
                1f);
    }

    /**
     * Get the bounds of the search box in relation to the top level {@code parentView}.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *     to the {@code parentView}.
     * @param parentView The top level parent view used to translate search box bounds.
     * @param scrollDelegate The scroll delegate for NTP.
     * @param searchBoxBoundsVerticalInset Vertical inset to add to the top and bottom of the search
     *     box bounds. May be 0 if no inset should be applied.
     */
    void getSearchBoxBounds(
            Rect bounds,
            Point translation,
            View parentView,
            FeedSurfaceScrollDelegate scrollDelegate,
            int searchBoxBoundsVerticalInset) {
        int searchBoxX = (int) mView.getX();
        int searchBoxY = (int) mView.getY();
        bounds.set(
                searchBoxX,
                searchBoxY,
                searchBoxX + mView.getWidth(),
                searchBoxY + mView.getHeight());

        translation.set(0, 0);

        if (isSearchBoxOffscreen(scrollDelegate)) {
            translation.y = Integer.MIN_VALUE;
        } else {
            View view = mView;
            while (true) {
                view = (View) view.getParent();
                if (view == null) {
                    // The |mView| is not a child of this view. This can happen if the
                    // RecyclerView detaches the NewTabPageLayout after it has been scrolled out of
                    // view. Set the translation to the minimum Y value as an approximation.
                    translation.y = Integer.MIN_VALUE;
                    break;
                }
                translation.offset(-view.getScrollX(), -view.getScrollY());
                if (view == parentView) break;
                translation.offset((int) view.getX(), (int) view.getY());
            }
        }

        bounds.offset(translation.x, translation.y);
        if (translation.y != Integer.MIN_VALUE) {
            bounds.inset(0, searchBoxBoundsVerticalInset);
        }
    }

    /** Returns whether the search box view is scrolled off the screen. */
    boolean isSearchBoxOffscreen(FeedSurfaceScrollDelegate scrollDelegate) {
        if (!scrollDelegate.isScrollViewInitialized()) return false;

        return !scrollDelegate.isChildVisibleAtPosition(0)
                || scrollDelegate.getVerticalScrollOffset() > mView.getTop() + mTransitionEndOffset;
    }

    private void updateAiChip() {
        boolean shouldShow =
                OmniboxCapabilities.isDesktopPlatform()
                        && ComposeplateUtils.isComposeplateEnabled(mProfile);
        mModel.set(SearchBoxProperties.AI_CHIP_VISIBILITY, shouldShow);
    }

    private void onAiChipClick(View view) {
        GURL url = mTemplateUrlService.getComposeplateUrl();
        if (url != null) {
            mNewTabPageManager.loadUrl(new LoadUrlParams(url), /* incognito= */ false);
        }
    }

    void startHintAnimation() {
        cancelHintAnimation();
        mHintAnimator = createHintAnimation();
        mHintAnimator.start();
    }

    void cancelHintAnimation() {
        if (mHintAnimator != null) {
            mHintAnimator.cancel();
        }
    }

    private AnimatorSet createHintAnimation() {
        String initialHint = mModel.get(SearchBoxProperties.SEARCH_BOX_HINT_TEXT);
        ColorStateList initialTint =
                assumeNonNull(mModel.get(SearchBoxProperties.PLUS_BUTTON_TINT));
        @ColorInt int initialColor = initialTint.getDefaultColor();
        @ColorInt int accentColor = SemanticColorUtils.getDefaultIconColorAccent1(mContext);
        String makeAPlanHint = mContext.getString(R.string.ntp_search_box_hint_make_a_plan);
        String askAboutTabHint = mContext.getString(R.string.ntp_search_box_hint_ask_about_tab);

        ObjectAnimator spin =
                PropertyModelAnimatorFactory.ofFloat(
                                mModel, SearchBoxProperties.PLUS_BUTTON_ROTATION, 360f)
                        .setDuration(PLUS_ROTATION_DURATION_MS);
        spin.setInterpolator(Interpolators.EMPHASIZED);

        Animator tintIn = createTintAnimator(initialColor, accentColor);
        Animator tintOut = createTintAnimator(accentColor, initialColor);

        List<Animator> sequence = new ArrayList<>();
        sequence.add(spin);
        sequence.add(tintOut);
        addHintCrossfade(sequence, makeAPlanHint, INITIAL_HINT_DELAY_MS);
        addHintCrossfade(sequence, askAboutTabHint, HINT_HOLD_DURATION_MS);
        addHintCrossfade(sequence, initialHint, HINT_HOLD_DURATION_MS);

        AnimatorSet animator = new AnimatorSet();
        animator.playSequentially(sequence);
        animator.play(spin).with(tintIn);
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mHintAnimator = null;
                        mModel.set(SearchBoxProperties.PLUS_BUTTON_ROTATION, 0f);
                        mModel.set(SearchBoxProperties.PLUS_BUTTON_TINT, initialTint);
                        mModel.set(SearchBoxProperties.HINT_TEXT_ALPHA, 1f);
                        mModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, initialHint);
                    }
                });
        return animator;
    }

    private Animator createTintAnimator(@ColorInt int fromColor, @ColorInt int toColor) {
        ValueAnimator animator =
                ValueAnimator.ofArgb(fromColor, toColor).setDuration(FADE_DURATION_MS);
        animator.addUpdateListener(
                a ->
                        mModel.set(
                                SearchBoxProperties.PLUS_BUTTON_TINT,
                                ColorStateList.valueOf((int) a.getAnimatedValue())));
        return animator;
    }

    private void addHintCrossfade(
            List<Animator> sequence, @Nullable String nextHint, long delayMs) {
        ObjectAnimator fadeOut =
                PropertyModelAnimatorFactory.ofFloat(
                                mModel, SearchBoxProperties.HINT_TEXT_ALPHA, 0f)
                        .setDuration(FADE_DURATION_MS);
        fadeOut.setStartDelay(delayMs);

        ObjectAnimator fadeIn =
                PropertyModelAnimatorFactory.ofFloat(
                                mModel, SearchBoxProperties.HINT_TEXT_ALPHA, 1f)
                        .setDuration(FADE_DURATION_MS);
        fadeIn.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        mModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, nextHint);
                    }
                });

        sequence.add(fadeOut);
        sequence.add(fadeIn);
    }

    boolean isHintAnimatorActiveForTesting() {
        return mHintAnimator != null;
    }

    void setHintAnimatorForTesting(@Nullable AnimatorSet animator) {
        mHintAnimator = animator;
    }
}
