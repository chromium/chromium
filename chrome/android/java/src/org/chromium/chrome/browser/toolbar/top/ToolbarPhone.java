// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Property;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewDebug;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.graphics.drawable.DrawableWrapper;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.KeyboardNavigationListener;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.UrlExpansionObserver;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Phone specific toolbar implementation.
 */
public class ToolbarPhone extends ToolbarLayout implements OnClickListener, TabCountObserver {
    /** The amount of time transitioning from one theme color to another should take in ms. */
    public static final long THEME_COLOR_TRANSITION_DURATION = 250;

    public static final int URL_FOCUS_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS = 100;
    private static final int URL_CLEAR_FOCUS_EXPERIMENTAL_BUTTON_DELAY_MS = 150;
    private static final int URL_CLEAR_FOCUS_TABSTACK_DELAY_MS = 200;
    private static final int URL_CLEAR_FOCUS_MENU_DELAY_MS = 250;

    private static final int TAB_SWITCHER_MODE_EXIT_FADE_ANIMATION_DURATION_MS = 100;
    private static final int TAB_SWITCHER_MODE_POST_EXIT_ANIMATION_DURATION_MS = 100;

    // Values used during animation to show/hide optional toolbar button.
    public static final int LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int EXPERIMENTAL_ICON_ANIMATION_DURATION_MS = 100;
    private static final int EXPERIMENTAL_ICON_ANIMATION_DELAY_MS = 125;

    private static final float UNINITIALIZED_FRACTION = -1f;

    /** States that the toolbar can be in regarding the tab switcher. */
    protected static final int STATIC_TAB = 0;
    protected static final int TAB_SWITCHER = 1;
    protected static final int ENTERING_TAB_SWITCHER = 2;
    protected static final int EXITING_TAB_SWITCHER = 3;

    @ViewDebug.ExportedProperty(category = "chrome", mapping = {
            @ViewDebug.IntToString(from = STATIC_TAB, to = "STATIC_TAB"),
            @ViewDebug.IntToString(from = TAB_SWITCHER, to = "TAB_SWITCHER"),
            @ViewDebug.IntToString(from = ENTERING_TAB_SWITCHER, to = "ENTERING_TAB_SWITCHER"),
            @ViewDebug.IntToString(from = EXITING_TAB_SWITCHER, to = "EXITING_TAB_SWITCHER")
            })

    static final int LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 51;

    private TabCountProvider mTabCountProvider;

    protected LocationBarCoordinator mLocationBar;

    private ViewGroup mToolbarButtonsContainer;
    protected @Nullable ToggleTabStackButton mToggleTabStackButton;
    protected @Nullable HomeButton mHomeButton;
    private TextView mUrlBar;
    protected View mUrlActionContainer;
    protected ImageView mToolbarShadow;
    private @Nullable ImageButton mOptionalButton;
    private boolean mOptionalButtonUsesTint;

    private ObjectAnimator mTabSwitcherModeAnimation;
    private ObjectAnimator mDelayedTabSwitcherModeAnimation;

    @ViewDebug.ExportedProperty(category = "chrome")
    protected int mTabSwitcherState;
    private boolean mIsShowingStartSurface;
    private boolean mForceHideShadow;

    // This determines whether or not the toolbar draws as expected (false) or whether it always
    // draws as if it's showing the non-tabswitcher, non-animating toolbar. This is used in grabbing
    // a bitmap to use as a texture representation of this view.
    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mTextureCaptureMode;
    private boolean mForceTextureCapture;
    private boolean mLightDrawablesUsedForLastTextureCapture;
    private int mTabCountForLastTextureCapture;

    @ViewDebug.ExportedProperty(category = "chrome")
    private boolean mAnimateNormalToolbar;
    @ViewDebug.ExportedProperty(category = "chrome")
    private boolean mDelayingTabSwitcherAnimation;

    private TabSwitcherDrawable mTabSwitcherAnimationTabStackDrawable;
    // Value that determines the amount of transition from the normal toolbar mode to TabSwitcher
    // mode.  0 = entirely in normal mode and 1.0 = entirely in TabSwitcher mode.  In between values
    // can be used for animating between the two view modes.
    @ViewDebug.ExportedProperty(category = "chrome")
    protected float mTabSwitcherModeFraction;

    // Used to clip the toolbar during the fade transition into and out of TabSwitcher mode.  Only
    // used when |mAnimateNormalToolbar| is false.
    @ViewDebug.ExportedProperty(category = "chrome")
    private Rect mClipRect;

    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mUrlFocusChangeInProgress;

    /** 1.0 is 100% focused, 0 is completely unfocused */
    @ViewDebug.ExportedProperty(category = "chrome")
    private float mUrlFocusChangeFraction;

    /**
     * The degree to which the omnibox has expanded to full width, either because it is getting
     * focused or the NTP search box is being scrolled up. Note that in the latter case, the actual
     * width of the omnibox is not interpolated linearly from this value. The value will be the
     * maximum of {@link #mUrlFocusChangeFraction} and {@link #mNtpSearchBoxScrollFraction}.
     *
     * 0.0 == no expansion, 1.0 == fully expanded.
     */
    @ViewDebug.ExportedProperty(category = "chrome")
    protected float mUrlExpansionFraction;
    private AnimatorSet mUrlFocusLayoutAnimator;

    protected boolean mDisableLocationBarRelayout;
    protected boolean mLayoutLocationBarInFocusedMode;
    private boolean mLayoutLocationBarWithoutExtraButton;
    protected int mUnfocusedLocationBarLayoutWidth;
    protected int mUnfocusedLocationBarLayoutLeft;
    protected int mUnfocusedLocationBarLayoutRight;
    private boolean mUnfocusedLocationBarUsesTransparentBg;

    private int mLocationBarBackgroundAlpha = 255;
    private float mNtpSearchBoxScrollFraction = UNINITIALIZED_FRACTION;
    protected ColorDrawable mToolbarBackground;

    /** The omnibox background (white with a shadow). */
    private Drawable mLocationBarBackground;
    private Drawable mActiveLocationBarBackground;

    protected boolean mForceDrawLocationBarBackground;

    /** The boundaries of the omnibox, without the NTP-specific offset applied. */
    protected final Rect mLocationBarBackgroundBounds = new Rect();

    private final Rect mBackgroundOverlayBounds = new Rect();

    /** Offset applied to the bounds of the omnibox if we are showing a New Tab Page. */
    private final Rect mLocationBarBackgroundNtpOffset = new Rect();

    /**
     * Offsets applied to the <i>contents</i> of the omnibox if we are showing a New Tab Page.
     * This can be different from {@link #mLocationBarBackgroundNtpOffset} due to the fact that we
     * extend the omnibox horizontally beyond the screen boundaries when focused, to hide its
     * rounded corners.
     */
    private float mLocationBarNtpOffsetLeft;
    private float mLocationBarNtpOffsetRight;

    private final Rect mNtpSearchBoxBounds = new Rect();
    protected final Point mNtpSearchBoxTranslation = new Point();

    protected final int mToolbarSidePadding;

    private ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private boolean mIsHomeButtonEnabled;

    private Runnable mLayoutUpdater;

    /** The vertical inset of the location bar background. */
    private int mLocationBarBackgroundVerticalInset;

    /** The current color of the location bar. */
    private int mCurrentLocationBarColor;

    /** Whether the toolbar has a pending request to call {@link triggerUrlFocusAnimation()}. */
    private boolean mPendingTriggerUrlFocusRequest;

    /**
     * Used to specify the visual state of the toolbar.
     */
    @IntDef({VisualState.NORMAL, VisualState.INCOGNITO, VisualState.BRAND_COLOR,
            VisualState.NEW_TAB_NORMAL})
    @Retention(RetentionPolicy.SOURCE)
    private @interface VisualState {
        int NORMAL = 0;
        int INCOGNITO = 1;
        int BRAND_COLOR = 2;
        int NEW_TAB_NORMAL = 3;
    }

    protected @VisualState int mVisualState = VisualState.NORMAL;

    private float mPreTextureCaptureAlpha = 1f;
    private int mPreTextureCaptureVisibility;
    private boolean mIsOverlayTabStackDrawableLight;

    private AnimatorSet mOptionalButtonAnimator;
    private boolean mOptionalButtonAnimationRunning;
    private int mOptionalButtonTranslation;
    private int mUrlFocusTranslationX;

    /**
     * The progress fraction for the location bar width change animation that is run when the
     * optional button is shown/hidden. Animates from 1.f to 0.f when showing the button and
     * 0.f to 1.f when hiding the button, where 0.f indicates the location bar width is not offset
     * at all for the animation.
     */
    private float mLocBarWidthChangeFraction;

    /**
     * A global layout listener used to capture a new texture when the experimental toolbar button
     * is added or removed.
     */
    private ViewTreeObserver.OnGlobalLayoutListener mOptionalButtonLayoutListener;

    // The following are some properties used during animation.  We use explicit property classes
    // to avoid the cost of reflection for each animation setup.

    private final Property<ToolbarPhone, Float> mUrlFocusChangeFractionProperty =
            new Property<ToolbarPhone, Float>(Float.class, "") {
                @Override
                public Float get(ToolbarPhone object) {
                    return object.mUrlFocusChangeFraction;
                }

                @Override
                public void set(ToolbarPhone object, Float value) {
                    setUrlFocusChangeFraction(value);
                }
            };

    private final Property<ToolbarPhone, Float> mTabSwitcherModeFractionProperty =
            new Property<ToolbarPhone, Float>(Float.class, "") {
                @Override
                public Float get(ToolbarPhone object) {
                    return object.mTabSwitcherModeFraction;
                }

                @Override
                public void set(ToolbarPhone object, Float value) {
                    object.mTabSwitcherModeFraction = value;
                    triggerPaintInvalidate(ToolbarPhone.this::postInvalidateOnAnimation);
                }
            };

    private final Property<ToolbarPhone, Float> mLocBarWidthChangeFractionProperty =
            new Property<ToolbarPhone, Float>(Float.class, "") {
                @Override
                public Float get(ToolbarPhone object) {
                    return object.mLocBarWidthChangeFraction;
                }

                @Override
                public void set(ToolbarPhone object, Float value) {
                    mLocBarWidthChangeFraction = value;
                    updateLocationBarLayoutForExpansionAnimation();
                }
            };

    /**
     * Constructs a ToolbarPhone object.
     *
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public ToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
        mToolbarSidePadding = getResources().getDimensionPixelOffset(R.dimen.toolbar_edge_padding);
    }

    @Override
    public void onFinishInflate() {
        try (TraceEvent te = TraceEvent.scoped("ToolbarPhone.onFinishInflate")) {
            super.onFinishInflate();
            mToolbarButtonsContainer = (ViewGroup) findViewById(R.id.toolbar_buttons);
            mHomeButton = findViewById(R.id.home_button);
            mUrlBar = (TextView) findViewById(R.id.url_bar);
            mUrlActionContainer = findViewById(R.id.url_action_container);
            mToolbarBackground =
                    new ColorDrawable(getToolbarColorForVisualState(VisualState.NORMAL));

            setLayoutTransition(null);

            if (getMenuButtonCoordinator() != null) {
                getMenuButtonCoordinator().setVisibility(true);
            }

            inflateTabSwitchingResources();

            setWillNotDraw(false);
            mUrlFocusTranslationX =
                    getResources().getDimensionPixelSize(R.dimen.toolbar_url_focus_translation_x);
        }
    }

    @Override
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {
        mLocationBar = locationBarCoordinator;
        initLocationBarBackground();
    }

    @Override
    void destroy() {
        cancelAnimations();
        super.destroy();
    }

    /**
     * Initializes the background, padding, margins, etc. for the location bar background.
     */
    private void initLocationBarBackground() {
        Resources res = getResources();
        mLocationBarBackgroundVerticalInset =
                res.getDimensionPixelSize(R.dimen.location_bar_vertical_margin);
        mLocationBarBackground = createModernLocationBarBackground(getResources());

        int lateralPadding = res.getDimensionPixelOffset(R.dimen.location_bar_lateral_padding);
        mLocationBar.getPhoneCoordinator().setPadding(lateralPadding, 0, lateralPadding, 0);

        mActiveLocationBarBackground = mLocationBarBackground;
    }

    /**
     * @return The drawable for the modern location bar background.
     */
    public static Drawable createModernLocationBarBackground(Resources resources) {
        Drawable drawable = ApiCompatibilityUtils.getDrawable(
                resources, R.drawable.modern_toolbar_text_box_background_with_primary_color);
        drawable.mutate();
        drawable.setColorFilter(
                ApiCompatibilityUtils.getColor(resources, R.color.toolbar_text_box_background),
                PorterDuff.Mode.SRC_IN);
        return drawable;
    }

    /**
     * Set the background color of the location bar to appropriately match the theme color.
     */
    private void updateModernLocationBarColor(int color) {
        if (mCurrentLocationBarColor == color) return;
        mCurrentLocationBarColor = color;
        mLocationBarBackground.setColorFilter(color, PorterDuff.Mode.SRC_IN);
    }

