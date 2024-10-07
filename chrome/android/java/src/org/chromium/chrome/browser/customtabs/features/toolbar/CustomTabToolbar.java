// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;

import static org.chromium.base.MathUtils.interpolate;

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
import android.os.Handler;
import android.os.Looper;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
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
import androidx.annotation.Dimension;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent.CloseButtonPosition;
import androidx.core.view.MarginLayoutParamsCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabFeatureOverridesManager;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingDelegate;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayCoordinator;
import org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.PageInfoIPHController;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivityClientImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
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

import java.util.Optional;
import java.util.function.Consumer;

/** The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs. */
public class CustomTabToolbar extends ToolbarLayout implements View.OnLongClickListener {
    private static final Object ORIGIN_SPAN = new Object();
    private ImageView mIncognitoImageView;
    private LinearLayout mCustomActionButtons;
    private LinearLayout mCloseMinimizeLayout;
    private ImageButton mCloseButton;
    private ImageButton mMinimizeButton;
    private MenuButton mMenuButton;
    // This View will be non-null only for bottom sheet custom tabs.
    private Drawable mHandleDrawable;

    // Color scheme and tint that will be applied to icons and text.
    private @BrandedColorScheme int mBrandedColorScheme;
    private ColorStateList mTint;

    private ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private GURL mFirstUrl;

    private final CustomTabLocationBar mLocationBar = new CustomTabLocationBar();
    private LocationBarModel mLocationBarModel;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private @Nullable CustomTabCaptureStateToken mLastCustomTabCaptureStateToken;
    private ObserverList<Callback<Integer>> mContainerVisibilityChangeObserverList =
            new ObserverList<>();
    private @Nullable CustomTabFeatureOverridesManager mFeatureOverridesManager;

    // Whether the maximization button should be shown when it can. Set to {@code true}
    // while the side sheet is running with the maximize button option on.
    private boolean mMaximizeButtonEnabled;
    private boolean mMinimizeButtonEnabled;

    private OnClickListener mCloseClickListener;
    private CookieControlsBridge mCookieControlsBridge;
    private boolean mShouldHighlightCookieControlsIcon;
    private boolean mCookieControlsVisible;
    private boolean mThirdPartyCookiesBlocked;
    private int mBlockingStatus3pcd;

    private final Handler mTaskHandler = new Handler();

    // The resource ID of the most recently set security icon. Used for testing since
    // VectorDrawables can't be straightforwardly tested for equality..
    private int mSecurityIconResourceForTesting;

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

    private HandleStrategy mHandleStrategy;
    private @CloseButtonPosition int mCloseButtonPosition;

    /** Callback used to notify the maximize button on side sheet PCCT click event. */
    public interface MaximizeButtonCallback {
        /**
         * @return {@code true} if the PCCT gets maximized. {@code false} if restored.
         */
        boolean onClick();
    }

    /** Constructor for getting this class inflated from an xml layout file. */
    public CustomTabToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTint = ChromeColors.getPrimaryIconTint(getContext(), false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        final int backgroundColor = ChromeColors.getDefaultThemeColor(getContext(), false);
        setBackground(new ColorDrawable(backgroundColor));
        mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;

        mIncognitoImageView = findViewById(R.id.incognito_cct_logo_image_view);
        mCustomActionButtons = findViewById(R.id.action_buttons);
        mCloseButton = findViewById(R.id.close_button);
        mCloseButton.setOnLongClickListener(this);
        mCloseMinimizeLayout = findViewById(R.id.close_minimize_layout);
        mMenuButton = findViewById(R.id.menu_button_wrapper);

        mLocationBar.onFinishInflate(this);

        if (!ChromeFeatureList.sCctIntentFeatureOverrides.isEnabled()) {
            maybeInitMinimizeButton();
        }

        // Set hover tooltip texts for toolbar buttons.
        super.setTooltipTextForToolbarButtons();
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        mLocationBar.onNativeLibraryReady();
    }

