// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;

import static org.chromium.base.MathUtils.interpolate;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.SHARE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.UNKNOWN;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.Pair;
import android.util.TypedValue;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.Dimension;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsIntent.CloseButtonPosition;
import androidx.browser.customtabs.ExperimentalOpenInBrowser;
import androidx.core.content.ContextCompat;
import androidx.core.view.MarginLayoutParamsCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CustomTabsButtonState;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.CustomTabDimensionUtils;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingDelegate;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayCoordinator;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabSideSheetStrategy.MaximizeButtonCallback;
import org.chromium.chrome.browser.customtabs.features.toolbar.ButtonVisibilityRule.ButtonId;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.PageInfoIphController;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

/** The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs. */
@NullMarked
public class CustomTabToolbar extends ToolbarLayout implements View.OnLongClickListener {
    private static final String TAG = "CctToolbar";
    private static final Object ORIGIN_SPAN = new Object();
    private ImageView mIncognitoImageView;
    private @Nullable LinearLayout mCustomActionButtons;
    private ImageButton mCloseButton;
    private @Nullable ImageButton mMinimizeButton;
    private MenuButton mMenuButton;
    // This View will be non-null only for bottom sheet custom tabs.
    private @Nullable Drawable mHandleDrawable;

    // Color scheme and tint that will be applied to icons and text.
    private @BrandedColorScheme int mBrandedColorScheme;
    private ColorStateList mTint;

    private @Nullable ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private @Nullable GURL mFirstUrl;

    private final CustomTabLocationBar mLocationBar = new CustomTabLocationBar();
    private @MonotonicNonNull LocationBarModel mLocationBarModel;
    private @MonotonicNonNull BrowserStateBrowserControlsVisibilityDelegate
            mBrowserControlsVisibilityDelegate;
    private @Nullable CustomTabCaptureStateToken mLastCustomTabCaptureStateToken;
    private final ObserverList<Callback<Integer>> mContainerVisibilityChangeObserverList =
            new ObserverList<>();
    private final boolean mIsRtl;
    private final OneshotSupplierImpl<Boolean> mOptionalButtonVisibilitySupplier =
            new OneshotSupplierImpl<>();

    // Whether the maximization button should be shown when it can. Set to {@code true}
    // while the side sheet is running with the maximize button option on.
    private boolean mMaximizeButtonEnabled;
    private boolean mMinimizeButtonEnabled;

    private @Nullable CookieControlsBridge mCookieControlsBridge;
    private boolean mShouldHighlightCookieControlsIcon;
    private int mBlockingStatus3pcd;
    private @MonotonicNonNull BrowserServicesIntentDataProvider mIntentDataProvider;

    @SuppressWarnings("NullAway")
    private Supplier<AppMenuHandler> mAppMenuHandler = () -> null;

    private @Nullable AppMenuObserver mAppMenuObserver;
    private @MonotonicNonNull Activity mActivity;

    private final Handler mTaskHandler = new Handler();
    private final ButtonVisibilityRule mButtonVisibilityRule =
            new ButtonVisibilityRule(
                    getResources().getDimensionPixelSize(R.dimen.location_bar_min_url_width),
                    !ChromeFeatureList.sCctToolbarRefactor.isEnabled());

    // The resource ID of the most recently set security icon. Used for testing since
    // VectorDrawables can't be straightforwardly tested for equality..
    private int mSecurityIconResourceForTesting;

    // region CCTToolbarRefactor

    private int mToolbarWidth;
    private @Nullable FrameLayout mCustomButtonsParent; // TODO(crbug.com/402213312): Non-null?
    private @Nullable ImageButton mSideSheetMaximizeButton;
    private @Nullable View mOptionalButton;

    /** Listener interface to be notified when the toolbar is measured with a new width. */
    public interface OnNewWidthMeasuredListener {
        void onNewWidthMeasured(int width);
    }

    private @Nullable OnNewWidthMeasuredListener mOnNewWidthMeasuredListener;

    /** Observer interface to be notified when the toolbar color scheme changes. */
    public interface OnColorSchemeChangedObserver {
        /**
         * Called when the toolbar color scheme changes.
         *
         * @param toolbarColor The new toolbar color.
         * @param colorScheme The {@link BrandedColorScheme}.
         */
        void onColorSchemeChanged(@ColorInt int toolbarColor, @BrandedColorScheme int colorScheme);
    }

    private @Nullable OnColorSchemeChangedObserver mOnColorSchemeChangedObserver;

    public static final class OmniboxParams {
        /** The {@link SearchActivityClient} instance used to request Omnibox. */
        public SearchActivityClient searchClient;

        /** The package name of the Custom Tabs embedder. */
        public String clientPackageName;

        /** A handler for taps on the omnibox, or null if the default handler should be used. */
        public @Nullable Consumer<Tab> tapHandler;

        /**
         * A handler for taps on the omnibox. The function returns true if the tap was handled,
         * false otherwise.
         */
        public Function<Tab, Boolean> tapHandlerWithVerification;

        public OmniboxParams(
                SearchActivityClient searchClient,
                String clientPackageName,
                @Nullable Consumer<Tab> tapHandler,
                Function<Tab, Boolean> tapHandlerWithVerification) {
            this.searchClient = searchClient;
            this.clientPackageName = clientPackageName;
            this.tapHandler = tapHandler;
            this.tapHandlerWithVerification = tapHandlerWithVerification;
        }
    }

    // endregion

    /** Whether to use the toolbar as handle to resize the Window height. */
    public interface HandleStrategy {
        /**
         * Decide whether we need to intercept the touch events so the events will be passed to the
         * {@link #onTouchEvent()} method.
         *
         * @param event The touch event to be examined.
         * @return whether the event will be passed to {@link #onTouchEvent()}.
         */
        boolean onInterceptTouchEvent(MotionEvent event);

        /**
         * Handling the touch events.
         *
         * @param event The touch event to be handled.
         * @return whether the event is consumed..
         */
        boolean onTouchEvent(MotionEvent event);

        /**
         * Set a handler to close the current tab.
         *
         * @param handler The handler for closing the current tab.
         */
        void setCloseClickHandler(Runnable handler);
    }

    private @Nullable HandleStrategy mHandleStrategy;
    private @CloseButtonPosition int mCloseButtonPosition;
    private @AdaptiveToolbarButtonVariant int mVariantForFallbackMenu;

    private final List<Integer> mCustomButtonsForMetric = new ArrayList<>();
    private int mOptionalButtonForMetric = UNKNOWN;

    // Used to record which buttons are shown in top toolbar.
    // LINT.IfChange(CctActions)
    @IntDef({
        CctActions.INVALID,
        CctActions.NONE,
        CctActions.SHARE_OIB,
        CctActions.SHARE_CUSTOM,
        CctActions.SHARE_ONLY,
        CctActions.SHARE_MTB,
        CctActions.OIB_CUSTOM,
        CctActions.OIB_ONLY,
        CctActions.OIB_MTB,
        CctActions.CUSTOM_ONLY,
        CctActions.CUSTOM_MTB,
        CctActions.MTB_ONLY,
        CctActions.MAX_VALUE,
    })
    @interface CctActions {
        int INVALID = -1;
        int NONE = 0;
        int SHARE_OIB = 1;
        int SHARE_CUSTOM = 2;
        int SHARE_ONLY = 3;
        int SHARE_MTB = 4;
        int OIB_CUSTOM = 5;
        int OIB_ONLY = 6;
        int OIB_MTB = 7;
        int CUSTOM_ONLY = 8;
        int CUSTOM_MTB = 9;
        int MTB_ONLY = 10;
        int MAX_VALUE = MTB_ONLY;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/custom_tabs/enums.xml:CustomTabsToolbarButtons)

    /** Constructor for getting this class inflated from an xml layout file. */
    public CustomTabToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTint = ChromeColors.getPrimaryIconTint(getContext(), false);
        mIsRtl =
                getResources().getConfiguration().getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        final int backgroundColor =
                ChromeColors.getDefaultThemeColor(getContext(), /* isIncognito= */ false);
        setBackground(new ColorDrawable(backgroundColor));
        mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;

