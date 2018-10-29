// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.os.Parcelable;
import android.support.annotation.Nullable;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusViewCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsList;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * This class represents the location bar where the user types in URLs and
 * search terms.
 */
public class LocationBarLayout extends FrameLayout
        implements OnClickListener, LocationBar, AutocompleteDelegate, FakeboxDelegate,
                   ScrimView.ScrimObserver, LocationBarVoiceRecognitionHandler.Delegate,
                   StatusViewCoordinator.Delegate {
    private static final String TAG = "cr_LocationBar";

    private final int mLightScrimColor;

    /** Params that control how the location bar interacts with the scrim. */
    private ScrimParams mScrimParams;

    protected AppCompatImageButton mDeleteButton;
    protected AppCompatImageButton mMicButton;
    protected View mUrlBar;
    private final boolean mIsTablet;

    protected UrlBarCoordinator mUrlCoordinator;
    protected AutocompleteCoordinator mAutocompleteCoordinator;

    protected ToolbarDataProvider mToolbarDataProvider;
    private ObserverList<UrlFocusChangeListener> mUrlFocusChangeListeners = new ObserverList<>();

    protected boolean mNativeInitialized;

    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();

    protected StatusViewCoordinator mStatusViewCoordinator;

    private String mOriginalUrl = "";

    private WindowAndroid mWindowAndroid;
    private WindowDelegate mWindowDelegate;

    private ScrimView mScrim;

    private boolean mUrlHasFocus;
    protected boolean mUrlFocusChangeInProgress;
    private boolean mUrlFocusedFromFakebox;
    private boolean mUrlFocusedWithoutAnimations;

    private boolean mVoiceSearchEnabled;

    private OmniboxPrerender mOmniboxPrerender;

    private boolean mUseDarkColors;

    private boolean mOmniboxVoiceSearchAlwaysVisible;
    protected float mUrlFocusChangePercent;
    protected LinearLayout mUrlActionContainer;

    protected LocationBarVoiceRecognitionHandler mVoiceRecognitionHandler;

    /**
     * Class to handle input from a hardware keyboard when the focus is on the URL bar. In
     * particular, handle navigating the suggestions list from the keyboard.
     */
    private final class UrlBarKeyListener implements OnKeyListener {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (mAutocompleteCoordinator.handleKeyEvent(keyCode, event)) {
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_BACK) {
                if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                    // Tell the framework to start tracking this event.
                    getKeyDispatcherState().startTracking(event, this);
                    return true;
                } else if (event.getAction() == KeyEvent.ACTION_UP) {
                    getKeyDispatcherState().handleUpEvent(event);
                    if (event.isTracking() && !event.isCanceled()) {
                        backKeyPressed();
                        return true;
                    }
                }
            } else if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
                if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                    revertChanges();
                    return true;
                }
            }
            return false;
        }
    }
    public LocationBarLayout(Context context, AttributeSet attrs) {
        this(context, attrs, R.layout.location_bar);
    }

    public LocationBarLayout(Context context, AttributeSet attrs, int layoutId) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(layoutId, this, true);

        mLightScrimColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.omnibox_focused_fading_background_color_light);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        mDeleteButton = (AppCompatImageButton) findViewById(R.id.delete_button);

        mUrlBar = findViewById(R.id.url_bar);
        mUrlCoordinator = new UrlBarCoordinator((UrlBar) mUrlBar);
        mUrlCoordinator.setDelegate(this);

        OmniboxSuggestionsList.OmniboxSuggestionListEmbedder embedder =
                new OmniboxSuggestionsList.OmniboxSuggestionListEmbedder() {
                    @Override
                    public boolean isTablet() {
                        return mIsTablet;
                    }

                    @Override
                    public boolean isIncognito() {
                        return mToolbarDataProvider.isIncognito();
                    }

                    @Override
                    public WindowDelegate getWindowDelegate() {
                        return mWindowDelegate;
                    }

                    @Override
                    public View getAnchorView() {
                        return getRootView().findViewById(R.id.toolbar);
                    }

                    @Override
                    public View getAlignmentView() {
                        return mIsTablet ? LocationBarLayout.this : null;
                    }
                };
        mAutocompleteCoordinator =
                new AutocompleteCoordinator(this, this, embedder, mUrlCoordinator);
        addUrlFocusChangeListener(mAutocompleteCoordinator);
        mUrlCoordinator.setUrlTextChangeListener(mAutocompleteCoordinator);

        mMicButton = (AppCompatImageButton) findViewById(R.id.mic_button);

        mUrlActionContainer = (LinearLayout) findViewById(R.id.url_action_container);

        mVoiceRecognitionHandler = new LocationBarVoiceRecognitionHandler(this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setLayoutTransition(null);

        mStatusViewCoordinator = new StatusViewCoordinator(mIsTablet, this, this);

        mUrlBar.setOnKeyListener(new UrlBarKeyListener());

        // mLocationBar's direction is tied to this UrlBar's text direction. Icons inside the
        // location bar, e.g. lock, refresh, X, should be reversed if UrlBar's text is RTL.
        mUrlCoordinator.setUrlDirectionListener(new UrlBar.UrlDirectionListener() {
            @Override
            public void onUrlDirectionChanged(int layoutDirection) {
                ViewCompat.setLayoutDirection(LocationBarLayout.this, layoutDirection);
                mAutocompleteCoordinator.updateSuggestionListLayoutDirection();
            }
        });
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        updateLayoutParams();
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        boolean retVal = super.dispatchKeyEvent(event);
        if (retVal && mUrlHasFocus && mUrlFocusedWithoutAnimations
                && event.getAction() == KeyEvent.ACTION_DOWN && event.isPrintingKey()
                && event.hasNoModifiers()) {
            handleUrlFocusAnimation(mUrlHasFocus);
        }
        return retVal;
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mUrlHasFocus && mUrlFocusedWithoutAnimations
                && newConfig.keyboard != Configuration.KEYBOARD_QWERTY) {
            // If we lose the hardware keyboard and the focus animations were not run, then the
            // user has not typed any text, so we will just clear the focus instead.
            setUrlBarFocus(false);
        }
    }

    @Override
    public void initializeControls(WindowDelegate windowDelegate,
            WindowAndroid windowAndroid) {
        mWindowDelegate = windowDelegate;
        mWindowAndroid = windowAndroid;

        mUrlCoordinator.setWindowDelegate(windowDelegate);
        mStatusViewCoordinator.setWindowAndroid(windowAndroid);
    }

    /**
     * @param focusable Whether the url bar should be focusable.
     */
    public void setUrlBarFocusable(boolean focusable) {
        if (mUrlCoordinator == null) return;
        mUrlCoordinator.setAllowFocus(focusable);
    }

    /**
     * @return The WindowDelegate for the LocationBar. This should be used for all Window related
     * state queries.
     */
    protected WindowDelegate getWindowDelegate() {
        return mWindowDelegate;
    }

    @Override
    public AutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    /**
     * Handles native dependent initialization for this class.
     */
    @Override
    public void onNativeLibraryReady() {
        mNativeInitialized = true;

        mAutocompleteCoordinator.onNativeInitialized();
        mStatusViewCoordinator.onNativeInitialized();
        updateMicButtonState();
        mDeleteButton.setOnClickListener(this);
        mMicButton.setOnClickListener(this);

        mOmniboxPrerender = new OmniboxPrerender();

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();

        updateVisualsForState();

        mOmniboxVoiceSearchAlwaysVisible =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE);
        updateMicButtonVisibility(mUrlFocusChangePercent);
    }

    @Override
    public boolean shouldAnimateIconChanges() {
        return mUrlHasFocus;
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    @Override
    public void setAutocompleteProfile(Profile profile) {
        // This will only be called once at least one tab exists, and the tab model is told to
        // update its state. During Chrome initialization the tab model update happens after the
        // call to onNativeLibraryReady, so this assert will not fire.
        assert mNativeInitialized
                : "Setting Autocomplete Profile before native side initialized";
        mAutocompleteCoordinator.setAutocompleteProfile(profile);
        mOmniboxPrerender.initializeForProfile(profile);
    }

    /** Focuses the current page. */
    private void focusCurrentTab() {
        if (mToolbarDataProvider.hasTab()) getCurrentTab().requestFocus();
    }

    @Override
    public void setUrlBarFocus(boolean shouldBeFocused) {
        if (shouldBeFocused) {
            mUrlBar.requestFocus();
        } else {
            hideKeyboard();
            mUrlBar.clearFocus();
        }
    }

    @Override
    public boolean isUrlBarFocused() {
        return mUrlHasFocus;
    }

    @Override
    public void selectAll() {
        mUrlCoordinator.selectAll();
    }

    @Override
    public void revertChanges() {
        if (!mUrlHasFocus) {
            setUrlToPageUrl();
        } else {
            String currentUrl = mToolbarDataProvider.getCurrentUrl();
            if (NativePageFactory.isNativePageUrl(currentUrl, mToolbarDataProvider.isIncognito())) {
                setUrlBarTextEmpty();
            } else {
                setUrlBarText(mToolbarDataProvider.getUrlBarData(), UrlBar.ScrollType.NO_SCROLL,
                        SelectionState.SELECT_ALL);
            }
            hideKeyboard();
        }
    }

    /**
     * @return Whether the URL focus change is taking place, e.g. a focus animation is running on
     *         a phone device.
     */
    @Override
    public boolean isUrlFocusChangeInProgress() {
        return mUrlFocusChangeInProgress;
    }

    /**
     * @param inProgress Whether a URL focus change is taking place.
     */
    protected void setUrlFocusChangeInProgress(boolean inProgress) {
        mUrlFocusChangeInProgress = inProgress;
        if (!inProgress) {
            updateButtonVisibility();
        }
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    public void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
        updateButtonVisibility();
        updateNavigationButton();

        if (hasFocus) {
            if (mNativeInitialized) RecordUserAction.record("FocusLocation");
            UrlBarData urlBarData = mToolbarDataProvider.getUrlBarData();
            if (urlBarData.editingText != null) {
                setUrlBarText(urlBarData, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
            }

            // Explicitly tell InputMethodManager that the url bar is focused before any callbacks
            // so that it updates the active view accordingly. Otherwise, it may fail to update
            // the correct active view if ViewGroup.addView() or ViewGroup.removeView() is called
            // to update a view that accepts text input.
            InputMethodManager imm = (InputMethodManager) mUrlBar.getContext().getSystemService(
                    Context.INPUT_METHOD_SERVICE);
            imm.viewClicked(mUrlBar);
        } else {
            mUrlFocusedFromFakebox = false;
            mUrlFocusedWithoutAnimations = false;

            // Focus change caused by a close-tab may result in an invalid current tab.
            if (mToolbarDataProvider.hasTab()) {
                setUrlToPageUrl();
            }

            // Moving focus away from UrlBar(EditText) to a non-editable focus holder, such as
            // ToolbarPhone, won't automatically hide keyboard app, but restart it with TYPE_NULL,
            // which will result in a visual glitch. Also, currently, we do not allow moving focus
            // directly from omnibox to web content's form field. Therefore, we hide keyboard on
            // focus blur indiscriminately here. Note that hiding keyboard may lower FPS of other
            // animation effects, but we found it tolerable in an experiment.
            InputMethodManager imm = (InputMethodManager) getContext().getSystemService(
                    Context.INPUT_METHOD_SERVICE);
            if (imm.isActive(mUrlBar)) imm.hideSoftInputFromWindow(getWindowToken(), 0, null);
        }

        if (mToolbarDataProvider.isUsingBrandColor()) updateVisualsForState();

        mStatusViewCoordinator.onUrlFocusChange(mUrlHasFocus);

        if (!mUrlFocusedWithoutAnimations) handleUrlFocusAnimation(hasFocus);

        if (hasFocus && mToolbarDataProvider.hasTab() && !mToolbarDataProvider.isIncognito()) {
            if (mNativeInitialized
                    && TemplateUrlService.getInstance().isDefaultSearchEngineGoogle()) {
                GeolocationHeader.primeLocationForGeoHeader();
            } else {
                mDeferredNativeRunnables.add(new Runnable() {
                    @Override
                    public void run() {
                        if (TemplateUrlService.getInstance().isDefaultSearchEngineGoogle()) {
                            GeolocationHeader.primeLocationForGeoHeader();
                        }
                    }
                });
            }
        }
    }

    /**
     * Handle and run any necessary animations that are triggered off focusing the UrlBar.
     * @param hasFocus Whether focus was gained.
     */
    protected void handleUrlFocusAnimation(boolean hasFocus) {
        if (hasFocus) mUrlFocusedWithoutAnimations = false;
        for (UrlFocusChangeListener listener : mUrlFocusChangeListeners) {
            listener.onUrlFocusChange(hasFocus);
        }

        updateFadingBackgroundView(hasFocus, false);
    }

    @Override
    public void onUrlTextChanged() {
        updateButtonVisibility();
        updateNavigationButton();
    }

    @Override
    public void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback) {
        mUrlCoordinator.setActionModeCallback(callback);
    }

    @Override
    public void requestUrlFocusFromFakebox(String pastedText) {
        mUrlFocusedFromFakebox = true;
        if (mUrlHasFocus && mUrlFocusedWithoutAnimations) {
            handleUrlFocusAnimation(mUrlHasFocus);
        } else {
            setUrlBarFocus(true);
        }

        if (pastedText != null) {
            ToolbarManager.recordOmniboxFocusReason(
                    ToolbarManager.OmniboxFocusReason.FAKE_BOX_LONG_PRESS);
            // This must be happen after requestUrlFocus(), which changes the selection.
            mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(pastedText),
                    UrlBar.ScrollType.NO_SCROLL, UrlBarCoordinator.SelectionState.SELECT_END);
            mAutocompleteCoordinator.onTextChangedForAutocomplete();
        } else {
            ToolbarManager.recordOmniboxFocusReason(ToolbarManager.OmniboxFocusReason.FAKE_BOX_TAP);
        }
    }

    @Override
    public boolean didFocusUrlFromFakebox() {
        return mUrlFocusedFromFakebox;
    }

    @Override
    public boolean isCurrentPage(NativePage nativePage) {
        assert nativePage != null;
        return nativePage == mToolbarDataProvider.getNewTabPageForCurrentTab();
    }

    @Override
    public void showUrlBarCursorWithoutFocusAnimations() {
        if (mUrlHasFocus || mUrlFocusedFromFakebox) return;

        mUrlFocusedWithoutAnimations = true;
        setUrlBarFocus(true);
    }

    /**
     * Sets the toolbar that owns this LocationBar.
     */
    @Override
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;

        updateButtonVisibility();

        mAutocompleteCoordinator.setToolbarDataProvider(toolbarDataProvider);
        mStatusViewCoordinator.setToolbarDataProvider(toolbarDataProvider);
        mUrlCoordinator.setOnFocusChangedCallback(this::onUrlFocusChange);
    }

    @Override
    public void addUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mUrlFocusChangeListeners.addObserver(listener);
    }

    @Override
    public void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mUrlFocusChangeListeners.removeObserver(listener);
    }

    @Override
    public final ToolbarDataProvider getToolbarDataProvider() {
        return mToolbarDataProvider;
    }

    private static @StatusViewCoordinator.NavigationButtonType
    int suggestionTypeToNavigationButtonType(OmniboxSuggestion suggestion) {
        if (suggestion.isUrlSuggestion()) {
            return StatusViewCoordinator.NavigationButtonType.PAGE;
        } else {
            return StatusViewCoordinator.NavigationButtonType.MAGNIFIER;
        }
    }

    // Updates the navigation button based on the URL string
    private void updateNavigationButton() {
        @StatusViewCoordinator.NavigationButtonType
        int type = StatusViewCoordinator.NavigationButtonType.EMPTY;
        if (mIsTablet && mAutocompleteCoordinator.getSuggestionCount() > 0) {
            // If there are suggestions showing, show the icon for the default suggestion.
            type = suggestionTypeToNavigationButtonType(
                    mAutocompleteCoordinator.getSuggestionAt(0));
        } else if (mIsTablet) {
            type = StatusViewCoordinator.NavigationButtonType.PAGE;
        }

        mStatusViewCoordinator.setNavigationButtonType(type);
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    @Override
    public void updateSecurityIcon() {
        mStatusViewCoordinator.updateSecurityIcon();
        // Update the URL in case the scheme change triggers a URL emphasis change.
        setUrlToPageUrl();
    }

    /**
     * @return The margin to be applied to the URL bar based on the buttons currently visible next
     *         to it, used to avoid text overlapping the buttons and vice versa.
     */
    @Override
    public int getUrlContainerMarginEnd() {
        int urlContainerMarginEnd = 0;
        for (View childView : getUrlContainerViewsForMargin()) {
            ViewGroup.MarginLayoutParams childLayoutParams =
                    (ViewGroup.MarginLayoutParams) childView.getLayoutParams();
            urlContainerMarginEnd += childLayoutParams.width
                    + MarginLayoutParamsCompat.getMarginStart(childLayoutParams)
                    + MarginLayoutParamsCompat.getMarginEnd(childLayoutParams);
        }
        return urlContainerMarginEnd;
    }

    /**
     * Updates the layout params for the location bar start aligned views.
     */
    protected void updateLayoutParams() {
        int startMargin = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (childView.getVisibility() != GONE) {
                LayoutParams childLayoutParams = (LayoutParams) childView.getLayoutParams();
                if (MarginLayoutParamsCompat.getMarginStart(childLayoutParams) != startMargin) {
                    MarginLayoutParamsCompat.setMarginStart(childLayoutParams, startMargin);
                    childView.setLayoutParams(childLayoutParams);
                }
                if (childView == mUrlBar) break;

                int widthMeasureSpec;
                int heightMeasureSpec;
                if (childLayoutParams.width == LayoutParams.WRAP_CONTENT) {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredWidth(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredWidth(), MeasureSpec.EXACTLY);
                } else {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.width, MeasureSpec.EXACTLY);
                }
                if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredHeight(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredHeight(), MeasureSpec.EXACTLY);
                } else {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.height, MeasureSpec.EXACTLY);
                }
                childView.measure(widthMeasureSpec, heightMeasureSpec);
                startMargin += childView.getMeasuredWidth();
            }
        }

        int urlContainerMarginEnd = getUrlContainerMarginEnd();
        LayoutParams urlLayoutParams = (LayoutParams) mUrlBar.getLayoutParams();
        if (MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams) != urlContainerMarginEnd) {
            MarginLayoutParamsCompat.setMarginEnd(urlLayoutParams, urlContainerMarginEnd);
            mUrlBar.setLayoutParams(urlLayoutParams);
        }
    }

    /**
     * Gets the list of views that need to be taken into account for adding margin to the end of the
     * URL bar.
     *
     * @return A {@link List} of the views to be taken into account for URL bar margin to avoid
     *         overlapping text and buttons.
     */
    protected List<View> getUrlContainerViewsForMargin() {
        List<View> outList = new ArrayList<View>();
        if (mUrlActionContainer == null) return outList;

        for (int i = 0; i < mUrlActionContainer.getChildCount(); i++) {
            View childView = mUrlActionContainer.getChildAt(i);
            if (childView.getVisibility() != GONE) outList.add(childView);
        }
        return outList;
    }

    /**
     * @return Whether the delete button should be shown.
     */
    protected boolean shouldShowDeleteButton() {
        // Show the delete button at the end when the bar has focus and has some text.
        boolean hasText = !TextUtils.isEmpty(mUrlCoordinator.getTextWithAutocomplete());
        return hasText && (mUrlBar.hasFocus() || mUrlFocusChangeInProgress);
    }

    /**
     * Updates the display of the delete URL content button.
     */
    protected void updateDeleteButtonVisibility() {
        mDeleteButton.setVisibility(shouldShowDeleteButton() ? VISIBLE : GONE);
    }

    @Override
    public void onSuggestionsHidden() {
        updateNavigationButton();
    }

    @Override
    public void hideKeyboard() {
        getWindowAndroid().getKeyboardDelegate().hideKeyboard(mUrlBar);
    }

    @Override
    public void onSuggestionsChanged(String autocompleteText) {
        String userText = mUrlCoordinator.getTextWithoutAutocomplete();
        if (mUrlCoordinator.shouldAutocomplete()) {
            mUrlCoordinator.setAutocompleteText(userText, autocompleteText);
        }

        // Handle the case where suggestions (in particular zero suggest) are received without the
        // URL focusing happening.
        if (mUrlFocusedWithoutAnimations && mUrlHasFocus) {
            handleUrlFocusAnimation(mUrlHasFocus);
        }

        // Update the navigation button to show the default suggestion's icon.
        updateNavigationButton();

        if (mNativeInitialized
                && !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_INSTANT)
                && PrivacyPreferencesManager.getInstance().shouldPrerender()
                && mToolbarDataProvider.hasTab()) {
            mOmniboxPrerender.prerenderMaybe(userText, getOriginalUrl(),
                    mAutocompleteCoordinator.getCurrentNativeAutocompleteResult(),
                    mToolbarDataProvider.getProfile(), mToolbarDataProvider.getTab());
        }
    }

    @Override
    protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        // Don't restore the state of the location bar, it can lead to all kind of bad states with
        // the popup.
        // When we restore tabs, we focus the selected tab so the URL of the page shows.
    }

    /**
     * Performs a search query on the current {@link Tab}.  This calls
     * {@link TemplateUrlService#getUrlForSearchQuery(String)} to get a url based on {@code query}
     * and loads that url in the current {@link Tab}.
     * @param query The {@link String} that represents the text query that should be searched for.
     */
    @VisibleForTesting
    public void performSearchQueryForTest(String query) {
        if (TextUtils.isEmpty(query)) return;

        String queryUrl = TemplateUrlService.getInstance().getUrlForSearchQuery(query);

        if (!TextUtils.isEmpty(queryUrl)) {
            loadUrl(queryUrl, PageTransition.GENERATED, 0);
        } else {
            setSearchQuery(query);
        }
    }

    /**
     * Sets the query string in the omnibox (ensuring the URL bar has focus and triggering
     * autocomplete for the specified query) as if the user typed it.
     *
     * @param query The query to be set in the omnibox.
     */
    @Override
    public void setSearchQuery(final String query) {
        if (TextUtils.isEmpty(query)) return;

        if (!mNativeInitialized) {
            mDeferredNativeRunnables.add(new Runnable() {
                @Override
                public void run() {
                    setSearchQuery(query);
                }
            });
            return;
        }

        setUrlBarText(UrlBarData.forNonUrlText(query), UrlBar.ScrollType.NO_SCROLL,
                SelectionState.SELECT_ALL);
        setUrlBarFocus(true);
        mAutocompleteCoordinator.startAutocompleteForQuery(query);
        post(new Runnable() {
            @Override
            public void run() {
                getWindowAndroid().getKeyboardDelegate().showKeyboard(mUrlBar);
            }
        });
    }


    @Override
    public void onClick(View v) {
        if (v == mDeleteButton) {
            setUrlBarTextEmpty();
            updateButtonVisibility();

            RecordUserAction.record("MobileOmniboxDeleteUrl");
            return;
        } else if (v == mMicButton && mVoiceRecognitionHandler != null) {
            RecordUserAction.record("MobileOmniboxVoiceSearch");
            mVoiceRecognitionHandler.startVoiceRecognition(
                    LocationBarVoiceRecognitionHandler.VoiceInteractionSource.OMNIBOX);
        }
    }

    @Override
    public void backKeyPressed() {
        setUrlBarFocus(false);
        // Revert the URL to match the current page.
        setUrlToPageUrl();
        focusCurrentTab();
    }

    @Override
    public boolean shouldForceLTR() {
        return !mToolbarDataProvider.shouldDisplaySearchTerms();
    }

    @Override
    public boolean shouldCutCopyVerbatim() {
        // When cutting/copying text in the URL bar, it will try to copy some version of the actual
        // URL to the clipboard, not the currently displayed URL bar contents. We want to avoid this
        // when displaying search terms.
        return mToolbarDataProvider.shouldDisplaySearchTerms();
    }

    /**
     * @return Returns the original url of the page.
     */
    public String getOriginalUrl() {
        return mOriginalUrl;
    }

    /**
     * Sets the displayed URL to be the URL of the page currently showing.
     *
     * <p>The URL is converted to the most user friendly format (removing HTTP:// for example).
     *
     * <p>If the current tab is null, the URL text will be cleared.
     */
    @Override
    public void setUrlToPageUrl() {
        String currentUrl = mToolbarDataProvider.getCurrentUrl();

        // If the URL is currently focused, do not replace the text they have entered with the URL.
        // Once they stop editing the URL, the current tab's URL will automatically be filled in.
        if (mUrlBar.hasFocus()) {
            if (mUrlFocusedWithoutAnimations && !NewTabPage.isNTPUrl(currentUrl)) {
                // If we did not run the focus animations, then the user has not typed any text.
                // So, clear the focus and accept whatever URL the page is currently attempting to
                // display. If the NTP is showing, the current page's URL should not be displayed.
                setUrlBarFocus(false);
            } else {
                return;
            }
        }

        mOriginalUrl = currentUrl;
        @ScrollType
        int scrollType = mToolbarDataProvider.shouldDisplaySearchTerms()
                ? UrlBar.ScrollType.SCROLL_TO_BEGINNING
                : UrlBar.ScrollType.SCROLL_TO_TLD;
        setUrlBarText(mToolbarDataProvider.getUrlBarData(), scrollType, SelectionState.SELECT_ALL);
        if (!mToolbarDataProvider.hasTab()) return;

        // Profile may be null if switching to a tab that has not yet been initialized.
        Profile profile = mToolbarDataProvider.getProfile();
        if (profile != null) mOmniboxPrerender.clear(profile);
    }

    /**
     * Changes the text on the url bar.  The text update will be applied regardless of the current
     * focus state (comparing to {@link #setUrlToPageUrl()} which only applies text updates when
     * not focused).
     *
     * @param urlBarData The contents of the URL bar, both for editing and displaying.
     * @param scrollType Specifies how the text should be scrolled in the unfocused state.
     * @param selectionState Specifies how the text should be selected in the focused state.
     * @return Whether the URL was changed as a result of this call.
     */
    private boolean setUrlBarText(
            UrlBarData urlBarData, @ScrollType int scrollType, @SelectionState int selectionState) {
        return mUrlCoordinator.setUrlBarData(urlBarData, scrollType, selectionState);
    }

    /**
     * Clear any text in the URL bar.
     * @return Whether this changed the existing text.
     */
    private boolean setUrlBarTextEmpty() {
        boolean textChanged = mUrlCoordinator.setUrlBarData(
                UrlBarData.EMPTY, UrlBar.ScrollType.SCROLL_TO_BEGINNING, SelectionState.SELECT_ALL);
        mAutocompleteCoordinator.onTextChangedForAutocomplete();
        return textChanged;
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(text), UrlBar.ScrollType.NO_SCROLL,
                UrlBarCoordinator.SelectionState.SELECT_END);
    }

    @Override
    public void loadUrlFromVoice(String url) {
        loadUrl(url, PageTransition.TYPED, 0);
    }

    /**
     * Load the url given with the given transition. Exposed for child classes to overwrite as
     * necessary.
     */
    @Override
    public void loadUrl(String url, @PageTransition int transition, long inputStart) {
        Tab currentTab = getCurrentTab();

        // The code of the rest of this class ensures that this can't be called until the native
        // side is initialized
        assert mNativeInitialized : "Loading URL before native side initialized";

        if (currentTab != null
                && (currentTab.isNativePage() || NewTabPage.isNTPUrl(currentTab.getUrl()))) {
            NewTabPageUma.recordOmniboxNavigation(url, transition);
            // Passing in an empty string should not do anything unless the user is at the NTP.
            // Since the NTP has no url, pressing enter while clicking on the URL bar should refresh
            // the page as it does when you click and press enter on any other site.
            if (url.isEmpty()) url = currentTab.getUrl();
        }

        // Loads the |url| in a new tab or the current ContentView and gives focus to the
        // ContentView.
        if (currentTab != null && !url.isEmpty()) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setVerbatimHeaders(GeolocationHeader.getGeoHeader(url, currentTab));
            loadUrlParams.setTransitionType(transition | PageTransition.FROM_ADDRESS_BAR);
            if (inputStart != 0) {
                loadUrlParams.setInputStartTimestamp(inputStart);
            }

            currentTab.loadUrl(loadUrlParams);
            RecordUserAction.record("MobileOmniboxUse");
        }
        LocaleManager.getInstance().recordLocaleBasedSearchMetrics(false, url, transition);

        focusCurrentTab();
    }

    /**
     * Update the location bar visuals based on a loading state change.
     * @param updateUrl Whether to update the URL as a result of this call.
     */
    @Override
    public void updateLoadingState(boolean updateUrl) {
        if (updateUrl) setUrlToPageUrl();
        updateNavigationButton();
        mStatusViewCoordinator.updateSecurityIcon();
    }

    /** @return The current active {@link Tab}. */
    @Nullable
    private Tab getCurrentTab() {
        if (mToolbarDataProvider == null) return null;
        return mToolbarDataProvider.getTab();
    }

    @Override
    public View getViewForUrlBackFocus() {
        Tab tab = getCurrentTab();
        if (tab == null) return null;
        return tab.getView();
    }

    @Override
    public boolean allowKeyboardLearning() {
        if (mToolbarDataProvider == null) return false;
        return !mToolbarDataProvider.isIncognito();
    }

    @Override
    public void onScrimClick() {
        setUrlBarFocus(false);
        updateFadingBackgroundView(false, false);
    }

    @Override
    public void onScrimVisibilityChanged(boolean visible) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (!(activity instanceof ChromeActivity)) return;
        ChromeActivity chromeActivity = (ChromeActivity) activity;

        if (visible) {
            chromeActivity.addViewObscuringAllTabs(mScrim);
        } else {
            chromeActivity.removeViewObscuringAllTabs(mScrim);
        }
    }

    @Override
    public void setScrim(ScrimView scrim) {
        mScrim = scrim;

        // In some cases, users can start chrome and immediately start tapping the omnibox. In that
        // case, the omnibox will focus, but there is no scrim. This checks if the scrim needs to
        // be visible and updates it accordingly.
        updateFadingBackgroundView(isUrlBarFocused(), true);
    }

    /**
     * Update the fading background view that shows when the omnibox is focused. If Chrome Home is
     * enabled, this method is a no-op.
     * @param visible Whether the background should be made visible.
     * @param ignoreNtpChecks Whether the checks for the ntp should be considered when updating the
     *                        scrim.
     */
    protected void updateFadingBackgroundView(boolean visible, boolean ignoreNtpChecks) {
        if (mScrim == null) return;
        NewTabPage ntp = mToolbarDataProvider.getNewTabPageForCurrentTab();
        boolean locationBarShownInNTP = ntp != null && ntp.isLocationBarShownInNTP();

        if (visible && (!locationBarShownInNTP || ignoreNtpChecks)) {
            if (mScrimParams == null) {
                int topMargin = getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
                View omniboxSuggestionsContainer =
                        mAutocompleteCoordinator.getSuggestionContainerView();
                if (omniboxSuggestionsContainer == null) return;
                mScrimParams =
                        new ScrimParams(omniboxSuggestionsContainer, false, false, topMargin, this);
            }
            mScrimParams.backgroundColor =
                    !mIsTablet && !mToolbarDataProvider.isIncognito() ? mLightScrimColor : null;

            // If the location bar is shown in the NTP, the toolbar will eventually trigger a
            // fade in.
            mScrim.showScrim(mScrimParams);
        } else {
            mScrim.hideScrim(!locationBarShownInNTP);
        }
    }

    @Override
    public void setUnfocusedWidth(float unfocusedWidth) {
        mStatusViewCoordinator.setUnfocusedLocationBarWidth(unfocusedWidth);
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility == View.VISIBLE) updateMicButtonState();
    }

    /**
     * Call to update the visibility of the buttons inside the location bar.
     */
    protected void updateButtonVisibility() {
        updateDeleteButtonVisibility();
    }

    /**
     * Call to notify the location bar that the state of the voice search microphone button may
     * need to be updated.
     */
    @Override
    public void updateMicButtonState() {
        mVoiceSearchEnabled = mVoiceRecognitionHandler.isVoiceSearchEnabled();
        updateButtonVisibility();
    }

    /**
     * Updates the display of the mic button.
     *
     * @param urlFocusChangePercent The completion percentage of the URL focus change animation.
     */
    protected void updateMicButtonVisibility(float urlFocusChangePercent) {
        boolean visible = mOmniboxVoiceSearchAlwaysVisible || !shouldShowDeleteButton();
        boolean showMicButton = mVoiceSearchEnabled && visible
                && (mUrlBar.hasFocus() || mUrlFocusChangeInProgress || urlFocusChangePercent > 0f);
        mMicButton.setVisibility(showMicButton ? VISIBLE : GONE);
    }

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    @Override
    public void updateVisualsForState() {
        if (updateUseDarkColors()) mStatusViewCoordinator.setUseDarkColors(mUseDarkColors);
        int id = mUseDarkColors ? R.color.dark_mode_tint : R.color.light_mode_tint;
        ColorStateList colorStateList = AppCompatResources.getColorStateList(getContext(), id);
        ApiCompatibilityUtils.setImageTintList(mMicButton, colorStateList);
        ApiCompatibilityUtils.setImageTintList(mDeleteButton, colorStateList);

        // If the URL changed colors and is not focused, update the URL to account for the new
        // color scheme.
        if (mUrlCoordinator.setUseDarkTextColors(mUseDarkColors) && !mUrlBar.hasFocus()) {
            setUrlToPageUrl();
        }

        mAutocompleteCoordinator.updateVisualsForState(mUseDarkColors);
    }

    /**
     * Checks the current specs and updates {@link LocationBarLayout#mUseDarkColors} if necessary.
     * @return Whether {@link LocationBarLayout#mUseDarkColors} has been updated.
     */
    private boolean updateUseDarkColors() {
        boolean brandColorNeedsLightText = false;
        if (mToolbarDataProvider.isUsingBrandColor() && !mUrlHasFocus) {
            int currentPrimaryColor = mToolbarDataProvider.getPrimaryColor();
            brandColorNeedsLightText =
                    ColorUtils.shouldUseLightForegroundOnBackground(currentPrimaryColor);
        }

        boolean useDarkColors = !mToolbarDataProvider.isIncognito()
                && (!mToolbarDataProvider.hasTab() || !brandColorNeedsLightText);
        boolean hasChanged = useDarkColors != mUseDarkColors;
        mUseDarkColors = useDarkColors;

        return hasChanged;
    }

    @Override
    public void onTabLoadingNTP(NewTabPage ntp) {
        ntp.setFakeboxDelegate(this);
        ntp.setVoiceRecognitionHandler(mVoiceRecognitionHandler);
    }

    @Override
    public View getContainerView() {
        return this;
    }

    @Override
    public View getSecurityIconView() {
        return mStatusViewCoordinator.getSecurityIconView();
    }

    @Override
    public void setTitleToPageTitle() { }

    @Override
    public void setShowTitle(boolean showTitle) { }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @VisibleForTesting
    public StatusViewCoordinator getStatusViewCoordinatorForTesting() {
        return mStatusViewCoordinator;
    }
}