    @Override
    protected void setCloseButtonImageResource(Drawable drawable) {
        mCloseButton.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
        mCloseButton.setImageDrawable(drawable);
        if (drawable != null) {
            updateButtonTint(mCloseButton);
        }
    }

    @Override
    protected void setCustomTabCloseClickHandler(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    @Override
    protected void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        ImageButton button =
                (ImageButton)
                        LayoutInflater.from(getContext())
                                .inflate(
                                        R.layout.custom_tabs_toolbar_button,
                                        mCustomActionButtons,
                                        false);
        button.setOnLongClickListener(this);
        button.setOnClickListener(listener);
        button.setVisibility(VISIBLE);

        updateCustomActionButtonVisuals(button, drawable, description);

        // Add the view at the beginning of the child list.
        mCustomActionButtons.addView(button, 0);
    }

    @Override
    protected void updateCustomActionButton(int index, Drawable drawable, String description) {
        ImageButton button =
                (ImageButton)
                        mCustomActionButtons.getChildAt(
                                mCustomActionButtons.getChildCount() - 1 - index);
        assert button != null;
        updateCustomActionButtonVisuals(button, drawable, description);
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
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (maximizeButton == null) {
            ViewStub maximizeButtonStub = findViewById(R.id.maximize_button_stub);
            maximizeButtonStub.inflate();
            maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        }
        mMaximizeButtonEnabled = true;
        setMaximizeButtonDrawable(maximizedOnInit);
        maximizeButton.setOnClickListener((v) -> setMaximizeButtonDrawable(callback.onClick()));

        // The visibility will set after the location bar completes its layout. But there are
        // cases where the location bar layout gets already completed. Trigger the visibility
        // update manually here.
        setMaximizeButtonVisibility();
    }

    public void setFeatureOverridesManager(CustomTabFeatureOverridesManager manager) {
        mFeatureOverridesManager = manager;

        maybeInitMinimizeButton();
    }

    /**
     * Sets the {@link CustomTabMinimizeDelegate} to allow the toolbar to minimize the tab.
     *
     * @param delegate The {@link CustomTabMinimizeDelegate}.
     */
    public void setMinimizeDelegate(@NonNull CustomTabMinimizeDelegate delegate) {
        mMinimizeButton.setOnClickListener(view -> delegate.minimize());
    }

    /**
     * Enables the interactive Omnibox in CCT.
     *
     * @param clientPackageName the package name of the custom tabs embedder.
     * @param tapHandler a handler for taps on the omnibox, or null if the default handler should be
     *     used.
     */
    public void setOmniboxEnabled(String clientPackageName, @Nullable Consumer<Tab> tapHandler) {
        mLocationBar.setOmniboxEnabled(clientPackageName, tapHandler);
    }

    private void setButtonsVisibility() {
        setMaximizeButtonVisibility();
        setMinimizeButtonVisibility();
    }