    /**
     * Get the corresponding location bar color for a toolbar color.
     * @param toolbarColor The color of the toolbar.
     * @return The location bar color.
     */
    private int getLocationBarColorForToolbarColor(int toolbarColor) {
        return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                getResources(), toolbarColor, isIncognito());
    }

    private void inflateTabSwitchingResources() {
        mToggleTabStackButton = findViewById(R.id.tab_switcher_button);
        mToggleTabStackButton.setClickable(false);
    }

    private void enableTabSwitchingResources() {
        mToggleTabStackButton.setOnKeyListener(new KeyboardNavigationListener() {
            @Override
            public View getNextFocusForward() {
                if (isMenuButtonPresent()) {
                    return getMenuButtonCoordinator().getMenuButton();
                } else {
                    return getCurrentTabView();
                }
            }

            @Override
            public View getNextFocusBackward() {
                return findViewById(R.id.url_bar);
            }
        });
    }

    /**
     * Sets up click and key listeners once we have native library available to handle clicks.
     */
    @Override
    protected void onNativeLibraryReady() {
        super.onNativeLibraryReady();

        enableTabSwitchingResources();

        if (mHomeButton != null) {
            mHomeButton.setOnClickListener(this);
        }

        getMenuButtonCoordinator().setOnKeyListener(new KeyboardNavigationListener() {
            @Override
            public View getNextFocusForward() {
                return getCurrentTabView();
            }

            @Override
            public View getNextFocusBackward() {
                return mToggleTabStackButton;
            }

            @Override
            protected boolean handleEnterKeyPress() {
                return getMenuButtonCoordinator().onEnterKeyPress();
            }
        });

        // Calls the {@link triggerUrlFocusAnimation()} here to finish the pending focus request if
        // it has been skipped in {@link onStartSurfaceStateChanged()}.
        if (mPendingTriggerUrlFocusRequest) {
            // This pending focus must be requested before native initialization when instant start
            // is enabled, whether user clicks omnibox or not.
            assert getToolbarDataProvider().isInOverviewAndShowingOmnibox();
            mPendingTriggerUrlFocusRequest = false;
            triggerUrlFocusAnimation(true);
        }

        updateVisualsForLocationBarState();
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // If the NTP is partially scrolled, prevent all touch events to the child views.  This
        // is to not allow a secondary touch event to trigger entering the tab switcher, which
        // can lead to really odd snapshots and transitions to the switcher.
        if (mNtpSearchBoxScrollFraction != 0f && mNtpSearchBoxScrollFraction != 1f
                && mNtpSearchBoxScrollFraction != UNINITIALIZED_FRACTION) {
            return true;
        }

        return super.onInterceptTouchEvent(ev);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Forward touch events to the NTP if the toolbar is moved away but the search box hasn't
        // reached the top of the page yet.
        if (mNtpSearchBoxTranslation.y < 0
                && mLocationBar.getPhoneCoordinator().getTranslationY() > 0) {
            return getToolbarDataProvider().getNewTabPageDelegate().dispatchTouchEvent(ev);
        }

        return super.onTouchEvent(ev);
    }

    @Override
    public void onClick(View v) {
        // Don't allow clicks while the omnibox is being focused.
        if (mLocationBar != null && mLocationBar.getPhoneCoordinator().hasFocus()) {
            return;
        }
        if (mHomeButton != null && mHomeButton == v) {
            openHomepage();
            if (isNativeLibraryReady()
                    && PartnerBrowserCustomizations.getInstance()
                               .isHomepageProviderAvailableAndEnabled()) {
                Profile profile = getToolbarDataProvider().getProfile();
                TrackerFactory.getTrackerForProfile(profile).notifyEvent(
                        EventConstants.PARTNER_HOME_PAGE_BUTTON_PRESSED);
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (!mDisableLocationBarRelayout) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);

            boolean changed = layoutLocationBar(MeasureSpec.getSize(widthMeasureSpec));
            if (!isInTabSwitcherMode()) updateUrlExpansionAnimation();
            if (!changed) return;
        } else {
            updateUnfocusedLocationBarLayoutParams();
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * @return True if layout bar's unfocused width has changed, potentially causing updates to
     *         visual elements. If this happens during measurement pass, then toolbar's layout needs
     *         to be remeasured.
     */
    private boolean updateUnfocusedLocationBarLayoutParams() {
        int leftViewBounds = getViewBoundsLeftOfLocationBar(mVisualState);
        int rightViewBounds = getViewBoundsRightOfLocationBar(mVisualState);

        mUnfocusedLocationBarLayoutLeft = leftViewBounds;
        mUnfocusedLocationBarLayoutRight = rightViewBounds;
        int unfocusedLocationBarLayoutWidth = rightViewBounds - leftViewBounds;
        if (mUnfocusedLocationBarLayoutWidth != unfocusedLocationBarLayoutWidth) {
            mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
            mLocationBar.setUnfocusedWidth(mUnfocusedLocationBarLayoutWidth);
            return true;
        }
        return false;
    }

    /**
     * @return The background drawable for the toolbar view.
     */
    @VisibleForTesting
    public ColorDrawable getBackgroundDrawable() {
        return mToolbarBackground;
    }

    @SuppressLint("RtlHardcoded")
    private boolean layoutLocationBar(int containerWidth) {
        TraceEvent.begin("ToolbarPhone.layoutLocationBar");
        // Note that Toolbar's direction depends on system layout direction while
        // LocationBar's direction depends on its text inside.
        FrameLayout.LayoutParams locationBarLayoutParams =
                getFrameLayoutParams(getLocationBar().getContainerView());

        // Chrome prevents layout_gravity="left" from being defined in XML, but it simplifies
        // the logic, so it is manually specified here.
        locationBarLayoutParams.gravity = Gravity.TOP | Gravity.LEFT;

        int width = 0;
        int leftMargin = 0;

        boolean changed = false;

        // Always update the unfocused layout params regardless of whether we are using
        // those in this current layout pass as they are needed for animations.
        changed |= updateUnfocusedLocationBarLayoutParams();

        if (mLayoutLocationBarInFocusedMode
                || (mVisualState == VisualState.NEW_TAB_NORMAL
                        && mTabSwitcherState == STATIC_TAB)) {
            int priorVisibleWidth =
                    mLocationBar.getPhoneCoordinator().getOffsetOfFirstVisibleFocusedView();
            width = getFocusedLocationBarWidth(containerWidth, priorVisibleWidth);
            leftMargin = getFocusedLocationBarLeftMargin(priorVisibleWidth);
        } else {
            width = mUnfocusedLocationBarLayoutWidth;
            leftMargin = mUnfocusedLocationBarLayoutLeft;
        }

        if (mLayoutLocationBarWithoutExtraButton) {
            float offset = getLocationBarWidthOffsetForOptionalButton();
            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) leftMargin -= (int) offset;
            width += (int) offset;
        }

        changed |= (width != locationBarLayoutParams.width);
        locationBarLayoutParams.width = width;

        changed |= (leftMargin != locationBarLayoutParams.leftMargin);
        locationBarLayoutParams.leftMargin = leftMargin;

        if (changed) updateLocationBarLayoutForExpansionAnimation();

        TraceEvent.end("ToolbarPhone.layoutLocationBar");
        return changed;
    }

    /**
     * @param containerWidth The width of the view containing the location bar.
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @return The width of the location bar when it has focus.
     */
    private int getFocusedLocationBarWidth(int containerWidth, int priorVisibleWidth) {
        int width = containerWidth - (2 * mToolbarSidePadding) + priorVisibleWidth;

        return width;
    }

    /**
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @return The left margin of the location bar when it has focus.
     */
    private int getFocusedLocationBarLeftMargin(int priorVisibleWidth) {
        int baseMargin = mToolbarSidePadding;
        if (mLocationBar.getPhoneCoordinator().getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            return baseMargin;
        } else {
            return baseMargin - priorVisibleWidth;
        }
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The left bounds of the location bar, accounting for any buttons on the left side
     *         of the toolbar.
     */
    private int getViewBoundsLeftOfLocationBar(@VisualState int visualState) {
        // Uses getMeasuredWidth()s instead of getLeft() because this is called in onMeasure
        // and the layout values have not yet been set.
        if (visualState == VisualState.NEW_TAB_NORMAL && mTabSwitcherState == STATIC_TAB) {
            return mToolbarSidePadding;
        } else if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            return getBoundsAfterAccountingForRightButtons();
        } else {
            return getBoundsAfterAccountingForLeftButton();
        }
    }

    /**
     * @return The left bounds of the location bar after accounting for any visible left buttons.
     */
    private int getBoundsAfterAccountingForLeftButton() {
        int padding = mToolbarSidePadding;
        if (mHomeButton != null && mHomeButton.getVisibility() != GONE) {
            padding = mHomeButton.getMeasuredWidth();
        }
        return padding;
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The right bounds of the location bar, accounting for any buttons on the right side
     *         of the toolbar.
     */
    private int getViewBoundsRightOfLocationBar(@VisualState int visualState) {
        // Uses getMeasuredWidth()s instead of getRight() because this is called in onMeasure
        // and the layout values have not yet been set.
        if (visualState == VisualState.NEW_TAB_NORMAL && mTabSwitcherState == STATIC_TAB) {
            return getMeasuredWidth() - mToolbarSidePadding;
        } else if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            return getMeasuredWidth() - getBoundsAfterAccountingForLeftButton();
        } else {
            return getMeasuredWidth() - getBoundsAfterAccountingForRightButtons();
        }
    }

    /**
     * @return The right bounds of the location bar after accounting for any visible left buttons.
     */
    private int getBoundsAfterAccountingForRightButtons() {
        return Math.max(mToolbarSidePadding, mToolbarButtonsContainer.getMeasuredWidth());
    }

    private void updateToolbarBackground(int color) {
        if (mToolbarBackground.getColor() == color) return;
        mToolbarBackground.setColor(color);
        invalidate();
    }

    private void updateToolbarBackgroundFromState(@VisualState int visualState) {
        updateToolbarBackground(getToolbarColorForVisualState(visualState));
    }


    private int getToolbarColorForVisualState(final @VisualState int visualState) {
        Resources res = getResources();
        switch (visualState) {
            case VisualState.NEW_TAB_NORMAL:
                // When the NTP fake search box is visible, the background color should be
                // transparent. When the location bar reaches the top of the screen (i.e. location
                // bar is fully expanded), the background needs to change back to the default
                // toolbar color so that the NTP content is not visible beneath the toolbar. In
                // between the transition, we set a translucent default toolbar color based on
                // the expansion progress of the toolbar.
                return androidx.core.graphics.ColorUtils.setAlphaComponent(
                        ChromeColors.getDefaultThemeColor(getResources(), false),
                        Math.round(mUrlExpansionFraction * 255));
            case VisualState.NORMAL:
                return ChromeColors.getDefaultThemeColor(getResources(), false);
            case VisualState.INCOGNITO:
                return ChromeColors.getDefaultThemeColor(getResources(), true);
            case VisualState.BRAND_COLOR:
                return getToolbarDataProvider().getPrimaryColor();
            default:
                assert false;
                return ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary);
        }
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (!mTextureCaptureMode && mToolbarBackground.getColor() != Color.TRANSPARENT) {
            // Update to compensate for orientation changes.
            mToolbarBackground.setBounds(0, 0, getWidth(), getHeight());
            mToolbarBackground.draw(canvas);
        }

        if (mLocationBarBackground != null
                && (mLocationBar.getPhoneCoordinator().getVisibility() == VISIBLE
                        || mTextureCaptureMode)) {
            updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, mVisualState);
        }

        if (mTextureCaptureMode) {
            drawTabSwitcherAnimationOverlay(canvas, 0.f);
        } else {
            boolean tabSwitcherAnimationFinished = false;
            if (mTabSwitcherModeAnimation != null) {
                tabSwitcherAnimationFinished = !mTabSwitcherModeAnimation.isRunning();

                // Perform the fade logic before super.dispatchDraw(canvas) so that we can properly
                // set the values before the draw happens.
                if (!mAnimateNormalToolbar) {
                    drawTabSwitcherFadeAnimation(
                            tabSwitcherAnimationFinished, mTabSwitcherModeFraction);
                }
            }

            super.dispatchDraw(canvas);

            if (mTabSwitcherModeAnimation != null) {
                // Perform the overlay logic after super.dispatchDraw(canvas) as we need to draw on
                // top of the current views.
                if (mAnimateNormalToolbar) {
                    drawTabSwitcherAnimationOverlay(canvas, mTabSwitcherModeFraction);
                }

                // Clear the animation.
                if (tabSwitcherAnimationFinished) mTabSwitcherModeAnimation = null;
            }
        }
    }

    @Override
    protected boolean verifyDrawable(Drawable who) {
        return super.verifyDrawable(who) || who == mActiveLocationBarBackground;
    }

    private void onNtpScrollChanged(float scrollFraction) {
        mNtpSearchBoxScrollFraction = scrollFraction;
        updateUrlExpansionFraction();
        updateUrlExpansionAnimation();
    }

    /**
     * @return True if the toolbar is showing tab switcher assets, including during transitions.
     */
    public boolean isInTabSwitcherMode() {
        return mTabSwitcherState != STATIC_TAB;
    }

    /**
     * Calculate the bounds for the location bar background and set them to {@code out}.
     */
    private void updateLocationBarBackgroundBounds(Rect out, @VisualState int visualState) {
        // Calculate the visible boundaries of the left and right most child views of the
        // location bar.
        float expansion = getExpansionFractionForVisualState(visualState);
        int leftViewPosition = getLeftPositionOfLocationBarBackground(visualState);
        int rightViewPosition = getRightPositionOfLocationBarBackground(visualState);

        // The bounds are set by the following:
        // - The left most visible location bar child view.
        // - The top of the viewport is aligned with the top of the location bar.
        // - The right most visible location bar child view.
        // - The bottom of the viewport is aligned with the bottom of the location bar.
        // Additional padding can be applied for use during animations.
        out.set(leftViewPosition,
                mLocationBar.getPhoneCoordinator().getTop() + mLocationBarBackgroundVerticalInset,
                rightViewPosition,
                mLocationBar.getPhoneCoordinator().getBottom()
                        - mLocationBarBackgroundVerticalInset);
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The left drawing position for the location bar background.
     */
    private int getLeftPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionFractionForVisualState(visualState);
        int leftViewPosition =
                (int) MathUtils.interpolate(getViewBoundsLeftOfLocationBar(visualState),
                        getFocusedLeftPositionOfLocationBarBackground(), expansion);

        if (mOptionalButtonAnimationRunning && getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            leftViewPosition -= getLocationBarBackgroundOffsetForOptionalButton();
        }

        return leftViewPosition;
    }

    /**
     * @return The left drawing position for the location bar background when the location bar
     *         has focus.
     */
    private int getFocusedLeftPositionOfLocationBarBackground() {
        return mToolbarSidePadding;
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The right drawing position for the location bar background.
     */
    private int getRightPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionFractionForVisualState(visualState);
        int rightViewPosition =
                (int) MathUtils.interpolate(getViewBoundsRightOfLocationBar(visualState),
                        getFocusedRightPositionOfLocationBarBackground(), expansion);

        if (mOptionalButtonAnimationRunning && !(getLayoutDirection() == LAYOUT_DIRECTION_RTL)) {
            rightViewPosition += getLocationBarBackgroundOffsetForOptionalButton();
        }

        return rightViewPosition;
    }

    /**
     * @return The location bar background position offset, for use when the optional button
     *         show/hide animation is running.
     */
    private int getLocationBarBackgroundOffsetForOptionalButton() {
        return (int) (getLocationBarWidthOffsetForOptionalButton() * mLocBarWidthChangeFraction);
    }

    /**
     * @return The difference in the location bar width when the optional button is hidden
     *         rather than showing. This is effectively the width of the optional button with
     *         some adjustment to account for possible padding differences when the button
     *         visibility changes.
     */
    @VisibleForTesting
    float getLocationBarWidthOffsetForOptionalButton() {
        float widthChange = mOptionalButton.getWidth();

        // When the optional button is the only visible button after the location bar and the
        // button is hidden mToolbarSidePadding is used for the padding after the location bar.
        if (!isMenuButtonPresent()) {
            widthChange -= mToolbarSidePadding;
        }
        return widthChange;
    }

    /**
     * @return The right drawing position for the location bar background when the location bar
     *         has focus.
     */
    private int getFocusedRightPositionOfLocationBarBackground() {
        return getWidth() - mToolbarSidePadding;
    }

    private float getExpansionFractionForVisualState(@VisualState int visualState) {
        return visualState == VisualState.NEW_TAB_NORMAL && mTabSwitcherState == STATIC_TAB
                        || getToolbarDataProvider().isInOverviewAndShowingOmnibox()
                ? 1
                : mUrlExpansionFraction;
    }

    /**
     * Updates progress of current the URL focus change animation.
     *
     * @param fraction 1.0 is 100% focused, 0 is completely unfocused.
     */
    private void setUrlFocusChangeFraction(float fraction) {
        mUrlFocusChangeFraction = fraction;
        updateUrlExpansionFraction();
        updateUrlExpansionAnimation();
    }

    private void updateUrlExpansionFraction() {
        mUrlExpansionFraction = Math.max(mNtpSearchBoxScrollFraction, mUrlFocusChangeFraction);
        for (UrlExpansionObserver observer : mUrlExpansionObservers) {
            observer.onUrlExpansionProgressChanged(mUrlExpansionFraction);
        }
        assert mUrlExpansionFraction >= 0;
        assert mUrlExpansionFraction <= 1;
    }

    /**
     * Updates the parameters relating to expanding the location bar, as the result of either a
     * focus change or scrolling the New Tab Page.
     */
    private void updateUrlExpansionAnimation() {
        // TODO(https://crbug.com/865801): Prevent url expansion signals from happening while the
        // toolbar is not visible (e.g. in tab switcher mode).
        if (isInTabSwitcherMode()) return;

        int toolbarButtonVisibility = getToolbarButtonVisibility();
        mToolbarButtonsContainer.setVisibility(toolbarButtonVisibility);
        if (!getToolbarDataProvider().isInOverviewAndShowingOmnibox()) {
            if (mHomeButton != null && mHomeButton.getVisibility() != GONE) {
                mHomeButton.setVisibility(toolbarButtonVisibility);
            }
        }

        updateLocationBarLayoutForExpansionAnimation();
    }

    /**
     * @return The visibility for {@link #mToolbarButtonsContainer}.
     */
    private int getToolbarButtonVisibility() {
        return mUrlExpansionFraction == 1f ? INVISIBLE : VISIBLE;
    }

    /**
     * Updates the location bar layout, as the result of either a focus change or scrolling the
     * New Tab Page.
     */
    private void updateLocationBarLayoutForExpansionAnimation() {
        TraceEvent.begin("ToolbarPhone.updateLocationBarLayoutForExpansionAnimation");
        FrameLayout.LayoutParams locationBarLayoutParams =
                mLocationBar.getPhoneCoordinator().getFrameLayoutParams();
        int currentLeftMargin = locationBarLayoutParams.leftMargin;
        int currentWidth = locationBarLayoutParams.width;

        float locationBarBaseTranslationX = mUnfocusedLocationBarLayoutLeft - currentLeftMargin;
        if (mOptionalButtonAnimationRunning) {
            // When showing the button, we disable location bar relayout
            // (mDisableLocationBarRelayout), so the location bar's left margin and
            // mUnfocusedLocationBarLayoutLeft have not been updated to take into account the
            // appearance of the optional icon. The views to left of the location bar will
            // be wider than mUnfocusedlocationBarLayoutLeft in RTL, so adjust the translation by
            // that amount.
            // When hiding the button, we force a relayout without the optional toolbar button
            // (mLayoutLocationBarWithoutExtraButton). mUnfocusedLocationBarLayoutLeft reflects
            // the view bounds left of the location bar, which still includes the optional
            // button. The location bar left margin, however, has been adjusted to reflect its
            // end value when the optional button is fully hidden. The
            // locationBarBaseTranslationX above accounts for the difference between
            // mUnfocusedLocationBarLayoutLeft and the location bar's current left margin.
            locationBarBaseTranslationX +=
                    getViewBoundsLeftOfLocationBar(mVisualState) - mUnfocusedLocationBarLayoutLeft;
        }

        // When the dse icon is visible, the LocationBar needs additional translation to compensate
        // for the dse icon being laid out when focused. This also affects the UrlBar, which is
        // handled below. See comments in LocationBar#getLocationBarOffsetForFocusAnimation() for
        // implementation details.
        boolean isIncognito = getToolbarDataProvider().isIncognito();
        if (SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(isIncognito)) {
            locationBarBaseTranslationX += getLocationBarOffsetForFocusAnimation(hasFocus());
        }

        boolean isLocationBarRtl =
                mLocationBar.getPhoneCoordinator().getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if (isLocationBarRtl) {
            locationBarBaseTranslationX += mUnfocusedLocationBarLayoutWidth - currentWidth;
        }

        locationBarBaseTranslationX *= 1f
                - (mOptionalButtonAnimationRunning ? mLocBarWidthChangeFraction
                                                   : mUrlExpansionFraction);

        mLocationBarBackgroundNtpOffset.setEmpty();
        mLocationBarNtpOffsetLeft = 0;
        mLocationBarNtpOffsetRight = 0;

        Tab currentTab = getToolbarDataProvider().getTab();
        if (currentTab != null) {
            getToolbarDataProvider().getNewTabPageDelegate().setUrlFocusChangeAnimationPercent(
                    mUrlFocusChangeFraction);
            if (isLocationBarShownInNTP()
                    && !getToolbarDataProvider().isInOverviewAndShowingOmnibox()) {
                updateNtpTransitionAnimation();
            } else {
                // Reset these values in case we transitioned to a different page during the
                // transition.
                resetNtpAnimationValues();
            }
        }

        float locationBarTranslationX;
        if (isLocationBarRtl) {
            locationBarTranslationX = locationBarBaseTranslationX + mLocationBarNtpOffsetRight;
        } else {
            locationBarTranslationX = locationBarBaseTranslationX + mLocationBarNtpOffsetLeft;
        }

        mLocationBar.getPhoneCoordinator().setTranslationX(locationBarTranslationX);

        // When the dse icon is enabled, the UrlBar needs additional translation to compensate for
        // the additional translation applied to the LocationBar. See comments in
        // LocationBar#getUrlBarTranslationXForToolbarAnimation() for implementation details.
        if (SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(isIncognito)) {
            mUrlBar.setTranslationX(
                    getUrlBarTranslationXForToolbarAnimation(mUrlExpansionFraction, hasFocus()));
        } else if (SearchEngineLogoUtils.getInstance().isSearchEngineLogoEnabled()) {
            mUrlBar.setTranslationX(0);
        }

        if (!mOptionalButtonAnimationRunning) {
            mUrlActionContainer.setTranslationX(getUrlActionsTranslationXForExpansionAnimation(
                    isLocationBarRtl, locationBarBaseTranslationX));
            mLocationBar.setUrlFocusChangeFraction(mUrlExpansionFraction);

            // Only transition theme colors if in static tab mode that is not the NTP. In practice
            // this only runs when you focus the omnibox on a web page.
            if (!isLocationBarShownInNTP() && mTabSwitcherState == STATIC_TAB) {
                int defaultColor = ChromeColors.getDefaultThemeColor(getResources(), isIncognito());
                int defaultLocationBarColor = getLocationBarColorForToolbarColor(defaultColor);
                int primaryColor = getToolbarDataProvider().getPrimaryColor();
                int themedLocationBarColor = getLocationBarColorForToolbarColor(primaryColor);

                updateToolbarBackground(ColorUtils.getColorWithOverlay(
                        primaryColor, defaultColor, mUrlFocusChangeFraction));

                updateModernLocationBarColor(ColorUtils.getColorWithOverlay(
                        themedLocationBarColor, defaultLocationBarColor, mUrlFocusChangeFraction));
            }
        }

        // Force an invalidation of the location bar to properly handle the clipping of the URL
        // bar text as a result of the URL action container translations.
        mLocationBar.getPhoneCoordinator().invalidate();
        invalidate();
        TraceEvent.end("ToolbarPhone.updateLocationBarLayoutForExpansionAnimation");
    }

    /**
     * Calculates the translation X for the URL actions container for use in the URL expansion
     * animation.
     *
     * @param isLocationBarRtl Whether the location bar layout is RTL.
     * @param locationBarBaseTranslationX The base location bar translation for the URL expansion
     *                                    animation.
     * @return The translation X for the URL actions container.
     */
    private float getUrlActionsTranslationXForExpansionAnimation(
            boolean isLocationBarRtl, float locationBarBaseTranslationX) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        float urlActionsTranslationX = 0;
        if (!isLocationBarRtl || isRtl) {
            // Negate the location bar translation to keep the URL action container in the same
            // place during the focus expansion.
            urlActionsTranslationX = -locationBarBaseTranslationX;
        }

        if (isRtl) {
            urlActionsTranslationX += mLocationBarNtpOffsetLeft - mLocationBarNtpOffsetRight;
        } else {
            urlActionsTranslationX += mLocationBarNtpOffsetRight - mLocationBarNtpOffsetLeft;
        }

        return urlActionsTranslationX;
    }

    /**
     * Reset the parameters for the New Tab Page transition animation (expanding the location bar as
     * a result of scrolling the New Tab Page) to their default values.
     */
    private void resetNtpAnimationValues() {
        mLocationBarBackgroundNtpOffset.setEmpty();
        mActiveLocationBarBackground = mLocationBarBackground;
        mNtpSearchBoxTranslation.set(0, 0);
        mLocationBar.getPhoneCoordinator().setTranslationY(0);
        if (!mUrlFocusChangeInProgress) {
            mToolbarButtonsContainer.setTranslationY(0);
            if (mHomeButton != null) mHomeButton.setTranslationY(0);
        }

        if (!mUrlFocusChangeInProgress) {
            mToolbarShadow.setAlpha(mUrlBar.hasFocus() ? 0.f : 1.f);
        }

        mLocationBar.getPhoneCoordinator().setAlpha(1);
        mForceDrawLocationBarBackground = false;
        mLocationBarBackgroundAlpha = 255;
        if (isIncognito()
                || (mUnfocusedLocationBarUsesTransparentBg && !mUrlFocusChangeInProgress
                        && !mLocationBar.getPhoneCoordinator().hasFocus())) {
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        }

        setAncestorsShouldClipChildren(true);
        mNtpSearchBoxScrollFraction = UNINITIALIZED_FRACTION;
        updateUrlExpansionFraction();
    }

    /**
     * Updates the parameters of the New Tab Page transition animation (expanding the location bar
     * as a result of scrolling the New Tab Page).
     */
    private void updateNtpTransitionAnimation() {
        // Skip if in or entering tab switcher mode.
        if (mTabSwitcherState == TAB_SWITCHER || mTabSwitcherState == ENTERING_TAB_SWITCHER) return;

        boolean isExpanded = mUrlExpansionFraction > 0f;
        setAncestorsShouldClipChildren(!isExpanded);
        if (!mUrlFocusChangeInProgress) {
            float alpha = 0.f;
            if (!mUrlBar.hasFocus() && mNtpSearchBoxScrollFraction == 1.f) {
                alpha = 1.f;
            }
            mToolbarShadow.setAlpha(alpha);
        }

        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        ntpDelegate.getSearchBoxBounds(mNtpSearchBoxBounds, mNtpSearchBoxTranslation);
        int locationBarTranslationY = Math.max(
                0, (mNtpSearchBoxBounds.top - mLocationBar.getPhoneCoordinator().getTop()));
        mLocationBar.getPhoneCoordinator().setTranslationY(locationBarTranslationY);

        updateButtonsTranslationY();

        // Linearly interpolate between the bounds of the search box on the NTP and the omnibox
        // background bounds. |shrinkage| is the scaling factor for the offset -- if it's 1, we are
        // shrinking the omnibox down to the size of the search box.
        float shrinkage = 1f
                - Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR.getInterpolation(
                        mUrlExpansionFraction);

        int leftBoundDifference = mNtpSearchBoxBounds.left - mLocationBarBackgroundBounds.left;
        int rightBoundDifference = mNtpSearchBoxBounds.right - mLocationBarBackgroundBounds.right;
        int verticalInset = (int) (getResources().getDimensionPixelSize(
                                           R.dimen.ntp_search_box_bounds_vertical_inset_modern)
                * (1.f - mUrlExpansionFraction));
        mLocationBarBackgroundNtpOffset.set(Math.round(leftBoundDifference * shrinkage),
                locationBarTranslationY, Math.round(rightBoundDifference * shrinkage),
                locationBarTranslationY);
        mLocationBarBackgroundNtpOffset.inset(0, verticalInset);

        mLocationBarNtpOffsetLeft = leftBoundDifference * shrinkage;
        mLocationBarNtpOffsetRight = rightBoundDifference * shrinkage;

        mLocationBarBackgroundAlpha = isExpanded ? 255 : 0;
        mForceDrawLocationBarBackground = mLocationBarBackgroundAlpha > 0;
        float relativeAlpha = mLocationBarBackgroundAlpha / 255f;
        mLocationBar.getPhoneCoordinator().setAlpha(relativeAlpha);

        // The search box on the NTP is visible if our omnibox is invisible, and vice-versa.
        ntpDelegate.setSearchBoxAlpha(1f - relativeAlpha);
        if (!mForceDrawLocationBarBackground) {
            if (mActiveLocationBarBackground instanceof NtpSearchBoxDrawable) {
                ((NtpSearchBoxDrawable) mActiveLocationBarBackground).resetBoundsToLastNonToolbar();
            }
        }

        updateToolbarBackgroundFromState(mVisualState);
    }

    /**
     * Update the y translation of the buttons to make it appear as if they were scrolling with
     * the new tab page.
     */
    private void updateButtonsTranslationY() {
        int transY = mTabSwitcherState == STATIC_TAB ? Math.min(mNtpSearchBoxTranslation.y, 0) : 0;

        mToolbarButtonsContainer.setTranslationY(transY);
        if (mHomeButton != null) mHomeButton.setTranslationY(transY);
    }

    private void setAncestorsShouldClipChildren(boolean clip) {
        if (!isLocationBarShownInNTP()) return;

        ViewUtils.setAncestorsShouldClipChildren(this, clip);
    }

    private void drawTabSwitcherFadeAnimation(boolean animationFinished, float progress) {
        setAlpha(progress);
        if (animationFinished) {
            mClipRect = null;
        } else if (mClipRect == null) {
            mClipRect = new Rect();
        }
        if (mClipRect != null) mClipRect.set(0, 0, getWidth(), (int) (getHeight() * progress));
    }

    /**
     * When entering and exiting the TabSwitcher mode, we fade out or fade in the browsing
     * mode of the toolbar on top of the TabSwitcher mode version of it.  We do this by
     * drawing all of the browsing mode views on top of the android view.
     */
    @VisibleForTesting
    void drawTabSwitcherAnimationOverlay(Canvas canvas, float animationProgress) {
        if (!isNativeLibraryReady()) return;

        float floatAlpha = 1 - animationProgress;
        int rgbAlpha = (int) (255 * floatAlpha);
        canvas.save();
        canvas.translate(0, -animationProgress * mBackgroundOverlayBounds.height());
        canvas.clipRect(mBackgroundOverlayBounds);

        float previousAlpha = 0.f;
        if (mHomeButton != null && mHomeButton.getVisibility() != View.GONE) {
            // Draw the New Tab button used in the URL view.
            previousAlpha = mHomeButton.getAlpha();
            mHomeButton.setAlpha(previousAlpha * floatAlpha);
            drawChild(canvas, mHomeButton, SystemClock.uptimeMillis());
            mHomeButton.setAlpha(previousAlpha);
        }

        // Draw the location/URL bar.
        previousAlpha = mLocationBar.getPhoneCoordinator().getAlpha();
        mLocationBar.getPhoneCoordinator().setAlpha(previousAlpha * floatAlpha);
        // If the location bar is now fully transparent, do not bother drawing it.
        if (mLocationBar.getPhoneCoordinator().getAlpha() != 0 && isLocationBarCurrentlyShown()) {
            drawLocationBar(canvas, SystemClock.uptimeMillis());
        }
        mLocationBar.getPhoneCoordinator().setAlpha(previousAlpha);

        // Translate to draw end toolbar buttons.
        ViewUtils.translateCanvasToView(this, mToolbarButtonsContainer, canvas);

        // Draw the optional button if necessary.
        if (mOptionalButton != null && mOptionalButton.getVisibility() != View.GONE) {
            canvas.save();
            Drawable optionalButtonDrawable = mOptionalButton.getDrawable();

            ViewUtils.translateCanvasToView(mToolbarButtonsContainer, mOptionalButton, canvas);

            int backgroundWidth = mOptionalButton.getDrawable().getIntrinsicWidth();
            int backgroundHeight = mOptionalButton.getDrawable().getIntrinsicHeight();
            int backgroundLeft = (mOptionalButton.getWidth() - mOptionalButton.getPaddingLeft()
                                         - mOptionalButton.getPaddingRight() - backgroundWidth)
                    / 2;
            backgroundLeft += mOptionalButton.getPaddingLeft();
            int backgroundTop = (mOptionalButton.getHeight() - mOptionalButton.getPaddingTop()
                                        - mOptionalButton.getPaddingBottom() - backgroundHeight)
                    / 2;
            backgroundTop += mOptionalButton.getPaddingTop();
            canvas.translate(backgroundLeft, backgroundTop);

            optionalButtonDrawable.setAlpha(rgbAlpha);
            optionalButtonDrawable.draw(canvas);

            canvas.restore();
        }

        // Draw the tab stack button and associated text if necessary.
        if (mTabSwitcherAnimationTabStackDrawable != null && mToggleTabStackButton != null
                && mUrlExpansionFraction != 1f) {
            // Draw the tab stack button image.
            canvas.save();
            ViewUtils.translateCanvasToView(
                    mToolbarButtonsContainer, mToggleTabStackButton, canvas);

            int backgroundWidth = mToggleTabStackButton.getDrawable().getIntrinsicWidth();
            int backgroundHeight = mToggleTabStackButton.getDrawable().getIntrinsicHeight();
            int backgroundLeft =
                    (mToggleTabStackButton.getWidth() - mToggleTabStackButton.getPaddingLeft()
                            - mToggleTabStackButton.getPaddingRight() - backgroundWidth)
                    / 2;
            backgroundLeft += mToggleTabStackButton.getPaddingLeft();
            int backgroundTop =
                    (mToggleTabStackButton.getHeight() - mToggleTabStackButton.getPaddingTop()
                            - mToggleTabStackButton.getPaddingBottom() - backgroundHeight)
                    / 2;
            backgroundTop += mToggleTabStackButton.getPaddingTop();
            canvas.translate(backgroundLeft, backgroundTop);

            mTabSwitcherAnimationTabStackDrawable.setBounds(
                    mToggleTabStackButton.getDrawable().getBounds());
            mTabSwitcherAnimationTabStackDrawable.setAlpha(rgbAlpha);
            mTabSwitcherAnimationTabStackDrawable.draw(canvas);
            canvas.restore();
        }

        // Draw the menu button if necessary.
        final MenuButtonCoordinator menuButtonCoordinator = getMenuButtonCoordinator();
        if (menuButtonCoordinator != null) {
            menuButtonCoordinator.drawTabSwitcherAnimationOverlay(
                    mToolbarButtonsContainer, canvas, rgbAlpha);
        }

        mLightDrawablesUsedForLastTextureCapture = useLight();

        if (mTabSwitcherAnimationTabStackDrawable != null && mToggleTabStackButton != null) {
            mTabCountForLastTextureCapture = mTabSwitcherAnimationTabStackDrawable.getTabCount();
        }

        canvas.restore();
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        if (mLocationBar != null
                && child == mLocationBar.getPhoneCoordinator().getViewForDrawing()) {
            return drawLocationBar(canvas, drawingTime);
        }
        boolean clipped = false;

        if (mLocationBarBackground != null) {
            canvas.save();

            int translationY = (int) mLocationBar.getPhoneCoordinator().getTranslationY();
            int clipTop = mLocationBarBackgroundBounds.top + translationY;
            if (mUrlExpansionFraction != 0f && clipTop < child.getBottom()) {
                // For other child views, use the inverse clipping of the URL viewport.
                // Only necessary during animations.
                // Hardware mode does not support unioned clip regions, so clip using the
                // appropriate bounds based on whether the child is to the left or right of the
                // location bar.
                boolean isLeft = isChildLeft(child);

                int clipBottom = mLocationBarBackgroundBounds.bottom + translationY;
                boolean verticalClip = false;
                if (translationY > 0f) {
                    clipTop = child.getTop();
                    clipBottom = clipTop;
                    verticalClip = true;
                }

                if (isLeft) {
                    int clipRight = verticalClip ? child.getMeasuredWidth()
                                                 : mLocationBarBackgroundBounds.left;
                    canvas.clipRect(0, clipTop, clipRight, clipBottom);
                } else {
                    int clipLeft = verticalClip ? 0 : mLocationBarBackgroundBounds.right;
                    canvas.clipRect(clipLeft, clipTop, getMeasuredWidth(), clipBottom);
                }
            }
            clipped = true;
        }
        boolean retVal = super.drawChild(canvas, child, drawingTime);
        if (clipped) canvas.restore();
        return retVal;
    }

    private boolean isChildLeft(View child) {
        return (mHomeButton != null && child == mHomeButton) ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * @return Whether or not the location bar should be drawing at any particular state of the
     *         toolbar.
     */
    private boolean shouldDrawLocationBar() {
        return mLocationBarBackground != null
                && (mTabSwitcherState == STATIC_TAB || mTextureCaptureMode);
    }

    private boolean drawLocationBar(Canvas canvas, long drawingTime) {
        TraceEvent.begin("ToolbarPhone.drawLocationBar");
        boolean clipped = false;
        if (shouldDrawLocationBar()) {
            canvas.save();

            if (shouldDrawLocationBarBackground()) {
                if (mActiveLocationBarBackground instanceof NtpSearchBoxDrawable) {
                    ((NtpSearchBoxDrawable) mActiveLocationBarBackground)
                            .markPendingBoundsUpdateFromToolbar();
                }
                mActiveLocationBarBackground.setBounds(
                        mLocationBarBackgroundBounds.left + mLocationBarBackgroundNtpOffset.left,
                        mLocationBarBackgroundBounds.top + mLocationBarBackgroundNtpOffset.top,
                        mLocationBarBackgroundBounds.right + mLocationBarBackgroundNtpOffset.right,
                        mLocationBarBackgroundBounds.bottom
                                + mLocationBarBackgroundNtpOffset.bottom);
                mActiveLocationBarBackground.draw(canvas);
            }

            float locationBarClipLeft =
                    mLocationBarBackgroundBounds.left + mLocationBarBackgroundNtpOffset.left;
            float locationBarClipRight =
                    mLocationBarBackgroundBounds.right + mLocationBarBackgroundNtpOffset.right;
            float locationBarClipTop =
                    mLocationBarBackgroundBounds.top + mLocationBarBackgroundNtpOffset.top;
            float locationBarClipBottom =
                    mLocationBarBackgroundBounds.bottom + mLocationBarBackgroundNtpOffset.bottom;

            final int locationBarPaddingStart =
                    mLocationBar.getPhoneCoordinator().getPaddingStart();
            final int locationBarPaddingEnd = mLocationBar.getPhoneCoordinator().getPaddingEnd();
            final int locationBarDirection =
                    mLocationBar.getPhoneCoordinator().getLayoutDirection();
            // When unexpanded, the location bar's visible content boundaries are inset from the
            // viewport used to draw the background.  During expansion transitions, compensation
            // is applied to increase the clip regions such that when the location bar converts
            // to the narrower collapsed layout the visible content is the same.
            if (mUrlExpansionFraction != 1f && !mOptionalButtonAnimationRunning) {
                int leftDelta = mUnfocusedLocationBarLayoutLeft
                        - getViewBoundsLeftOfLocationBar(mVisualState);
                int rightDelta = getViewBoundsRightOfLocationBar(mVisualState)
                        - mUnfocusedLocationBarLayoutLeft - mUnfocusedLocationBarLayoutWidth;
                float remainingFraction = 1f - mUrlExpansionFraction;

                locationBarClipLeft += leftDelta * remainingFraction;
                locationBarClipRight -= rightDelta * remainingFraction;

                // When the defocus animation is running, the location bar padding needs to be
                // subtracted from the clip bounds so that the location bar text width in the last
                // frame of the animation matches the text width of the unfocused location bar.
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipLeft += locationBarPaddingStart * remainingFraction;
                } else {
                    locationBarClipRight -= locationBarPaddingEnd * remainingFraction;
                }
            }
            if (mOptionalButtonAnimationRunning) {
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipLeft += locationBarPaddingStart;
                } else {
                    locationBarClipRight -= locationBarPaddingEnd;
                }
            }

            // Offset the clip rect by a set amount to ensure the Google G is completely inside the
            // omnibox background when animating in.
            if (SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(isIncognito())
                    && isLocationBarShownInNTP() && urlHasFocus() && mUrlFocusChangeInProgress) {
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipRight -= locationBarPaddingStart;
                } else {
                    locationBarClipLeft += locationBarPaddingStart;
                }
            }

            // Clip the location bar child to the URL viewport calculated in onDraw.
            canvas.clipRect(locationBarClipLeft, locationBarClipTop, locationBarClipRight,
                    locationBarClipBottom);
            clipped = true;
        }

        // TODO(1133482): Hide this View interaction if possible.
        boolean retVal = super.drawChild(
                canvas, mLocationBar.getPhoneCoordinator().getViewForDrawing(), drawingTime);

        if (clipped) canvas.restore();
        TraceEvent.end("ToolbarPhone.drawLocationBar");
        return retVal;
    }

    /**
     * @return Whether the location bar background should be drawn in
     *         {@link #drawLocationBar(Canvas, long)}.
     */
    private boolean shouldDrawLocationBarBackground() {
        return (mLocationBar.getPhoneCoordinator().getAlpha() > 0
                       || mForceDrawLocationBarBackground)
                && !mTextureCaptureMode;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mBackgroundOverlayBounds.set(0, 0, w, h);
        super.onSizeChanged(w, h, oldw, oldh);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mToolbarShadow = (ImageView) getRootView().findViewById(R.id.toolbar_shadow);
        updateShadowVisibility();

        // This is a workaround for http://crbug.com/574928. Since Jelly Bean is the lowest version
        // we support now and the next deprecation target, we decided to simply workaround.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN) {
            mToolbarShadow.setImageDrawable(ApiCompatibilityUtils.getDrawable(
                    getResources(), R.drawable.modern_toolbar_shadow));
        }
    }

    @Override
    public void draw(Canvas canvas) {
        // If capturing a texture of the toolbar, ensure the alpha is set prior to draw(...) being
        // called.  The alpha is being used prior to getting to draw(...), so updating the value
        // after this point was having no affect.
        if (mTextureCaptureMode) assert getAlpha() == 1f;

        // mClipRect can change in the draw call, so cache this value to ensure the canvas is
        // restored correctly.
        boolean shouldClip = !mTextureCaptureMode && mClipRect != null;
        if (shouldClip) {
            canvas.save();
            canvas.clipRect(mClipRect);
        }
        super.draw(canvas);
        if (shouldClip) {
            canvas.restore();

            // Post an invalidate when the clip rect becomes null to ensure another draw pass occurs
            // and the full toolbar is drawn again.
            if (mClipRect == null) postInvalidate();
        }
    }

    @Override
    public void onStateRestored() {
        if (mToggleTabStackButton != null) mToggleTabStackButton.setClickable(true);
    }

    @Override
    public boolean isReadyForTextureCapture() {
        if (mForceTextureCapture) {
            return true;
        }
        return !(urlHasFocus() || mUrlFocusChangeInProgress);
    }

    @Override
    public boolean setForceTextureCapture(boolean forceTextureCapture) {
        if (forceTextureCapture) {
            // Only force a texture capture if the tint for the toolbar drawables is changing or
            // if the tab count has changed since the last texture capture.
            mForceTextureCapture = mLightDrawablesUsedForLastTextureCapture != useLight();

            if (mTabSwitcherAnimationTabStackDrawable != null && mToggleTabStackButton != null) {
                mForceTextureCapture = mForceTextureCapture
                        || mTabCountForLastTextureCapture
                                != mTabSwitcherAnimationTabStackDrawable.getTabCount();
            }

            return mForceTextureCapture;
        }

        mForceTextureCapture = forceTextureCapture;
        return false;
    }

    @Override
    public void setLayoutUpdater(Runnable layoutUpdater) {
        mLayoutUpdater = layoutUpdater;
    }

    @Override
    public void finishAnimations() {
        mClipRect = null;
        if (mTabSwitcherModeAnimation != null) {
            mTabSwitcherModeAnimation.end();
            mTabSwitcherModeAnimation = null;
        }
        if (mDelayedTabSwitcherModeAnimation != null) {
            mDelayedTabSwitcherModeAnimation.end();
            mDelayedTabSwitcherModeAnimation = null;
        }

        // The Android framework calls onAnimationEnd() on listeners before Animator#isRunning()
        // returns false. Sometimes this causes the progress bar visibility to be set incorrectly.
        // Update the visibility now that animations are set to null. (see crbug.com/606419)
        updateProgressBarVisibility();
    }

    @Override
    public void getLocationBarContentRect(Rect outRect) {
        updateLocationBarBackgroundBounds(outRect, VisualState.NORMAL);
    }

    @Override
    public void onHomeButtonUpdate(boolean homeButtonEnabled) {
        mIsHomeButtonEnabled = homeButtonEnabled;
        updateButtonVisibility();
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        updateButtonVisibility();
    }

    @Override
    public void updateButtonVisibility() {
        if (mHomeButton != null) {
            boolean hideHomeButton = !mIsHomeButtonEnabled
                    || ReturnToChromeExperimentsUtil.shouldHideHomeButtonForStartSurface(
                            isIncognito(), false /* isTablet */);
            if (hideHomeButton) {
                removeHomeButton();
            } else {
                addHomeButton();
            }
        }
        if (mOptionalButton != null) {
            if (isMenuButtonPresent()) {
                int padding = getResources().getDimensionPixelSize(
                        R.dimen.toolbar_phone_optional_button_padding);
                mOptionalButton.setPaddingRelative(padding, 0, 0, 0);
            } else {
                mOptionalButton.setPadding(0, 0, 0, 0);
            }
        }
    }

    @Override
    public void onTintChanged(ColorStateList tint, boolean useLight) {
        if (mHomeButton != null) {
            ApiCompatibilityUtils.setImageTintList(mHomeButton, tint);
        }

        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setUseLightDrawables(useLight);
            if (mTabSwitcherAnimationTabStackDrawable != null) {
                mTabSwitcherAnimationTabStackDrawable.setTint(tint);
            }
        }

        if (mOptionalButton != null && mOptionalButtonUsesTint) {
            ApiCompatibilityUtils.setImageTintList(mOptionalButton, tint);
        }

        // TODO(amaralp): Have the LocationBar listen to tint changes.
        if (mLocationBar != null) mLocationBar.updateVisualsForState();

        if (mLayoutUpdater != null) mLayoutUpdater.run();
    }

    @Override
    public HomeButton getHomeButton() {
        return mHomeButton;
    }

    private void removeHomeButton() {
        mHomeButton.setVisibility(GONE);
    }

    private void addHomeButton() {
        mHomeButton.setVisibility(
                urlHasFocus() || isTabSwitcherAnimationRunning() ? INVISIBLE : VISIBLE);
    }

    private ObjectAnimator createEnterTabSwitcherModeAnimation() {
        ObjectAnimator enterAnimation =
                ObjectAnimator.ofFloat(this, mTabSwitcherModeFractionProperty, 1.f);
        enterAnimation.setDuration(
                TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS);
        enterAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        return enterAnimation;
    }

    private ObjectAnimator createExitTabSwitcherAnimation(final boolean animateNormalToolbar) {
        ObjectAnimator exitAnimation =
                ObjectAnimator.ofFloat(this, mTabSwitcherModeFractionProperty, 0.f);
        exitAnimation.setDuration(animateNormalToolbar
                        ? TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS
                        : TAB_SWITCHER_MODE_EXIT_FADE_ANIMATION_DURATION_MS);
        exitAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        exitAnimation.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animation) {
                onExitTabSwitcherAnimationEnd();
            }
        });

        return exitAnimation;
    }

    private ObjectAnimator createPostExitTabSwitcherAnimation() {
        ObjectAnimator exitAnimation =
                ObjectAnimator.ofFloat(this, View.TRANSLATION_Y, -getHeight(), 0.f);
        exitAnimation.setDuration(TAB_SWITCHER_MODE_POST_EXIT_ANIMATION_DURATION_MS);
        exitAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        exitAnimation.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                updateViewsForTabSwitcherMode();
                // On older builds, force an update to ensure the new visuals are used
                // when bringing in the toolbar.  crbug.com/404571
                if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN) {
                    requestLayout();
                }
            }

            @Override
            public void onEnd(Animator animation) {
                mDelayedTabSwitcherModeAnimation = null;
                onExitTabSwitcherAnimationEnd();
            }
        });

        return exitAnimation;
    }

    private void onExitTabSwitcherAnimationEnd() {
        updateViewsForTabSwitcherMode();

        // Request a texture update to ensure a texture is captured before the user
        // re-enters the tab switcher.
        postInvalidate();
        mLayoutUpdater.run();
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        assert mTextureCaptureMode != textureMode;
        mTextureCaptureMode = textureMode;
        if (mTextureCaptureMode) {
            if (!hideShadowForIncognitoNtp() && !hideShadowForInterstitial()
                    && !hideShadowForRegularNtpTextureCapture()) {
                mToolbarShadow.setVisibility(VISIBLE);
            }
            mPreTextureCaptureAlpha = getAlpha();
            mPreTextureCaptureVisibility = getVisibility();
            setAlpha(1);
            setVisibility(View.VISIBLE);
        } else {
            setAlpha(mPreTextureCaptureAlpha);
            setVisibility(mPreTextureCaptureVisibility);
            updateShadowVisibility();
            mPreTextureCaptureAlpha = 1f;
        }
    }

    private boolean hideShadowForRegularNtpTextureCapture() {
        return !isIncognito() && UrlUtilities.isNTPUrl(getToolbarDataProvider().getCurrentUrl())
                && mNtpSearchBoxScrollFraction < 1.f;
    }

    // TODO(dtrainor): This is always true when in the tab switcher (crbug.com/710750).
    private boolean isTabSwitcherAnimationRunning() {
        return mTabSwitcherState == ENTERING_TAB_SWITCHER
                || mTabSwitcherState == EXITING_TAB_SWITCHER;
    }

    private void updateViewsForTabSwitcherMode() {
        setVisibility(mTabSwitcherState == STATIC_TAB ? View.VISIBLE : View.INVISIBLE);

        updateProgressBarVisibility();
        updateShadowVisibility();
        updateTabSwitcherButtonRipple();
    }

    private void updateProgressBarVisibility() {
        getProgressBar().setVisibility(mTabSwitcherState != STATIC_TAB ? INVISIBLE : VISIBLE);
    }

    @Override
    public void setContentAttached(boolean attached) {
        updateVisualsForLocationBarState();
    }

    @Override
    public void setTabSwitcherMode(boolean inTabSwitcherMode, boolean showToolbar,
            boolean delayAnimation, MenuButtonCoordinator menuButtonCoordinator) {
        setTabSwitcherMode(inTabSwitcherMode, showToolbar, delayAnimation, true);
    }

    /**
     * See {@link #setTabSwitcherMode(boolean, boolean, boolean, MenuButtonCoordinator)}.
     */
    public void setTabSwitcherMode(boolean inTabSwitcherMode, boolean showToolbar,
            boolean delayAnimation, boolean animate) {
        // This method is only used for grid tab switcher with the start surface disabled. When
        // start surface is enabled, omnibox state is updated in onStartSurfaceStateChanged(), which
        // is always called before setTabSwitcherMode(), so skip here.
        if (getToolbarDataProvider().shouldShowLocationBarInOverviewMode()) return;

        // If setting tab switcher mode to true and the browser is already animating or in the tab
        // switcher skip.
        if (inTabSwitcherMode
                && (mTabSwitcherState == TAB_SWITCHER
                        || mTabSwitcherState == ENTERING_TAB_SWITCHER)) {
            return;
        }

        // Likewise if exiting the tab switcher.
        if (!inTabSwitcherMode
                && (mTabSwitcherState == STATIC_TAB || mTabSwitcherState == EXITING_TAB_SWITCHER)) {
            return;
        }
        mTabSwitcherState = inTabSwitcherMode ? ENTERING_TAB_SWITCHER : EXITING_TAB_SWITCHER;

        // The width of location bar depends on mTabSwitcherState so layout request is needed. See
        // crbug.com/974745.
        requestLayout();

        mLocationBar.setUrlBarFocusable(false);

        finishAnimations();

        mDelayingTabSwitcherAnimation = delayAnimation;

        if (inTabSwitcherMode) {
            if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
                mUrlFocusLayoutAnimator.end();
                mUrlFocusLayoutAnimator = null;
                // After finishing the animation, force a re-layout of the location bar,
                // so that the final translation position is correct (since onMeasure updates
                // won't happen in tab switcher mode). crbug.com/518795.
                layoutLocationBar(getMeasuredWidth());
                updateUrlExpansionAnimation();
            }

            updateViewsForTabSwitcherMode();
            mTabSwitcherModeAnimation = createEnterTabSwitcherModeAnimation();
        } else {
            if (!mDelayingTabSwitcherAnimation) {
                mTabSwitcherModeAnimation = createExitTabSwitcherAnimation(showToolbar);
            }
        }

        updateButtonsTranslationY();
        mAnimateNormalToolbar = showToolbar;
        if (mTabSwitcherModeAnimation != null) mTabSwitcherModeAnimation.start();

        if (DeviceClassManager.enableAccessibilityLayout() || !animate) finishAnimations();

        postInvalidateOnAnimation();
    }

    /**
     * Enables or disables the tab switcher ripple depending on whether we are in or out of the tab
     * switcher mode.
     */
    private void updateTabSwitcherButtonRipple() {
        if (mToggleTabStackButton == null) return;
        if (mTabSwitcherState == ENTERING_TAB_SWITCHER) {
            mToggleTabStackButton.setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), android.R.color.transparent));
        } else {
            // TODO(https://crbug.com/912358): This appears to no longer work.
            TypedValue outValue = new TypedValue();

            // The linked style here will have to be changed if it is updated in the XML.
            getContext().getTheme().resolveAttribute(R.style.ToolbarButton, outValue, true);
            mToggleTabStackButton.setBackgroundResource(outValue.resourceId);
        }
    }

    @Override
    void onStartSurfaceStateChanged(boolean isShowingStartSurface) {
        super.onStartSurfaceStateChanged(isShowingStartSurface);
        mIsShowingStartSurface = isShowingStartSurface;
        updateUrlExpansionState();
    }

    /**
     * Update url expansion state when start surface state is changed. If start surface homepage is
     * showing, |mIsShowingStartSurface| is set to true, and toolbar is always expanded. Otherwise
     * expansion state is consistent with urlHasFocus().
     */
    private void updateUrlExpansionState() {
        if (mToggleTabStackButton != null) {
            boolean isGone = mIsShowingStartSurface;
            mToggleTabStackButton.setVisibility(isGone ? GONE : VISIBLE);
        }

        getMenuButtonCoordinator().setVisibility(!mIsShowingStartSurface);

        // The URL focusing animator set shouldn't be populated before native initialization. It is
        // possible that this function is called before native initialization when Instant Start
        // is enabled.
        if (isNativeLibraryReady()) {
            // When the url has got focused, we don't clear the focus.
            triggerUrlFocusAnimation(urlHasFocus());
        } else {
            mPendingTriggerUrlFocusRequest = true;
        }
    }

    @Override
    public void onTabSwitcherTransitionFinished() {
        setAlpha(1.f);
        mClipRect = null;

        // Detect what was being transitioned from and set the new state appropriately.
        if (mTabSwitcherState == EXITING_TAB_SWITCHER) {
            mLocationBar.setUrlBarFocusable(true);
            mTabSwitcherState = STATIC_TAB;
            updateVisualsForLocationBarState();
        }
        if (mTabSwitcherState == ENTERING_TAB_SWITCHER) mTabSwitcherState = TAB_SWITCHER;

        // The width of location bar depends on mTabSwitcherState so layout request is needed. See
        // crbug.com/974745.
        requestLayout();

        mTabSwitcherModeFraction = mTabSwitcherState != STATIC_TAB ? 1.0f : 0.0f;

        if (!mAnimateNormalToolbar) {
            finishAnimations();
            updateVisualsForLocationBarState();
        }

        if (mDelayingTabSwitcherAnimation) {
            mDelayingTabSwitcherAnimation = false;
            mDelayedTabSwitcherModeAnimation = createPostExitTabSwitcherAnimation();
            mDelayedTabSwitcherModeAnimation.start();
        } else {
            updateViewsForTabSwitcherMode();
        }
    }

    @Override
    public void setOnTabSwitcherClickHandler(OnClickListener listener) {
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setOnTabSwitcherClickHandler(listener);
        }
    }

    @Override
    void setOnTabSwitcherLongClickHandler(OnLongClickListener listener) {
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setOnTabSwitcherLongClickHandler(listener);
        }
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return super.shouldIgnoreSwipeGesture() || mUrlExpansionFraction > 0f
                || mNtpSearchBoxTranslation.y < 0f;
    }

    private Property<TextView, Integer> buildUrlScrollProperty(
            final View containerView, final boolean isContainerRtl) {
        // If the RTL-ness of the container view changes during an animation, the scroll values
        // become invalid.  If that happens, snap to the ending position and no longer update.
        return new Property<TextView, Integer>(Integer.class, "scrollX") {
            private boolean mRtlStateInvalid;

            @Override
            public Integer get(TextView view) {
                return view.getScrollX();
            }

            @Override
            public void set(TextView view, Integer scrollX) {
                if (mRtlStateInvalid) return;
                boolean rtl = containerView.getLayoutDirection() == LAYOUT_DIRECTION_RTL;
                if (rtl != isContainerRtl) {
                    mRtlStateInvalid = true;
                    if (!rtl || mUrlBar.getLayout() != null) {
                        scrollX = 0;
                        if (rtl) {
                            scrollX = (int) view.getLayout().getPrimaryHorizontal(0);
                            scrollX -= view.getWidth();
                        }
                    }
                }
                view.setScrollX(scrollX);
            }
        };
    }

    private void populateUrlExpansionAnimatorSet(List<Animator> animators) {
        TraceEvent.begin("ToolbarPhone.populateUrlFocusingAnimatorSet");
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangeFractionProperty, 1f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);

        mLocationBar.getPhoneCoordinator().populateFadeAnimations(
                animators, 0, URL_FOCUS_CHANGE_ANIMATION_DURATION_MS, 0);

        float density = getContext().getResources().getDisplayMetrics().density;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        float toolbarButtonTranslationX =
                MathUtils.flipSignIf(mUrlFocusTranslationX, isRtl) * density;

        animator = getMenuButtonCoordinator().getUrlFocusingAnimator(true);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        animators.add(animator);

        if (mToggleTabStackButton != null) {
            animator = ObjectAnimator.ofFloat(
                    mToggleTabStackButton, TRANSLATION_X, toolbarButtonTranslationX);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mToggleTabStackButton, ALPHA, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);
        }

        if (mOptionalButton != null && mOptionalButton.getVisibility() != View.GONE) {
            animator = ObjectAnimator.ofFloat(
                    mOptionalButton, TRANSLATION_X, toolbarButtonTranslationX);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mOptionalButton, ALPHA, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);
        }

        if (mToolbarShadow != null) {
            animator = ObjectAnimator.ofFloat(mToolbarShadow, ALPHA, urlHasFocus() ? 0 : 1);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }
        TraceEvent.end("ToolbarPhone.populateUrlFocusingAnimatorSet");
    }

    private void populateUrlClearExpansionAnimatorSet(List<Animator> animators) {
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangeFractionProperty, 0f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);

        animator = getMenuButtonCoordinator().getUrlFocusingAnimator(false);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        animators.add(animator);

        if (mToggleTabStackButton != null) {
            animator = ObjectAnimator.ofFloat(mToggleTabStackButton, TRANSLATION_X, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_TABSTACK_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mToggleTabStackButton, ALPHA, 1);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_TABSTACK_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

        if (mOptionalButton != null && mOptionalButton.getVisibility() != View.GONE) {
            // TODO(twellington): it's possible that the optional button was shown while
            // the url bar was focused, in which case the translation x and alpha animators
            // are a no-op. Account for this case.
            animator = ObjectAnimator.ofFloat(mOptionalButton, TRANSLATION_X, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_EXPERIMENTAL_BUTTON_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mOptionalButton, ALPHA, 1);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_EXPERIMENTAL_BUTTON_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

        mLocationBar.getPhoneCoordinator().populateFadeAnimations(
                animators, URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS, URL_CLEAR_FOCUS_MENU_DELAY_MS, 1);

        if (isLocationBarShownInNTP() && mNtpSearchBoxScrollFraction == 0f) return;

        if (mToolbarShadow != null) {
            animator = ObjectAnimator.ofFloat(mToolbarShadow, ALPHA, 1);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }
    }

    @Override
    public void onUrlFocusChange(final boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);

        if (mToggleTabStackButton != null) mToggleTabStackButton.setClickable(!hasFocus);
        triggerUrlFocusAnimation(hasFocus);
    }

    /**
     * @param showExpandedState Whether the url bar should be expanded.
     */
    private void triggerUrlFocusAnimation(boolean showExpandedState) {
        boolean shouldShowKeyboard = urlHasFocus();

        // On start surface omnibox should be always expanded without being focused, while whether
        // the keyboard should show up or not depends on whether url has focus.
        if (mIsShowingStartSurface) {
            showExpandedState = true;
        }

        TraceEvent.begin("ToolbarPhone.triggerUrlFocusAnimation");
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.cancel();
            mUrlFocusLayoutAnimator = null;
        }
        if (mOptionalButtonAnimationRunning) mOptionalButtonAnimator.end();

        List<Animator> animators = new ArrayList<>();
        if (showExpandedState) {
            populateUrlExpansionAnimatorSet(animators);
        } else {
            populateUrlClearExpansionAnimatorSet(animators);
        }
        mUrlFocusLayoutAnimator = new AnimatorSet();
        mUrlFocusLayoutAnimator.playTogether(animators);

        mUrlFocusChangeInProgress = true;
        // |showExpandedState| needs to be final when accessed within inner class.
        final boolean showExpandedStateFinal = showExpandedState;
        mUrlFocusLayoutAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                if (!showExpandedStateFinal) {
                    mDisableLocationBarRelayout = true;
                } else {
                    mLayoutLocationBarInFocusedMode = true;
                    requestLayout();
                }
            }

            @Override
            public void onCancel(Animator animation) {
                if (!showExpandedStateFinal) mDisableLocationBarRelayout = false;

                mUrlFocusChangeInProgress = false;
            }

            @Override
            public void onEnd(Animator animation) {
                if (!showExpandedStateFinal) {
                    mDisableLocationBarRelayout = false;
                    mLayoutLocationBarInFocusedMode = false;
                    requestLayout();
                }
                mLocationBar.finishUrlFocusChange(showExpandedStateFinal, shouldShowKeyboard,
                        getToolbarDataProvider().shouldShowLocationBarInOverviewMode());
                mUrlFocusChangeInProgress = false;
            }
        });
        mUrlFocusLayoutAnimator.start();
        TraceEvent.end("ToolbarPhone.triggerUrlFocusAnimation");
    }

    @Override
    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        mTabCountProvider.addObserver(this);
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setTabCountProvider(tabCountProvider);
        }
    }

    @Override
    public void onTabCountChanged(int numberOfTabs, boolean isIncognito) {
        if (mHomeButton != null) mHomeButton.setEnabled(true);

        if (mToggleTabStackButton == null) return;

        boolean useTabStackDrawableLight =
                isIncognito || ColorUtils.shouldUseLightForegroundOnBackground(getTabThemeColor());
        if (mTabSwitcherAnimationTabStackDrawable == null
                || mIsOverlayTabStackDrawableLight != useTabStackDrawableLight) {
            mTabSwitcherAnimationTabStackDrawable = TabSwitcherDrawable.createTabSwitcherDrawable(
                    getContext(), useTabStackDrawableLight);
            int[] stateSet = {android.R.attr.state_enabled};
            mTabSwitcherAnimationTabStackDrawable.setState(stateSet);
            mIsOverlayTabStackDrawableLight = useTabStackDrawableLight;
        }

        if (mTabSwitcherAnimationTabStackDrawable != null) {
            mTabSwitcherAnimationTabStackDrawable.updateForTabCount(numberOfTabs, isIncognito);
        }
    }

    /**
     * Get the theme color for the currently active tab. This is not affected by the tab switcher's
     * theme color.
     * @return The current tab's theme color.
     */
    private int getTabThemeColor() {
        if (getToolbarDataProvider() != null) return getToolbarDataProvider().getPrimaryColor();
        return getToolbarColorForVisualState(
                isIncognito() ? VisualState.INCOGNITO : VisualState.NORMAL);
    }

    @Override
    public void onTabContentViewChanged() {
        super.onTabContentViewChanged();
        updateNtpAnimationState();
        updateVisualsForLocationBarState();
    }

    @Override
    public void onTabOrModelChanged() {
        super.onTabOrModelChanged();
        updateNtpAnimationState();
        updateVisualsForLocationBarState();
    }

    private static boolean isVisualStateValidForBrandColorTransition(@VisualState int state) {
        return state == VisualState.NORMAL || state == VisualState.BRAND_COLOR;
    }

    @Override
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        super.onPrimaryColorChanged(shouldAnimate);
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.end();

        final int initialColor = mToolbarBackground.getColor();
        final int finalColor = getToolbarDataProvider().getPrimaryColor();
        if (initialColor == finalColor) return;

        final int initialLocationBarColor = getLocationBarColorForToolbarColor(initialColor);
        final int finalLocationBarColor = getLocationBarColorForToolbarColor(finalColor);

        if (!isVisualStateValidForBrandColorTransition(mVisualState)) return;

        if (!shouldAnimate) {
            updateToolbarBackground(finalColor);
            return;
        }

        boolean shouldUseOpaque = ColorUtils.shouldUseOpaqueTextboxBackground(finalColor);
        final int initialAlpha = mLocationBarBackgroundAlpha;
        final int finalAlpha = shouldUseOpaque ? 255 : LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        final boolean shouldAnimateAlpha = initialAlpha != finalAlpha;
        mBrandColorTransitionAnimation =
                ValueAnimator.ofFloat(0, 1).setDuration(THEME_COLOR_TRANSITION_DURATION);
        mBrandColorTransitionAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        mBrandColorTransitionAnimation.addUpdateListener(new AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float fraction = animation.getAnimatedFraction();
                if (shouldAnimateAlpha) {
                    mLocationBarBackgroundAlpha =
                            (int) (MathUtils.interpolate(initialAlpha, finalAlpha, fraction));
                }
                updateToolbarBackground(
                        ColorUtils.getColorWithOverlay(initialColor, finalColor, fraction));
                updateModernLocationBarColor(ColorUtils.getColorWithOverlay(
                        initialLocationBarColor, finalLocationBarColor, fraction));
            }
        });
        mBrandColorTransitionAnimation.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animation) {
                mBrandColorTransitionActive = false;
                updateVisualsForLocationBarState();
            }
        });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
        if (mLayoutUpdater != null) mLayoutUpdater.run();
    }

    private void updateNtpAnimationState() {
        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();

        // Store previous NTP scroll before calling reset as that clears this value.
        boolean wasShowingNtp = ntpDelegate.wasShowingNtp();
        float previousNtpScrollFraction = mNtpSearchBoxScrollFraction;

        resetNtpAnimationValues();
        ntpDelegate.setSearchBoxScrollListener(this::onNtpScrollChanged);
        if (ntpDelegate.isLocationBarShown()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                NtpSearchBoxDrawable ntpSearchBox = new NtpSearchBoxDrawable(getContext(), this);
                ntpDelegate.setSearchBoxBackground(ntpSearchBox);
                mActiveLocationBarBackground = ntpSearchBox;
            }

            requestLayout();
        } else if (wasShowingNtp) {
            // Convert the previous NTP scroll progress to URL focus progress because that
            // will give a nicer transition animation from the expanded NTP omnibox to the
            // collapsed normal omnibox on other non-NTP pages.
            if (mTabSwitcherState == STATIC_TAB && previousNtpScrollFraction > 0f) {
                mUrlFocusChangeFraction =
                        Math.max(previousNtpScrollFraction, mUrlFocusChangeFraction);
                triggerUrlFocusAnimation(false);
            }
            requestLayout();
        }
    }

    @Override
    public void onDefaultSearchEngineChanged() {
        super.onDefaultSearchEngineChanged();
        // Post an update for the toolbar state, which will allow all other listeners
        // for the search engine change to update before we check on the state of the
        // world for a UI update.
        // TODO(tedchoc): Move away from updating based on the search engine change and instead
        //                add the toolbar as a listener to the NewTabPage and udpate only when
        //                it notifies the listeners that it has changed its state.
        post(new Runnable() {
            @Override
            public void run() {
                updateVisualsForLocationBarState();
                updateNtpAnimationState();
            }
        });
    }

    @Override
    public void handleFindLocationBarStateChange(boolean showing) {
        setVisibility(showing ? View.GONE
                              : mTabSwitcherState == STATIC_TAB ? View.VISIBLE : View.INVISIBLE);
    }

    private boolean isLocationBarShownInNTP() {
        return getToolbarDataProvider().getNewTabPageDelegate().isLocationBarShown();
    }

    private boolean isLocationBarCurrentlyShown() {
        return !isLocationBarShownInNTP() || mUrlExpansionFraction > 0;
    }

    @Override
    void setForceHideShadow(boolean forceHideShadow) {
        mForceHideShadow = forceHideShadow;
        updateShadowVisibility();
    }

    /**
     * Update the visibility of the toolbar shadow.
     */
    private void updateShadowVisibility() {
        boolean shouldDrawShadow = shouldDrawShadow();
        int shadowVisibility = shouldDrawShadow ? View.VISIBLE : View.INVISIBLE;

        if (mToolbarShadow != null && mToolbarShadow.getVisibility() != shadowVisibility) {
            mToolbarShadow.setVisibility(shadowVisibility);
        }
    }

    /**
     * @return Whether the toolbar shadow should be drawn.
     */
    private boolean shouldDrawShadow() {
        // TODO(twellington): Move this shadow state information to ToolbarDataProvider and show
        // shadow when incognito NTP is scrolled.
        return mTabSwitcherState == STATIC_TAB && !mForceHideShadow && !hideShadowForIncognitoNtp()
                && !hideShadowForInterstitial() && getVisibility() == View.VISIBLE;
    }

    private boolean hideShadowForIncognitoNtp() {
        return isIncognito() && UrlUtilities.isNTPUrl(getToolbarDataProvider().getCurrentUrl());
    }

    private boolean hideShadowForInterstitial() {
        return getToolbarDataProvider() != null && getToolbarDataProvider().getTab() != null
                && (getToolbarDataProvider().getTab().isShowingErrorPage());
    }

    private @VisualState int computeVisualState() {
        if (isLocationBarShownInNTP()) return VisualState.NEW_TAB_NORMAL;
        if (isIncognito()) return VisualState.INCOGNITO;
        if (getToolbarDataProvider().isUsingBrandColor()) return VisualState.BRAND_COLOR;
        return VisualState.NORMAL;
    }

    /**
     * @return The color that progress bar should use.
     */
    private int getProgressBarColor() {
        return getToolbarDataProvider().getPrimaryColor();
    }

    private void updateVisualsForLocationBarState() {
        TraceEvent.begin("ToolbarPhone.updateVisualsForLocationBarState");
        // These are used to skip setting state unnecessarily while in the tab switcher.
        boolean inOrEnteringStaticTab =
                mTabSwitcherState == STATIC_TAB || mTabSwitcherState == EXITING_TAB_SWITCHER;

        @VisualState
        int newVisualState = computeVisualState();

        // If we are navigating to or from a brand color, allow the transition animation
        // to run to completion as it will handle the triggering this path again and committing
        // the proper visual state when it finishes.  Brand color transitions are only valid
        // between normal non-incognito pages and brand color pages, so if the visual states
        // do not match then cancel the animation below.
        if (mBrandColorTransitionActive && isVisualStateValidForBrandColorTransition(mVisualState)
                && isVisualStateValidForBrandColorTransition(newVisualState)) {
            TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
            return;
        } else if (mBrandColorTransitionAnimation != null
                && mBrandColorTransitionAnimation.isRunning()) {
            mBrandColorTransitionAnimation.end();
        }

        boolean visualStateChanged = mVisualState != newVisualState;

        int currentPrimaryColor = getToolbarDataProvider().getPrimaryColor();
        int themeColorForProgressBar = getProgressBarColor();

        // If The page is native force the use of the standard theme for the progress bar.
        if (getToolbarDataProvider() != null && getToolbarDataProvider().getTab() != null
                && getToolbarDataProvider().getTab().isNativePage()) {
            @VisualState
            int visualState = isIncognito() ? VisualState.INCOGNITO : VisualState.NORMAL;
            themeColorForProgressBar = getToolbarColorForVisualState(visualState);
        }

        if (mVisualState == VisualState.BRAND_COLOR && !visualStateChanged) {
            boolean unfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            if (unfocusedLocationBarUsesTransparentBg != mUnfocusedLocationBarUsesTransparentBg) {
                visualStateChanged = true;
            } else {
                updateToolbarBackgroundFromState(VisualState.BRAND_COLOR);
                getProgressBar().setThemeColor(themeColorForProgressBar, isIncognito());
            }
        }

        mVisualState = newVisualState;

        // Refresh the toolbar texture.
        if ((mVisualState == VisualState.BRAND_COLOR || visualStateChanged)
                && mLayoutUpdater != null) {
            mLayoutUpdater.run();
        }
        updateShadowVisibility();
        updateUrlExpansionAnimation();

        // This exception is to prevent early change of theme color when exiting the tab switcher
        // since currently visual state does not map correctly to tab switcher state. See
        // https://crbug.com/832594 for more info.
        if (mTabSwitcherState != EXITING_TAB_SWITCHER) {
            updateToolbarBackgroundFromState(mVisualState);
        }

        if (!visualStateChanged) {
            if (mVisualState == VisualState.NEW_TAB_NORMAL) {
                updateNtpTransitionAnimation();
            } else {
                resetNtpAnimationValues();
            }
            TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
            return;
        }

        mUnfocusedLocationBarUsesTransparentBg = false;
        mLocationBarBackgroundAlpha = 255;
        getProgressBar().setThemeColor(themeColorForProgressBar, isIncognito());

        if (isIncognito()) {
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        } else if (mVisualState == VisualState.BRAND_COLOR) {
            mUnfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            mLocationBarBackgroundAlpha = mUnfocusedLocationBarUsesTransparentBg
                    ? LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA
                    : 255;
        }

        updateModernLocationBarColor(getLocationBarColorForToolbarColor(currentPrimaryColor));

        mLocationBar.updateVisualsForState();

        // We update the alpha before comparing the visual state as we need to change
        // its value when entering and exiting TabSwitcher mode.
        if (isLocationBarShownInNTP() && inOrEnteringStaticTab) {
            updateNtpTransitionAnimation();
        }

        getMenuButtonCoordinator().setVisibility(true);

        DrawableCompat.setTint(mLocationBarBackground,
                isIncognito() ? Color.WHITE
                              : ApiCompatibilityUtils.getColor(
                                      getResources(), R.color.toolbar_text_box_background));
        TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    @Override
    void updateOptionalButton(ButtonData buttonData) {
        if (mOptionalButton == null) {
            ViewStub viewStub = findViewById(R.id.optional_button_stub);
            mOptionalButton = (ImageButton) viewStub.inflate();

            mOptionalButtonTranslation = getResources().getDimensionPixelSize(
                    R.dimen.toolbar_optional_button_animation_translation);
            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) mOptionalButtonTranslation *= -1;
        } else if (mOptionalButtonAnimationRunning) {
            // TODO(https://crbug.com/865801): refine this logic to allow for same-button updates,
            // e.g. swapping in a new drawable part way through the animation.
            mOptionalButtonAnimator.end();
        }

        ButtonSpec buttonSpec = buttonData.getButtonSpec();
        mOptionalButton.setOnClickListener(buttonSpec.getOnClickListener());
        mOptionalButton.setImageDrawable(buttonSpec.getDrawable());
        mOptionalButton.setContentDescription(
                getContext().getResources().getString(buttonSpec.getContentDescriptionResId()));
        mOptionalButton.setEnabled(buttonData.isEnabled());

        mOptionalButtonUsesTint = buttonSpec.getSupportsTinting();
        if (mOptionalButtonUsesTint) {
            ApiCompatibilityUtils.setImageTintList(mOptionalButton, getTint());
        } else {
            ApiCompatibilityUtils.setImageTintList(mOptionalButton, null);
        }
        mOptionalButtonLayoutListener = () -> requestLayoutHostUpdateForOptionalButton();
        if (mTabSwitcherState == STATIC_TAB) {
            if (!mUrlFocusChangeInProgress && !urlHasFocus()
                    && mOptionalButton.getVisibility() == View.GONE) {
                runShowOptionalButtonAnimation();
            } else {
                mOptionalButton.setVisibility(View.VISIBLE);
                onOptionalButtonAnimationEnd();
            }
        } else {
            mOptionalButton.setVisibility(View.VISIBLE);
            getViewTreeObserver().addOnGlobalLayoutListener(mOptionalButtonLayoutListener);
        }
    }

    @Override
    void hideOptionalButton() {
        // mLayoutLocationBarWithoutExtraButton implies that the hide animation is currently
        // running.
        if (mOptionalButton == null || mOptionalButton.getVisibility() == View.GONE
                || mLayoutLocationBarWithoutExtraButton) {
            return;
        }

        boolean transitioningAwayFromLocationBarInNTP =
                getToolbarDataProvider().getNewTabPageDelegate().transitioningAwayFromLocationBar();

        if (mTabSwitcherState == STATIC_TAB && !mUrlFocusChangeInProgress && !urlHasFocus()
                && !transitioningAwayFromLocationBarInNTP) {
            runHideOptionalButtonsAnimators();
        } else {
            mOptionalButton.setVisibility(View.GONE);
            getViewTreeObserver().addOnGlobalLayoutListener(mOptionalButtonLayoutListener);
        }
    }

    @Override
    @VisibleForTesting
    public View getOptionalButtonView() {
        return mOptionalButton;
    }

    /**
     * Whether the menu button is visible. Used as a proxy for whether there are end toolbar
     * buttons besides the optional button.
     */
    private boolean isMenuButtonPresent() {
        return getMenuButtonCoordinator().isVisible();
    }

    private void requestLayoutHostUpdateForOptionalButton() {
        if (mLayoutUpdater != null) mLayoutUpdater.run();
        getViewTreeObserver().removeOnGlobalLayoutListener(mOptionalButtonLayoutListener);
    }

    /**
     * Runs an animation that fades in the optional button while shortening the location bar
     * background.
     */
    private void runShowOptionalButtonAnimation() {
        if (mOptionalButtonAnimationRunning) mOptionalButtonAnimator.end();

        List<Animator> animators = new ArrayList<>();

        mLocBarWidthChangeFraction = 1.f;
        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mLocBarWidthChangeFractionProperty, 0.f);
        widthChangeAnimator.setDuration(LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(widthChangeAnimator);

        mOptionalButton.setAlpha(0.f);
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(mOptionalButton, View.ALPHA, 1.f);
        buttonAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        buttonAnimator.setStartDelay(EXPERIMENTAL_ICON_ANIMATION_DELAY_MS);
        buttonAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonAnimator);

        mOptionalButton.setTranslationX(mOptionalButtonTranslation);
        ObjectAnimator buttonTranslationAnimator =
                ObjectAnimator.ofFloat(mOptionalButton, View.TRANSLATION_X, 0);
        buttonTranslationAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        buttonTranslationAnimator.setStartDelay(EXPERIMENTAL_ICON_ANIMATION_DELAY_MS);
        buttonTranslationAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonTranslationAnimator);

        mOptionalButtonAnimator = new AnimatorSet();
        mOptionalButtonAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                mDisableLocationBarRelayout = true;
                mOptionalButtonAnimationRunning = true;
                mOptionalButton.setVisibility(View.VISIBLE);
            }

            @Override
            public void onEnd(Animator animation) {
                onOptionalButtonAnimationEnd();
                mDisableLocationBarRelayout = false;
                mOptionalButtonAnimationRunning = false;
                getViewTreeObserver().addOnGlobalLayoutListener(mOptionalButtonLayoutListener);
                requestLayout();
            }
        });
        mOptionalButtonAnimator.playTogether(animators);
        mOptionalButtonAnimator.start();
    }

    /**
     * Runs an animation that fades out the optional button while lengthening the location bar
     * background.
     */
    private void runHideOptionalButtonsAnimators() {
        if (mOptionalButtonAnimationRunning) mOptionalButtonAnimator.end();

        List<Animator> animators = new ArrayList<>();
        mLocBarWidthChangeFraction = 0.f;
        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mLocBarWidthChangeFractionProperty, 1.f);
        widthChangeAnimator.setDuration(LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(widthChangeAnimator);

        mOptionalButton.setAlpha(1.f);
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(mOptionalButton, View.ALPHA, 0.f);
        buttonAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        buttonAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonAnimator);

        mOptionalButton.setTranslationX(0);
        ObjectAnimator buttonTranslationAnimator = ObjectAnimator.ofFloat(
                mOptionalButton, View.TRANSLATION_X, mOptionalButtonTranslation);
        buttonTranslationAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        buttonTranslationAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonTranslationAnimator);

        mOptionalButtonAnimator = new AnimatorSet();
        mOptionalButtonAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                mLayoutLocationBarWithoutExtraButton = true;
                mOptionalButtonAnimationRunning = true;
                requestLayout();
            }

            @Override
            public void onEnd(Animator animation) {
                onOptionalButtonAnimationEnd();
                mOptionalButton.setVisibility(View.GONE);
                mLayoutLocationBarWithoutExtraButton = false;
                mOptionalButtonAnimationRunning = false;
                getViewTreeObserver().addOnGlobalLayoutListener(mOptionalButtonLayoutListener);
            }
        });
        mOptionalButtonAnimator.playTogether(animators);
        mOptionalButtonAnimator.start();
    }

    /**
     * Resets the alpha and translation X for all views affected by the animations for showing or
     * hiding buttons.
     */
    private void onOptionalButtonAnimationEnd() {
        mOptionalButtonAnimator = null;
        mOptionalButton.setAlpha(1.f);
        mOptionalButton.setTranslationX(0);
    }

    /**
     * Custom drawable that allows sharing the NTP search box drawable between the toolbar and the
     * NTP.  This allows animations to continue as the drawable is switched between the two owning
     * views.
     */
    private static class NtpSearchBoxDrawable extends DrawableWrapper {
        private final Drawable.Callback mCallback;

        private int mBoundsLeft;
        private int mBoundsTop;
        private int mBoundsRight;
        private int mBoundsBottom;
        private boolean mPendingBoundsUpdateFromToolbar;
        private boolean mDrawnByNtp;

        /**
         * Constructs the NTP search box drawable.
         *
         * @param context The context used to inflate the drawable.
         * @param callback The callback to be notified on changes ot the drawable.
         */
        public NtpSearchBoxDrawable(Context context, Drawable.Callback callback) {
            super(ApiCompatibilityUtils.getDrawable(
                    context.getResources(), R.drawable.ntp_search_box));
            mCallback = callback;
            setCallback(mCallback);
        }

        /**
         * Mark that the pending bounds update is coming from the toolbar.
         */
        void markPendingBoundsUpdateFromToolbar() {
            mPendingBoundsUpdateFromToolbar = true;
        }

        /**
         * Reset the bounds of the drawable to the last bounds received that was not marked from
         * the toolbar.
         */
        void resetBoundsToLastNonToolbar() {
            setBounds(mBoundsLeft, mBoundsTop, mBoundsRight, mBoundsBottom);
        }

        @Override
        public void setBounds(int left, int top, int right, int bottom) {
            super.setBounds(left, top, right, bottom);
            if (!mPendingBoundsUpdateFromToolbar) {
                mBoundsLeft = left;
                mBoundsTop = top;
                mBoundsRight = right;
                mBoundsBottom = bottom;
                mDrawnByNtp = true;
            } else {
                mDrawnByNtp = false;
            }
            mPendingBoundsUpdateFromToolbar = false;
        }

        @Override
        public boolean setVisible(boolean visible, boolean restart) {
            // Ignore visibility changes.  The NTP can toggle the visibility based on the scroll
            // position of the page, so we simply ignore all of this as we expect the drawable to
            // be visible at all times of the NTP.
            return false;
        }

        @Override
        public Callback getCallback() {
            return mDrawnByNtp ? super.getCallback() : mCallback;
        }
    }

    private void cancelAnimations() {
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.cancel();
        }

        if (mBrandColorTransitionAnimation != null && mBrandColorTransitionAnimation.isRunning()) {
            mBrandColorTransitionAnimation.cancel();
        }

        if (mOptionalButtonAnimator != null && mOptionalButtonAnimator.isRunning()) {
            mOptionalButtonAnimator.cancel();
        }
    }

    /**
     * Calculates the offset required for the focused LocationBar to appear as if it's still
     * unfocused so it can animate to a focused state.
     *
     * @param hasFocus True if the LocationBar has focus, this will be true between the focus
     *                 animation starting and the unfocus animation starting.
     * @return The offset for the location bar when showing the dse icon.
     */
    private int getLocationBarOffsetForFocusAnimation(boolean hasFocus) {
        StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
        if (statusCoordinator == null) return 0;

        // No offset is required if the experiment is disabled.
        if (!SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(
                    getToolbarDataProvider().isIncognito())) {
            return 0;
        }

        // On non-NTP pages, there will always be an icon when unfocused.
        if (!getToolbarDataProvider().getNewTabPageDelegate().isCurrentlyVisible()) return 0;

        // This offset is only required when the focus animation is running.
        if (!hasFocus) return 0;

        // We're on the NTP with the fakebox showing.
        // The value returned changes based on if the layout is LTR OR RTL.
        // For LTR, the value is negative because we are making space on the left-hand side.
        // For RTL, the value is positive because we are pushing the icon further to the
        // right-hand side.
        int offset = statusCoordinator.getStatusIconWidth() - getAdditionalOffsetForNTP();
        return mLocationBar.isLayoutRtl() ? offset : -offset;
    }

    /**
     * Function used to position the url bar inside the location bar during omnibox animation.
     *
     * @param urlExpansionPercent The current expansion percent, 1 is fully focused and 0 is
     *                            completely unfocused.
     * @param hasFocus True if the LocationBar has focus, this will be true between the focus
     *                 animation starting and the unfocus animation starting.
     * @return The X translation for the URL bar, used in the toolbar animation.
     */
    private float getUrlBarTranslationXForToolbarAnimation(
            float urlExpansionPercent, boolean hasFocus) {
        StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
        if (statusCoordinator == null) return 0;

        // No offset is required if the experiment is disabled.
        if (!SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(
                    getToolbarDataProvider().isIncognito())) {
            return 0;
        }

        boolean isRtl = mLocationBar.isLayoutRtl();
        // The calculation here is: the difference in padding between the focused vs unfocused
        // states and also accounts for the translation that the status icon will do. In the end,
        // this translation will be the distance that the url bar needs to travel to arrive at the
        // desired padding when focused.
        float translation =
                urlExpansionPercent * statusCoordinator.getEndPaddingPixelSizeOnFocusDelta();

        boolean scrollingOnNtp = !hasFocus && statusCoordinator.isSearchEngineStatusIconVisible()
                && UrlUtilities.isNTPUrl(getToolbarDataProvider().getCurrentUrl());
        if (scrollingOnNtp) {
            // When:
            // 1. unfocusing the LocationBar on the NTP.
            // 2. scrolling the fakebox to the LocationBar on the NTP.
            // The status icon and the URL bar text overlap in the animation.
            //
            // This branch calculates the negative distance the URL bar needs to travel to
            // completely overlap the status icon and end up in a state that matches the fakebox.
            float overStatusIconTranslation = translation
                    - (1f - urlExpansionPercent)
                            * (statusCoordinator.getStatusIconWidth()
                                    - getAdditionalOffsetForNTP());
            // The value returned changes based on if the layout is LTR or RTL.
            // For LTR, the value is negative because the status icon is left of the url bar on the
            // x/y plane.
            // For RTL, the value is positive because the status icon is right of the url bar on the
            // x/y plane.
            return isRtl ? -overStatusIconTranslation : overStatusIconTranslation;
        }

        return isRtl ? -translation : translation;
    }

    private int getAdditionalOffsetForNTP() {
        return getResources().getDimensionPixelSize(R.dimen.location_bar_lateral_padding)
                - getResources().getDimensionPixelSize(R.dimen.sei_location_bar_lateral_padding);
    }
}