        mIncognitoImageView = findViewById(R.id.incognito_cct_logo_image_view);
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) {
            mCustomButtonsParent = findViewById(R.id.action_buttons);
        } else {
            mCustomActionButtons = findViewById(R.id.action_buttons);
        }
        mCloseButton = findViewById(R.id.close_button);
        if (mCloseButton != null) {
            mCloseButton.setOnLongClickListener(this);
        }
        mMenuButton = findViewById(R.id.menu_button_wrapper);
        mButtonVisibilityRule.addButton(ButtonId.MENU, findViewById(R.id.menu_button), true);
        mLocationBar.onFinishInflate(this);

        maybeInitMinimizeButton();
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        mLocationBar.onNativeLibraryReady();
    }

    /** Returns the incognito image view. */
    @Nullable ImageView getIncognitoImageView() {
        return mIncognitoImageView;
    }

    /** Returns the incognito image view, inflating it first if necessary. */
    ImageView ensureIncognitoImageViewInflated() {
        if (mIncognitoImageView != null) {
            return mIncognitoImageView;
        }

        ViewStub stub = findViewById(R.id.incognito_icon_stub);
        mIncognitoImageView = (ImageView) stub.inflate();
        return mIncognitoImageView;
    }

    /** Returns the close button. */
    @Nullable ImageButton getCloseButton() {
        return mCloseButton;
    }

    /** Returns the close button, inflating and/or making it visible first if necessary. */
    ImageButton ensureCloseButtonInflated() {
        if (mCloseButton != null) {
            mCloseButton.setVisibility(VISIBLE);
            return mCloseButton;
        }

        LayoutInflater.from(getContext()).inflate(R.layout.custom_tab_close_button, this, true);
        mCloseButton = findViewById(R.id.close_button);
        return mCloseButton;
    }

    /** Returns the menu button. */
    @Nullable MenuButton getMenuButton() {
        return mMenuButton;
    }

    /** Returns the menu button, inflating and/or making it visible first if necessary. */
    MenuButton ensureMenuButtonInflated() {
        if (mMenuButton != null) {
            mMenuButton.setVisibility(VISIBLE);
            return mMenuButton;
        }

        LayoutInflater.from(getContext()).inflate(R.layout.custom_tab_menu_button, this, true);
        mMenuButton = findViewById(R.id.menu_button_wrapper);
        return mMenuButton;
    }

    /** Returns the minimize button. */
    @Nullable ImageButton getMinimizeButton() {
        return mMinimizeButton;
    }

    /** Returns the minimize button, inflating and/or making it visible first if necessary. */
    ImageButton ensureMinimizeButtonInflated() {
        if (mMinimizeButton != null) {
            mMinimizeButton.setVisibility(VISIBLE);
            return mMinimizeButton;
        }

        LayoutInflater.from(getContext()).inflate(R.layout.custom_tabs_minimize_button, this, true);
        mMinimizeButton = findViewById(R.id.custom_tabs_minimize_button);
        return mMinimizeButton;
    }

    /** Returns the side-sheet maximize button. */
    @Nullable ImageButton getSideSheetMaximizeButton() {
        return mSideSheetMaximizeButton;
    }

    /**
     * Returns the side-sheet maximize button, inflating and/or making it visible first if
     * necessary.
     */
    @EnsuresNonNull("mSideSheetMaximizeButton")
    ImageButton ensureSideSheetMaximizeButtonInflated() {
        if (mSideSheetMaximizeButton != null) {
            mSideSheetMaximizeButton.setVisibility(VISIBLE);
            return mSideSheetMaximizeButton;
        }

        LayoutInflater.from(getContext())
                .inflate(R.layout.custom_tabs_sidepanel_maximize, this, true);
        mSideSheetMaximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        return mSideSheetMaximizeButton;
    }

    /** Returns the parent view for the custom action buttons. */
    @Nullable FrameLayout getCustomActionButtonsParent() {
        return mCustomButtonsParent;
    }

    /** Returns the optional button, inflating it first if necessary. */
    View ensureOptionalButtonInflated() {
        if (mOptionalButton == null) {
            LayoutInflater.from(getContext())
                    .inflate(R.layout.optional_button_layout, mCustomButtonsParent, true);
            mOptionalButton = findViewById(R.id.optional_button);
            var lp = (FrameLayout.LayoutParams) mOptionalButton.getLayoutParams();
            lp.width = getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
            mOptionalButton.setLayoutParams(lp);
        }
        return mOptionalButton;
    }

    @Nullable View getOptionalButton() {
        return mOptionalButton;
    }

    // TODO(crbug.com/428261559): Delete this after the refactoring.
    public void setAppMenuHandler(Supplier<AppMenuHandler> appMenuHandler) {
        mAppMenuHandler = appMenuHandler;
    }

    /**
     * Display menu dot indicating there is a menu item available in place of the hidden contextual
     * page action button. TODO(crbug.com/428261559): Move to CustomTabToolbarButton MVC
     *
     * @param buttonVariant Type of the action button.
     */
    public void setUpOptionalButtonFallbackUi(@AdaptiveToolbarButtonVariant int buttonVariant) {
        View indicator = mMenuButton.findViewById(R.id.menu_dot);
        boolean show =
                buttonVariant == AdaptiveToolbarButtonVariant.PRICE_TRACKING
                        || buttonVariant == AdaptiveToolbarButtonVariant.PRICE_INSIGHTS
                        || (buttonVariant == AdaptiveToolbarButtonVariant.READER_MODE
                                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                        ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                                        ReaderModeManager.CPA_FALLBACK_MENU_PARAM,
                                        false));
        Log.i(TAG, "fallback-ui-variant: " + buttonVariant + " show: " + show);
        if (!show) {
            resetOptionalButtonState();
            return;
        }
        indicator.setVisibility(View.VISIBLE);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.AdaptiveToolbarButton.FallbackIndicator.Shown",
                buttonVariant,
                AdaptiveToolbarButtonVariant.MAX_VALUE);
        var lp = (MarginLayoutParams) indicator.getLayoutParams();
        int topMargin = getDimensionPx(R.dimen.custom_tabs_toolbar_menu_dot_top_margin);
        int endMargin = getDimensionPx(R.dimen.custom_tabs_toolbar_menu_dot_end_margin);
        // THe parent view of the dot may have a top/end padding that could keep the dot
        // from being positioned where we want. Use a negative margin in such case.
        lp.topMargin = -mMenuButton.getPaddingTop() + topMargin;
        lp.setMarginEnd(-mMenuButton.getPaddingEnd() + endMargin);
        indicator.setLayoutParams(lp);

        addFallbackMenuItem(buttonVariant);
    }

    /**
     * Record the metric indicating that user clicked the fallback dot when it's shown on the
     * overflow menu icon.
     */
    private void maybeRecordCpaFallbackIndicatorClicked() {
        View indicator = mMenuButton.findViewById(R.id.menu_dot);
        if (indicator.getVisibility() != View.VISIBLE) return;

        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.AdaptiveToolbarButton.FallbackIndicator.Clicked",
                mVariantForFallbackMenu,
                AdaptiveToolbarButtonVariant.MAX_VALUE);
    }

    /**
     * Sets up the menu item that can be used as a fallback to the hidden contextual page action
     * button. TODO(crbug.com/428261559): Move to CustomTabToolbarButton MVC
     *
     * @param buttonVariant Type of the action button.
     */
    private void addFallbackMenuItem(@AdaptiveToolbarButtonVariant int buttonVariant) {
        mVariantForFallbackMenu = buttonVariant;
        var menuInfo = getHighlightMenuInfo(buttonVariant);
        if (menuInfo == null) return; // PriceTracking might not be enabled for showing.

        int menuId = menuInfo.first;

        mAppMenuHandler.get().setMenuHighlight(menuId, false);
        View menuIcon = mMenuButton.findViewById(R.id.menu_button);
        menuIcon.setContentDescription(
                getContext().getString(R.string.accessibility_custom_tab_menu_with_dot));
        if (mAppMenuObserver != null) mAppMenuHandler.get().removeObserver(mAppMenuObserver);
        mAppMenuObserver =
                new AppMenuObserver() {
                    @Override
                    public void onMenuVisibilityChanged(boolean isVisible) {
                        // TODO(crbug.com/424807997): Do this toggling in MenuButton MVC.
                        if (isVisible) {
                            maybeRecordCpaFallbackIndicatorClicked();
                            mLocationBar.resetOptionalButtonState(/* resetFallbackMenu= */ false);
                            String menuTitle = getContext().getString(menuInfo.second);
                            int textId = R.string.accessibility_custom_tab_menu_item_highlight;
                            String highlightedMenu = getContext().getString(textId, menuTitle);
                            mAppMenuHandler.get().setContentDescription(highlightedMenu);
                        }
                    }

                    @Override
                    public void onMenuHighlightChanged(boolean highlighting) {}
                };
        mAppMenuHandler.get().addObserver(mAppMenuObserver);
    }

    private @Nullable Pair<Integer, Integer> getHighlightMenuInfo(
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        return switch (buttonVariant) {
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING -> {
                // Figure out which of the two menu items (enable/disable) appears and needs
                // highlighting.
                // TODO(crbug.com/424807997): Avoid casting.
                var appMenuDelegate =
                        (AppMenuPropertiesDelegateImpl)
                                mAppMenuHandler.get().getMenuPropertiesDelegate();
                var showEnabled = appMenuDelegate.getPriceTrackingMenuItemInfo(getCurrentTab());
                if (showEnabled == null) yield null;
                yield showEnabled
                        ? Pair.create(
                                R.id.enable_price_tracking_menu_id,
                                R.string.enable_price_tracking_menu_item)
                        : Pair.create(
                                R.id.disable_price_tracking_menu_id,
                                R.string.disable_price_tracking_menu_item);
            }
            case AdaptiveToolbarButtonVariant.PRICE_INSIGHTS ->
                    Pair.create(R.id.price_insights_menu_id, R.string.price_insights_title);
            case AdaptiveToolbarButtonVariant.READER_MODE ->
                    Pair.create(R.id.reader_mode_menu_id, R.string.show_reading_mode_text);
            default -> null;
        };
    }

    // Modify the inset of the optional background drawable to match that of the icon secondary
    // background.
    public void setOptionalButtonBackgroundInset() {
        View optionalButton = findViewById(R.id.optional_toolbar_button);
        LayerDrawable backgroundDrawable = (LayerDrawable) optionalButton.getBackground();
        int height = getDimensionPixelSize(R.dimen.custom_tabs_adaptive_button_bg_height);
        int padding =
                getDimensionPixelSize(R.dimen.custom_tabs_adaptive_button_bg_horizontal_padding);
        backgroundDrawable.setLayerHeight(/* index= */ 0, height);
        backgroundDrawable.setLayerInset(/* index= */ 0, padding, /* t= */ 0, padding, /* b= */ 0);
    }

    private int getDimensionPixelSize(@DimenRes int dimenId) {
        return getResources().getDimensionPixelSize(dimenId);
    }

    /**
     * Sets an {@link OnNewWidthMeasuredListener}.
     *
     * @param listener The {@link OnNewWidthMeasuredListener}. A null value clears the listener.
     */
    public void setOnNewWidthMeasuredListener(@Nullable OnNewWidthMeasuredListener listener) {
        mOnNewWidthMeasuredListener = listener;
    }

    /**
     * Sets an {@link OnColorSchemeChangedObserver}.
     *
     * @param observer The {@link OnColorSchemeChangedObserver}. A null value clears the observer.
     */
    public void setOnColorSchemeChangedObserver(@Nullable OnColorSchemeChangedObserver observer) {
        mOnColorSchemeChangedObserver = observer;
    }

    private void notifyColorSchemeChanged() {
        if (mOnColorSchemeChangedObserver != null) {
            mOnColorSchemeChangedObserver.onColorSchemeChanged(
                    getBackground().getColor(), mBrandedColorScheme);
        }
    }

    /**
     * Initialize the toolbar with menu.
     *
     * @param activity The {@link Activity} that the toolbar is attached to.
     * @param appMenuHandler Supplier of {@link AppMenuHandler}.
     * @param intentDataProvider {@link BrowserServicesIntentDataProvider} for accessing CCT intent
     *     data.
     */
    @ExperimentalOpenInBrowser
    public void initVisibilityRule(
            Activity activity,
            Supplier<AppMenuHandler> appMenuHandler,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mActivity = activity;
        mAppMenuHandler = appMenuHandler;
        if (mIntentDataProvider == null) {
            mIntentDataProvider = intentDataProvider;
            @CustomTabsButtonState
            int shareState =
                    switch (intentDataProvider.getShareButtonState()) {
                        case CustomTabsIntent.SHARE_STATE_OFF -> CustomTabsButtonState
                                .BUTTON_STATE_OFF;
                        case CustomTabsIntent.SHARE_STATE_DEFAULT -> CustomTabsButtonState
                                .BUTTON_STATE_DEFAULT;
                        case CustomTabsIntent.SHARE_STATE_ON -> CustomTabsButtonState
                                .BUTTON_STATE_ON;
                        default -> CustomTabsButtonState.BUTTON_STATE_DEFAULT;
                    };
            @CustomTabsButtonState
            int oibState =
                    switch (intentDataProvider.getOpenInBrowserButtonState()) {
                        case CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF -> CustomTabsButtonState
                                .BUTTON_STATE_OFF;
                        case CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT -> CustomTabsButtonState
                                .BUTTON_STATE_DEFAULT;
                        case CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON -> CustomTabsButtonState
                                .BUTTON_STATE_ON;
                        default -> CustomTabsButtonState.BUTTON_STATE_DEFAULT;
                    };
            mButtonVisibilityRule.setCustomButtonState(shareState, oibState);
        }
        mButtonVisibilityRule.setToolbarWidth(
                CustomTabDimensionUtils.getInitialWidth(activity, intentDataProvider));
    }

    @Override
    protected void setCustomActionsVisibility(boolean isVisible) {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;

        int visibility = isVisible ? View.VISIBLE : View.GONE;
        if (visibility == assumeNonNull(mCustomActionButtons).getVisibility()) return;

        mCustomActionButtons.setVisibility(visibility);
    }

    private static void setHorizontalPadding(View view, @Px int startPadding, @Px int endPadding) {
        view.setPaddingRelative(
                startPadding, view.getPaddingTop(), endPadding, view.getPaddingBottom());
    }

    @Override
    protected void setCloseButtonImageResource(@Nullable Drawable drawable) {
        mCloseButton.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
        mCloseButton.setImageDrawable(drawable);
        if (drawable != null) {
            updateButtonTint(mCloseButton);
        }
        mButtonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, drawable != null);
    }

    @Override
    protected void setCustomTabCloseClickHandler(@Nullable OnClickListener listener) {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;

        assert listener != null;
        mCloseButton.setOnClickListener(listener);
    }

    @Override
    protected void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener, @ButtonType int type) {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;

        // TODO: Update action buttons in the refactored toolbar too.
        mCustomButtonsForMetric.add(type);
        ImageButton button =
                (ImageButton)
                        LayoutInflater.from(getContext())
                                .inflate(
                                        R.layout.custom_tabs_toolbar_button,
                                        assumeNonNull(mCustomActionButtons),
                                        false);
        button.setOnLongClickListener(this);
        button.setOnClickListener(listener);
        button.setVisibility(VISIBLE);

        updateCustomActionButtonVisuals(button, drawable, description);
        int buttonWidth = getDimensionPx(R.dimen.toolbar_button_width);
        button.setLayoutParams(new ViewGroup.LayoutParams(buttonWidth, LayoutParams.MATCH_PARENT));
        int index = mCustomActionButtons.getChildCount() < 2 ? 0 : 1;
        mButtonVisibilityRule.addButtonForCustomAction(
                index == 0 ? ButtonId.CUSTOM_1 : ButtonId.CUSTOM_2, button, true, type);
        if (index == 1) {
            // The 2nd custom button disables optional button.
            mButtonVisibilityRule.removeButton(ButtonId.MTB);
        }
        @Dimension int paddingHoriz;
        paddingHoriz = getDimensionPx(R.dimen.custom_tabs_toolbar_button_horizontal_padding);
        button.setPaddingRelative(paddingHoriz, /* top= */ 0, paddingHoriz, /* bottom= */ 0);

        // Add the view at the beginning of the child list.
        mCustomActionButtons.addView(button, 0);
    }

    private @Dimension int getDimensionPx(@DimenRes int resId) {
        return getResources().getDimensionPixelSize(resId);
    }

    @Override
    protected void updateCustomActionButton(int index, Drawable drawable, String description) {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;

        // |index| -> childIndex should ignore the optional button always present at the end.
        int childIndex = assumeNonNull(mCustomActionButtons).getChildCount() - 2 - index;
        assert 0 <= childIndex && childIndex <= mCustomActionButtons.getChildCount() - 2;
        ImageButton button = (ImageButton) mCustomActionButtons.getChildAt(childIndex);
        assert button != null;
        updateCustomActionButtonVisuals(button, drawable, description);
        @Dimension int paddingHoriz;
        paddingHoriz = getDimensionPx(R.dimen.custom_tabs_toolbar_button_horizontal_padding);
        button.setPaddingRelative(paddingHoriz, /* top= */ 0, paddingHoriz, /* bottom= */ 0);
    }

    /**
     * Creates and returns a CustomTab-specific LocationBar. This also retains a reference to the
     * passed LocationBarModel.
     * @param locationBarModel {@link LocationBarModel} to be used for accessing LocationBar
     *         state.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @param modalDialogManagerSupplier Supplier of {@link ModalDialogManager}.
     * @param ephemeralTabCoordinatorSupplier Supplier of {@link EphemeralTabCoordinator}.
     * @param controlsVisibilityDelegate {@link BrowserStateBrowserControlsVisibilityDelegate} to
     *         show / hide the browser control. Used to ensure toolbar is shown for a certain
     *         duration.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     * @return The LocationBar implementation for this CustomTabToolbar.
     */
    public LocationBar createLocationBar(
            LocationBarModel locationBarModel,
            ActionMode.Callback actionModeCallback,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            TabCreator tabCreator) {
        mLocationBarModel = locationBarModel;
        mLocationBar.init(
                locationBarModel,
                modalDialogManagerSupplier,
                ephemeralTabCoordinatorSupplier,
                tabCreator,
                actionModeCallback);
        mBrowserControlsVisibilityDelegate = controlsVisibilityDelegate;
        return mLocationBar;
    }

    /**
     * Initialize the maximize button for side sheet CCT. Create one if not instantiated.
     *
     * @param maximizedOnInit {@code true} if the side sheet is starting in maximized state.
     */
    public void initSideSheetMaximizeButton(
            boolean maximizedOnInit, MaximizeButtonCallback callback) {
        assert !ChromeFeatureList.sCctToolbarRefactor.isEnabled();
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (maximizeButton == null) {
            ViewStub maximizeButtonStub = findViewById(R.id.maximize_button_stub);
            maximizeButtonStub.inflate();
            maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
            mButtonVisibilityRule.addButton(ButtonId.EXPAND, maximizeButton, true);
        }
        mMaximizeButtonEnabled = true;
        setMaximizeButtonDrawable(maximizedOnInit);
        maximizeButton.setOnClickListener((v) -> setMaximizeButtonDrawable(callback.onClick()));

        // The visibility will set after the location bar completes its layout. But there are
        // cases where the location bar layout gets already completed. Trigger the visibility
        // update manually here.
        setMaximizeButtonVisibility();
    }

    /**
     * Sets the {@link CustomTabMinimizeDelegate} to allow the toolbar to minimize the tab.
     *
     * @param delegate The {@link CustomTabMinimizeDelegate}.
     */
    public void setMinimizeDelegate(CustomTabMinimizeDelegate delegate) {
        assumeNonNull(mMinimizeButton).setOnClickListener(view -> delegate.minimize());
    }

    /**
     * Sets params for the interactive Omnibox in CCT.
     *
     * @param omniboxParams The {@link OmniboxParams} to be used.
     */
    public void setOmniboxParams(OmniboxParams omniboxParams) {
        mLocationBar.setOmniboxParams(omniboxParams);
    }

    private void setButtonsVisibility() {
        setMaximizeButtonVisibility();
        setMinimizeButtonVisibility();
    }

    private void setMaximizeButtonVisibility() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;

        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (!mMaximizeButtonEnabled || maximizeButton == null) {
            if (maximizeButton != null) maximizeButton.setVisibility(View.GONE);
            mButtonVisibilityRule.update(ButtonId.EXPAND, false);
            setUrlTitleBarMargin(0);
            return;
        }
        // Find the title/url width threshold that turns the maximize button visible.
        int containerWidthPx = mLocationBar.mTitleUrlContainer.getWidth();
        if (containerWidthPx == 0) return;
        mButtonVisibilityRule.refresh();
        if (maximizeButton.getVisibility() == View.VISIBLE) {
            mLocationBar.removeButtonsVisibilityUpdater();
            int maximizeButtonWidthPx =
                    getResources().getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
            setUrlTitleBarMargin(maximizeButtonWidthPx);
        }
    }

    private void setUrlTitleBarMargin(int margin) {
        setViewRightMargin(mLocationBar.mTitleBar, margin);
        setViewRightMargin(mLocationBar.mUrlBar, margin);
    }

    private static void setViewRightMargin(View view, int margin) {
        if (view == null) return;
        var lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        if (lp != null && lp.rightMargin != margin) {
            lp.rightMargin = margin;
            view.setLayoutParams(lp);
        }
    }

    private void setMaximizeButtonDrawable(boolean maximized) {
        @DrawableRes
        int drawableId = maximized ? R.drawable.ic_fullscreen_exit : R.drawable.ic_fullscreen_enter;
        int buttonDescId =
                maximized
                        ? R.string.custom_tab_side_sheet_minimize
                        : R.string.custom_tab_side_sheet_maximize;
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        var d = UiUtils.getTintedDrawable(getContext(), drawableId, mTint);
        updateCustomActionButtonVisuals(maximizeButton, d, getResources().getString(buttonDescId));
    }

    /** Remove maximize button from side sheet CCT toolbar. */
    public void removeSideSheetMaximizeButton() {
        assert !ChromeFeatureList.sCctToolbarRefactor.isEnabled();
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        mMaximizeButtonEnabled = false;
        if (maximizeButton == null) return; // Toolbar could be already destroyed.

        maximizeButton.setOnClickListener(null);
        maximizeButton.setVisibility(View.GONE);
    }

    /**
     * Inflates and prepares the minimize button if it should be enabled, when CCTToolbarRefactor is
     * disabled.
     */
    @VisibleForTesting
    void maybeInitMinimizeButton() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;
        if (!MinimizedFeatureUtils.isMinimizedCustomTabAvailable(getContext())) {
            return;
        }

        ViewStub minimizeButtonStub = findViewById(R.id.minimize_button_stub);
        if (minimizeButtonStub != null) {
            minimizeButtonStub.inflate();
        }
        mMinimizeButton = findViewById(R.id.custom_tabs_minimize_button);
        var d = UiUtils.getTintedDrawable(getContext(), R.drawable.ic_minimize, mTint);
        mMinimizeButton.setTag(R.id.custom_tabs_toolbar_tintable, true);
        mMinimizeButton.setImageDrawable(d);
        updateButtonTint(mMinimizeButton);
        mMinimizeButton.setOnLongClickListener(this);
        mMinimizeButtonEnabled = true;
        mButtonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
    }

    private void setMinimizeButtonVisibility() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;
        if (mMinimizeButton == null) return;

        if (!mMinimizeButtonEnabled || isInMultiWindowMode()) {
            if (mMinimizeButton.getVisibility() != View.GONE) {
                mMinimizeButton.setVisibility(View.GONE);
                mButtonVisibilityRule.update(ButtonId.MINIMIZE, false);
                maybeAdjustButtonSpacingForCloseButtonPosition();
            }
            return;
        } else if (!mButtonVisibilityRule.isSuppressed(ButtonId.MINIMIZE)
                && mMinimizeButton.getVisibility() == View.GONE) {
            mMinimizeButton.setVisibility(View.VISIBLE);
            mButtonVisibilityRule.update(ButtonId.MINIMIZE, true);
        }
        updateToolbarLayoutMargin();
    }

    private boolean isInMultiWindowMode() {
        Activity activity = getActivityFromCurrentTab();
        if (activity == null) return false;
        return !activity.isInPictureInPictureMode()
                && MultiWindowUtils.getInstance().isInMultiWindowMode(activity);
    }

    /** Returns {@link OneshotSupplier} indicating if the optional button will be visible. */
    public OneshotSupplier<Boolean> getShowOptionalButton() {
        // If any of the following is already known, set the visibility ahead. Otherwise it will be
        // determined the first time its visibility is examined in #initializeOptionalButton:
        // - if we already have 2 dev buttons
        var optionalButtonVisibility = mOptionalButtonVisibilitySupplier.get();
        if (optionalButtonVisibility == null && hasMultipleDevButtons()) {
            mOptionalButtonVisibilitySupplier.set(false);
        }
        return mOptionalButtonVisibilitySupplier;
    }

    private boolean hasMultipleDevButtons() {
        // Dev button + optional button (view stub).
        return assumeNonNull(mCustomActionButtons).getChildCount() > 2;
    }

    @Override
    protected void updateOptionalButton(ButtonData buttonData) {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;
        if (!assumeNonNull(mIntentDataProvider).isOptionalButtonSupported()) return;

        mLocationBar.updateOptionalButton(buttonData);
    }

    @Override
    protected void hideOptionalButton() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;
        if (!assumeNonNull(mIntentDataProvider).isOptionalButtonSupported()) return;

        mLocationBar.hideOptionalButton();
    }

    /** Resets optional button internal state. */
    public void resetOptionalButtonState() {
        mLocationBar.resetOptionalButtonState(/* resetFallbackMenu= */ true);
    }

    @Override
    public void requestKeyboardFocus() {
        setFocusOnFirstFocusableDescendant(this);
    }

    private void updateCustomActionButtonVisuals(
            ImageButton button, Drawable drawable, String description) {
        Resources resources = getResources();

        // The height will be scaled to match spec while keeping the aspect ratio, so get the scaled
        // width through that.
        int sourceHeight = drawable.getIntrinsicHeight();
        int sourceScaledHeight = resources.getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int sourceWidth = drawable.getIntrinsicWidth();
        int sourceScaledWidth = sourceWidth * sourceScaledHeight / sourceHeight;
        int minPadding = resources.getDimensionPixelSize(R.dimen.min_toolbar_icon_side_padding);

        int sidePadding = Math.max((2 * sourceScaledHeight - sourceScaledWidth) / 2, minPadding);
        int topPadding = button.getPaddingTop();
        int bottomPadding = button.getPaddingBottom();
        button.setPadding(sidePadding, topPadding, sidePadding, bottomPadding);
        button.setImageDrawable(drawable);
        updateButtonTint(button);

        button.setContentDescription(description);
    }

    public void setMinimizeButtonEnabled(boolean enabled) {
        mMinimizeButtonEnabled = enabled;
        mButtonVisibilityRule.update(ButtonId.MINIMIZE, enabled);
        setMinimizeButtonVisibility();
    }

    /**
     * @return The custom action button with the given {@code index}. For test purpose only.
     * @param index The index of the custom action button to return.
     */
    public @Nullable ImageButton getCustomActionButtonForTest(int index) {
        var parent =
                ChromeFeatureList.sCctToolbarRefactor.isEnabled()
                        ? mCustomButtonsParent
                        : mCustomActionButtons;
        View childView = assumeNonNull(parent).getChildAt(index);

        // The child could be ViewStub if not inflated. Returns null in such case as
        // it means there is no custom action button added to the container.
        return childView instanceof ImageButton button ? button : null;
    }

    /** Returns the number of custom action buttons. */
    public static int getCustomActionButtonCountForTesting(ViewGroup container) {
        int count = 0;
        for (int i = 0; i < container.getChildCount(); ++i) {
            View child = container.getChildAt(i);
            // Rule out invisible children that doesn't count toward the valid action button.
            count += child.getVisibility() != View.GONE ? 1 : 0;
        }
        return count;
    }

    public ImageButton getMaximizeButtonForTest() {
        return findViewById(R.id.custom_tabs_sidepanel_maximize);
    }

    @Override
    protected int getTabStripHeightFromResource() {
        return 0;
    }

    /** @return The current active {@link Tab}. */
    private @Nullable Tab getCurrentTab() {
        return getToolbarDataProvider().getTab();
    }

    @Override
    public void setUrlBarHidden(boolean hideUrlBar) {
        mLocationBar.setUrlBarHidden(hideUrlBar);
    }

    @Override
    protected void onNavigatedToDifferentPage() {
        super.onNavigatedToDifferentPage();
        assumeNonNull(mLocationBarModel).notifyTitleChanged();
        if (mLocationBar.isShowingTitleOnly()) {
            if (mFirstUrl == null || mFirstUrl.isEmpty()) {
                mFirstUrl = assumeNonNull(getCurrentTab()).getUrl();
            } else {
                if (mFirstUrl.equals(assumeNonNull(getCurrentTab()).getUrl())) return;
                setUrlBarHidden(false);
            }
        }
        mLocationBarModel.notifySecurityStateChanged();
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        if (mHandleStrategy != null) {
            return mHandleStrategy.onTouchEvent(event);
        }
        return false;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (mHandleStrategy != null) {
            return mHandleStrategy.onInterceptTouchEvent(event);
        }
        return false;
    }

    public void setHandleStrategy(HandleStrategy strategy) {
        mHandleStrategy = strategy;
        mHandleStrategy.setCloseClickHandler(mCloseButton::callOnClick);
    }

    /**
     * Sets the close button position for this toolbar.
     *
     * @param closeButtonPosition The {@link CloseButtonPosition}.
     */
    public void setCloseButtonPosition(@CloseButtonPosition int closeButtonPosition) {
        mCloseButtonPosition = closeButtonPosition;
    }

    private void updateButtonsTint() {
        // TODO(crbug.com/402213312): Remove tinting code here once it's fully MVC-ified.
        updateButtonTint(mCloseButton);
        if (mMinimizeButton != null) {
            updateButtonTint(mMinimizeButton);
        }
        ViewGroup actionButtons =
                ChromeFeatureList.sCctToolbarRefactor.isEnabled()
                        ? mCustomButtonsParent
                        : mCustomActionButtons;
        if (actionButtons != null) {
            int numCustomActionButtons = actionButtons.getChildCount();
            for (int i = 0; i < numCustomActionButtons; i++) {
                View actionButton = actionButtons.getChildAt(i);
                if (actionButton instanceof ImageButton button) {
                    updateButtonTint(button);
                }
            }
        }
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (maximizeButton != null) updateButtonTint(maximizeButton);
        updateButtonTint(mLocationBar.getSecurityButton());
        mLocationBar.updateOptionalButtonTint();
    }

    private void updateButtonTint(ImageButton button) {
        if (button == null) return;

        Drawable drawable = button.getDrawable();
        if (drawable instanceof TintedDrawable tintedDrawable) {
            tintedDrawable.setTint(mTint);
        } else if (button.getTag(R.id.custom_tabs_toolbar_tintable) != null) {
            drawable.setTintList(mTint);
        }
    }

    private void maybeSwapCloseAndMenuButtons() {
        if (mCloseButtonPosition != CLOSE_BUTTON_POSITION_END) return;

        final View closeButton = findViewById(R.id.close_button);
        final View menuButtonWrapper = findViewById(R.id.menu_button_wrapper);
        final int menuButtonIndex = indexOfChild(menuButtonWrapper);
        final var menuButtonLayoutParams =
                (FrameLayout.LayoutParams) menuButtonWrapper.getLayoutParams();
        removeViewAt(menuButtonIndex);
        int closeButtonIndex = indexOfChild(closeButton);
        int buttonWidth = getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        menuButtonLayoutParams.setMarginEnd(buttonWidth);
        addView(menuButtonWrapper, closeButtonIndex, menuButtonLayoutParams);

        var closeButtonLayoutParams = (FrameLayout.LayoutParams) closeButton.getLayoutParams();
        closeButtonIndex = indexOfChild(closeButton);
        removeViewAt(closeButtonIndex);
        closeButtonLayoutParams.gravity = Gravity.CENTER_VERTICAL | Gravity.END;
        addView(closeButton, menuButtonIndex, closeButtonLayoutParams);
    }

    private void maybeAdjustButtonSpacingForCloseButtonPosition() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;
        if (mCloseButtonPosition != CLOSE_BUTTON_POSITION_END) return;

        final @Dimension int buttonWidth =
                getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        final FrameLayout.LayoutParams menuButtonLayoutParams =
                (FrameLayout.LayoutParams) mMenuButton.getLayoutParams();
        menuButtonLayoutParams.width = buttonWidth;
        menuButtonLayoutParams.gravity = Gravity.CENTER_VERTICAL | Gravity.START;
        mMenuButton.setLayoutParams(menuButtonLayoutParams);
        mMenuButton.setPaddingRelative(0, 0, 0, 0);

        FrameLayout.LayoutParams actionButtonsLayoutParams =
                (FrameLayout.LayoutParams) assumeNonNull(mCustomActionButtons).getLayoutParams();
        if (MinimizedFeatureUtils.isMinimizedCustomTabAvailable(getContext())) {
            actionButtonsLayoutParams.setMarginEnd(buttonWidth);
            var lpTitle = (ViewGroup.MarginLayoutParams) mLocationBar.mTitleBar.getLayoutParams();
            var lpUrl = (ViewGroup.MarginLayoutParams) mLocationBar.mUrlBar.getLayoutParams();
            LayoutParams lp = (LayoutParams) mLocationBar.getLayout().getLayoutParams();
            // Prevent URL and title from bleeding over minimize button
            lpTitle.setMarginEnd(buttonWidth);
            lpUrl.setMarginEnd(buttonWidth);
            lp.setMarginStart(buttonWidth);
            if (mIsRtl) {
                var lpSecurity =
                        (ViewGroup.MarginLayoutParams)
                                mLocationBar.getSecurityIconView().getLayoutParams();
                lpTitle.setMarginEnd(0);
                lpUrl.setMarginEnd(0);
                mLocationBar.getSecurityIconView().setLayoutParams(lpSecurity);
            }
            mLocationBar.getLayout().setLayoutParams(lp);
            mLocationBar.mTitleBar.setLayoutParams(lpTitle);
            mLocationBar.mUrlBar.setLayoutParams(lpUrl);
        } else {
            actionButtonsLayoutParams.setMarginEnd(buttonWidth);
        }
        mCustomActionButtons.setLayoutParams(actionButtonsLayoutParams);
    }

    private void updateToolbarLayoutMargin() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;
        if (mIncognitoImageView != null) {
            final boolean shouldShowIncognitoIcon = isIncognitoBranded();
            mIncognitoImageView.setVisibility(shouldShowIncognitoIcon ? VISIBLE : GONE);
        }

        int startMargin = calculateStartMarginForStartButtonVisibility();

        updateStartMarginOfVisibleElementsUntilLocationBarFrameLayout(startMargin);

        int locationBarLayoutChildIndex = getLocationBarFrameLayoutIndex();
        assert locationBarLayoutChildIndex != -1;
        updateLocationBarLayoutEndMargin(locationBarLayoutChildIndex);

        // Update left margin of mTitleUrlContainer here to make sure the security icon is
        // always placed left of the urlbar.
        mLocationBar.updateLeftMarginOfTitleUrlContainer();
    }

    private int calculateStartMarginForStartButtonVisibility() {
        final View buttonAtStart =
                mCloseButtonPosition == CLOSE_BUTTON_POSITION_END ? mMenuButton : mCloseButton;
        return (buttonAtStart.getVisibility() == GONE)
                ? getResources()
                        .getDimensionPixelSize(
                                R.dimen.custom_tabs_toolbar_horizontal_margin_no_start)
                : 0;
    }

    private void updateStartMarginOfVisibleElementsUntilLocationBarFrameLayout(int startMargin) {
        int locationBarFrameLayoutIndex = getLocationBarFrameLayoutIndex();
        for (int i = 0; i < locationBarFrameLayoutIndex; ++i) {
            View childView = getChildAt(i);
            if (childView.getVisibility() == GONE) continue;

            updateViewLayoutParams(childView, startMargin);

            LayoutParams childLayoutParams = (LayoutParams) childView.getLayoutParams();
            int widthMeasureSpec = calcWidthMeasure(childLayoutParams);
            int heightMeasureSpec = calcHeightMeasure(childLayoutParams);
            childView.measure(widthMeasureSpec, heightMeasureSpec);
            int width = childView.getMeasuredWidth();
            startMargin += width;
        }
        updateStartMarginOfLocationBarFrameLayout(startMargin);
    }

    private void updateStartMarginOfLocationBarFrameLayout(int startMargin) {
        int locationBarFrameLayoutIndex = getLocationBarFrameLayoutIndex();
        View locationBarLayoutView = getChildAt(locationBarFrameLayoutIndex);
        updateViewLayoutParams(locationBarLayoutView, startMargin);
    }

    private void updateViewLayoutParams(View view, int margin) {
        LayoutParams layoutParams = (LayoutParams) view.getLayoutParams();
        if (MarginLayoutParamsCompat.getMarginStart(layoutParams) != margin) {
            MarginLayoutParamsCompat.setMarginStart(layoutParams, margin);
            view.setLayoutParams(layoutParams);
        }
    }

    private void updateLocationBarLayoutEndMargin(int startIndex) {
        int locationBarLayoutEndMargin = 0;
        for (int i = startIndex + 1; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (childView.getVisibility() != GONE) {
                locationBarLayoutEndMargin += childView.getMeasuredWidth();
            }
        }
        LayoutParams urlLayoutParams = (LayoutParams) mLocationBar.getLayout().getLayoutParams();

        if (MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams) != locationBarLayoutEndMargin) {
            MarginLayoutParamsCompat.setMarginEnd(urlLayoutParams, locationBarLayoutEndMargin);
            mLocationBar.getLayout().setLayoutParams(urlLayoutParams);
        }
    }

    private int getLocationBarFrameLayoutIndex() {
        assert mLocationBar.getLayout().getVisibility() != GONE;
        for (int i = 0; i < getChildCount(); i++) {
            if (getChildAt(i) == mLocationBar.getLayout()) return i;
        }
        return -1;
    }

    private int calcWidthMeasure(LayoutParams childLayoutParams) {
        if (childLayoutParams.width == LayoutParams.WRAP_CONTENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.AT_MOST);
        }

        if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
        }

        return MeasureSpec.makeMeasureSpec(childLayoutParams.width, MeasureSpec.EXACTLY);
    }

    private int calcHeightMeasure(LayoutParams childLayoutParams) {
        if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.AT_MOST);
        }

        if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
        }

        return MeasureSpec.makeMeasureSpec(childLayoutParams.height, MeasureSpec.EXACTLY);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mLocationBar.addButtonsVisibilityUpdater();
        assumeNonNull(mLocationBarModel).notifyTitleChanged();
        mLocationBarModel.notifyUrlChanged(false);
        mLocationBarModel.notifyPrimaryColorChanged();
    }

    @Override
    public ColorDrawable getBackground() {
        return (ColorDrawable) super.getBackground();
    }

    public @BrandedColorScheme int getBrandedColorScheme() {
        return mBrandedColorScheme;
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    @Override
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        if (mBrandColorTransitionActive) assumeNonNull(mBrandColorTransitionAnimation).cancel();

        final ColorDrawable background = getBackground();
        final int startColor = background.getColor();
        final int endColor = getToolbarDataProvider().getPrimaryColor();

        if (background.getColor() == endColor) return;

        mBrandColorTransitionAnimation =
                ValueAnimator.ofFloat(0, 1)
                        .setDuration(ToolbarPhone.THEME_COLOR_TRANSITION_DURATION);
        mBrandColorTransitionAnimation.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        mBrandColorTransitionAnimation.addUpdateListener(
                animation -> {
                    float fraction = animation.getAnimatedFraction();
                    int red =
                            (int) interpolate(Color.red(startColor), Color.red(endColor), fraction);
                    int blue =
                            (int)
                                    interpolate(
                                            Color.blue(startColor), Color.blue(endColor), fraction);
                    int green =
                            (int)
                                    interpolate(
                                            Color.green(startColor),
                                            Color.green(endColor),
                                            fraction);
                    int color = Color.rgb(red, green, blue);
                    background.setColor(color);
                    notifyToolbarColorChanged(color);
                    setHandleViewBackgroundColor(color);
                });
        mBrandColorTransitionAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mBrandColorTransitionActive = false;

                        // Using the current background color instead of the final color in case
                        // this
                        // animation was cancelled.  This ensures the assets are updated to the
                        // visible color.
                        updateColorsForBackground(background.getColor());
                    }
                });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
        if (!shouldAnimate) mBrandColorTransitionAnimation.end();
    }

    private void updateColorsForBackground(@ColorInt int background) {
        final @BrandedColorScheme int brandedColorScheme =
                OmniboxResourceProvider.getBrandedColorScheme(
                        getContext(), isIncognitoBranded(), background);
        if (mBrandedColorScheme == brandedColorScheme) return;
        mBrandedColorScheme = brandedColorScheme;
        final ColorStateList tint =
                ThemeUtils.getThemedToolbarIconTint(getContext(), mBrandedColorScheme);
        mTint = tint;
        mLocationBar.updateColors();
        setToolbarHairlineColor(background);
        notifyToolbarColorChanged(background);
        notifyColorSchemeChanged();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int measuredWidth = MeasureSpec.getSize(widthMeasureSpec);
        if (measuredWidth > 0 && mToolbarWidth != measuredWidth) {
            mToolbarWidth = measuredWidth;
            if (mOnNewWidthMeasuredListener != null) {
                mOnNewWidthMeasuredListener.onNewWidthMeasured(mToolbarWidth);
            }
        }

        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        maybeSwapCloseAndMenuButtons();
        Activity activity = getActivityFromCurrentTab();
        if (activity != null) {
            mButtonVisibilityRule.setToolbarWidth(
                    CustomTabDimensionUtils.getInitialWidth(
                            activity, assumeNonNull(mIntentDataProvider)));
        }

        updateToolbarLayoutMargin();
        maybeAdjustButtonSpacingForCloseButtonPosition();
        setMaximizeButtonVisibility();
        setMinimizeButtonVisibility();

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private @Nullable Activity getActivityFromCurrentTab() {
        Tab currentTab = getCurrentTab();
        return currentTab != null
                ? assumeNonNull(currentTab.getWindowAndroid()).getActivity().get()
                : null;
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    /** Return the delegate used to control branding UI changes on the location bar. */
    public ToolbarBrandingDelegate getBrandingDelegate() {
        return mLocationBar;
    }

    public void setHandleBackground(Drawable handleDrawable) {
        mHandleDrawable = handleDrawable;
        setHandleViewBackgroundColor(getBackground().getColor());
    }

    private void setHandleViewBackgroundColor(int color) {
        if (mHandleDrawable == null) return;
        ((GradientDrawable) mHandleDrawable.mutate()).setColor(color);
    }

    @Override
    public boolean onLongClick(View v) {
        if (v == mCloseButton || v == mMinimizeButton || v.getParent() == mCustomActionButtons) {
            return Toast.showAnchoredToast(getContext(), v, v.getContentDescription());
        }
        return false;
    }

    @VisibleForTesting
    static String parsePublisherNameFromUrl(GURL url) {
        // TODO(ianwen): Make it generic to parse url from URI path. http://crbug.com/599298
        // The url should look like: https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html
        // or https://www.google.com/amp/www.nyt.com/ampthml/blogs.html.
        String[] segments = url.getPath().split("/");
        if (segments.length >= 4 && "amp".equals(segments[1])) {
            if (segments[2].length() > 1) return segments[2];
            return segments[3];
        }
        return url.getSpec();
    }

    @Override
    protected void onMenuButtonDisabled() {
        super.onMenuButtonDisabled();
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) return;

        mButtonVisibilityRule.update(ButtonId.MENU, false);
        // In addition to removing the menu button, we also need to remove the margin on the custom
        // action button.
        ViewGroup.MarginLayoutParams p =
                (ViewGroup.MarginLayoutParams)
                        assumeNonNull(mCustomActionButtons).getLayoutParams();
        p.setMarginEnd(0);
        mCustomActionButtons.setLayoutParams(p);
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        CustomTabCaptureStateToken currentToken = generateCaptureStateToken();
        final @ToolbarSnapshotDifference int difference =
                currentToken.getAnyDifference(mLastCustomTabCaptureStateToken);
        if (difference == ToolbarSnapshotDifference.NONE) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SNAPSHOT_SAME);
        } else {
            return CaptureReadinessResult.readyWithSnapshotDifference(difference);
        }
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        if (textureMode) {
            mLastCustomTabCaptureStateToken = generateCaptureStateToken();
        }
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        // Ignore when the changed view is our self. This happens on startup, and is not our
        // container being changed.
        if (changedView != this) {
            for (Callback<Integer> observer : mContainerVisibilityChangeObserverList) {
                observer.onResult(visibility);
            }
        }
    }

    @Override
    public void setToolbarColorObserver(ToolbarColorObserver toolbarColorObserver) {
        super.setToolbarColorObserver(toolbarColorObserver);
        notifyToolbarColorChanged(getBackground().getColor());
    }

    /** Subscribe to container visibility changes. */
    public void addContainerVisibilityChangeObserver(Callback<Integer> observer) {
        mContainerVisibilityChangeObserverList.addObserver(observer);
    }

    /** Unsubscribe to container visibility changes. */
    public void removeContainerVisibilityChangeObserver(Callback<Integer> observer) {
        mContainerVisibilityChangeObserverList.removeObserver(observer);
    }

    private CustomTabCaptureStateToken generateCaptureStateToken() {
        // Must convert CharSequence to String in order for equality to be clearly defined.
        String url = mLocationBar.mUrlBar.getText().toString();
        String title = mLocationBar.mTitleBar.getText().toString();
        boolean minimizeVisible =
                mMinimizeButton != null && mMinimizeButton.getVisibility() == VISIBLE;
        var minimizeTag =
                mMinimizeButton != null ? mMinimizeButton.getTag(R.id.highlight_state) : null;
        boolean minimizeHighlighted = Boolean.TRUE.equals(minimizeTag);
        return new CustomTabCaptureStateToken(
                url,
                title,
                getBackground().getColor(),
                mLocationBar.mAnimDelegate.getSecurityIconRes(),
                mLocationBar.mAnimDelegate.isInAnimation(),
                getWidth(),
                minimizeVisible,
                minimizeHighlighted);
    }

    /**
     * Record the histogram for fallback UI used instead of the hidden adaptive toolbar button.
     *
     * @param variant Toolbar button type.
     */
    public void maybeRecordHistogramForAdaptiveToolbarButtonFallbackUi(
            @AdaptiveToolbarButtonVariant int variant) {
        if (variant != mVariantForFallbackMenu) return;

        RecordHistogram.recordEnumeratedHistogram(
                "CustomTab.AdaptiveToolbarButton.FallbackUi",
                variant,
                AdaptiveToolbarButtonVariant.MAX_VALUE);
        mVariantForFallbackMenu = AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    /** Custom tab-specific implementation of the LocationBar interface. */
    @VisibleForTesting
    public class CustomTabLocationBar
            implements LocationBar,
                    UrlBar.UrlBarDelegate,
                    LocationBarDataProvider.Observer,
                    View.OnLongClickListener,
                    ToolbarBrandingDelegate,
                    CookieControlsObserver {
        private static final int TITLE_ANIM_DELAY_MS = 800;
        private static final int MIN_URL_BAR_VISIBLE_TIME_POST_BRANDING_MS = 3000;

        private static final int STATE_DOMAIN_ONLY = 0;
        private static final int STATE_TITLE_ONLY = 1;
        private static final int STATE_DOMAIN_AND_TITLE = 2;
        private static final int STATE_EMPTY = 3; // Not used as a regular state.
        private static final int COOKIE_CONTROLS_ICON_DISPLAY_TIMEOUT = 8500;
        private int mState = STATE_DOMAIN_ONLY;

        // Used for After branding runnables
        private static final int KEY_UPDATE_TITLE_POST_BRANDING = 0;
        private static final int KEY_UPDATE_URL_POST_BRANDING = 1;
        private static final int TOTAL_POST_BRANDING_KEYS = 2;

        private LocationBarDataProvider mLocationBarDataProvider;
        private @Nullable Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
        private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
        private UrlBarCoordinator mUrlCoordinator;
        private @Nullable TabCreator mTabCreator;

        private TextView mUrlBar;
        private TextView mTitleBar;
        private View mLocationBarFrameLayout;
        private View mTitleUrlContainer;
        private ImageButton mSecurityButton;

        private CustomTabToolbarAnimationDelegate mAnimDelegate;
        private final Runnable mTitleAnimationStarter =
                new Runnable() {
                    @Override
                    public void run() {
                        mAnimDelegate.startTitleAnimation(getContext());
                    }
                };

        private final @Nullable Runnable[] mAfterBrandingRunnables =
                new Runnable[TOTAL_POST_BRANDING_KEYS];
        private final View.OnLayoutChangeListener mButtonsVisibilityUpdater =
                (v, l, t, r, b, ol, ot, or, ob) -> setButtonsVisibility();
        private boolean mCurrentlyShowingBranding;
        private boolean mBrandingStarted;
        private boolean mOmniboxEnabled;
        private @Nullable Drawable mOmniboxBackground;
        private @Nullable CallbackController mCallbackController = new CallbackController();
        // Cached the state before branding start so we can reset to the state when its done.
        private @Nullable Integer mPreBandingState;
        private @Nullable PageInfoIphController mPageInfoIphController;
        private int mTouchTargetSize;
        private @Nullable ToolbarBrandingOverlayCoordinator mBrandingOverlayCoordinator;

        private @Nullable OptionalButtonCoordinator mOptionalButtonCoordinator;
        private final ObservableSupplierImpl<Tracker> mTrackerSupplier =
                new ObservableSupplierImpl<>();

        /** Returns {@code true} if optional button MVC was initialized successfully. */
        private boolean initializeOptionalButton() {
            if (mOptionalButtonCoordinator != null) return true;

            if (!ChromeFeatureList.sCctAdaptiveButton.isEnabled()) return false;
            if (hasMultipleDevButtons()) {
                RecordHistogram.recordEnumeratedHistogram(
                        "CustomTabs.AdaptiveToolbarButton.HiddenReason",
                        CustomTabMtbHiddenReason.NO_BUTTON_SPACE,
                        CustomTabMtbHiddenReason.COUNT);
                return false;
            }
            if (CustomTabsConnection.getInstance()
                    .shouldEnableOmniboxForIntent(assumeNonNull(mIntentDataProvider))) {
                // We disable the optional button when omnibox in CCT is on.
                RecordHistogram.recordEnumeratedHistogram(
                        "CustomTabs.AdaptiveToolbarButton.HiddenReason",
                        CustomTabMtbHiddenReason.OMNIBOX_ENABLED,
                        CustomTabMtbHiddenReason.COUNT);
                return false;
            }

            ViewStub optionalButtonStub = findViewById(R.id.optional_button_stub);
            if (optionalButtonStub == null) return false;

            optionalButtonStub.setLayoutResource(R.layout.optional_button_layout);
            View optionalButton = optionalButtonStub.inflate();
            var lp = (LinearLayout.LayoutParams) optionalButton.getLayoutParams();
            lp.width = getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
            optionalButton.setLayoutParams(lp);

            int paddingStart =
                    getDimensionPx(R.dimen.custom_tabs_toolbar_button_horizontal_padding);
            View icon = optionalButton.findViewById(R.id.swappable_icon_animation_image);
            setHorizontalPadding(icon, paddingStart, icon.getPaddingEnd());

            View menu = optionalButton.findViewById(R.id.optional_toolbar_button);
            // Following commands should be identical to a single #setPaddingRelative in theory
            // but is not. This might be happening if the padding is applied to image view
            // whose scale type depends to RTL.
            if (mIsRtl) {
                menu.setPadding(0, menu.getPaddingTop(), paddingStart, menu.getPaddingBottom());
            } else {
                menu.setPadding(paddingStart, menu.getPaddingTop(), 0, menu.getPaddingBottom());
            }

            int paddingVert = getDimensionPx(R.dimen.custom_tabs_adaptive_button_bg_padding_vert);
            int paddingHori =
                    getDimensionPx(R.dimen.custom_tabs_adaptive_button_bg_horizontal_padding);
            View background = optionalButton.findViewById(R.id.swappable_icon_secondary_background);
            background.setPaddingRelative(paddingHori, paddingVert, paddingHori, paddingVert);

            mOptionalButtonCoordinator =
                    new OptionalButtonCoordinator(
                            optionalButton,
                            /* userEducationHelper= */ () -> {
                                return new UserEducationHelper(
                                        assumeNonNull(mActivity),
                                        getProfileSupplier(),
                                        new Handler());
                            },
                            /* transitionRoot= */ CustomTabToolbar.this,
                            /* isAnimationAllowedPredicate= */ () -> true,
                            mTrackerSupplier);

            mOptionalButtonCoordinator.setBackgroundColorFilter(getBackgroundColor());
            mOptionalButtonCoordinator.setIconForegroundColor(mTint);
            int width = getDimensionPx(R.dimen.toolbar_button_width);
            mOptionalButtonCoordinator.setCollapsedStateWidth(width);
            mOptionalButtonCoordinator.setTransitionFinishedCallback(
                    transitionType -> {
                        switch (transitionType) {
                            case TransitionType.EXPANDING_ACTION_CHIP:
                                setUrlTitleBarMargin(
                                        assumeNonNull(mOptionalButtonCoordinator).getViewWidth());
                                break;
                        }
                        CustomTabToolbar.this.requestLayout();
                    });
            View optionalButtonContainer = findViewById(R.id.optional_toolbar_button_container);
            optionalButtonContainer.setVisibility(View.VISIBLE);
            mButtonVisibilityRule.addButtonWithCallback(
                    ButtonId.MTB,
                    optionalButtonContainer,
                    true,
                    mOptionalButtonCoordinator::setCanChangeVisibility);

            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_ADAPTIVE_BUTTON_TEST_SWITCH, "always-animate", false)) {
                mOptionalButtonCoordinator.setAlwaysShowActionChip(true);
            }
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_ADAPTIVE_BUTTON_TEST_SWITCH, "hide-button", false)) {
                mButtonVisibilityRule.setHidingOptionalButton();
            }
            return true;
        }

        private Supplier getProfileSupplier() {
            Tab tab = getCurrentTab();
            if (tab != null) return () -> tab.getProfile();

            // Passing OneshotSupplier effectively delays UserEducationHelper#requestShowIph()
            // till Profile becomes reachable via the current Tab.
            var profileSupplier = new OneshotSupplierImpl<Profile>();
            assumeNonNull(mLocationBarModel)
                    .addObserver(
                            new LocationBarDataProvider.Observer() {
                                @Override
                                public void onTabChanged(@Nullable Tab previousTab) {
                                    Tab tab = getCurrentTab();
                                    if (tab != null) {
                                        profileSupplier.set(tab.getProfile());
                                        mLocationBarModel.removeObserver(this);
                                    }
                                }
                            });
            return profileSupplier;
        }

        private @Px int getDimensionPx(@DimenRes int resId) {
            return getResources().getDimensionPixelSize(resId);
        }

        private @ColorInt int getBackgroundColor() {
            return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                    getContext(),
                    getBackground().getColor(),
                    mBrandedColorScheme == BrandedColorScheme.INCOGNITO,
                    /* isCustomTab= */ true);
        }

        private void updateOptionalButton(ButtonData buttonData) {
            boolean showOptionalButton = true;
            if (mOptionalButtonCoordinator == null) showOptionalButton = initializeOptionalButton();
            if (showOptionalButton && mButtonVisibilityRule.isSuppressed(ButtonId.MTB)) {
                showOptionalButton = false;
                RecordHistogram.recordEnumeratedHistogram(
                        "CustomTabs.AdaptiveToolbarButton.HiddenReason",
                        CustomTabMtbHiddenReason.TOOLBAR_WIDTH_LIMIT,
                        CustomTabMtbHiddenReason.COUNT);
            }
            var buttonVariant = buttonData.getButtonSpec().getButtonVariant();
            var optionalButtonVisibility = mOptionalButtonVisibilitySupplier.get();
            if (optionalButtonVisibility == null) {
                mOptionalButtonVisibilitySupplier.set(showOptionalButton);
            }
            if (showOptionalButton) {
                RecordHistogram.recordEnumeratedHistogram(
                        "CustomTabs.AdaptiveToolbarButton.Shown",
                        buttonVariant,
                        AdaptiveToolbarButtonVariant.MAX_VALUE);
                mOptionalButtonForMetric = buttonVariant;

            } else {
                // See if we should show an indicator (a dot) if optional button cannot be shown.
                // This check needs to be invoked _after_ optional button initialization is
                // attempted, in order to determine its visibility in case it gets hidden due to
                // toolbar width/button count constraints.
                maybeShowActionMenuIndicator(buttonVariant);
                return;
            }
            Tab tab = getCurrentTab();
            if (tab != null && mTrackerSupplier.get() == null) {
                mTrackerSupplier.set(TrackerFactory.getTrackerForProfile(tab.getProfile()));
            }
            assumeNonNull(mOptionalButtonCoordinator)
                    .updateButton(buttonData, isIncognitoBranded());
            setOptionalButtonBackgroundInset();
        }

        private void hideOptionalButton() {
            if (mOptionalButtonCoordinator == null
                    || mOptionalButtonCoordinator.getViewVisibility() == View.GONE) {
                return;
            }
            mOptionalButtonCoordinator.hideButton();
        }

        // Display a (blue) dot on the overflow menu icon for the optional button that cannot be
        // shown on the toolbar to indicate that the action is available through the menu.
        private void maybeShowActionMenuIndicator(@AdaptiveToolbarButtonVariant int buttonVariant) {
            if (CustomTabsConnection.getInstance()
                    .shouldEnableOmniboxForIntent(assumeNonNull(mIntentDataProvider))) {
                return;
            }
            setUpOptionalButtonFallbackUi(buttonVariant);
        }

        private void updateOptionalButtonTint() {
            if (mOptionalButtonCoordinator != null) {
                mOptionalButtonCoordinator.setIconForegroundColor(mTint);
                ImageView menuDot = mMenuButton.findViewById(R.id.menu_dot);
                if (assumeNonNull(mIntentDataProvider).getCustomTabMode()
                        == CustomTabProfileType.INCOGNITO) {
                    @ColorRes int tint = R.color.default_icon_color_blue_light;
                    ImageViewCompat.setImageTintList(
                            menuDot, AppCompatResources.getColorStateList(getContext(), tint));
                } else if (mIntentDataProvider.getColorProvider().hasCustomToolbarColor()) {
                    ImageViewCompat.setImageTintList(menuDot, mTint);
                }
            }
        }

        /**
         * Resets optional button internal state regarding the fallback UI indicator for CPA.
         *
         * @param resetFallbackMenu {@code true} if the CPA type for which a fallback menu is shown
         *     should be reset.
         */
        public void resetOptionalButtonState(boolean resetFallbackMenu) {
            if (mAppMenuHandler.get() == null) return;

            // Hides the menu dot, and turns off the highlight on the fallback menu item.
            View indicator = mMenuButton.findViewById(R.id.menu_dot);
            if (indicator.getVisibility() != View.GONE) {
                indicator.setVisibility(View.GONE);
                View menuIcon = mMenuButton.findViewById(R.id.menu_button);
                menuIcon.setContentDescription(
                        getContext().getString(R.string.accessibility_toolbar_btn_menu));
                if (mAppMenuHandler.get() != null) {
                    mAppMenuHandler.get().setContentDescription(null);
                }
            }
            if (resetFallbackMenu) {
                mVariantForFallbackMenu = AdaptiveToolbarButtonVariant.UNKNOWN;
            }
            if (mAppMenuObserver != null) {
                mAppMenuHandler.get().removeObserver(mAppMenuObserver);
                mAppMenuObserver = null;
            }
        }

        public View getLayout() {
            return mLocationBarFrameLayout;
        }

        public ImageButton getSecurityButton() {
            return mSecurityButton;
        }

        public boolean isShowingTitleOnly() {
            return mState == STATE_TITLE_ONLY;
        }

        @Override
        public void showBrandingLocationBar() {
            mBrandingStarted = true;

            ViewStub stub = findViewById(R.id.branding_stub);
            if (stub != null) {
                PropertyModel model =
                        new PropertyModel.Builder(ToolbarBrandingOverlayProperties.ALL_KEYS)
                                .with(
                                        ToolbarBrandingOverlayProperties.COLOR_DATA,
                                        new ToolbarBrandingOverlayProperties.ColorData(
                                                getBackground().getColor(), mBrandedColorScheme))
                                .build();
                mBrandingOverlayCoordinator = new ToolbarBrandingOverlayCoordinator(stub, model);

                return;
            }

            // Store the title and domain setting, if the empty state is not in used. Otherwise
            // regular state has already been stored.
            if (!mCurrentlyShowingBranding) {
                mCurrentlyShowingBranding = true;
                cacheRegularState();
            }

            // We use url bar to show the branding text and hide the title bar so the text will
            // align with the security icon.
            setUrlBarHiddenIgnoreBranding(false);
            setShowTitleIgnoreBranding(false);

            mAnimDelegate.setUseRotationSecurityButtonTransition(true);
            showBrandingIconAndText();
        }

        @Override
        public void showEmptyLocationBar() {
            mBrandingStarted = true;
            mCurrentlyShowingBranding = true;

            // Force setting the LocationBar element visibility, while cache their state.
            cacheRegularState();

            mState = STATE_EMPTY;
            mUrlBar.setVisibility(View.GONE);
            mTitleBar.setVisibility(View.GONE);
        }

        @Override
        public void showRegularToolbar() {
            mCurrentlyShowingBranding = false;

            if (mBrandingOverlayCoordinator != null) {
                mBrandingOverlayCoordinator.hideAndDestroy();
            }

            recoverFromRegularState();
            runAfterBrandingRunnables();
            mAnimDelegate.setUseRotationSecurityButtonTransition(false);

            int token = assumeNonNull(mBrowserControlsVisibilityDelegate).showControlsPersistent();
            PostTask.postDelayedTask(
                    TaskTraits.UI_USER_VISIBLE,
                    () -> mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(token),
                    MIN_URL_BAR_VISIBLE_TIME_POST_BRANDING_MS);
        }

        // CookieControlsObserver interface
        @Override
        public void onHighlightCookieControl(boolean shouldHighlight) {
            if (mShouldHighlightCookieControlsIcon) return;
            mShouldHighlightCookieControlsIcon = shouldHighlight;
        }

        @Override
        public void onStatusChanged(
                int controlsState, int enforcement, int blockingStatus, long expiration) {
            mBlockingStatus3pcd = blockingStatus;
        }

        private void cacheRegularState() {
            String assertMsg =
                    "mPreBandingState already exists! mPreBandingState = " + mPreBandingState;
            assert mPreBandingState == null : assertMsg;

            mPreBandingState = mState;
        }

        private void recoverFromRegularState() {
            assert !mCurrentlyShowingBranding;
            assert mPreBandingState != null;

            boolean showTitle =
                    mPreBandingState == STATE_TITLE_ONLY
                            || mPreBandingState == STATE_DOMAIN_AND_TITLE;
            boolean hideUrl = mPreBandingState == STATE_TITLE_ONLY;
            mPreBandingState = null;

            setUrlBarHiddenIgnoreBranding(hideUrl);
            setShowTitleIgnoreBranding(showTitle);
        }

        @Initializer
        public void onFinishInflate(View container) {
            mUrlBar = container.findViewById(R.id.url_bar);
            mUrlBar.setHint("");
            mUrlBar.setEnabled(false);
            mUrlBar.setPaddingRelative(0, 0, 0, 0);

            mTitleBar = container.findViewById(R.id.title_bar);
            mLocationBarFrameLayout = container.findViewById(R.id.location_bar_frame_layout);
            mTitleUrlContainer = container.findViewById(R.id.title_url_container);
            mTitleUrlContainer.setOnLongClickListener(this);

            int securityButtonId =
                    shouldNestSecurityIcon() ? R.id.security_icon : R.id.security_button;
            mSecurityButton = mLocationBarFrameLayout.findViewById(securityButtonId);
            mSecurityButton.setVisibility(INVISIBLE);

            // If the security icon is nested, only the url bar should be offset by it.
            View securityButtonOffsetTarget =
                    shouldNestSecurityIcon()
                            ? mTitleUrlContainer.findViewById(R.id.url_bar)
                            : mTitleUrlContainer;

            mAnimDelegate =
                    new CustomTabToolbarAnimationDelegate(
                            mSecurityButton,
                            securityButtonOffsetTarget,
                            this::adjustTitleUrlBarPadding,
                            shouldNestSecurityIcon()
                                    ? R.dimen.custom_tabs_security_icon_width
                                    : R.dimen.location_bar_icon_width);

            addButtonsVisibilityUpdater();
            adjustLocationBarPadding();
        }

        private void adjustLocationBarPadding() {
            if (shouldNestSecurityIcon() && !isIncognitoBranded()) {
                int horizontalPadding =
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_location_bar_horizontal_padding);
                mLocationBarFrameLayout.setPadding(
                        horizontalPadding,
                        mLocationBarFrameLayout.getPaddingTop(),
                        horizontalPadding,
                        mLocationBarFrameLayout.getPaddingBottom());
            }
        }

        private void removeButtonsVisibilityUpdater() {
            mTitleUrlContainer.removeOnLayoutChangeListener(mButtonsVisibilityUpdater);
        }

        private void addButtonsVisibilityUpdater() {
            if (mTitleUrlContainer != null) {
                mTitleUrlContainer.addOnLayoutChangeListener(mButtonsVisibilityUpdater);
            }
        }

        @Initializer
        public void init(
                LocationBarDataProvider locationBarDataProvider,
                Supplier<ModalDialogManager> modalDialogManagerSupplier,
                Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
                TabCreator tabCreator,
                ActionMode.Callback actionModeCallback) {
            mLocationBarDataProvider = locationBarDataProvider;
            mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
            mLocationBarDataProvider.addObserver(this);
            mModalDialogManagerSupplier = modalDialogManagerSupplier;
            mUrlCoordinator =
                    new UrlBarCoordinator(
                            getContext(),
                            (UrlBar) mUrlBar,
                            actionModeCallback,
                            /* focusChangeCallback= */ (unused) -> {},
                            this,
                            new NoOpkeyboardVisibilityDelegate(),
                            isIncognitoBranded(),
                            /* onLongClickListener= */ null);
            mUrlCoordinator.setIsInCct(true);
            mTabCreator = tabCreator;
            mTouchTargetSize = getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
            updateColors();
            updateSecurityIcon();
            updateProgressBarColors();
            updateUrlBar();
        }

        public void setUrlBarHidden(boolean hideUrlBar) {
            if (mCurrentlyShowingBranding) {
                mAfterBrandingRunnables[KEY_UPDATE_URL_POST_BRANDING] =
                        () -> setUrlBarHiddenIgnoreBranding(hideUrlBar);
                return;
            }
            setUrlBarHiddenIgnoreBranding(hideUrlBar);
        }

        private void setUrlBarHiddenIgnoreBranding(boolean hideUrlBar) {
            // Urlbar visibility cannot be toggled if it is the only visible element.
            if (mState == STATE_DOMAIN_ONLY) return;

            if (hideUrlBar && mState == STATE_DOMAIN_AND_TITLE) {
                mState = STATE_TITLE_ONLY;
                mAnimDelegate.disableTitleAnimation();
                mUrlBar.setVisibility(View.GONE);
                mTitleBar.setVisibility(View.VISIBLE);
                LayoutParams lp = (LayoutParams) mTitleBar.getLayoutParams();
                lp.bottomMargin = 0;
                mTitleBar.setLayoutParams(lp);
                mTitleBar.setTextSize(
                        TypedValue.COMPLEX_UNIT_PX,
                        getResources().getDimension(R.dimen.location_bar_url_text_size));
            } else if (!hideUrlBar && mState == STATE_TITLE_ONLY) {
                mState = STATE_DOMAIN_AND_TITLE;
                mTitleBar.setVisibility(View.VISIBLE);
                mUrlBar.setTextSize(
                        TypedValue.COMPLEX_UNIT_PX,
                        getResources().getDimension(R.dimen.custom_tabs_url_text_size));
                mUrlBar.setVisibility(View.VISIBLE);
                LayoutParams lp = (LayoutParams) mTitleBar.getLayoutParams();
                lp.bottomMargin =
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_toolbar_vertical_padding);
                mTitleBar.setLayoutParams(lp);
                mTitleBar.setTextSize(
                        TypedValue.COMPLEX_UNIT_PX,
                        getResources().getDimension(R.dimen.custom_tabs_title_text_size));
                // Refresh the status icon and url bar.
                updateUrlBar();
                assumeNonNull(mLocationBarModel).notifySecurityStateChanged();
            } else if (mState == STATE_EMPTY) {
                // If state is empty, that means Location bar is recovering from empty location bar
                // to whatever new state it is. We skip the state assertion and the end.
                if (!hideUrlBar) {
                    mState = STATE_DOMAIN_ONLY;
                    mUrlBar.setVisibility(View.VISIBLE);
                }
            } else {
                assert false : "Unreached state";
            }
        }

        public void onNativeLibraryReady() {
            mSecurityButton.setOnClickListener(v -> showPageInfo());
            if (shouldNestSecurityIcon()) {
                mTitleUrlContainer.setOnClickListener(v -> showPageInfo());
                // The title and url are independently focusable for accessibility. Set
                // AccessibilityNodeInfo on each to indicate they respond to clicks / long clicks
                // via the listeners set on mTitleUrlContainer.
                setTitleUrlBarAccessibilityDelegate(mTitleBar);
                setTitleUrlBarAccessibilityDelegate(mUrlBar);
            }
        }

        private void setTitleUrlBarAccessibilityDelegate(View view) {
            view.setAccessibilityDelegate(
                    new View.AccessibilityDelegate() {
                        @Override
                        public void onInitializeAccessibilityNodeInfo(
                                View host, AccessibilityNodeInfo info) {
                            super.onInitializeAccessibilityNodeInfo(host, info);
                            info.setLongClickable(true);
                            info.setClickable(true);
                            info.setEnabled(true);
                            info.setEditable(false);
                        }
                    });
        }

        private void showPageInfo() {
            Tab currentTab = mLocationBarDataProvider.getTab();
            if (currentTab == null) return;
            WebContents webContents = currentTab.getWebContents();
            if (webContents == null) return;
            @Nullable Activity activity =
                    assumeNonNull(currentTab.getWindowAndroid()).getActivity().get();
            if (activity == null) return;
            if (mCurrentlyShowingBranding) return;
            // For now we don't show "store info" row for custom tab.
            new ChromePageInfo(
                            mModalDialogManagerSupplier,
                            TrustedCdn.getContentPublisher(
                                    assumeNonNull(getToolbarDataProvider().getTab())),
                            OpenedFromSource.TOOLBAR,
                            /* storeInfoActionHandlerSupplier= */ null,
                            mEphemeralTabCoordinatorSupplier,
                            mTabCreator)
                    .show(currentTab, ChromePageInfoHighlight.noHighlight());
        }

        @Override
        public @Nullable View getViewForUrlBackFocus() {
            Tab tab = getCurrentTab();
            if (tab == null) return null;
            return tab.getView();
        }

        @Override
        public boolean allowKeyboardLearning() {
            return !CustomTabToolbar.this.isOffTheRecord();
        }

        @Override
        public void onFocusByTouch() {}

        @Override
        public void onTouchAfterFocus() {}

        // LocationBarDataProvider.Observer implementation
        // Using the default empty onIncognitoStateChanged.
        // Using the default empty onNtpStartedLoading.

        @Override
        public void onPrimaryColorChanged() {
            updateColors();
            updateSecurityIcon();
            updateProgressBarColors();
        }

        @Override
        public void onSecurityStateChanged() {
            updateSecurityIcon();
        }

        @Override
        public void onTitleChanged() {
            updateTitleBar();
        }

        @Override
        public void onUrlChanged(boolean isTabChanging) {
            updateUrlBar();
        }

        @Override
        public void onPageLoadStopped() {
            if (mBlockingStatus3pcd != CookieBlocking3pcdStatus.NOT_IN3PCD) {
                return;
            }
            if (mPageInfoIphController == null) {
                Tab currentTab = getCurrentTab();
                if (currentTab == null) return;
                @Nullable Activity activity =
                        assumeNonNull(currentTab.getWindowAndroid()).getActivity().get();
                if (activity == null) return;
                mPageInfoIphController =
                        new PageInfoIphController(
                                new UserEducationHelper(
                                        activity,
                                        currentTab.getProfile(),
                                        new Handler(Looper.getMainLooper())),
                                getSecurityIconView());
            }
            if (mShouldHighlightCookieControlsIcon) {
                mPageInfoIphController.showCookieControlsIph(
                        COOKIE_CONTROLS_ICON_DISPLAY_TIMEOUT, R.string.cookie_controls_iph_message);
                animateCookieControlsIcon();
                mShouldHighlightCookieControlsIcon = false;
            }
        }

        @Override
        public void updateVisualsForState() {
            updateColorsForBackground(getBackground().getColor());
            updateSecurityIcon();
            updateProgressBarColors();
            updateUrlBar();
        }

        private void updateLeftMarginOfTitleUrlContainer() {
            // If the security icon is nested, we shouldn't move the whole title-url container since
            // the icon is part of the container now.
            if (shouldNestSecurityIcon()) return;

            FrameLayout securityButtonWrapper = findViewById(R.id.security_button_wrapper);
            int leftMargin =
                    securityButtonWrapper.getVisibility() == View.VISIBLE
                            ? securityButtonWrapper.getLayoutParams().width
                            : 0;
            LayoutParams lp = (LayoutParams) mTitleUrlContainer.getLayoutParams();
            lp.leftMargin = leftMargin;
            mTitleUrlContainer.setLayoutParams(lp);
        }

        private void updateProgressBarColors() {
            final ToolbarProgressBar progressBar = getProgressBar();
            if (progressBar == null) return;
            final Context context = getContext();
            final int backgroundColor = getBackground().getColor();
            if (ThemeUtils.isUsingDefaultToolbarColor(
                    context, /* isIncognito= */ false, backgroundColor)) {
                if (ChromeFeatureList.sAndroidProgressBarVisualUpdate.isEnabled()) {
                    progressBar.setBackgroundColor(
                            SemanticColorUtils.getProgressBarTrackColor(context));
                } else {
                    progressBar.setBackgroundColor(
                            context.getColor(R.color.progress_bar_bg_color_list));
                }

                progressBar.setForegroundColor(
                        SemanticColorUtils.getProgressBarForeground(context));
            } else {
                progressBar.setThemeColor(backgroundColor, /* isIncognito= */ false);
            }
        }

        private void showBrandingIconAndText() {
            ColorStateList colorStateList =
                    AppCompatResources.getColorStateList(
                            getContext(), mLocationBarDataProvider.getSecurityIconColorStateList());
            ImageViewCompat.setImageTintList(mSecurityButton, colorStateList);
            mAnimDelegate.updateSecurityButton(R.drawable.chromelogo16);

            mUrlCoordinator.setUrlBarData(
                    UrlBarData.forNonUrlText(
                            getContext().getString(R.string.twa_running_in_chrome)),
                    UrlBar.ScrollType.NO_SCROLL,
                    SelectionState.SELECT_ALL);
        }

        private void runAfterBrandingRunnables() {
            // Always refresh the security icon and URL bar when branding is finished.
            // If Title is changed during branding, it should already get addressed in
            // #setShowTitle.
            updateUrlBar();
            assumeNonNull(mLocationBarModel).notifySecurityStateChanged();

            for (int i = 0; i < mAfterBrandingRunnables.length; i++) {
                Runnable runnable = mAfterBrandingRunnables[i];
                if (runnable != null) {
                    runnable.run();
                    mAfterBrandingRunnables[i] = null;
                }
            }
        }

        private void updateSecurityIcon() {
            if (mState == STATE_TITLE_ONLY || mCurrentlyShowingBranding) return;

            int securityIconResource = 0;
            if (!shouldNestSecurityIcon() || !isSecureOrNeutralLevel()) {
                securityIconResource =
                        mLocationBarDataProvider.getSecurityIconResource(
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()));
                FrameLayout securityButtonWrapper = findViewById(R.id.security_button_wrapper);
                securityButtonWrapper.setVisibility(View.VISIBLE);
                mButtonVisibilityRule.addButton(ButtonId.SECURITY, securityButtonWrapper, true);
            }
            if (securityIconResource != 0) {
                ColorStateList colorStateList =
                        AppCompatResources.getColorStateList(
                                getContext(),
                                mLocationBarDataProvider.getSecurityIconColorStateList());
                ImageViewCompat.setImageTintList(mSecurityButton, colorStateList);
            }
            mAnimDelegate.updateSecurityButton(securityIconResource);
            mSecurityIconResourceForTesting = securityIconResource;

            int contentDescriptionId =
                    mLocationBarDataProvider.getSecurityIconContentDescriptionResourceId();
            String contentDescription = getContext().getString(contentDescriptionId);
            mSecurityButton.setContentDescription(contentDescription);
        }

        /** Returns whether the current security level is considered secure. */
        private boolean isSecureOrNeutralLevel() {
            @ConnectionSecurityLevel
            int securityLevel = mLocationBarDataProvider.getSecurityLevel();
            return securityLevel == ConnectionSecurityLevel.NONE
                    || securityLevel == ConnectionSecurityLevel.SECURE;
        }

        @VisibleForTesting(otherwise = VisibleForTesting.NONE)
        public int getSecurityIconResourceForTesting() {
            return mSecurityIconResourceForTesting;
        }

        private void animateCookieControlsIcon() {
            mTaskHandler.removeCallbacksAndMessages(null);
            mAnimDelegate.setUseRotationSecurityButtonTransition(true);
            mAnimDelegate.updateSecurityButton(R.drawable.ic_eye_crossed);

            Runnable finishIconAnimation =
                    () -> {
                        updateSecurityIcon();
                        mAnimDelegate.setUseRotationSecurityButtonTransition(false);
                    };
            mTaskHandler.postDelayed(finishIconAnimation, COOKIE_CONTROLS_ICON_DISPLAY_TIMEOUT);
        }

        private void updateTitleBar() {
            if (mCurrentlyShowingBranding) return;
            String title = mLocationBarDataProvider.getTitle();

            // If the url is about:blank, we shouldn't show a title as it is prone to spoofing.
            if (!mLocationBarDataProvider.hasTab()
                    || TextUtils.isEmpty(title)
                    || ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(getUrl().getSpec())) {
                mTitleBar.setText("");
                return;
            }

            // It takes some time to parse the title of the webcontent, and before that
            // LocationBarDataProvider#getTitle always returns the url. We postpone the title
            // animation until the title is authentic.
            if ((mState == STATE_DOMAIN_AND_TITLE || mState == STATE_TITLE_ONLY)
                    && !title.equals(mLocationBarDataProvider.getCurrentGurl().getSpec())
                    && !title.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)) {
                // Delay the title animation until security icon animation finishes.
                // If this is updated after branding, we don't need to wait.
                PostTask.postDelayedTask(
                        TaskTraits.UI_DEFAULT,
                        mTitleAnimationStarter,
                        mBrandingStarted ? 0 : TITLE_ANIM_DELAY_MS);
            }

            mTitleBar.setText(title);
        }

        private void adjustTitleUrlBarPadding() {
            // Title/URL container height should get bigger to meet GAR guideline. Distribute
            // the diff evenly as a padding of title/URL view to keep them staying where
            // they are, and only make the content-wrapping container get bigger accordingly.
            // TODO(jinsukkim): Make the animation work for further navigation like
            //     title/url -> url -> title/url 1) the url-only view should be centered,
            //     and 2) the animation for the transition to title/url should work as well.
            int padding = (mTouchTargetSize - mTitleUrlContainer.getHeight()) / 2;
            mTitleUrlContainer.setMinimumHeight(mTouchTargetSize);
            mUrlBar.setPadding(0, 0, 0, padding);
            mTitleBar.setPadding(0, padding, 0, 0);

            // When the security icon is nested, it will be in the same container as the Url Bar.
            // So, they should have the same bottom padding to keep it aligned.
            if (shouldNestSecurityIcon()) {
                mSecurityButton.setPaddingRelative(
                        mSecurityButton.getPaddingStart(),
                        mSecurityButton.getPaddingTop(),
                        mSecurityButton.getPaddingEnd(),
                        padding);
            }
        }

        private void updateUrlBar() {
            if (mCurrentlyShowingBranding) return;
            Tab tab = getCurrentTab();
            if (tab == null) {
                mUrlCoordinator.setUrlBarData(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
                return;
            }

            if (mState == STATE_TITLE_ONLY) {
                if (!TextUtils.isEmpty(mLocationBarDataProvider.getTitle())) {
                    updateTitleBar();
                }
            }

            GURL publisherUrl = TrustedCdn.getPublisherUrl(tab);
            GURL url = getUrl();
            final CharSequence displayText;
            final int originStart;
            final int originEnd;
            if (!mOmniboxEnabled && publisherUrl != null) {
                String plainDisplayText =
                        getContext()
                                .getString(
                                        R.string.custom_tab_amp_publisher_url,
                                        UrlUtilities.extractPublisherFromPublisherUrl(
                                                publisherUrl));
                SpannableString formattedDisplayText =
                        SpanApplier.applySpans(
                                plainDisplayText,
                                new SpanInfo("<pub>", "</pub>", ORIGIN_SPAN),
                                new SpanInfo(
                                        "<bg>",
                                        "</bg>",
                                        new ForegroundColorSpan(mTint.getDefaultColor())));
                originStart = formattedDisplayText.getSpanStart(ORIGIN_SPAN);
                originEnd = formattedDisplayText.getSpanEnd(ORIGIN_SPAN);
                formattedDisplayText.removeSpan(ORIGIN_SPAN);
                displayText = formattedDisplayText;
            } else {
                UrlBarData urlBarData = mLocationBarDataProvider.getUrlBarData();
                originStart = 0;
                if (mOmniboxEnabled) {
                    displayText = urlBarData.displayText;
                    originEnd = urlBarData.originEndIndex;
                } else if (urlBarData.displayText.length() != 0) {
                    displayText =
                            urlBarData.displayText.subSequence(
                                    urlBarData.originStartIndex, urlBarData.originEndIndex);
                    originEnd = displayText.length();
                } else {
                    displayText = "";
                    originEnd = 0;
                }
            }

            mUrlCoordinator.setUrlBarData(
                    UrlBarData.create(url, displayText, originStart, originEnd, url.getSpec()),
                    UrlBar.ScrollType.SCROLL_TO_TLD,
                    SelectionState.SELECT_ALL);

            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                BrowserContextHandle originalBrowserContext =
                        tab.isOffTheRecord()
                                ? Profile.fromWebContents(webContents).getOriginalProfile()
                                : null;
                if (mCookieControlsBridge != null) {
                    mCookieControlsBridge.updateWebContents(
                            webContents,
                            originalBrowserContext,
                            Profile.fromWebContents(webContents).isIncognitoBranded());
                } else {
                    mCookieControlsBridge =
                            new CookieControlsBridge(
                                    this,
                                    webContents,
                                    originalBrowserContext,
                                    Profile.fromWebContents(webContents).isIncognitoBranded());
                }
            }
        }

        private GURL getUrl() {
            Tab tab = getCurrentTab();
            if (tab == null) return GURL.emptyGURL();

            GURL publisherUrl = TrustedCdn.getPublisherUrl(tab);
            return publisherUrl != null ? publisherUrl : tab.getUrl();
        }

        private void updateColors() {
            updateOmniboxBackground();
            updateButtonsTint();

            if (mUrlCoordinator.setBrandedColorScheme(mBrandedColorScheme)) {
                // Update the URL to make it use the new color scheme.
                updateUrlBar();
            }

            mTitleBar.setTextColor(
                    OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                            getContext(), mBrandedColorScheme));
        }

        private void updateOmniboxBackground() {
            if (mOmniboxBackground == null) return;
            mOmniboxBackground.setTint(getBackgroundColor());
        }

        @Override
        public void setShowTitle(boolean showTitle) {
            if (mCurrentlyShowingBranding) {
                mAfterBrandingRunnables[KEY_UPDATE_TITLE_POST_BRANDING] =
                        () -> setShowTitleIgnoreBranding(showTitle);
                return;
            }
            setShowTitleIgnoreBranding(showTitle);
        }

        private void setShowTitleIgnoreBranding(boolean showTitle) {
            if (showTitle) {
                if (mState == STATE_EMPTY) {
                    mState = STATE_TITLE_ONLY;
                } else {
                    mState = STATE_DOMAIN_AND_TITLE;
                }
                if (shouldNestSecurityIcon()) {
                    int width =
                            getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.custom_tabs_security_icon_width_nested);
                    mSecurityButton.getLayoutParams().width = width;
                    int paddingLeft =
                            getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.custom_tabs_security_icon_padding_left_nested);
                    int paddingRight =
                            getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.custom_tabs_security_icon_padding_right_nested);
                    mSecurityButton.setPadding(
                            paddingLeft,
                            mSecurityButton.getPaddingTop(),
                            paddingRight,
                            mSecurityButton.getPaddingBottom());
                    mAnimDelegate.setSecurityButtonWidth(width);
                }
                mAnimDelegate.prepareTitleAnim(mUrlBar, mTitleBar);
                setUrlBarVisuals(Gravity.BOTTOM, 0, R.dimen.custom_tabs_url_text_size);
            } else {
                mState = STATE_DOMAIN_ONLY;
                mTitleBar.setVisibility(View.GONE);

                // URL bar height should be as big as the touch target size when shown alone.
                // Update its minHeight and center it vertically.
                setUrlBarVisuals(
                        Gravity.CENTER_VERTICAL,
                        mTouchTargetSize,
                        R.dimen.custom_tabs_title_text_size);
            }
            assumeNonNull(mLocationBarModel).notifyTitleChanged();
        }

        private void setUrlBarVisuals(int gravity, int minHeight, int sizeId) {
            var params = (LinearLayout.LayoutParams) mUrlBar.getLayoutParams();
            params.gravity = gravity;
            mUrlBar.setLayoutParams(params);
            mUrlBar.setTextSize(TypedValue.COMPLEX_UNIT_PX, getResources().getDimension(sizeId));
            mUrlBar.setMinimumHeight(minHeight);
            mTitleUrlContainer.setMinimumHeight(0);
        }

        @Override
        public View getContainerView() {
            return CustomTabToolbar.this;
        }

        @Override
        public View getSecurityIconView() {
            return mSecurityButton;
        }

        @Override
        public void backKeyPressed() {
            assert false : "The URL bar should never take focus in CCTs.";
        }

        @SuppressWarnings("NullAway")
        @Override
        public void destroy() {
            logActionButtonComboMetric();
            if (mTaskHandler != null) {
                mTaskHandler.removeCallbacksAndMessages(null);
            }
            if (mCallbackController != null) {
                mCallbackController.destroy();
                mCallbackController = null;
            }
            if (mLocationBarDataProvider != null) {
                mLocationBarDataProvider.removeObserver(this);
                mLocationBarDataProvider = null;
            }
            if (mBrandingOverlayCoordinator != null) {
                mBrandingOverlayCoordinator.destroy();
                mBrandingOverlayCoordinator = null;
            }
        }

        private void logActionButtonComboMetric() {
            int logActions = CctActions.INVALID;
            if (mCustomButtonsForMetric.size() == 2) {
                boolean hasShare =
                        mCustomButtonsForMetric.get(0) == ButtonType.CCT_SHARE_BUTTON
                                || mCustomButtonsForMetric.get(1) == ButtonType.CCT_SHARE_BUTTON;
                boolean hasOib =
                        mCustomButtonsForMetric.get(0) == ButtonType.CCT_OPEN_IN_BROWSER_BUTTON
                                || mCustomButtonsForMetric.get(1)
                                        == ButtonType.CCT_OPEN_IN_BROWSER_BUTTON;
                if (hasShare && hasOib) {
                    logActions = CctActions.SHARE_OIB;
                } else if (hasShare) {
                    logActions = CctActions.SHARE_CUSTOM;
                } else if (hasOib) {
                    logActions = CctActions.OIB_CUSTOM;
                } else {
                    logActions = CctActions.CUSTOM_ONLY;
                }
            } else if (mCustomButtonsForMetric.size() == 1) {
                int customActionType = mCustomButtonsForMetric.get(0);
                int optionalActionType = mOptionalButtonForMetric;
                switch (customActionType) {
                    case ButtonType.CCT_SHARE_BUTTON:
                        switch (optionalActionType) {
                            case UNKNOWN:
                                logActions = CctActions.SHARE_ONLY;
                                break;
                            case OPEN_IN_BROWSER:
                                logActions = CctActions.SHARE_OIB;
                                break;
                            default:
                                logActions = CctActions.SHARE_MTB;
                                break;
                        }
                        break;
                    case ButtonType.CCT_OPEN_IN_BROWSER_BUTTON:
                        switch (optionalActionType) {
                            case UNKNOWN:
                                logActions = CctActions.OIB_ONLY;
                                break;
                            case SHARE:
                                logActions = CctActions.SHARE_OIB;
                                break;
                            default:
                                logActions = CctActions.OIB_MTB;
                                break;
                        }
                        break;
                    case ButtonType.OTHER:
                    case ButtonType.EXTERNAL:
                        switch (optionalActionType) {
                            case UNKNOWN:
                                logActions = CctActions.CUSTOM_ONLY;
                                break;
                            case SHARE:
                                logActions = CctActions.SHARE_CUSTOM;
                                break;
                            case OPEN_IN_BROWSER:
                                logActions = CctActions.OIB_CUSTOM;
                                break;
                            default:
                                logActions = CctActions.CUSTOM_MTB;
                                break;
                        }
                        break;
                }
            } else {
                switch (mOptionalButtonForMetric) {
                    case UNKNOWN:
                        logActions = CctActions.NONE;
                        break;
                    case SHARE:
                        logActions = CctActions.SHARE_ONLY;
                        break;
                    case OPEN_IN_BROWSER:
                        logActions = CctActions.OIB_ONLY;
                        break;
                    default:
                        logActions = CctActions.MTB_ONLY;
                        break;
                }
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTab.AdaptiveToolbarButton.ActionButtons",
                    logActions,
                    CctActions.MAX_VALUE);
        }

        @Override
        public void showUrlBarCursorWithoutFocusAnimations() {}

        @Override
        public void clearUrlBarCursorWithoutFocusAnimations() {}

        @Override
        public void selectAll() {}

        @Override
        public void revertChanges() {}

        @Override
        public @Nullable OmniboxStub getOmniboxStub() {
            return null;
        }

        @Override
        public UrlBarData getUrlBarData() {
            return mUrlCoordinator.getUrlBarData();
        }

        @Override
        public @Nullable OmniboxSuggestionsVisualState getOmniboxSuggestionsVisualState() {
            return null;
        }

        @Override
        public boolean onLongClick(View v) {
            if (v == mTitleUrlContainer) {
                Tab tab = getCurrentTab();
                if (tab == null) return false;
                Clipboard.getInstance().copyUrlToClipboard(tab.getOriginalUrl());
                return true;
            }
            return false;
        }

        void setAnimDelegateForTesting(CustomTabToolbarAnimationDelegate animDelegate) {
            mAnimDelegate = animDelegate;
        }

        void setTitleUrlContainerForTesting(View titleUrlContainer) {
            mTitleUrlContainer = titleUrlContainer;
        }

        void setIphControllerForTesting(PageInfoIphController pageInfoIphController) {
            mPageInfoIphController = pageInfoIphController;
        }

        void setOmniboxParams(OmniboxParams omniboxParams) {
            assert omniboxParams != null;
            mOmniboxEnabled = true;
            mOmniboxBackground =
                    AppCompatResources.getDrawable(
                            getContext(), R.drawable.custom_tabs_url_bar_omnibox_bg);
            mOmniboxBackground.mutate();
            mOmniboxBackground.setTint(
                    ContextCompat.getColor(getContext(), R.color.toolbar_text_box_bg_color));
            mLocationBarFrameLayout.setBackground(mOmniboxBackground);
            var lp = mLocationBarFrameLayout.getLayoutParams();
            lp.height =
                    getResources()
                            .getDimensionPixelSize(R.dimen.custom_tabs_location_bar_active_height);
            mLocationBarFrameLayout.setLayoutParams(lp);

            lp = mUrlBar.getLayoutParams();
            lp.height =
                    getResources().getDimensionPixelSize(R.dimen.custom_tabs_url_bar_active_height);
            mUrlBar.setLayoutParams(lp);

            View urlBarWrapper = findViewById(R.id.url_bar_wrapper);
            FrameLayout.LayoutParams locationBarLayoutParams =
                    (FrameLayout.LayoutParams) urlBarWrapper.getLayoutParams();
            locationBarLayoutParams.gravity = Gravity.CENTER_VERTICAL;
            urlBarWrapper.setLayoutParams(locationBarLayoutParams);

            mTitleUrlContainer.setPadding(
                    mTitleUrlContainer.getPaddingLeft(),
                    mTitleUrlContainer.getPaddingTop(),
                    getResources().getDimensionPixelSize(R.dimen.toolbar_edge_padding),
                    mTitleUrlContainer.getPaddingBottom());

            mTitleUrlContainer.setOnClickListener(
                    v -> {
                        RecordUserAction.record("CustomTabs.OmniboxClicked");
                        var tab = assumeNonNull(getCurrentTab());
                        if (omniboxParams.tapHandlerWithVerification.apply(tab)) {
                            return;
                        }
                        if (omniboxParams.tapHandler != null) {
                            omniboxParams.tapHandler.accept(tab);
                        } else {
                            var intent =
                                    omniboxParams
                                            .searchClient
                                            .newIntentBuilder()
                                            .setPageUrl(tab.getUrl())
                                            .setReferrer(omniboxParams.clientPackageName)
                                            .setIncognito(tab.isIncognitoBranded())
                                            .setResolutionType(ResolutionType.SEND_TO_CALLER)
                                            .build();
                            omniboxParams.searchClient.requestOmniboxForResult(intent);
                        }
                    });

            mUrlBar.setAccessibilityDelegate(
                    new View.AccessibilityDelegate() {
                        @Override
                        public void onInitializeAccessibilityNodeInfo(
                                View host, AccessibilityNodeInfo info) {
                            super.onInitializeAccessibilityNodeInfo(host, info);
                            info.setClickable(true);
                            info.setLongClickable(true);
                            info.setEnabled(true);
                            info.setEditable(false);
                        }
                    });
            updateAnimationsForOmnibox();
        }

        private void updateAnimationsForOmnibox() {
            mSecurityButton = mLocationBarFrameLayout.findViewById(R.id.security_button);
            mSecurityButton.setVisibility(VISIBLE);
            mLocationBarFrameLayout.findViewById(R.id.security_icon).setVisibility(GONE);
            mAnimDelegate.setSecurityButton(mSecurityButton);
            mAnimDelegate.setSecurityButtonWidth(0);
        }

        private boolean shouldNestSecurityIcon() {
            return ChromeFeatureList.sCctNestedSecurityIcon.isEnabled() && !mOmniboxEnabled;
        }
    }

    public void setToolbarWidthForTesting(int toolbarWidthPx) {
        mButtonVisibilityRule.setToolbarWidth(toolbarWidthPx);
    }

    boolean isMaximizeButtonEnabledForTesting() {
        return mMaximizeButtonEnabled;
    }

    @Nullable OptionalButtonCoordinator getOptionalButtonCoordinatorForTesting() {
        return mLocationBar.mOptionalButtonCoordinator;
    }

    @AdaptiveToolbarButtonVariant
    int getVariantForFallbackMenuForTesting() {
        return mVariantForFallbackMenu;
    }
}
