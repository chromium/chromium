// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
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
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.animation.FastOutSlowInInterpolator;
import android.support.v7.graphics.drawable.DrawableWrapper;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;
import android.util.Property;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewDebug;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.ViewTreeObserver;
import android.view.animation.Interpolator;
import android.view.animation.LinearInterpolator;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarPhone;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Phone specific toolbar implementation.
 */
public class ToolbarPhone extends ToolbarLayout
        implements Invalidator.Client, OnClickListener, OnLongClickListener,
                NewTabPage.OnSearchBoxScrollListener {

    /** The amount of time transitioning from one theme color to another should take in ms. */
    public static final long THEME_COLOR_TRANSITION_DURATION = 250;

    public static final int URL_FOCUS_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int URL_FOCUS_TOOLBAR_BUTTONS_TRANSLATION_X_DP = 10;
    private static final int URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS = 100;
    private static final int URL_CLEAR_FOCUS_EXPERIMENTAL_BUTTON_DELAY_MS = 150;
    private static final int URL_CLEAR_FOCUS_TABSTACK_DELAY_MS = 200;
    private static final int URL_CLEAR_FOCUS_MENU_DELAY_MS = 250;

    private static final int TAB_SWITCHER_MODE_ENTER_ANIMATION_DURATION_MS = 200;
    private static final int TAB_SWITCHER_MODE_EXIT_NORMAL_ANIMATION_DURATION_MS = 200;
    private static final int TAB_SWITCHER_MODE_EXIT_FADE_ANIMATION_DURATION_MS = 100;
    private static final int TAB_SWITCHER_MODE_POST_EXIT_ANIMATION_DURATION_MS = 100;

    // Values used during animation to show/hide optional toolbar button.
    public static final int LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int EXPERIMENTAL_ICON_ANIMATION_DURATION_MS = 100;
    private static final int EXPERIMENTAL_ICON_ANIMATION_DELAY_MS = 125;

    private static final float UNINITIALIZED_PERCENT = -1f;

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

    private static final Interpolator NTP_SEARCH_BOX_EXPANSION_INTERPOLATOR =
            new FastOutSlowInInterpolator();

    private TabModelSelector mTabModelSelector;

    protected LocationBarPhone mLocationBar;

    protected ViewGroup mToolbarButtonsContainer;
    private IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    protected ImageView mToggleTabStackButton;
    protected NewTabButton mNewTabButton;
    protected @Nullable AppCompatImageButton mHomeButton;
    private TextView mUrlBar;
    protected View mUrlActionContainer;
    protected ImageView mToolbarShadow;
    private @Nullable AppCompatImageButton mExperimentalButton;

    private final int mProgressBackBackgroundColorWhite;

    private ObjectAnimator mTabSwitcherModeAnimation;
    private ObjectAnimator mDelayedTabSwitcherModeAnimation;

    private final List<View> mTabSwitcherModeViews = new ArrayList<>();
    protected final Set<View> mBrowsingModeViews = new HashSet<>();
    @ViewDebug.ExportedProperty(category = "chrome")
    protected int mTabSwitcherState;

    // This determines whether or not the toolbar draws as expected (false) or whether it always
    // draws as if it's showing the non-tabswitcher, non-animating toolbar. This is used in grabbing
    // a bitmap to use as a texture representation of this view.
    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mTextureCaptureMode;
    private boolean mForceTextureCapture;
    private boolean mUseLightDrawablesForTextureCapture;
    private boolean mLightDrawablesUsedForLastTextureCapture;

    @ViewDebug.ExportedProperty(category = "chrome")
    private boolean mAnimateNormalToolbar;
    @ViewDebug.ExportedProperty(category = "chrome")
    private boolean mDelayingTabSwitcherAnimation;

    private TabSwitcherDrawable mTabSwitcherAnimationTabStackDrawable;
    private Drawable mTabSwitcherAnimationMenuDrawable;
    private Drawable mTabSwitcherAnimationMenuBadgeDarkDrawable;
    private Drawable mTabSwitcherAnimationMenuBadgeLightDrawable;
    // Value that determines the amount of transition from the normal toolbar mode to TabSwitcher
    // mode.  0 = entirely in normal mode and 1.0 = entirely in TabSwitcher mode.  In between values
    // can be used for animating between the two view modes.
    @ViewDebug.ExportedProperty(category = "chrome")
    protected float mTabSwitcherModePercent;

    // Used to clip the toolbar during the fade transition into and out of TabSwitcher mode.  Only
    // used when |mAnimateNormalToolbar| is false.
    @ViewDebug.ExportedProperty(category = "chrome")
    private Rect mClipRect;

    private OnClickListener mTabSwitcherListener;
    private OnClickListener mNewTabListener;

    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mUrlFocusChangeInProgress;

    /** 1.0 is 100% focused, 0 is completely unfocused */
    @ViewDebug.ExportedProperty(category = "chrome")
    private float mUrlFocusChangePercent;

    /**
     * The degree to which the omnibox has expanded to full width, either because it is getting
     * focused or the NTP search box is being scrolled up. Note that in the latter case, the actual
     * width of the omnibox is not interpolated linearly from this value. The value will be the
     * maximum of {@link #mUrlFocusChangePercent} and {@link #mNtpSearchBoxScrollPercent}.
     *
     * 0.0 == no expansion, 1.0 == fully expanded.
     */
    @ViewDebug.ExportedProperty(category = "chrome")
    protected float mUrlExpansionPercent;
    private AnimatorSet mUrlFocusLayoutAnimator;

    protected boolean mDisableLocationBarRelayout;
    protected boolean mLayoutLocationBarInFocusedMode;
    private boolean mLayoutLocationBarWithoutExtraButton;
    protected int mUnfocusedLocationBarLayoutWidth;
    protected int mUnfocusedLocationBarLayoutLeft;
    protected int mUnfocusedLocationBarLayoutRight;
    private boolean mUnfocusedLocationBarUsesTransparentBg;

    private int mLocationBarBackgroundAlpha = 255;
    private float mNtpSearchBoxScrollPercent = UNINITIALIZED_PERCENT;
    protected ColorDrawable mToolbarBackground;

    /** The omnibox background (white with a shadow). */
    private Drawable mLocationBarBackground;
    private Drawable mActiveLocationBarBackground;

    protected boolean mForceDrawLocationBarBackground;
    private TabSwitcherDrawable mTabSwitcherButtonDrawable;
    protected TabSwitcherDrawable mTabSwitcherButtonDrawableLight;

    private final int mLightModeDefaultColor;
    private final int mDarkModeDefaultColor;

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

    private LayoutUpdateHost mLayoutUpdateHost;

    /** The vertical inset of the location bar background. */
    private int mLocationBarBackgroundVerticalInset;

    /** The current color of the location bar. */
    private int mCurrentLocationBarColor;

    /**
     * Used to specify the visual state of the toolbar.
     */
    @IntDef({VisualState.TAB_SWITCHER_INCOGNITO, VisualState.TAB_SWITCHER_NORMAL,
            VisualState.NORMAL, VisualState.INCOGNITO, VisualState.BRAND_COLOR,
            VisualState.NEW_TAB_NORMAL})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface VisualState {
        int TAB_SWITCHER_INCOGNITO = 0;
        int TAB_SWITCHER_NORMAL = 1;
        int NORMAL = 2;
        int INCOGNITO = 3;
        int BRAND_COLOR = 4;
        int NEW_TAB_NORMAL = 5;
    }

    protected @VisualState int mVisualState = VisualState.NORMAL;
    protected boolean mUseLightToolbarDrawables;

    private NewTabPage mVisibleNewTabPage;
    private float mPreTextureCaptureAlpha = 1f;
    private boolean mIsOverlayTabStackDrawableLight;

    private AnimatorSet mExperimentalButtonAnimator;
    private boolean mExperimentalButtonAnimationRunning;
    private int mExperimentalButtonTranslation;
    /**
     * The percent completion for the location bar width change animation that is run when the
     * experimental button is shown/hidden. Animates from 1.f to 0.f when showing the button and
     * 0.f to 1.f when hiding the button, where 0.f indicates the location bar width is not offset
     * at all for the animation.
     */
    private float mLocBarWidthChangePercent;

    /**
     * A global layout listener used to capture a new texture when the experimental toolbar button
     * is added or removed.
     */
    private ViewTreeObserver.OnGlobalLayoutListener mExperimentalButtonLayoutListener;

    // The following are some properties used during animation.  We use explicit property classes
    // to avoid the cost of reflection for each animation setup.

    private final Property<ToolbarPhone, Float> mUrlFocusChangePercentProperty =
            new Property<ToolbarPhone, Float>(Float.class, "") {
        @Override
        public Float get(ToolbarPhone object) {
            return object.mUrlFocusChangePercent;
        }

        @Override
        public void set(ToolbarPhone object, Float value) {
            setUrlFocusChangePercent(value);
        }
    };

    private final Property<ToolbarPhone, Float> mTabSwitcherModePercentProperty =
            new Property<ToolbarPhone, Float>(Float.class, "") {
        @Override
        public Float get(ToolbarPhone object) {
            return object.mTabSwitcherModePercent;
        }

        @Override
        public void set(ToolbarPhone object, Float value) {
            object.mTabSwitcherModePercent = value;
            triggerPaintInvalidate(ToolbarPhone.this);
        }
    };

    private final Property<ToolbarPhone, Float> mLocBarWidthChangePercentProperty =
            new Property<ToolbarPhone, Float>(Float.class, "") {
                @Override
                public Float get(ToolbarPhone object) {
                    return object.mLocBarWidthChangePercent;
                }

                @Override
                public void set(ToolbarPhone object, Float value) {
                    mLocBarWidthChangePercent = value;
                    updateLocationBarLayoutForExpansionAnimation();
                }
            };

    /**
     * Constructs a ToolbarPhone object.
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public ToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
        mToolbarSidePadding = getResources().getDimensionPixelOffset(
                R.dimen.toolbar_edge_padding);
        mProgressBackBackgroundColorWhite = ApiCompatibilityUtils.getColor(getResources(),
                R.color.progress_bar_background_white);
        mLightModeDefaultColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.light_mode_tint);
        mDarkModeDefaultColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.dark_mode_tint);
    }

    @Override
    public void onFinishInflate() {
        try (TraceEvent te = TraceEvent.scoped("ToolbarPhone.onFinishInflate")) {
            super.onFinishInflate();
            mLocationBar = (LocationBarPhone) findViewById(R.id.location_bar);

            mToolbarButtonsContainer = (ViewGroup) findViewById(R.id.toolbar_buttons);

            mHomeButton = (AppCompatImageButton) findViewById(R.id.home_button);
            changeIconToNTPIcon(mHomeButton);
            if (FeatureUtilities.isBottomToolbarEnabled()) {
                disableMenuButton();
                if (mHomeButton != null) {
                    UiUtils.removeViewFromParent(mHomeButton);
                    mHomeButton = null;
                }
            }

            mUrlBar = (TextView) findViewById(R.id.url_bar);

            mUrlActionContainer = findViewById(R.id.url_action_container);

            mBrowsingModeViews.add(mLocationBar);

            mToolbarBackground =
                    new ColorDrawable(getToolbarColorForVisualState(VisualState.NORMAL));

            initLocationBarBackground();

            setLayoutTransition(null);

            if (getMenuButtonWrapper() != null) getMenuButtonWrapper().setVisibility(View.VISIBLE);
            inflateTabSwitchingResources();

            setWillNotDraw(false);
        }
    }

    /**
     * Initializes the background, padding, margins, etc. for the location bar background.
     */
    protected void initLocationBarBackground() {
        Resources res = getResources();
        mLocationBarBackgroundVerticalInset =
                res.getDimensionPixelSize(R.dimen.location_bar_vertical_margin);
        mLocationBarBackground = createModernLocationBarBackground(getResources());

        int lateralPadding = res.getDimensionPixelOffset(R.dimen.location_bar_lateral_padding);
        mLocationBar.setPadding(lateralPadding, 0, lateralPadding, 0);

        mActiveLocationBarBackground = mLocationBarBackground;
    }

    /**
     * @return The drawable for the modern location bar background.
     */
    public static Drawable createModernLocationBarBackground(Resources resources) {
        Drawable drawable = ApiCompatibilityUtils.getDrawable(
                resources, R.drawable.modern_toolbar_background_white);
        drawable.mutate();
        drawable.setColorFilter(ApiCompatibilityUtils.getColor(resources, R.color.modern_grey_100),
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
        return ColorUtils.getTextBoxColorForToolbarBackground(getResources(), false, toolbarColor);
    }

    private void inflateTabSwitchingResources() {
        mToggleTabStackButton = (ImageView) findViewById(R.id.tab_switcher_button);
        mNewTabButton = (NewTabButton) findViewById(R.id.new_tab_button);
        if (FeatureUtilities.isBottomToolbarEnabled()) {
            UiUtils.removeViewFromParent(mToggleTabStackButton);
            UiUtils.removeViewFromParent(mNewTabButton);
            mToggleTabStackButton = null;
            mNewTabButton = null;
        } else {
            mToggleTabStackButton.setClickable(false);
            mTabSwitcherButtonDrawable =
                    TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), false);
            mTabSwitcherButtonDrawableLight =
                    TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), true);
            mToggleTabStackButton.setImageDrawable(mTabSwitcherButtonDrawable);
            mTabSwitcherModeViews.add(mNewTabButton);
        }
    }

    private void enableTabSwitchingResources() {
        mToggleTabStackButton.setOnClickListener(this);
        mToggleTabStackButton.setOnLongClickListener(this);
        mToggleTabStackButton.setOnKeyListener(new KeyboardNavigationListener() {
            @Override
            public View getNextFocusForward() {
                final AppCompatImageButton menuButton = getMenuButton();
                if (menuButton != null && menuButton.isShown()) {
                    return menuButton;
                } else {
                    return getCurrentTabView();
                }
            }

            @Override
            public View getNextFocusBackward() {
                return findViewById(R.id.url_bar);
            }
        });
        mNewTabButton.setOnClickListener(this);
        mNewTabButton.setOnLongClickListener(this);
    }

    /**
     * Sets up click and key listeners once we have native library available to handle clicks.
     */
    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();

        getLocationBar().onNativeLibraryReady();

        if (!FeatureUtilities.isBottomToolbarEnabled()) enableTabSwitchingResources();

        if (mHomeButton != null) {
            changeIconToNTPIcon(mHomeButton);
            mHomeButton.setOnClickListener(this);
        }

        if (getMenuButton() != null)
            getMenuButton().setOnKeyListener(new KeyboardNavigationListener() {
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
                    return getMenuButtonHelper().onEnterKeyPress(getMenuButton());
                }
            });
        onHomeButtonUpdate(HomepageManager.isHomepageEnabled()
                || FeatureUtilities.isNewTabPageButtonEnabled());

        if (mNewTabButton != null) mNewTabButton.postNativeInitialization();

        setTabSwitcherAnimationMenuDrawable();
        updateVisualsForToolbarState();
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // If the NTP is partially scrolled, prevent all touch events to the child views.  This
        // is to not allow a secondary touch event to trigger entering the tab switcher, which
        // can lead to really odd snapshots and transitions to the switcher.
        if (mNtpSearchBoxScrollPercent != 0f
                && mNtpSearchBoxScrollPercent != 1f
                && mNtpSearchBoxScrollPercent != UNINITIALIZED_PERCENT) {
            return true;
        }

        return super.onInterceptTouchEvent(ev);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Forward touch events to the NTP if the toolbar is moved away but the search box hasn't
        // reached the top of the page yet.
        if (mNtpSearchBoxTranslation.y < 0 && mLocationBar.getTranslationY() > 0) {
            NewTabPage newTabPage = getToolbarDataProvider().getNewTabPageForCurrentTab();

            // No null check -- the toolbar should not be moved if we are not on an NTP.
            return newTabPage.getView().dispatchTouchEvent(ev);
        }

        return super.onTouchEvent(ev);
    }

    @Override
    public void onClick(View v) {
        // Don't allow clicks while the omnibox is being focused.
        if (mLocationBar != null && mLocationBar.hasFocus()) return;

        if (mToggleTabStackButton == v) {
            if (ChromeFeatureList.isInitialized()
                    && ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_MEMEX)) {
                openMemexUI();
                return;
            }
            handleToggleTabStack();
        } else if (mNewTabButton == v) {
            v.setEnabled(false);

            if (mNewTabListener != null) {
                mNewTabListener.onClick(v);
                RecordUserAction.record("MobileToolbarStackViewNewTab");
                RecordUserAction.record("MobileNewTabOpened");
                // TODO(kkimlabs): Record UMA action for homepage button.
            }
        } else if (mHomeButton != null && mHomeButton == v) {
            openHomepage();
            if (isNativeLibraryReady()
                    && PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled()) {
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile())
                        .notifyEvent(EventConstants.PARTNER_HOME_PAGE_BUTTON_PRESSED);
            }
        }
    }

    private void handleToggleTabStack() {
        // The button is clickable before the native library is loaded
        // and the listener is setup.
        if (mToggleTabStackButton != null && mToggleTabStackButton.isClickable()
                && mTabSwitcherListener != null) {
            cancelAppMenuUpdateBadgeAnimation();
            mTabSwitcherListener.onClick(mToggleTabStackButton);
        }
    }

    @Override
    public boolean onLongClick(View v) {
        if (v == mToggleTabStackButton && ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_MEMEX)) {
            handleToggleTabStack();
            return true;
        }
        CharSequence description = null;
        if (v == mToggleTabStackButton) {
            description = getResources().getString(R.string.open_tabs);
        } else if (v == mNewTabButton) {
            description = getResources().getString(isIncognito()
                            ? (ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)
                                              ? R.string.button_new_private_tab
                                              : R.string.button_new_incognito_tab)
                            : R.string.button_new_tab);
        } else {
            return false;
        }
        return AccessibilityUtil.showAccessibilityToast(getContext(), v, description);
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

    private void updateUnfocusedLocationBarLayoutParams() {
        int leftViewBounds = getViewBoundsLeftOfLocationBar(mVisualState);
        int rightViewBounds = getViewBoundsRightOfLocationBar(mVisualState);

        mUnfocusedLocationBarLayoutWidth = rightViewBounds - leftViewBounds;
        mUnfocusedLocationBarLayoutLeft = leftViewBounds;
        mUnfocusedLocationBarLayoutRight = rightViewBounds;
        mLocationBar.setUnfocusedWidth(mUnfocusedLocationBarLayoutWidth);
    }

    /**
     * @return The background drawable for the toolbar view.
     */
    @VisibleForTesting
    ColorDrawable getBackgroundDrawable() {
        return mToolbarBackground;
    }

    @SuppressLint("RtlHardcoded")
    private boolean layoutLocationBar(int containerWidth) {
        // Note that Toolbar's direction depends on system layout direction while
        // LocationBar's direction depends on its text inside.
        FrameLayout.LayoutParams locationBarLayoutParams =
                getFrameLayoutParams(getLocationBar().getContainerView());

        // Chrome prevents layout_gravity="left" from being defined in XML, but it simplifies
        // the logic, so it is manually specified here.
        locationBarLayoutParams.gravity = Gravity.TOP | Gravity.LEFT;

        int width = 0;
        int leftMargin = 0;

        // Always update the unfocused layout params regardless of whether we are using
        // those in this current layout pass as they are needed for animations.
        updateUnfocusedLocationBarLayoutParams();

        if (mLayoutLocationBarInFocusedMode || mVisualState == VisualState.NEW_TAB_NORMAL) {
            int priorVisibleWidth = 0;
            for (int i = 0; i < mLocationBar.getChildCount(); i++) {
                View child = mLocationBar.getChildAt(i);
                if (child == mLocationBar.getFirstViewVisibleWhenFocused()) break;
                if (child.getVisibility() == GONE) continue;
                priorVisibleWidth += child.getMeasuredWidth();
            }

            width = getFocusedLocationBarWidth(containerWidth, priorVisibleWidth);
            leftMargin = getFocusedLocationBarLeftMargin(priorVisibleWidth);
        } else {
            width = mUnfocusedLocationBarLayoutWidth;
            leftMargin = mUnfocusedLocationBarLayoutLeft;
        }

        if (mLayoutLocationBarWithoutExtraButton) {
            float offset = getLocationBarWidthOffsetForExperimentalButton();
            if (ApiCompatibilityUtils.isLayoutRtl(this)) leftMargin -= (int) offset;
            width += (int) offset;
        }

        boolean changed = false;
        changed |= (width != locationBarLayoutParams.width);
        locationBarLayoutParams.width = width;

        changed |= (leftMargin != locationBarLayoutParams.leftMargin);
        locationBarLayoutParams.leftMargin = leftMargin;

        if (changed) updateLocationBarLayoutForExpansionAnimation();

        return changed;
    }

    /**
     * @param containerWidth The width of the view containing the location bar.
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @return The width of the location bar when it has focus.
     */
    protected int getFocusedLocationBarWidth(int containerWidth, int priorVisibleWidth) {
        int width = containerWidth - (2 * mToolbarSidePadding) + priorVisibleWidth;

        return width;
    }

    /**
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @return The left margin of the location bar when it has focus.
     */
    protected int getFocusedLocationBarLeftMargin(int priorVisibleWidth) {
        int baseMargin = mToolbarSidePadding;
        if (ApiCompatibilityUtils.isLayoutRtl(mLocationBar)) {
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
    protected int getViewBoundsLeftOfLocationBar(@VisualState int visualState) {
        // Uses getMeasuredWidth()s instead of getLeft() because this is called in onMeasure
        // and the layout values have not yet been set.
        if (visualState == VisualState.NEW_TAB_NORMAL) {
            return mToolbarSidePadding;
        } else if (ApiCompatibilityUtils.isLayoutRtl(this)) {
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
    protected int getViewBoundsRightOfLocationBar(@VisualState int visualState) {
        // Uses getMeasuredWidth()s instead of getRight() because this is called in onMeasure
        // and the layout values have not yet been set.
        if (visualState == VisualState.NEW_TAB_NORMAL) {
            return getMeasuredWidth() - mToolbarSidePadding;
        } else if (ApiCompatibilityUtils.isLayoutRtl(this)) {
            return getMeasuredWidth() - getBoundsAfterAccountingForLeftButton();
        } else {
            return getMeasuredWidth() - getBoundsAfterAccountingForRightButtons();
        }
    }

    /**
     * @return The right bounds of the location bar after accounting for any visible left buttons.
     */
    protected int getBoundsAfterAccountingForRightButtons() {
        return Math.max(mToolbarSidePadding, mToolbarButtonsContainer.getMeasuredWidth());
    }

    protected void updateToolbarBackground(int color) {
        if (mToolbarBackground.getColor() == color) return;
        mToolbarBackground.setColor(color);
        invalidate();
    }

    protected void updateToolbarBackgroundFromState(@VisualState int visualState) {
        updateToolbarBackground(getToolbarColorForVisualState(visualState));
    }

    private boolean usingHorizontalTabSwitcher() {
        // The horizontal tab switcher flag does not affect the accessibiilty switcher. We do the
        // enableAccessibilityLayout() check first here to avoid logging an experiment exposure for
        // these users.
        return !DeviceClassManager.enableAccessibilityLayout()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
    }

    protected int getToolbarColorForVisualState(final @VisualState int visualState) {
        Resources res = getResources();
        switch (visualState) {
            case VisualState.NEW_TAB_NORMAL:
                if (mUrlExpansionPercent == 1.f) {
                    // When the location bar reaches the top of the screen, the background needs
                    // to change back to the default, solid color so that the NTP content is
                    // not visible beneath the toolbar.
                    return ColorUtils.getDefaultThemeColor(getResources(), false);
                }
                return Color.TRANSPARENT;
            case VisualState.NORMAL:
                return ColorUtils.getDefaultThemeColor(getResources(), false);
            case VisualState.INCOGNITO:
                return ColorUtils.getDefaultThemeColor(getResources(), true);
            case VisualState.BRAND_COLOR:
                return getToolbarDataProvider().getPrimaryColor();
            case VisualState.TAB_SWITCHER_NORMAL:
            case VisualState.TAB_SWITCHER_INCOGNITO:
                if (DeviceClassManager.enableAccessibilityLayout()) {
                    int colorId = visualState == VisualState.TAB_SWITCHER_NORMAL
                            ? R.color.modern_primary_color
                            : R.color.incognito_modern_primary_color;
                    return ApiCompatibilityUtils.getColor(res, colorId);
                }

                return Color.TRANSPARENT;
            default:
                assert false;
                return ApiCompatibilityUtils.getColor(res, R.color.modern_primary_color);
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
                && (mLocationBar.getVisibility() == VISIBLE || mTextureCaptureMode)) {
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
                            tabSwitcherAnimationFinished, mTabSwitcherModePercent);
                }
            }

            super.dispatchDraw(canvas);

            if (mTabSwitcherModeAnimation != null) {
                // Perform the overlay logic after super.dispatchDraw(canvas) as we need to draw on
                // top of the current views.
                if (mAnimateNormalToolbar) {
                    drawTabSwitcherAnimationOverlay(canvas, mTabSwitcherModePercent);
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

    // NewTabPage.OnSearchBoxScrollListener
    @Override
    public void onNtpScrollChanged(float scrollPercentage) {
        mNtpSearchBoxScrollPercent = scrollPercentage;
        updateUrlExpansionPercent();
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
    protected void updateLocationBarBackgroundBounds(Rect out, @VisualState int visualState) {
        // Calculate the visible boundaries of the left and right most child views of the
        // location bar.
        float expansion = getExpansionPercentForVisualState(visualState);
        int leftViewPosition = getLeftPositionOfLocationBarBackground(visualState);
        int rightViewPosition = getRightPositionOfLocationBarBackground(visualState);

        // The bounds are set by the following:
        // - The left most visible location bar child view.
        // - The top of the viewport is aligned with the top of the location bar.
        // - The right most visible location bar child view.
        // - The bottom of the viewport is aligned with the bottom of the location bar.
        // Additional padding can be applied for use during animations.
        out.set(leftViewPosition, mLocationBar.getTop() + mLocationBarBackgroundVerticalInset,
                rightViewPosition, mLocationBar.getBottom() - mLocationBarBackgroundVerticalInset);
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The left drawing position for the location bar background.
     */
    protected int getLeftPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionPercentForVisualState(visualState);
        int leftViewPosition =
                (int) MathUtils.interpolate(getViewBoundsLeftOfLocationBar(visualState),
                        getFocusedLeftPositionOfLocationBarBackground(), expansion);

        if (mExperimentalButtonAnimationRunning && ApiCompatibilityUtils.isLayoutRtl(this)) {
            leftViewPosition -= getLocationBarBackgroundOffsetForExperimentalButton();
        }

        return leftViewPosition;
    }

    /**
     * @return The left drawing position for the location bar background when the location bar
     *         has focus.
     */
    protected int getFocusedLeftPositionOfLocationBarBackground() {
        return mToolbarSidePadding;
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The right drawing position for the location bar background.
     */
    protected int getRightPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionPercentForVisualState(visualState);
        int rightViewPosition =
                (int) MathUtils.interpolate(getViewBoundsRightOfLocationBar(visualState),
                        getFocusedRightPositionOfLocationBarBackground(), expansion);

        if (mExperimentalButtonAnimationRunning && !ApiCompatibilityUtils.isLayoutRtl(this)) {
            rightViewPosition += getLocationBarBackgroundOffsetForExperimentalButton();
        }

        return rightViewPosition;
    }

    /**
     * @return The location bar background position offset, for use when the experimental button
     *         show/hide animation is running.
     */
    private int getLocationBarBackgroundOffsetForExperimentalButton() {
        return (int) (getLocationBarWidthOffsetForExperimentalButton() * mLocBarWidthChangePercent);
    }

    /**
     * @return The difference in the location bar width when the experimental button is hidden
     *         rather than showing. This is effectively the width of the experimental button with
     *         some adjustment to account for possible padding differences when the button
     *         visibility changes.
     */
    private float getLocationBarWidthOffsetForExperimentalButton() {
        float widthChange = mExperimentalButton.getWidth();

        // When the experimental button is the only visible button after the location bar and the
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
    protected int getFocusedRightPositionOfLocationBarBackground() {
        return getWidth() - mToolbarSidePadding;
    }

    private float getExpansionPercentForVisualState(@VisualState int visualState) {
        return visualState == VisualState.NEW_TAB_NORMAL ? 1 : mUrlExpansionPercent;
    }

    /**
     * Updates percentage of current the URL focus change animation.
     * @param percent 1.0 is 100% focused, 0 is completely unfocused.
     */
    private void setUrlFocusChangePercent(float percent) {
        mUrlFocusChangePercent = percent;
        updateUrlExpansionPercent();
        updateUrlExpansionAnimation();
    }

    private void updateUrlExpansionPercent() {
        mUrlExpansionPercent = Math.max(mNtpSearchBoxScrollPercent, mUrlFocusChangePercent);
        assert mUrlExpansionPercent >= 0;
        assert mUrlExpansionPercent <= 1;
    }

    /**
     * Updates the parameters relating to expanding the location bar, as the result of either a
     * focus change or scrolling the New Tab Page.
     */
    protected void updateUrlExpansionAnimation() {
        if (isInTabSwitcherMode()) {
            mToolbarButtonsContainer.setVisibility(VISIBLE);
            return;
        }

        int toolbarButtonVisibility = getToolbarButtonVisibility();
        mToolbarButtonsContainer.setVisibility(toolbarButtonVisibility);
        if (mHomeButton != null && mHomeButton.getVisibility() != GONE) {
            mHomeButton.setVisibility(toolbarButtonVisibility);
        }

        updateLocationBarLayoutForExpansionAnimation();
    }

    /**
     * @return The visibility for {@link #mToolbarButtonsContainer}.
     */
    protected int getToolbarButtonVisibility() {
        return mUrlExpansionPercent == 1f ? INVISIBLE : VISIBLE;
    }

    /**
     * Updates the location bar layout, as the result of either a focus change or scrolling the
     * New Tab Page.
     */
    private void updateLocationBarLayoutForExpansionAnimation() {
        FrameLayout.LayoutParams locationBarLayoutParams = getFrameLayoutParams(mLocationBar);
        int currentLeftMargin = locationBarLayoutParams.leftMargin;
        int currentWidth = locationBarLayoutParams.width;

        float locationBarBaseTranslationX = mUnfocusedLocationBarLayoutLeft - currentLeftMargin;
        if (mExperimentalButtonAnimationRunning) {
            // When showing the button, we disable location bar relayout
            // (mDisableLocationBarRelayout), so the location bar's left margin and
            // mUnfocusedLocationBarLayoutLeft have not been updated to take into account the
            // appearance of the experimental icon. The views to left of the location bar will
            // be wider than mUnfocusedlocationBarLayoutLeft in RTL, so adjust the translation by
            // that amount.
            // When hiding the button, we force a relayout without the experimental toolbar button
            // (mLayoutLocationBarWithoutExtraButton). mUnfocusedLocationBarLayoutLeft reflects
            // the view bounds left of the location bar, which still includes the experimental
            // button. The location bar left margin, however, has been adjusted to reflect its
            // end value when the experimental button is fully hidden. The
            // locationBarBaseTranslationX above accounts for the difference between
            // mUnfocusedLocationBarLayoutLeft and the location bar's current left margin.
            locationBarBaseTranslationX +=
                    getViewBoundsLeftOfLocationBar(mVisualState) - mUnfocusedLocationBarLayoutLeft;
        }

        boolean isLocationBarRtl = ApiCompatibilityUtils.isLayoutRtl(mLocationBar);
        if (isLocationBarRtl) {
            locationBarBaseTranslationX += mUnfocusedLocationBarLayoutWidth - currentWidth;
        }

        locationBarBaseTranslationX *= 1f
                - (mExperimentalButtonAnimationRunning ? mLocBarWidthChangePercent
                                                       : mUrlExpansionPercent);

        mLocationBarBackgroundNtpOffset.setEmpty();
        mLocationBarNtpOffsetLeft = 0;
        mLocationBarNtpOffsetRight = 0;

        Tab currentTab = getToolbarDataProvider().getTab();
        if (currentTab != null) {
            NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
            if (ntp != null) {
                ntp.setUrlFocusChangeAnimationPercent(mUrlFocusChangePercent);
            }

            if (isLocationBarShownInNTP()) {
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

        mLocationBar.setTranslationX(locationBarTranslationX);
        if (!mExperimentalButtonAnimationRunning) {
            mUrlActionContainer.setTranslationX(getUrlActionsTranslationXForExpansionAnimation(
                    isLocationBarRtl, locationBarBaseTranslationX));
            mLocationBar.setUrlFocusChangePercent(mUrlExpansionPercent);

            // Only transition theme colors if in static tab mode that is not the NTP. In practice
            // this only runs when you focus the omnibox on a web page.
            if (!isLocationBarShownInNTP() && mTabSwitcherState == STATIC_TAB) {
                int defaultColor = ColorUtils.getDefaultThemeColor(getResources(), isIncognito());
                int defaultLocationBarColor = getLocationBarColorForToolbarColor(defaultColor);
                int primaryColor = getToolbarDataProvider().getPrimaryColor();
                int themedLocationBarColor = getLocationBarColorForToolbarColor(primaryColor);

                updateToolbarBackground(ColorUtils.getColorWithOverlay(
                        primaryColor, defaultColor, mUrlFocusChangePercent));

                updateModernLocationBarColor(ColorUtils.getColorWithOverlay(
                        themedLocationBarColor, defaultLocationBarColor, mUrlFocusChangePercent));
            }
        }

        // Force an invalidation of the location bar to properly handle the clipping of the URL
        // bar text as a result of the URL action container translations.
        mLocationBar.invalidate();
        invalidate();
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
    protected float getUrlActionsTranslationXForExpansionAnimation(
            boolean isLocationBarRtl, float locationBarBaseTranslationX) {
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);
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
    protected void resetNtpAnimationValues() {
        mLocationBarBackgroundNtpOffset.setEmpty();
        mActiveLocationBarBackground = mLocationBarBackground;
        mNtpSearchBoxTranslation.set(0, 0);
        mLocationBar.setTranslationY(0);
        if (!mUrlFocusChangeInProgress) {
            mToolbarButtonsContainer.setTranslationY(0);
            if (mHomeButton != null) mHomeButton.setTranslationY(0);
        }

        if (!mUrlFocusChangeInProgress) {
            mToolbarShadow.setAlpha(mUrlBar.hasFocus() ? 0.f : 1.f);
        }

        mLocationBar.setAlpha(1);
        mForceDrawLocationBarBackground = false;
        mLocationBarBackgroundAlpha = 255;
        if (isIncognito()
                || (mUnfocusedLocationBarUsesTransparentBg && !mUrlFocusChangeInProgress
                        && !mLocationBar.hasFocus())) {
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        }

        setAncestorsShouldClipChildren(true);
        mNtpSearchBoxScrollPercent = UNINITIALIZED_PERCENT;
        updateUrlExpansionPercent();
    }

    /**
     * Updates the parameters of the New Tab Page transition animation (expanding the location bar
     * as a result of scrolling the New Tab Page).
     */
    private void updateNtpTransitionAnimation() {
        // Skip if in or entering tab switcher mode.
        if (mTabSwitcherState == TAB_SWITCHER || mTabSwitcherState == ENTERING_TAB_SWITCHER) return;

        boolean isExpanded = mUrlExpansionPercent > 0f;
        setAncestorsShouldClipChildren(!isExpanded);
        if (!mUrlFocusChangeInProgress) {
            float alpha = 0.f;
            if (!mUrlBar.hasFocus() && mNtpSearchBoxScrollPercent == 1.f) {
                alpha = 1.f;
            }
            mToolbarShadow.setAlpha(alpha);
        }

        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        ntp.getSearchBoxBounds(mNtpSearchBoxBounds, mNtpSearchBoxTranslation);
        int locationBarTranslationY =
                Math.max(0, (mNtpSearchBoxBounds.top - mLocationBar.getTop()));
        mLocationBar.setTranslationY(locationBarTranslationY);

        updateButtonsTranslationY();

        // Linearly interpolate between the bounds of the search box on the NTP and the omnibox
        // background bounds. |shrinkage| is the scaling factor for the offset -- if it's 1, we are
        // shrinking the omnibox down to the size of the search box.
        float shrinkage =
                1f - NTP_SEARCH_BOX_EXPANSION_INTERPOLATOR.getInterpolation(mUrlExpansionPercent);

        int leftBoundDifference = mNtpSearchBoxBounds.left - mLocationBarBackgroundBounds.left;
        int rightBoundDifference = mNtpSearchBoxBounds.right - mLocationBarBackgroundBounds.right;
        int verticalInset = (int) (getResources().getDimensionPixelSize(
                                           R.dimen.ntp_search_box_bounds_vertical_inset_modern)
                * (1.f - mUrlExpansionPercent));
        mLocationBarBackgroundNtpOffset.set(
                Math.round(leftBoundDifference * shrinkage),
                locationBarTranslationY,
                Math.round(rightBoundDifference * shrinkage),
                locationBarTranslationY);
        mLocationBarBackgroundNtpOffset.inset(0, verticalInset);

        mLocationBarNtpOffsetLeft = leftBoundDifference * shrinkage;
        mLocationBarNtpOffsetRight = rightBoundDifference * shrinkage;

        mLocationBarBackgroundAlpha = isExpanded ? 255 : 0;
        mForceDrawLocationBarBackground = mLocationBarBackgroundAlpha > 0;
        float relativeAlpha = mLocationBarBackgroundAlpha / 255f;
        mLocationBar.setAlpha(relativeAlpha);

        // The search box on the NTP is visible if our omnibox is invisible, and vice-versa.
        ntp.setSearchBoxAlpha(1f - relativeAlpha);
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

    protected void drawTabSwitcherFadeAnimation(boolean animationFinished, float progress) {
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
    protected void drawTabSwitcherAnimationOverlay(Canvas canvas, float animationProgress) {
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
        previousAlpha = mLocationBar.getAlpha();
        mLocationBar.setAlpha(previousAlpha * floatAlpha);
        // If the location bar is now fully transparent, do not bother drawing it.
        if (mLocationBar.getAlpha() != 0) {
            drawChild(canvas, mLocationBar, SystemClock.uptimeMillis());
        }
        mLocationBar.setAlpha(previousAlpha);

        // Translate to draw end toolbar buttons.
        translateCanvasToView(this, mToolbarButtonsContainer, canvas);

        // Draw the experimental button if necessary.
        if (mExperimentalButton != null && mExperimentalButton.getVisibility() != View.GONE) {
            canvas.save();
            Drawable expButton = mExperimentalButton.getDrawable();

            translateCanvasToView(mToolbarButtonsContainer, mExperimentalButton, canvas);

            int backgroundWidth = mExperimentalButton.getDrawable().getIntrinsicWidth();
            int backgroundHeight = mExperimentalButton.getDrawable().getIntrinsicHeight();
            int backgroundLeft =
                    (mExperimentalButton.getWidth() - mExperimentalButton.getPaddingLeft()
                            - mExperimentalButton.getPaddingRight() - backgroundWidth)
                    / 2;
            backgroundLeft += mExperimentalButton.getPaddingLeft();
            int backgroundTop =
                    (mExperimentalButton.getHeight() - mExperimentalButton.getPaddingTop()
                            - mExperimentalButton.getPaddingBottom() - backgroundHeight)
                    / 2;
            backgroundTop += mExperimentalButton.getPaddingTop();
            canvas.translate(backgroundLeft, backgroundTop);

            expButton.setAlpha(rgbAlpha);
            expButton.draw(canvas);

            canvas.restore();
        }

        // Draw the tab stack button and associated text.
        if (mTabSwitcherAnimationTabStackDrawable != null && mToggleTabStackButton != null
                && mUrlExpansionPercent != 1f) {
            // Draw the tab stack button image.
            canvas.save();
            translateCanvasToView(mToolbarButtonsContainer, mToggleTabStackButton, canvas);

            int backgroundWidth = mToggleTabStackButton.getDrawable().getIntrinsicWidth();
            int backgroundHeight = mToggleTabStackButton.getDrawable().getIntrinsicHeight();
            int backgroundLeft = (mToggleTabStackButton.getWidth()
                    - mToggleTabStackButton.getPaddingLeft()
                    - mToggleTabStackButton.getPaddingRight() - backgroundWidth) / 2;
            backgroundLeft += mToggleTabStackButton.getPaddingLeft();
            int backgroundTop = (mToggleTabStackButton.getHeight()
                    - mToggleTabStackButton.getPaddingTop()
                    - mToggleTabStackButton.getPaddingBottom() - backgroundHeight) / 2;
            backgroundTop += mToggleTabStackButton.getPaddingTop();
            canvas.translate(backgroundLeft, backgroundTop);

            mTabSwitcherAnimationTabStackDrawable.setAlpha(rgbAlpha);
            mTabSwitcherAnimationTabStackDrawable.draw(canvas);
            canvas.restore();
        }

        // Draw the menu button if necessary.
        final AppCompatImageButton menuButton = getMenuButton();
        if (menuButton != null && !mShowMenuBadge && mTabSwitcherAnimationMenuDrawable != null
                && mUrlExpansionPercent != 1f) {
            mTabSwitcherAnimationMenuDrawable.setBounds(menuButton.getPaddingLeft(),
                    menuButton.getPaddingTop(),
                    menuButton.getWidth() - menuButton.getPaddingRight(),
                    menuButton.getHeight() - menuButton.getPaddingBottom());
            translateCanvasToView(mToolbarButtonsContainer, menuButton, canvas);
            mTabSwitcherAnimationMenuDrawable.setAlpha(rgbAlpha);
            int color = mUseLightDrawablesForTextureCapture
                    ? mLightModeDefaultColor
                    : mDarkModeDefaultColor;
            mTabSwitcherAnimationMenuDrawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);
            mTabSwitcherAnimationMenuDrawable.draw(canvas);
        }

        // Draw the menu badge if necessary.
        Drawable badgeDrawable = mUseLightDrawablesForTextureCapture
                ? mTabSwitcherAnimationMenuBadgeLightDrawable
                        : mTabSwitcherAnimationMenuBadgeDarkDrawable;

        final View menuBadge = getMenuBadge();
        if (menuBadge != null && mShowMenuBadge && badgeDrawable != null
                && mUrlExpansionPercent != 1f) {
            badgeDrawable.setBounds(menuBadge.getPaddingLeft(), menuBadge.getPaddingTop(),
                    menuBadge.getWidth() - menuBadge.getPaddingRight(),
                    menuBadge.getHeight() - menuBadge.getPaddingBottom());
            translateCanvasToView(mToolbarButtonsContainer, menuBadge, canvas);
            badgeDrawable.setAlpha(rgbAlpha);
            badgeDrawable.draw(canvas);
        }

        mLightDrawablesUsedForLastTextureCapture = mUseLightDrawablesForTextureCapture;

        canvas.restore();
    }

    @Override
    public void doInvalidate() {
        postInvalidateOnAnimation();
    }

    /**
     * Translates the canvas to ensure the specified view's coordinates are at 0, 0.
     *
     * @param from The view the canvas is currently translated to.
     * @param to The view to translate to.
     * @param canvas The canvas to be translated.
     *
     * @throws IllegalArgumentException if {@code from} is not an ancestor of {@code to}.
     */
    protected static void translateCanvasToView(View from, View to, Canvas canvas)
            throws IllegalArgumentException {
        assert from != null;
        assert to != null;
        while (to != from) {
            canvas.translate(to.getLeft(), to.getTop());
            if (!(to.getParent() instanceof View)) {
                throw new IllegalArgumentException("View 'to' was not a desendent of 'from'.");
            }
            to = (View) to.getParent();
        }
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        if (child == mLocationBar) return drawLocationBar(canvas, drawingTime);
        boolean clipped = false;

        if (mLocationBarBackground != null
                && ((mTabSwitcherState == STATIC_TAB && !mTabSwitcherModeViews.contains(child))
                        || (mTabSwitcherState != STATIC_TAB
                                && mBrowsingModeViews.contains(child)))) {
            canvas.save();

            int translationY = (int) mLocationBar.getTranslationY();
            int clipTop = mLocationBarBackgroundBounds.top + translationY;
            if (mUrlExpansionPercent != 0f && clipTop < child.getBottom()) {
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

    protected boolean isChildLeft(View child) {
        return (child == mNewTabButton || (mHomeButton != null && child == mHomeButton))
                ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * @return Whether or not the location bar should be drawing at any particular state of the
     *         toolbar.
     */
    protected boolean shouldDrawLocationBar() {
        return mLocationBarBackground != null
                && (mTabSwitcherState == STATIC_TAB || mTextureCaptureMode);
    }

    private boolean drawLocationBar(Canvas canvas, long drawingTime) {
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

            // When unexpanded, the location bar's visible content boundaries are inset from the
            // viewport used to draw the background.  During expansion transitions, compensation
            // is applied to increase the clip regions such that when the location bar converts
            // to the narrower collapsed layout the visible content is the same.
            if (mUrlExpansionPercent != 1f && !mExperimentalButtonAnimationRunning) {
                int leftDelta = mUnfocusedLocationBarLayoutLeft
                        - getViewBoundsLeftOfLocationBar(mVisualState);
                int rightDelta = getViewBoundsRightOfLocationBar(mVisualState)
                        - mUnfocusedLocationBarLayoutLeft
                        - mUnfocusedLocationBarLayoutWidth;
                float inversePercent = 1f - mUrlExpansionPercent;
                locationBarClipLeft += leftDelta * inversePercent;
                locationBarClipRight -= rightDelta * inversePercent;

                // When the defocus animation is running, the location bar padding needs to be
                // subtracted from the clip bounds so that the location bar text width in the last
                // frame of the animation matches the text width of the unfocused location bar.
                if (ApiCompatibilityUtils.isLayoutRtl(mLocationBar)) {
                    locationBarClipLeft +=
                            ViewCompat.getPaddingStart(mLocationBar) * inversePercent;
                } else {
                    locationBarClipRight -= ViewCompat.getPaddingEnd(mLocationBar) * inversePercent;
                }
            }
            if (mExperimentalButtonAnimationRunning) {
                if (ApiCompatibilityUtils.isLayoutRtl(mLocationBar)) {
                    locationBarClipLeft += ViewCompat.getPaddingStart(mLocationBar);
                } else {
                    locationBarClipRight -= ViewCompat.getPaddingEnd(mLocationBar);
                }
            }

            // Clip the location bar child to the URL viewport calculated in onDraw.
            canvas.clipRect(
                    locationBarClipLeft, locationBarClipTop,
                    locationBarClipRight, locationBarClipBottom);
            clipped = true;
        }

        boolean retVal = super.drawChild(canvas, mLocationBar, drawingTime);

        if (clipped) canvas.restore();
        return retVal;
    }

    /**
     * @return Whether the location bar background should be drawn in
     *         {@link #drawLocationBar(Canvas, long)}.
     */
    protected boolean shouldDrawLocationBarBackground() {
        return (mLocationBar.getAlpha() > 0 || mForceDrawLocationBarBackground)
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
        return !(mTabSwitcherState == TAB_SWITCHER || mTabSwitcherModeAnimation != null
                || urlHasFocus() || mUrlFocusChangeInProgress);
    }

    @Override
    public boolean setForceTextureCapture(boolean forceTextureCapture) {
        if (forceTextureCapture) {
            setUseLightDrawablesForTextureCapture();
            // Only force a texture capture if the tint for the toolbar drawables is changing.
            mForceTextureCapture = mLightDrawablesUsedForLastTextureCapture
                    != mUseLightDrawablesForTextureCapture;
            return mForceTextureCapture;
        }

        mForceTextureCapture = forceTextureCapture;
        return false;
    }

    @Override
    public void setLayoutUpdateHost(LayoutUpdateHost layoutUpdateHost) {
        mLayoutUpdateHost = layoutUpdateHost;
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
    protected void onHomeButtonUpdate(boolean homeButtonEnabled) {
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
        if (mHomeButton == null) return;

        boolean isNTP = getToolbarDataProvider().getNewTabPageForCurrentTab() != null;
        boolean hideHomeButton = FeatureUtilities.isNewTabPageButtonEnabled()
                ? isNTP || isIncognito()
                : !mIsHomeButtonEnabled;
        if (hideHomeButton) {
            removeHomeButton();
        } else {
            addHomeButton();
        }
    }

    private void removeHomeButton() {
        mHomeButton.setVisibility(GONE);
        mBrowsingModeViews.remove(mHomeButton);
    }

    private void addHomeButton() {
        mHomeButton.setVisibility(
                urlHasFocus() || isTabSwitcherAnimationRunning() ? INVISIBLE : VISIBLE);
        ColorStateList tintList = mUseLightToolbarDrawables ? mLightModeTint : mDarkModeTint;
        ApiCompatibilityUtils.setImageTintList(mHomeButton, tintList);
        mBrowsingModeViews.add(mHomeButton);
    }

    private ObjectAnimator createEnterTabSwitcherModeAnimation() {
        ObjectAnimator enterAnimation =
                ObjectAnimator.ofFloat(this, mTabSwitcherModePercentProperty, 1.f);
        enterAnimation.setDuration(TAB_SWITCHER_MODE_ENTER_ANIMATION_DURATION_MS);
        enterAnimation.setInterpolator(new LinearInterpolator());
        enterAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                // This is to deal with the view going invisible when resuming the activity and
                // running this animation.  The view is still there and clickable but does not
                // render and only a layout triggers a refresh.  See crbug.com/306890.
                if (mToggleTabStackButton != null && !mToggleTabStackButton.isEnabled()) {
                    requestLayout();
                }
            }
        });

        return enterAnimation;
    }

    private ObjectAnimator createExitTabSwitcherAnimation(
            final boolean animateNormalToolbar) {
        ObjectAnimator exitAnimation =
                ObjectAnimator.ofFloat(this, mTabSwitcherModePercentProperty, 0.f);
        exitAnimation.setDuration(animateNormalToolbar
                ? TAB_SWITCHER_MODE_EXIT_NORMAL_ANIMATION_DURATION_MS
                : TAB_SWITCHER_MODE_EXIT_FADE_ANIMATION_DURATION_MS);
        exitAnimation.setInterpolator(new LinearInterpolator());
        exitAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                updateViewsForTabSwitcherMode();
            }
        });

        return exitAnimation;
    }

    private ObjectAnimator createPostExitTabSwitcherAnimation() {
        ObjectAnimator exitAnimation = ObjectAnimator.ofFloat(
                this, View.TRANSLATION_Y, -getHeight(), 0.f);
        exitAnimation.setDuration(TAB_SWITCHER_MODE_POST_EXIT_ANIMATION_DURATION_MS);
        exitAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        exitAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                updateViewsForTabSwitcherMode();
                // On older builds, force an update to ensure the new visuals are used
                // when bringing in the toolbar.  crbug.com/404571
                if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN) {
                    requestLayout();
                }
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mDelayedTabSwitcherModeAnimation = null;
                updateShadowVisibility();
                updateViewsForTabSwitcherMode();
            }
        });

        return exitAnimation;
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        assert mTextureCaptureMode != textureMode;
        mTextureCaptureMode = textureMode;
        if (mTextureCaptureMode) {
            if (!hideShadowForIncognitoNtp() && !hideShadowForInterstitial()) {
                mToolbarShadow.setVisibility(VISIBLE);
            }
            mPreTextureCaptureAlpha = getAlpha();
            setAlpha(1);
        } else {
            setAlpha(mPreTextureCaptureAlpha);
            updateShadowVisibility();
            mPreTextureCaptureAlpha = 1f;
        }
    }

    // TODO(dtrainor): This is always true when in the tab switcher (crbug.com/710750).
    protected boolean isTabSwitcherAnimationRunning() {
        return mTabSwitcherState == ENTERING_TAB_SWITCHER
                || mTabSwitcherState == EXITING_TAB_SWITCHER;
    }

    private void updateViewsForTabSwitcherMode() {
        int tabSwitcherViewsVisibility = mTabSwitcherState != STATIC_TAB  ? VISIBLE : INVISIBLE;
        int browsingViewsVisibility = mTabSwitcherState != STATIC_TAB ? INVISIBLE : VISIBLE;

        // Don't inflate the incognito toggle button unless the horizontal tab switcher experiment
        // is enabled and the user actually enters the tab switcher.
        if (!FeatureUtilities.isBottomToolbarEnabled() && mIncognitoToggleTabLayout == null
                && mTabSwitcherState != STATIC_TAB && usingHorizontalTabSwitcher()
                && PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
            ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
            mIncognitoToggleTabLayout =
                    (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();
            mIncognitoToggleTabLayout.setTabModelSelector(mTabModelSelector);
            mTabSwitcherModeViews.add(mIncognitoToggleTabLayout);
            mIncognitoToggleTabLayout.updateTabCount(mTabModelSelector.getModel(false).getCount());

            mBrowsingModeViews.add(mToggleTabStackButton);
        }

        for (View view : mTabSwitcherModeViews) {
            view.setVisibility(tabSwitcherViewsVisibility);
        }
        for (View view : mBrowsingModeViews) {
            view.setVisibility(browsingViewsVisibility);
        }
        if (mShowMenuBadge) {
            setMenuButtonContentDescription();
        }

        updateProgressBarVisibility();
        updateVisualsForToolbarState();
        updateTabSwitcherButtonRipple();
    }

    private void updateProgressBarVisibility() {
        getProgressBar().setVisibility(mTabSwitcherState != STATIC_TAB ? INVISIBLE : VISIBLE);
    }

    @Override
    protected void setContentAttached(boolean attached) {
        updateVisualsForToolbarState();
    }

    @Override
    protected void setTabSwitcherMode(
            boolean inTabSwitcherMode, boolean showToolbar, boolean delayAnimation) {
        setTabSwitcherMode(inTabSwitcherMode, showToolbar, delayAnimation, true);
    }

    /**
     * See {@link #setTabSwitcherMode(boolean, boolean, boolean)}.
     */
    protected void setTabSwitcherMode(boolean inTabSwitcherMode, boolean showToolbar,
            boolean delayAnimation, boolean animate) {
        // If setting tab switcher mode to true and the browser is already animating or in the tab
        // switcher skip.
        if (inTabSwitcherMode && (mTabSwitcherState == TAB_SWITCHER
                || mTabSwitcherState == ENTERING_TAB_SWITCHER)) {
            return;
        }

        // Likewise if exiting the tab switcher.
        if (!inTabSwitcherMode && (mTabSwitcherState == STATIC_TAB
                || mTabSwitcherState == EXITING_TAB_SWITCHER)) {
            return;
        }
        mTabSwitcherState = inTabSwitcherMode ? ENTERING_TAB_SWITCHER : EXITING_TAB_SWITCHER;

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
            if (mNewTabButton != null) mNewTabButton.setEnabled(true);
            updateViewsForTabSwitcherMode();
            mTabSwitcherModeAnimation = createEnterTabSwitcherModeAnimation();
        } else {
            if (!mDelayingTabSwitcherAnimation) {
                mTabSwitcherModeAnimation = createExitTabSwitcherAnimation(showToolbar);
                if (mIncognitoToggleTabLayout != null)
                    mIncognitoToggleTabLayout.setClickable(false);
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
            TypedValue outValue = new TypedValue();

            // The linked style here will have to be changed if it is updated in the XML.
            getContext().getTheme().resolveAttribute(R.style.ToolbarButton, outValue, true);
            mToggleTabStackButton.setBackgroundResource(outValue.resourceId);
        }
    }

    @Override
    protected void onTabSwitcherTransitionFinished() {
        if (mIncognitoToggleTabLayout != null) mIncognitoToggleTabLayout.setClickable(true);

        setAlpha(1.f);
        mClipRect = null;

        // Detect what was being transitioned from and set the new state appropriately.
        if (mTabSwitcherState == EXITING_TAB_SWITCHER) {
            mLocationBar.setUrlBarFocusable(true);
            mTabSwitcherState = STATIC_TAB;
            updateVisualsForToolbarState();
        }
        if (mTabSwitcherState == ENTERING_TAB_SWITCHER) mTabSwitcherState = TAB_SWITCHER;

        mTabSwitcherModePercent = mTabSwitcherState != STATIC_TAB ? 1.0f : 0.0f;

        if (!mAnimateNormalToolbar) {
            finishAnimations();
            updateVisualsForToolbarState();
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
        mTabSwitcherListener = listener;
    }

    @Override
    public void setOnNewTabClickHandler(OnClickListener listener) {
        mNewTabListener = listener;
    }

    @Override
    protected void onAccessibilityStatusChanged(boolean enabled) {
        super.onAccessibilityStatusChanged(enabled);
        if (mNewTabButton != null) mNewTabButton.onAccessibilityStatusChanged();
        if (mIncognitoToggleTabLayout != null) {
            if (enabled) {
                mIncognitoToggleTabLayout.setVisibility(View.GONE);
                mTabSwitcherModeViews.remove(mIncognitoToggleTabLayout);
            } else {
                if (mTabSwitcherState != STATIC_TAB) {
                    mIncognitoToggleTabLayout.setVisibility(View.VISIBLE);
                }
                mTabSwitcherModeViews.add(mIncognitoToggleTabLayout);
            }
        }
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return super.shouldIgnoreSwipeGesture() || mUrlExpansionPercent > 0f
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
                boolean rtl = ApiCompatibilityUtils.isLayoutRtl(containerView);
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

    private void populateUrlFocusingAnimatorSet(List<Animator> animators) {
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangePercentProperty, 1f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);

        for (int i = 0; i < mLocationBar.getChildCount(); i++) {
            View childView = mLocationBar.getChildAt(i);
            if (childView == mLocationBar.getFirstViewVisibleWhenFocused()) break;
            animator = ObjectAnimator.ofFloat(childView, ALPHA, 0);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

        float density = getContext().getResources().getDisplayMetrics().density;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);
        float toolbarButtonTranslationX = MathUtils.flipSignIf(
                URL_FOCUS_TOOLBAR_BUTTONS_TRANSLATION_X_DP, isRtl) * density;

        final View menuButtonWrapper = getMenuButtonWrapper();
        if (menuButtonWrapper != null) {
            animator = ObjectAnimator.ofFloat(
                    menuButtonWrapper, TRANSLATION_X, toolbarButtonTranslationX);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(menuButtonWrapper, ALPHA, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);
        }

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

        if (mExperimentalButton != null && mExperimentalButton.getVisibility() != View.GONE) {
            animator = ObjectAnimator.ofFloat(
                    mExperimentalButton, TRANSLATION_X, toolbarButtonTranslationX);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mExperimentalButton, ALPHA, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);
        }

        animator = ObjectAnimator.ofFloat(mToolbarShadow, ALPHA, 0);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);
    }

    private void populateUrlClearFocusingAnimatorSet(List<Animator> animators) {
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangePercentProperty, 0f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);

        final View menuButtonWrapper = getMenuButtonWrapper();
        if (menuButtonWrapper != null) {
            animator = ObjectAnimator.ofFloat(menuButtonWrapper, TRANSLATION_X, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_MENU_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(menuButtonWrapper, ALPHA, 1);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_MENU_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

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

        if (mExperimentalButton != null && mExperimentalButton.getVisibility() != View.GONE) {
            // TODO(twellington): it's possible that the experimental button was shown while
            // the url bar was focused, in which case the translation x and alpha animators
            // are a no-op. Account for this case.
            animator = ObjectAnimator.ofFloat(mExperimentalButton, TRANSLATION_X, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_EXPERIMENTAL_BUTTON_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mExperimentalButton, ALPHA, 1);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_EXPERIMENTAL_BUTTON_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

        for (int i = 0; i < mLocationBar.getChildCount(); i++) {
            View childView = mLocationBar.getChildAt(i);
            if (childView == mLocationBar.getFirstViewVisibleWhenFocused()) break;
            animator = ObjectAnimator.ofFloat(childView, ALPHA, 1);
            animator.setStartDelay(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setDuration(URL_CLEAR_FOCUS_MENU_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

        if (isLocationBarShownInNTP() && mNtpSearchBoxScrollPercent == 0f) return;

        animator = ObjectAnimator.ofFloat(mToolbarShadow, ALPHA, 1);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);
    }

    @Override
    public void onUrlFocusChange(final boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);

        triggerUrlFocusAnimation(hasFocus);
    }

    protected void triggerUrlFocusAnimation(final boolean hasFocus) {
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.cancel();
            mUrlFocusLayoutAnimator = null;
        }
        if (mExperimentalButtonAnimationRunning) mExperimentalButtonAnimator.end();

        List<Animator> animators = new ArrayList<>();
        if (hasFocus) {
            populateUrlFocusingAnimatorSet(animators);
        } else {
            populateUrlClearFocusingAnimatorSet(animators);
        }
        mUrlFocusLayoutAnimator = new AnimatorSet();
        mUrlFocusLayoutAnimator.playTogether(animators);

        mUrlFocusChangeInProgress = true;
        mUrlFocusLayoutAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                if (!hasFocus) {
                    mDisableLocationBarRelayout = true;
                } else {
                    mLayoutLocationBarInFocusedMode = true;
                    requestLayout();
                }
            }

            @Override
            public void onCancel(Animator animation) {
                if (!hasFocus) mDisableLocationBarRelayout = false;

                mUrlFocusChangeInProgress = false;
            }

            @Override
            public void onEnd(Animator animation) {
                if (!hasFocus) {
                    mDisableLocationBarRelayout = false;
                    mLayoutLocationBarInFocusedMode = false;
                    requestLayout();
                }
                mLocationBar.finishUrlFocusChange(hasFocus);
                onUrlFocusChangeAnimationFinished();
                mUrlFocusChangeInProgress = false;
            }
        });
        mUrlFocusLayoutAnimator.start();
    }

    /** Called when the URL focus change animation has finished. */
    protected void onUrlFocusChangeAnimationFinished() {}

    @Override
    protected void updateTabCountVisuals(int numberOfTabs) {
        if (mHomeButton != null) mHomeButton.setEnabled(true);

        if (mToggleTabStackButton == null) return;

        mToggleTabStackButton.setEnabled(numberOfTabs >= 1);
        mToggleTabStackButton.setContentDescription(
                getResources().getQuantityString(
                        R.plurals.accessibility_toolbar_btn_tabswitcher_toggle,
                        numberOfTabs, numberOfTabs));
        mTabSwitcherButtonDrawableLight.updateForTabCount(numberOfTabs, isIncognito());
        mTabSwitcherButtonDrawable.updateForTabCount(numberOfTabs, isIncognito());

        if (!isIncognito() && mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.updateTabCount(numberOfTabs);
        }

        boolean useTabStackDrawableLight = isIncognito()
                || ColorUtils.shouldUseLightForegroundOnBackground(getTabThemeColor());
        if (mTabSwitcherAnimationTabStackDrawable == null
                || mIsOverlayTabStackDrawableLight != useTabStackDrawableLight) {
            mTabSwitcherAnimationTabStackDrawable = TabSwitcherDrawable.createTabSwitcherDrawable(
                    getContext(), useTabStackDrawableLight);
            int[] stateSet = {android.R.attr.state_enabled};
            mTabSwitcherAnimationTabStackDrawable.setState(stateSet);
            mTabSwitcherAnimationTabStackDrawable.setBounds(
                    mToggleTabStackButton.getDrawable().getBounds());
            mIsOverlayTabStackDrawableLight = useTabStackDrawableLight;
        }

        if (mTabSwitcherAnimationTabStackDrawable != null) {
            mTabSwitcherAnimationTabStackDrawable.updateForTabCount(
                    numberOfTabs, isIncognito());
        }
    }

    /**
     * Get the theme color for the currently active tab. This is not affected by the tab switcher's
     * theme color.
     * @return The current tab's theme color.
     */
    protected int getTabThemeColor() {
        if (getToolbarDataProvider() != null) return getToolbarDataProvider().getPrimaryColor();
        return getToolbarColorForVisualState(
                isIncognito() ? VisualState.INCOGNITO : VisualState.NORMAL);
    }

    @Override
    protected void onTabContentViewChanged() {
        super.onTabContentViewChanged();
        updateNtpAnimationState();
        updateVisualsForToolbarState();
    }

    @Override
    protected void onTabOrModelChanged() {
        super.onTabOrModelChanged();
        updateNtpAnimationState();
        updateVisualsForToolbarState();
    }

    private static boolean isVisualStateValidForBrandColorTransition(@VisualState int state) {
        return state == VisualState.NORMAL || state == VisualState.BRAND_COLOR;
    }

    @Override
    protected void onPrimaryColorChanged(boolean shouldAnimate) {
        super.onPrimaryColorChanged(shouldAnimate);
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.cancel();

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
        final int finalAlpha =
                shouldUseOpaque ? 255 : LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        final boolean shouldAnimateAlpha = initialAlpha != finalAlpha;
        mBrandColorTransitionAnimation = ValueAnimator.ofFloat(0, 1)
                .setDuration(THEME_COLOR_TRANSITION_DURATION);
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
        mBrandColorTransitionAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mBrandColorTransitionActive = false;
                updateVisualsForToolbarState();
            }
        });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
    }

    private void updateNtpAnimationState() {
        // Store previous NTP scroll before calling reset as that clears this value.
        boolean wasShowingNtp = mVisibleNewTabPage != null;
        float previousNtpScrollPercent = mNtpSearchBoxScrollPercent;

        resetNtpAnimationValues();
        if (mVisibleNewTabPage != null) {
            mVisibleNewTabPage.setSearchBoxScrollListener(null);
            mVisibleNewTabPage = null;
        }
        mVisibleNewTabPage = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (mVisibleNewTabPage != null && mVisibleNewTabPage.isLocationBarShownInNTP()) {
            mVisibleNewTabPage.setSearchBoxScrollListener(this);

            NtpSearchBoxDrawable ntpSearchBox = new NtpSearchBoxDrawable(getContext(), this);
            mVisibleNewTabPage.setSearchBoxBackground(ntpSearchBox);
            mActiveLocationBarBackground = ntpSearchBox;

            requestLayout();
        } else if (wasShowingNtp) {
            // Convert the previous NTP scroll percentage to URL focus percentage because that
            // will give a nicer transition animation from the expanded NTP omnibox to the
            // collapsed normal omnibox on other non-NTP pages.
            if (mTabSwitcherState == STATIC_TAB && previousNtpScrollPercent > 0f) {
                mUrlFocusChangePercent =
                        Math.max(previousNtpScrollPercent, mUrlFocusChangePercent);
                triggerUrlFocusAnimation(false);
            }
            requestLayout();
        }
    }

    @Override
    protected void onDefaultSearchEngineChanged() {
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
                updateVisualsForToolbarState();
                updateNtpAnimationState();
            }
        });
    }

    @Override
    protected void handleFindToolbarStateChange(boolean showing) {
        setVisibility(showing ? View.GONE : View.VISIBLE);
    }

    private boolean isLocationBarShownInNTP() {
        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        return ntp != null && ntp.isLocationBarShownInNTP();
    }

    /**
     * Update the visibility of the toolbar shadow.
     */
    protected void updateShadowVisibility() {
        boolean shouldDrawShadow = shouldDrawShadow();
        int shadowVisibility = shouldDrawShadow ? View.VISIBLE : View.INVISIBLE;

        if (mToolbarShadow.getVisibility() != shadowVisibility) {
            mToolbarShadow.setVisibility(shadowVisibility);
        }
    }

    /**
     * @return Whether the toolbar shadow should be drawn.
     */
    protected boolean shouldDrawShadow() {
        // TODO(twellington): Move this shadow state information to ToolbarDataProvider and show
        // shadow when incognito NTP is scrolled.
        return mTabSwitcherState == STATIC_TAB && !hideShadowForIncognitoNtp()
                && !hideShadowForInterstitial();
    }

    private boolean hideShadowForIncognitoNtp() {
        return isIncognito() && NewTabPage.isNTPUrl(getToolbarDataProvider().getCurrentUrl());
    }

    private boolean hideShadowForInterstitial() {
        return getToolbarDataProvider() != null && getToolbarDataProvider().getTab() != null
                && (getToolbarDataProvider().getTab().isShowingInterstitialPage()
                           || getToolbarDataProvider().getTab().isShowingErrorPage());
    }

    private @VisualState int computeVisualState(boolean isInTabSwitcherMode) {
        if (isInTabSwitcherMode && isIncognito()) return VisualState.TAB_SWITCHER_INCOGNITO;
        if (isInTabSwitcherMode && !isIncognito()) return VisualState.TAB_SWITCHER_NORMAL;
        if (isLocationBarShownInNTP()) return VisualState.NEW_TAB_NORMAL;
        if (isIncognito()) return VisualState.INCOGNITO;
        if (getToolbarDataProvider().isUsingBrandColor()) return VisualState.BRAND_COLOR;
        return VisualState.NORMAL;
    }

    /**
     * @return The color that progress bar should use.
     */
    protected int getProgressBarColor() {
        return getToolbarDataProvider().getPrimaryColor();
    }

    protected void updateVisualsForToolbarState() {
        final boolean isIncognito = isIncognito();

        // These are important for setting visual state while the entering or leaving the tab
        // switcher.
        boolean inOrEnteringStaticTab = mTabSwitcherState == STATIC_TAB
                || mTabSwitcherState == EXITING_TAB_SWITCHER;
        boolean inOrEnteringTabSwitcher = !inOrEnteringStaticTab;

        @VisualState
        int newVisualState = computeVisualState(inOrEnteringTabSwitcher);

        // If we are navigating to or from a brand color, allow the transition animation
        // to run to completion as it will handle the triggering this path again and committing
        // the proper visual state when it finishes.  Brand color transitions are only valid
        // between normal non-incognito pages and brand color pages, so if the visual states
        // do not match then cancel the animation below.
        if (mBrandColorTransitionActive
                && isVisualStateValidForBrandColorTransition(mVisualState)
                && isVisualStateValidForBrandColorTransition(newVisualState)) {
            return;
        } else if (mBrandColorTransitionAnimation != null
                && mBrandColorTransitionAnimation.isRunning()) {
            mBrandColorTransitionAnimation.cancel();
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
            boolean useLightToolbarDrawables =
                    ColorUtils.shouldUseLightForegroundOnBackground(currentPrimaryColor);
            boolean unfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            if (useLightToolbarDrawables != mUseLightToolbarDrawables
                    || unfocusedLocationBarUsesTransparentBg
                            != mUnfocusedLocationBarUsesTransparentBg) {
                visualStateChanged = true;
            } else {
                updateToolbarBackgroundFromState(VisualState.BRAND_COLOR);
                getProgressBar().setThemeColor(themeColorForProgressBar, isIncognito());
            }
        }

        mVisualState = newVisualState;

        // Refresh the toolbar texture.
        if ((mVisualState == VisualState.BRAND_COLOR || visualStateChanged)
                && mLayoutUpdateHost != null) {
            setUseLightDrawablesForTextureCapture();
            mLayoutUpdateHost.requestUpdate();
        }
        updateShadowVisibility();
        updateUrlExpansionAnimation();

        // This exception is to prevent early change of theme color when exiting the tab switcher
        // since currently visual state does not map correctly to tab switcher state. See
        // https://crbug.com/832594 for more info.
        if (mTabSwitcherState != EXITING_TAB_SWITCHER)
            updateToolbarBackgroundFromState(mVisualState);

        if (!visualStateChanged) {
            if (mVisualState == VisualState.NEW_TAB_NORMAL) {
                updateNtpTransitionAnimation();
            } else {
                resetNtpAnimationValues();
            }
            return;
        }

        mUseLightToolbarDrawables = false;
        mUnfocusedLocationBarUsesTransparentBg = false;
        mLocationBarBackgroundAlpha = 255;
        getProgressBar().setThemeColor(themeColorForProgressBar, isIncognito());

        if (inOrEnteringTabSwitcher) {
            assert mVisualState == VisualState.TAB_SWITCHER_NORMAL
                    || mVisualState == VisualState.TAB_SWITCHER_INCOGNITO;
            int colorForVisualState = getToolbarColorForVisualState(mVisualState);
            mUseLightToolbarDrawables =
                    (ColorUtils.shouldUseLightForegroundOnBackground(colorForVisualState)
                            && colorForVisualState != Color.TRANSPARENT);
            mUseLightToolbarDrawables |= (usingHorizontalTabSwitcher() && isIncognito());
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
            getProgressBar().setBackgroundColor(mProgressBackBackgroundColorWhite);
            getProgressBar().setForegroundColor(ApiCompatibilityUtils.getColor(getResources(),
                    R.color.progress_bar_foreground_white));
        } else if (isIncognito()) {
            mUseLightToolbarDrawables = true;
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        } else if (mVisualState == VisualState.BRAND_COLOR) {
            mUseLightToolbarDrawables =
                    ColorUtils.shouldUseLightForegroundOnBackground(currentPrimaryColor);
            mUnfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            mLocationBarBackgroundAlpha = mUnfocusedLocationBarUsesTransparentBg
                    ? LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA
                    : 255;
        }

        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setImageDrawable(mUseLightToolbarDrawables
                    ? mTabSwitcherButtonDrawableLight : mTabSwitcherButtonDrawable);
            if (mTabSwitcherAnimationTabStackDrawable != null) {
                mTabSwitcherAnimationTabStackDrawable.setTint(
                        mUseLightToolbarDrawables ? mLightModeTint : mDarkModeTint);
            }
        }

        if (getMenuButton() != null) {
            ColorStateList tintList = mUseLightToolbarDrawables ? mLightModeTint : mDarkModeTint;
            ApiCompatibilityUtils.setImageTintList(getMenuButton(), tintList);
        }

        updateModernLocationBarColor(getLocationBarColorForToolbarColor(currentPrimaryColor));
        if (mExperimentalButton != null) {
            ColorStateList tintList = mUseLightToolbarDrawables ? mLightModeTint : mDarkModeTint;
            ApiCompatibilityUtils.setImageTintList(mExperimentalButton, tintList);
        }

        setMenuButtonHighlightDrawable(mHighlightingMenu);
        if (mShowMenuBadge && inOrEnteringStaticTab) {
            setAppMenuUpdateBadgeDrawable(mUseLightToolbarDrawables);
        }
        ColorStateList tint = mUseLightToolbarDrawables ? mLightModeTint : mDarkModeTint;
        if (mIsHomeButtonEnabled && mHomeButton != null) {
            ApiCompatibilityUtils.setImageTintList(mHomeButton, tint);
        }

        mLocationBar.updateVisualsForState();

        // We update the alpha before comparing the visual state as we need to change
        // its value when entering and exiting TabSwitcher mode.
        if (isLocationBarShownInNTP() && inOrEnteringStaticTab) {
            updateNtpTransitionAnimation();
        }

        if (mNewTabButton != null) mNewTabButton.setIsIncognito(isIncognito);

        if (getMenuButtonWrapper() != null) {
            getMenuButtonWrapper().setVisibility(View.VISIBLE);
        }

        DrawableCompat.setTint(mLocationBarBackground,
                isIncognito() ? Color.WHITE
                              : ApiCompatibilityUtils.getColor(
                                        getResources(), R.color.modern_grey_100));
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    @Override
    public boolean useLightDrawables() {
        return mUseLightToolbarDrawables;
    }

    @Override
    protected void setMenuButtonHighlightDrawable(boolean highlighting) {
        highlighting &= !isTabSwitcherAnimationRunning();
        super.setMenuButtonHighlightDrawable(highlighting);
    }

    @Override
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(mTabModelSelector);
        }
    }

    @Override
    public void showAppMenuUpdateBadge() {
        if (getMenuBadge() == null) return;
        super.showAppMenuUpdateBadge();

        // Set up variables.
        if (!mBrowsingModeViews.contains(getMenuBadge())) {
            mBrowsingModeViews.add(getMenuBadge());
        }

        // Finish any in-progress animations and set the TabSwitcherAnimationMenuBadgeDrawables.
        finishAnimations();
        setTabSwitcherAnimationMenuBadgeDrawable();

        // Show the badge.
        if (mTabSwitcherState == STATIC_TAB) {
            if (mUseLightToolbarDrawables) {
                setAppMenuUpdateBadgeDrawable(mUseLightToolbarDrawables);
            }
            setAppMenuUpdateBadgeToVisible(true);
        }
    }

    @Override
    public void removeAppMenuUpdateBadge(boolean animate) {
        if (getMenuBadge() == null) return;
        super.removeAppMenuUpdateBadge(animate);

        if (mBrowsingModeViews.contains(getMenuBadge())) {
            mBrowsingModeViews.remove(getMenuBadge());
            mTabSwitcherAnimationMenuBadgeDarkDrawable = null;
            mTabSwitcherAnimationMenuBadgeLightDrawable = null;
        }
    }

    @Override
    public void enableExperimentalButton(
            OnClickListener onClickListener, int drawableResId, int contentDescriptionResId) {
        if (mExperimentalButton == null) {
            ViewStub viewStub = findViewById(R.id.experimental_button_stub);
            mExperimentalButton = (AppCompatImageButton) viewStub.inflate();

            if (!isMenuButtonPresent()) mExperimentalButton.setPadding(0, 0, 0, 0);
            mExperimentalButtonTranslation = getResources().getDimensionPixelSize(
                    R.dimen.toolbar_optional_button_animation_translation);
            if (ApiCompatibilityUtils.isLayoutRtl(this)) mExperimentalButtonTranslation *= -1;
        } else {
            if (mExperimentalButtonAnimationRunning) {
                mExperimentalButtonAnimator.end();
            }
            assert mExperimentalButton.getVisibility()
                    == View.GONE : "#disableExperimentalButton() should be called first.";
        }

        mBrowsingModeViews.add(mExperimentalButton);
        mExperimentalButton.setOnClickListener(onClickListener);
        mExperimentalButton.setImageResource(drawableResId);
        mExperimentalButton.setContentDescription(
                getContext().getResources().getString(contentDescriptionResId));
        ApiCompatibilityUtils.setImageTintList(
                mExperimentalButton, mUseLightToolbarDrawables ? mLightModeTint : mDarkModeTint);

        mExperimentalButtonLayoutListener = () -> requestLayoutHostUpdateForExperimentalButton();
        if (mTabSwitcherState == STATIC_TAB) {
            if (!mUrlFocusChangeInProgress && !urlHasFocus()) {
                runShowExperimentalButtonAnimation();
            } else {
                mExperimentalButton.setVisibility(View.VISIBLE);
            }
        } else {
            mExperimentalButton.setVisibility(View.INVISIBLE);
            getViewTreeObserver().addOnGlobalLayoutListener(mExperimentalButtonLayoutListener);
        }
    }

    @Override
    public View getExperimentalButtonView() {
        return mExperimentalButton;
    }

    @Override
    public void disableExperimentalButton() {
        if (mExperimentalButton == null || mExperimentalButton.getVisibility() == View.GONE) {
            return;
        }

        if (mTabSwitcherState == STATIC_TAB && !mUrlFocusChangeInProgress && !urlHasFocus()) {
            runHideExperimentalButtonsAnimators();
        } else {
            mExperimentalButton.setVisibility(View.GONE);
            getViewTreeObserver().addOnGlobalLayoutListener(mExperimentalButtonLayoutListener);
        }

        mBrowsingModeViews.remove(mExperimentalButton);
    }

    /**
     * Whether the menu button is visible. Used as a proxy for whether there are end toolbar
     * buttons besides the experimental button.
     */
    private boolean isMenuButtonPresent() {
        return mMenuButton != null;
    }

    private void requestLayoutHostUpdateForExperimentalButton() {
        if (mLayoutUpdateHost != null) mLayoutUpdateHost.requestUpdate();
        getViewTreeObserver().removeOnGlobalLayoutListener(mExperimentalButtonLayoutListener);
    }

    /**
     * Runs an animation that fades in the experimental button while shortening the location bar
     * background.
     */
    private void runShowExperimentalButtonAnimation() {
        if (mExperimentalButtonAnimationRunning) mExperimentalButtonAnimator.end();

        List<Animator> animators = new ArrayList<>();

        mLocBarWidthChangePercent = 1.f;
        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mLocBarWidthChangePercentProperty, 0.f);
        widthChangeAnimator.setDuration(LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(widthChangeAnimator);

        mExperimentalButton.setAlpha(0.f);
        ObjectAnimator buttonAnimator =
                ObjectAnimator.ofFloat(mExperimentalButton, View.ALPHA, 1.f);
        buttonAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        buttonAnimator.setStartDelay(EXPERIMENTAL_ICON_ANIMATION_DELAY_MS);
        buttonAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonAnimator);

        mExperimentalButton.setTranslationX(mExperimentalButtonTranslation);
        ObjectAnimator buttonTranslationAnimator =
                ObjectAnimator.ofFloat(mExperimentalButton, View.TRANSLATION_X, 0);
        buttonTranslationAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        buttonTranslationAnimator.setStartDelay(EXPERIMENTAL_ICON_ANIMATION_DELAY_MS);
        buttonTranslationAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonTranslationAnimator);

        mExperimentalButtonAnimator = new AnimatorSet();
        mExperimentalButtonAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mDisableLocationBarRelayout = true;
                mExperimentalButtonAnimationRunning = true;
                mExperimentalButton.setVisibility(View.VISIBLE);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                onExperimentalButtonAnimationEnd();
                mDisableLocationBarRelayout = false;
                mExperimentalButtonAnimationRunning = false;
                getViewTreeObserver().addOnGlobalLayoutListener(mExperimentalButtonLayoutListener);
                requestLayout();
            }
        });
        mExperimentalButtonAnimator.playTogether(animators);
        mExperimentalButtonAnimator.start();
    }

    /**
     * Runs an animation that fades out the experimental button while lengthening the location bar
     * background.
     */
    private void runHideExperimentalButtonsAnimators() {
        if (mExperimentalButtonAnimationRunning) mExperimentalButtonAnimator.end();

        List<Animator> animators = new ArrayList<>();

        mLocBarWidthChangePercent = 0.f;
        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mLocBarWidthChangePercentProperty, 1.f);
        widthChangeAnimator.setDuration(LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(widthChangeAnimator);

        mExperimentalButton.setAlpha(1.f);
        ObjectAnimator buttonAnimator =
                ObjectAnimator.ofFloat(mExperimentalButton, View.ALPHA, 0.f);
        buttonAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        buttonAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonAnimator);

        mExperimentalButton.setTranslationX(0);
        ObjectAnimator buttonTranslationAnimator = ObjectAnimator.ofFloat(
                mExperimentalButton, View.TRANSLATION_X, mExperimentalButtonTranslation);
        buttonTranslationAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        buttonTranslationAnimator.setDuration(EXPERIMENTAL_ICON_ANIMATION_DURATION_MS);
        animators.add(buttonTranslationAnimator);

        mExperimentalButtonAnimator = new AnimatorSet();
        mExperimentalButtonAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mLayoutLocationBarWithoutExtraButton = true;
                mExperimentalButtonAnimationRunning = true;
                requestLayout();
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                onExperimentalButtonAnimationEnd();
                mExperimentalButton.setVisibility(View.GONE);
                mLayoutLocationBarWithoutExtraButton = false;
                mExperimentalButtonAnimationRunning = false;
                getViewTreeObserver().addOnGlobalLayoutListener(mExperimentalButtonLayoutListener);
            }
        });
        mExperimentalButtonAnimator.playTogether(animators);
        mExperimentalButtonAnimator.start();
    }

    /**
     * Resets the alpha and translation X for all views affected by the animations for showing or
     * hiding buttons.
     */
    private void onExperimentalButtonAnimationEnd() {
        mExperimentalButtonAnimator = null;
        mExperimentalButton.setAlpha(1.f);
        mExperimentalButton.setTranslationX(0);
    }

    @VisibleForTesting
    public View getExperimentalButtonForTesting() {
        return mExperimentalButton;
    }

    @VisibleForTesting
    public void endExperimentalButtonAnimationForTesting() {
        if (mExperimentalButtonAnimator != null) mExperimentalButtonAnimator.end();
    }

    private void setTabSwitcherAnimationMenuDrawable() {
        mTabSwitcherAnimationMenuDrawable =
                ApiCompatibilityUtils
                        .getDrawable(getResources(), R.drawable.ic_more_vert_black_24dp)
                        .mutate();
        ((BitmapDrawable) mTabSwitcherAnimationMenuDrawable).setGravity(Gravity.CENTER);
    }

    private void setTabSwitcherAnimationMenuBadgeDrawable() {
        Drawable darkDrawable =
                UpdateMenuItemHelper.getInstance().getDarkBadgeDrawable(this.getResources());
        Drawable lightDrawable =
                UpdateMenuItemHelper.getInstance().getLightBadgeDrawable(this.getResources());
        if (darkDrawable == null || lightDrawable == null) return;

        mTabSwitcherAnimationMenuBadgeDarkDrawable = darkDrawable;
        mTabSwitcherAnimationMenuBadgeDarkDrawable.mutate();
        ((BitmapDrawable) mTabSwitcherAnimationMenuBadgeDarkDrawable).setGravity(Gravity.CENTER);

        mTabSwitcherAnimationMenuBadgeLightDrawable = lightDrawable;
        mTabSwitcherAnimationMenuBadgeLightDrawable.mutate();
        ((BitmapDrawable) mTabSwitcherAnimationMenuBadgeLightDrawable).setGravity(Gravity.CENTER);
    }

    private void setUseLightDrawablesForTextureCapture() {
        int currentPrimaryColor = getToolbarDataProvider().getPrimaryColor();
        mUseLightDrawablesForTextureCapture =
                isIncognito()
                || (currentPrimaryColor != 0
                           && ColorUtils.shouldUseLightForegroundOnBackground(currentPrimaryColor));
    }

    /**
     * Custom drawable that allows sharing the NTP search box drawable between the toolbar and the
     * NTP.  This allows animations to continue as the drawable is switched between the two owning
     * views.
     */
    private static class NtpSearchBoxDrawable extends DrawableWrapper {
        private final Drawable.Callback mCallback;

        private int mBoundsLeft, mBoundsTop, mBoundsRight, mBoundsBottom;
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
}
