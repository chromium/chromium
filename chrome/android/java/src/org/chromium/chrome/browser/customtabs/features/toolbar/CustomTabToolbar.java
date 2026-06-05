// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.base.MathUtils.interpolate;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
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
import androidx.annotation.DimenRes;
import androidx.annotation.Dimension;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
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
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingDelegate;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayCoordinator;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
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
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

/** The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs. */
@NullMarked
public class CustomTabToolbar extends ToolbarLayout implements View.OnLongClickListener {
    private static final String TAG = "CctToolbar";
    private static final Object ORIGIN_SPAN = new Object();
    private ImageView mIncognitoImageView;
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
    // Whether the maximization button should be shown when it can. Set to {@code true}
    // while the side sheet is running with the maximize button option on.
    private boolean mMaximizeButtonEnabled;

    private @Nullable CookieControlsBridge mCookieControlsBridge;
    private boolean mShouldHighlightCookieControlsIcon;
    private Supplier<@Nullable AppMenuHandler> mAppMenuHandler = () -> null;

    private @Nullable AppMenuObserver mAppMenuObserver;
    private final Handler mTaskHandler = new Handler();

    // The resource ID of the most recently set security icon. Used for testing since
    // VectorDrawables can't be straightforwardly tested for equality..
    private int mSecurityIconResourceForTesting;

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
        public @Nullable String clientPackageName;

        /** A handler for taps on the omnibox, or null if the default handler should be used. */
        public @Nullable Consumer<Tab> tapHandler;

        /**
         * A handler for taps on the omnibox. The function returns true if the tap was handled,
         * false otherwise.
         */
        public Function<Tab, Boolean> tapHandlerWithVerification;