    private void setMaximizeButtonVisibility() {
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (!mMaximizeButtonEnabled || maximizeButton == null) {
            if (maximizeButton != null) maximizeButton.setVisibility(View.GONE);
            setUrlTitleBarMargin(0);
            return;
        }

        // Find the title/url width threshold that turns the maximize button visible.
        int containerWidthPx = mLocationBar.mTitleUrlContainer.getWidth();
        if (containerWidthPx == 0) return;

        int maximizeButtonWidthPx =
                getResources().getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        int titleUrlPaddingEndPx =
                getResources().getDimensionPixelSize(R.dimen.toolbar_edge_padding);
        if (containerWidthPx < maximizeButtonWidthPx * 2 - titleUrlPaddingEndPx) {
            // We expect to see at least as much URL text as the width of the maximize button.
            // Hide the button if we can't.
            maximizeButton.setVisibility(View.GONE);
        } else {
            mLocationBar.removeButtonsVisibilityUpdater();

            // Take some space from the title/url for maximization button.
            setUrlTitleBarMargin(maximizeButtonWidthPx);
            maximizeButton.setVisibility(View.VISIBLE);
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
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        maximizeButton.setOnClickListener(null);
        maximizeButton.setVisibility(View.GONE);
        mMaximizeButtonEnabled = false;
    }

    @VisibleForTesting
    void maybeInitMinimizeButton() {
        if (!MinimizedFeatureUtils.isMinimizedCustomTabAvailable(
                getContext(), mFeatureOverridesManager)) {
            return;
        }

        ViewStub minimizeButtonStub = findViewById(R.id.minimize_button_stub);
        minimizeButtonStub.inflate();
        ImageButton minimizeButton = findViewById(R.id.custom_tabs_minimize_button);
        var d =
                UiUtils.getTintedDrawable(
                        getContext(), MinimizedFeatureUtils.getMinimizeIcon(), mTint);
        minimizeButton.setTag(R.id.custom_tabs_toolbar_tintable, true);
        minimizeButton.setImageDrawable(d);
        updateButtonTint(minimizeButton);
        minimizeButton.setOnLongClickListener(this);
        mMinimizeButtonEnabled = true;
        mMinimizeButton = minimizeButton;
    }

    private void setMinimizeButtonVisibility() {
        if (mMinimizeButton == null) return;

        if (!mMinimizeButtonEnabled || isInMultiWindowMode()) {
            if (mMinimizeButton.getVisibility() != View.GONE) {
                mMinimizeButton.setVisibility(View.GONE);
                maybeAdjustButtonSpacingForCloseButtonPosition();
            }
            return;
        }

        // Find the title/url width threshold that turns the minimize button visible.
        int containerWidthPx = mLocationBar.mTitleUrlContainer.getWidth();
        int minUrlWidthPx =
                getResources().getDimensionPixelSize(R.dimen.location_bar_min_url_width);
        if (containerWidthPx == 0) return;
        if (containerWidthPx < minUrlWidthPx) {
            // We expect to see at least as much URL text as the width of the minimize button.
            // Hide the button if we can't.
            mMinimizeButton.setVisibility(View.GONE);
        } else {
            mMinimizeButton.setVisibility(View.VISIBLE);
            mLocationBar.removeButtonsVisibilityUpdater();
        }
        updateToolbarLayoutMargin();
    }

    private boolean isInMultiWindowMode() {
        Tab currentTab = getCurrentTab();
        if (currentTab == null) return false;

        Activity activity = currentTab.getWindowAndroid().getActivity().get();
        return MultiWindowUtils.getInstance().isInMultiWindowMode(activity);
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
        setMinimizeButtonVisibility();
    }

    /**
     * @return The custom action button with the given {@code index}. For test purpose only.
     * @param index The index of the custom action button to return.
     */
    public ImageButton getCustomActionButtonForTest(int index) {
        return (ImageButton) mCustomActionButtons.getChildAt(index);
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
        mLocationBarModel.notifyTitleChanged();
        if (mLocationBar.isShowingTitleOnly()) {
            if (mFirstUrl == null || mFirstUrl.isEmpty()) {
                mFirstUrl = getToolbarDataProvider().getTab().getUrl();
            } else {
                if (mFirstUrl.equals(getToolbarDataProvider().getTab().getUrl())) return;
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
        updateButtonTint(mCloseButton);
        if (mMinimizeButton != null) {
            updateButtonTint(mMinimizeButton);
        }
        int numCustomActionButtons = mCustomActionButtons.getChildCount();
        for (int i = 0; i < numCustomActionButtons; i++) {
            updateButtonTint((ImageButton) mCustomActionButtons.getChildAt(i));
        }
        ImageButton maximizeButton = findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (maximizeButton != null) updateButtonTint(maximizeButton);
        updateButtonTint(mLocationBar.getSecurityButton());
    }

    private void updateButtonTint(ImageButton button) {
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
        final View minButton = findViewById(R.id.custom_tabs_minimize_button);
        final ViewGroup closeMinButton = (ViewGroup) closeButton.getParent();
        final int closeButtonIndex = indexOfChild(closeButton);
        final View menuButton = findViewById(R.id.menu_button_wrapper);
        final int menuButtonIndex = indexOfChild(menuButton);
        final ViewGroup.LayoutParams menuButtonLayoutParams = menuButton.getLayoutParams();
        removeViewAt(menuButtonIndex);
        addView(menuButton, closeButtonIndex, menuButtonLayoutParams);
        closeMinButton.removeView(closeButton);
        if (minButton != null) {
            closeMinButton.removeView(minButton);
        }

        if (MinimizedFeatureUtils.isMinimizedCustomTabAvailable(
                        getContext(), mFeatureOverridesManager)
                && minButton != null) {
            closeMinButton.addView(minButton);
        }
        closeMinButton.addView(closeButton);
    }

    private void maybeAdjustButtonSpacingForCloseButtonPosition() {
        if (mCloseButtonPosition != CLOSE_BUTTON_POSITION_END) return;

        final @Dimension int buttonWidth =
                getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        final FrameLayout.LayoutParams menuButtonLayoutParams =
                (FrameLayout.LayoutParams) mMenuButton.getLayoutParams();
        menuButtonLayoutParams.width = buttonWidth;
        menuButtonLayoutParams.gravity = Gravity.CENTER_VERTICAL | Gravity.START;
        mMenuButton.setLayoutParams(menuButtonLayoutParams);
        mMenuButton.setPaddingRelative(0, 0, 0, 0);

        FrameLayout.LayoutParams closeMinLayout =
                (FrameLayout.LayoutParams) mCloseMinimizeLayout.getLayoutParams();
        closeMinLayout.gravity = Gravity.CENTER_VERTICAL | Gravity.END;
        closeMinLayout.setMarginStart(closeMinLayout.getMarginEnd());
        closeMinLayout.setMarginEnd(0);
        mCloseMinimizeLayout.setLayoutParams(closeMinLayout);

        FrameLayout.LayoutParams actionButtonsLayoutParams =
                (FrameLayout.LayoutParams) mCustomActionButtons.getLayoutParams();
        if (MinimizedFeatureUtils.isMinimizedCustomTabAvailable(
                getContext(), mFeatureOverridesManager)) {
            actionButtonsLayoutParams.setMarginEnd(
                    mMinimizeButton == null || mMinimizeButton.getVisibility() == View.GONE
                            ? buttonWidth
                            : buttonWidth * 2);
            var lpTitle = (ViewGroup.MarginLayoutParams) mLocationBar.mTitleBar.getLayoutParams();
            var lpUrl = (ViewGroup.MarginLayoutParams) mLocationBar.mUrlBar.getLayoutParams();
            LayoutParams lp = (LayoutParams) mLocationBar.getLayout().getLayoutParams();
            // Prevent URL and title from bleeding over minimize button
            lpTitle.setMarginEnd(buttonWidth);
            lpUrl.setMarginEnd(buttonWidth);
            lp.setMarginStart(buttonWidth);
            if (getResources().getConfiguration().getLayoutDirection()
                    == View.LAYOUT_DIRECTION_RTL) {
                var lpSecurity =
                        (ViewGroup.MarginLayoutParams)
                                mLocationBar.getSecurityIconView().getLayoutParams();
                var lpTitleUrlContainer =
                        (ViewGroup.MarginLayoutParams)
                                mLocationBar.mTitleUrlContainer.getLayoutParams();
                lpTitle.setMarginEnd(0);
                lpUrl.setMarginEnd(0);
                if (mMinimizeButton != null && mMinimizeButton.getVisibility() != View.GONE) {
                    lpSecurity.leftMargin = buttonWidth;
                    lpTitleUrlContainer.leftMargin += buttonWidth;
                    mLocationBar.mTitleUrlContainer.setLayoutParams(lpTitleUrlContainer);
                } else {
                    // No minimize button, don't need the extra affordance since minimize button is
                    // gone
                    lpSecurity.leftMargin = 0;
                    mLocationBar.updateLeftMarginOfTitleUrlContainer();
                }
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
        final boolean shouldShowIncognitoIcon = isIncognitoBranded();
        mIncognitoImageView.setVisibility(shouldShowIncognitoIcon ? VISIBLE : GONE);

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
            // close_minimize_layout is the first child view in the toolbar.
            // It includes two buttons when minimized is enabled, but when they are positioned at
            // the end our start margin is doubly large.
            if (mMinimizeButtonEnabled
                    && mCloseButtonPosition == CLOSE_BUTTON_POSITION_END
                    && i == 0) {
                width /= 2;
            }
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
        mLocationBarModel.notifyTitleChanged();
        mLocationBarModel.notifyUrlChanged();
        mLocationBarModel.notifyPrimaryColorChanged();
    }

    @Override
    public ColorDrawable getBackground() {
        return (ColorDrawable) super.getBackground();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    @Override
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.cancel();

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
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        maybeSwapCloseAndMenuButtons();
        updateToolbarLayoutMargin();
        maybeAdjustButtonSpacingForCloseButtonPosition();
        setMaximizeButtonVisibility();
        setMinimizeButtonVisibility();

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
        if (v == mCloseButton || v == mMinimizeButton || v.getParent() == mCustomActionButtons) {
            return Toast.showAnchoredToast(getContext(), v, v.getContentDescription());
        }
        return false;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
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
        // In addition to removing the menu button, we also need to remove the margin on the custom
        // action button.
        ViewGroup.MarginLayoutParams p =
                (ViewGroup.MarginLayoutParams) mCustomActionButtons.getLayoutParams();
        p.setMarginEnd(0);
        mCustomActionButtons.setLayoutParams(p);
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            CustomTabCaptureStateToken currentToken = generateCaptureStateToken();
            final @ToolbarSnapshotDifference int difference =
                    currentToken.getAnyDifference(mLastCustomTabCaptureStateToken);
            if (difference == ToolbarSnapshotDifference.NONE) {
                return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SNAPSHOT_SAME);
            } else {
                return CaptureReadinessResult.readyWithSnapshotDifference(difference);
            }
        } else {
            return CaptureReadinessResult.unknown(/* isReady= */ true);
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
    public void setToolbarColorObserver(@NonNull ToolbarColorObserver toolbarColorObserver) {
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

    private static boolean shouldNestSecurityIcon() {
        return ChromeFeatureList.sCctNestedSecurityIcon.isEnabled();
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
        private Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
        private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
        private UrlBarCoordinator mUrlCoordinator;
        private TabCreator mTabCreator;

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

        private final Runnable[] mAfterBrandingRunnables = new Runnable[TOTAL_POST_BRANDING_KEYS];
        private final View.OnLayoutChangeListener mButtonsVisibilityUpdater =
                (v, l, t, r, b, ol, ot, or, ob) -> setButtonsVisibility();
        private boolean mCurrentlyShowingBranding;
        private boolean mBrandingStarted;
        private boolean mOmniboxEnabled;
        private Drawable mOmniboxBackground;
        private CallbackController mCallbackController = new CallbackController();
        // Cached the state before branding start so we can reset to the state when its done.
        private @Nullable Integer mPreBandingState;
        private PageInfoIPHController mPageInfoIPHController;
        private int mTouchTargetSize;
        private ToolbarBrandingOverlayCoordinator mBrandingOverlayCoordinator;

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
        public boolean unfocusUrlBarOnBackPressed() {
            return false;
        }

        @Override
        public void showBrandingLocationBar() {
            mBrandingStarted = true;

            if (ChromeFeatureList.sCctRevampedBranding.isEnabled()) {
                ViewStub stub = findViewById(R.id.branding_stub);

                if (stub != null) {
                    PropertyModel model =
                            new PropertyModel.Builder(ToolbarBrandingOverlayProperties.ALL_KEYS)
                                    .with(
                                            ToolbarBrandingOverlayProperties.COLOR_DATA,
                                            new ToolbarBrandingOverlayProperties.ColorData(
                                                    getBackground().getColor(),
                                                    mBrandedColorScheme))
                                    .build();
                    mBrandingOverlayCoordinator =
                            new ToolbarBrandingOverlayCoordinator(stub, model);

                    return;
                }
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

            if (ChromeFeatureList.sCctRevampedBranding.isEnabled()) {
                if (mBrandingOverlayCoordinator != null) {
                    mBrandingOverlayCoordinator.hideAndDestroy();
                }
            }

            recoverFromRegularState();
            runAfterBrandingRunnables();
            mAnimDelegate.setUseRotationSecurityButtonTransition(false);

            int token = mBrowserControlsVisibilityDelegate.showControlsPersistent();
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
                boolean controlsVisible,
                boolean protectionsOn,
                int enforcement,
                int blockingStatus,
                long expiration) {
            mCookieControlsVisible = controlsVisible;
            mThirdPartyCookiesBlocked = protectionsOn;
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

        public void onFinishInflate(View container) {
            mUrlBar = container.findViewById(R.id.url_bar);
            mUrlBar.setHint("");
            mUrlBar.setEnabled(false);
            mTitleBar = container.findViewById(R.id.title_bar);
            mLocationBarFrameLayout = container.findViewById(R.id.location_bar_frame_layout);
            mTitleUrlContainer = container.findViewById(R.id.title_url_container);
            mTitleUrlContainer.setOnLongClickListener(this);

            int securityButtonId =
                    shouldNestSecurityIcon() ? R.id.security_icon : R.id.security_button;
            mSecurityButton = container.findViewById(securityButtonId);
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
                            R.dimen.location_bar_icon_width);
            addButtonsVisibilityUpdater();
        }

        private void removeButtonsVisibilityUpdater() {
            mTitleUrlContainer.removeOnLayoutChangeListener(mButtonsVisibilityUpdater);
        }

        private void addButtonsVisibilityUpdater() {
            if (mTitleUrlContainer != null) {
                mTitleUrlContainer.addOnLayoutChangeListener(mButtonsVisibilityUpdater);
            }
        }

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
                            /* windowDelegate= */ null,
                            actionModeCallback,
                            /* focusChangeCallback= */ (unused) -> {},
                            this,
                            new NoOpkeyboardVisibilityDelegate(),
                            isIncognitoBranded(),
                            /* onLongClickListener= */ null);
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
                mAnimDelegate.setTitleAnimationEnabled(false);
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
                mLocationBarModel.notifySecurityStateChanged();
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
            if (!mOmniboxEnabled && shouldNestSecurityIcon()) {
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
            Activity activity = currentTab.getWindowAndroid().getActivity().get();
            if (activity == null) return;
            if (mCurrentlyShowingBranding) return;
            // For now we don't show "store info" row for custom tab.
            new ChromePageInfo(
                            mModalDialogManagerSupplier,
                            TrustedCdn.getContentPublisher(getToolbarDataProvider().getTab()),
                            OpenedFromSource.TOOLBAR,
                            /* storeInfoActionHandlerSupplier= */ null,
                            mEphemeralTabCoordinatorSupplier,
                            mTabCreator)
                    .show(currentTab, ChromePageInfoHighlight.noHighlight());
        }

        @Override
        public View getViewForUrlBackFocus() {
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
        public void onUrlChanged() {
            updateUrlBar();
        }

        @Override
        public void onPageLoadStopped() {
            if (mPageInfoIPHController == null) {
                Tab currentTab = getCurrentTab();
                if (currentTab == null) return;
                Activity activity = currentTab.getWindowAndroid().getActivity().get();
                if (activity == null) return;
                mPageInfoIPHController =
                        new PageInfoIPHController(
                                new UserEducationHelper(
                                        activity,
                                        currentTab.getProfile(),
                                        new Handler(Looper.getMainLooper())),
                                getSecurityIconView());
            }
            if (mBlockingStatus3pcd != CookieBlocking3pcdStatus.NOT_IN3PCD) {
                if (!mCookieControlsVisible || !mThirdPartyCookiesBlocked) return;
                // TODO(b/332761678): Add reminder IPH here.
            } else if (mShouldHighlightCookieControlsIcon) {
                mPageInfoIPHController.showCookieControlsIPH(
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

            int leftMargin = mSecurityButton.getMeasuredWidth();
            LayoutParams lp = (LayoutParams) mTitleUrlContainer.getLayoutParams();

            if (mSecurityButton.getVisibility() == View.GONE) {
                leftMargin -= mSecurityButton.getMeasuredWidth();
            }
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
                progressBar.setBackgroundColor(
                        context.getColor(R.color.progress_bar_bg_color_list));
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
            mLocationBarModel.notifySecurityStateChanged();

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
            if (!shouldNestSecurityIcon() || !isSecureLevel()) {
                securityIconResource =
                        mLocationBarDataProvider.getSecurityIconResource(
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()));
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
        private boolean isSecureLevel() {
            @ConnectionSecurityLevel
            int securityLevel = mLocationBarDataProvider.getSecurityLevel();
            return securityLevel == ConnectionSecurityLevel.SECURE
                    || securityLevel == ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT;
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
            if (publisherUrl != null) {
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
                    originEnd = 0;
                } else if (urlBarData.displayText != null) {
                    displayText =
                            urlBarData.displayText.subSequence(
                                    urlBarData.originStartIndex, urlBarData.originEndIndex);
                    originEnd = displayText.length();
                } else {
                    displayText = null;
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
                    mCookieControlsBridge.updateWebContents(webContents, originalBrowserContext);
                } else {
                    mCookieControlsBridge =
                            new CookieControlsBridge(this, webContents, originalBrowserContext);
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
            @ColorInt int background = getBackground().getColor();
            @ColorInt
            int bg =
                    ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                            getContext(),
                            background,
                            mBrandedColorScheme == BrandedColorScheme.INCOGNITO,
                            /* isCustomTab= */ true);
            mOmniboxBackground.setTint(bg);
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
            mLocationBarModel.notifyTitleChanged();
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

        @Override
        public void destroy() {
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

        @Override
        public void showUrlBarCursorWithoutFocusAnimations() {}

        @Override
        public void clearUrlBarCursorWithoutFocusAnimations() {}

        @Override
        public void selectAll() {}

        @Override
        public void revertChanges() {}

        @Nullable
        @Override
        public OmniboxStub getOmniboxStub() {
            return null;
        }

        @Override
        public UrlBarData getUrlBarData() {
            return mUrlCoordinator.getUrlBarData();
        }

        @Override
        public Optional<OmniboxSuggestionsVisualState> getOmniboxSuggestionsVisualState() {
            return Optional.empty();
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

        void setIPHControllerForTesting(PageInfoIPHController pageInfoIPHController) {
            mPageInfoIPHController = pageInfoIPHController;
        }

        void setOmniboxEnabled(String clientPackageName, @Nullable Consumer<Tab> tapHandler) {
            mOmniboxEnabled = true;
            mOmniboxBackground =
                    AppCompatResources.getDrawable(
                            getContext(), R.drawable.custom_tabs_url_bar_omnibox_bg);
            mOmniboxBackground.mutate();
            mOmniboxBackground.setTint(
                    ChromeColors.getSurfaceColor(getContext(), R.dimen.toolbar_text_box_elevation));
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
                        var tab = getCurrentTab();
                        if (tapHandler != null) {
                            tapHandler.accept(tab);
                        } else {
                            new SearchActivityClientImpl()
                                    .requestOmniboxForResult(
                                            tab.getWindowAndroid().getActivity().get(),
                                            tab.getUrl(),
                                            IntentOrigin.CUSTOM_TAB,
                                            clientPackageName,
                                            tab.isIncognitoBranded());
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
        }
    }

    boolean isMaximizeButtonEnabledForTesting() {
        return mMaximizeButtonEnabled;
    }
}