        public OmniboxParams(
                SearchActivityClient searchClient,
                @Nullable String clientPackageName,
                @Nullable Consumer<Tab> tapHandler,
                Function<Tab, Boolean> tapHandlerWithVerification) {
            this.searchClient = searchClient;
            this.clientPackageName = clientPackageName;
            this.tapHandler = tapHandler;
            this.tapHandlerWithVerification = tapHandlerWithVerification;
        }
    }

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
    private @AdaptiveToolbarButtonVariant int mVariantForFallbackMenu;

    /** Constructor for getting this class inflated from an xml layout file. */
    public CustomTabToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTint = ChromeColors.getPrimaryIconTint(getContext(), false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        final int backgroundColor =
                ChromeColors.getDefaultThemeColor(getContext(), /* isIncognito= */ false);
        setBackground(new ColorDrawable(backgroundColor));
        mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;

        mIncognitoImageView = findViewById(R.id.incognito_cct_logo_image_view);
        mCustomButtonsParent = findViewById(R.id.action_buttons);
        mCloseButton = findViewById(R.id.close_button);
        if (mCloseButton != null) {
            mCloseButton.setOnLongClickListener(this);
        }
        mMenuButton = findViewById(R.id.menu_button_wrapper);
        mLocationBar.onFinishInflate(this);
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
    public void setAppMenuHandler(Supplier<@Nullable AppMenuHandler> appMenuHandler) {
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

        var handler = mAppMenuHandler.get();
        assumeNonNull(handler);
        handler.setMenuHighlight(menuId, false);
        View menuIcon = mMenuButton.findViewById(R.id.menu_button);
        menuIcon.setContentDescription(
                getContext().getString(R.string.accessibility_custom_tab_menu_with_dot));
        if (mAppMenuObserver != null) handler.removeObserver(mAppMenuObserver);
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
                            handler.setContentDescription(highlightedMenu);
                        }
                    }

                    @Override
                    public void onMenuHighlightChanged(boolean highlighting) {}
                };
        handler.addObserver(mAppMenuObserver);
    }

    private @Nullable Pair<Integer, Integer> getHighlightMenuInfo(
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        return switch (buttonVariant) {
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING -> {
                // Figure out which of the two menu items (enable/disable) appears and needs
                // highlighting.
                // TODO(crbug.com/424807997): Avoid casting.
                var handler = mAppMenuHandler.get();
                assumeNonNull(handler);
                var appMenuDelegate =
                        (AppMenuPropertiesDelegateImpl) handler.getMenuPropertiesDelegate();
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

    @Override
    protected void setCloseButtonImageResource(@Nullable Drawable drawable) {
        mCloseButton.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
        mCloseButton.setImageDrawable(drawable);
        if (drawable != null) {
            updateButtonTint(mCloseButton);
        }
    }

    @Override
    protected void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener, @ButtonType int type) {
    }

    private @Dimension int getDimensionPx(@DimenRes int resId) {
        return getResources().getDimensionPixelSize(resId);
    }

    @Override
    protected void updateCustomActionButton(int index, Drawable drawable, String description) {
    }

    /**
     * Creates and returns a CustomTab-specific LocationBar. This also retains a reference to the
     * passed LocationBarModel.
     *
     * @param locationBarModel {@link LocationBarModel} to be used for accessing LocationBar state.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @param modalDialogManagerSupplier Supplier of {@link ModalDialogManager}.
     * @param ephemeralTabCoordinatorSupplier Supplier of {@link EphemeralTabCoordinator}.
     * @param controlsVisibilityDelegate {@link BrowserStateBrowserControlsVisibilityDelegate} to
     *     show / hide the browser control. Used to ensure toolbar is shown for a certain duration.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     * @return The LocationBar implementation for this CustomTabToolbar.
     */
    public LocationBar createLocationBar(
            LocationBarModel locationBarModel,
            ActionMode.Callback actionModeCallback,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            Supplier<@Nullable EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
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
     * Sets params for the interactive Omnibox in CCT.
     *
     * @param omniboxParams The {@link OmniboxParams} to be used.
     */
    public void setOmniboxParams(OmniboxParams omniboxParams) {
        mLocationBar.setOmniboxParams(omniboxParams);
    }

    /** Resets optional button internal state. */
    public void resetOptionalButtonState() {
        mLocationBar.resetOptionalButtonState(/* resetFallbackMenu= */ true);
    }

    @Override
    public void requestKeyboardFocus() {
        setFocusOnFirstFocusableDescendant(this);
    }

    /**
     * @return The custom action button with the given {@code index}. For test purpose only.
     * @param index The index of the custom action button to return.
     */
    public @Nullable ImageButton getCustomActionButtonForTest(int index) {
        View childView = assumeNonNull(mCustomButtonsParent).getChildAt(index);

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
    public int getTabStripHeightFromResource() {
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

    private void updateButtonsTint() {
        // TODO(crbug.com/402213312): Remove tinting code here once it's fully MVC-ified.
        updateButtonTint(mCloseButton);
        if (mMinimizeButton != null) {
            updateButtonTint(mMinimizeButton);
        }
        if (mCustomButtonsParent != null) {
            int numCustomActionButtons = mCustomButtonsParent.getChildCount();
            for (int i = 0; i < numCustomActionButtons; i++) {
                View actionButton = mCustomButtonsParent.getChildAt(i);
                if (actionButton instanceof ImageButton button) {
                    updateButtonTint(button);
                }
            }
        }
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (maximizeButton != null) updateButtonTint(maximizeButton);
        updateButtonTint(mLocationBar.getSecurityButton());
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

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
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

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
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
        if (v == mCloseButton || v == mMinimizeButton || v.getParent() == mCustomButtonsParent) {
            return Toast.showAnchoredToast(getContext(), v, v.getContentDescription());
        }
        return false;
    }

    @VisibleForTesting
    static String parsePublisherNameFromUrl(GURL url) {
        // TODO(ianwen): Make it generic to parse url from URI path. http://crbug.com/40463066
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
        private Supplier<@Nullable EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
        private Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
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

        private @ColorInt int getBackgroundColor() {
            return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                    getContext(),
                    getBackground().getColor(),
                    mBrandedColorScheme == BrandedColorScheme.INCOGNITO,
                    /* isCustomTab= */ true);
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

        @Initializer
        public void init(
                LocationBarDataProvider locationBarDataProvider,
                Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
                Supplier<@Nullable EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
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
                            /* onLongClickListener= */ null,
                            /* textChangeListener= */ null,
                            /* richTextChangeListener= */ null,
                            /* keyDownListener= */ null);
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
                    getContext()
                            .getColorStateList(
                                    mLocationBarDataProvider.getSecurityIconColorStateList());
            ImageViewCompat.setImageTintList(mSecurityButton, colorStateList);
            mAnimDelegate.updateSecurityButton(R.drawable.chromelogo16);

            mUrlCoordinator.setUrlBarData(
                    UrlBarData.forNonUrlText(
                            getContext().getString(R.string.twa_running_in_chrome)),
                    UrlBar.ScrollType.NO_SCROLL,
                    TextSelection.SELECT_ALL);
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
            }
            if (securityIconResource != 0) {
                ColorStateList colorStateList =
                        getContext()
                                .getColorStateList(
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
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, TextSelection.SELECT_ALL);
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
                    TextSelection.SELECT_ALL);

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
            mUrlCoordinator.setBrandedColorScheme(mBrandedColorScheme);
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
            if (mTaskHandler != null) {
                mTaskHandler.removeCallbacksAndMessages(null);
            }
            if (mCookieControlsBridge != null) {
                mCookieControlsBridge.destroy();
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

        @Override
        public void showUrlBarCursorWithoutFocusAnimations() {}

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

    boolean isMaximizeButtonEnabledForTesting() {
        return mMaximizeButtonEnabled;
    }

    @AdaptiveToolbarButtonVariant
    int getVariantForFallbackMenuForTesting() {
        return mVariantForFallbackMenu;
    }
}
