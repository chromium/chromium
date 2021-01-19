// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * This class represents the location bar where the user types in URLs and
 * search terms.
 */
public class LocationBarLayout extends FrameLayout implements OnClickListener {

    protected ImageButton mDeleteButton;
    protected ImageButton mMicButton;
    private boolean mShouldShowMicButtonWhenUnfocused;
    protected UrlBar mUrlBar;

    protected UrlBarCoordinator mUrlCoordinator;
    protected AutocompleteCoordinator mAutocompleteCoordinator;

    protected LocationBarDataProvider mLocationBarDataProvider;
    private final ObserverList<UrlFocusChangeListener> mUrlFocusChangeListeners =
            new ObserverList<>();

    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();

    protected StatusCoordinator mStatusCoordinator;

    private WindowAndroid mWindowAndroid;

    private boolean mUrlFocusChangeInProgress;
    protected boolean mNativeInitialized;
    private boolean mUrlHasFocus;
    private boolean mUrlFocusedFromFakebox;
    private boolean mUrlFocusedFromQueryTiles;
    private boolean mUrlFocusedWithoutAnimations;
    protected boolean mVoiceSearchEnabled;

    protected float mUrlFocusChangeFraction;
    protected LinearLayout mUrlActionContainer;

    private VoiceRecognitionHandler mVoiceRecognitionHandler;

    protected CompositeTouchDelegate mCompositeTouchDelegate;

    public LocationBarLayout(Context context, AttributeSet attrs) {
        this(context, attrs, R.layout.location_bar);

        mCompositeTouchDelegate = new CompositeTouchDelegate(this);
        setTouchDelegate(mCompositeTouchDelegate);
    }

    public LocationBarLayout(Context context, AttributeSet attrs, int layoutId) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(layoutId, this, true);

        mDeleteButton = findViewById(R.id.delete_button);
        mUrlBar = findViewById(R.id.url_bar);
        mMicButton = findViewById(R.id.mic_button);
        mUrlActionContainer = (LinearLayout) findViewById(R.id.url_action_container);
    }

    /**
     * Called when activity is being destroyed.
     */
    void destroy() {
        mUrlFocusChangeListeners.clear();

        if (mAutocompleteCoordinator != null) {
            // Don't call destroy() on mAutocompleteCoordinator since we don't own it.
            mAutocompleteCoordinator = null;
        }

        mUrlCoordinator = null;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setLayoutTransition(null);

        StatusView statusView = findViewById(R.id.location_bar_status);
        statusView.setCompositeTouchDelegate(mCompositeTouchDelegate);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        updateLayoutParams();
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Initializes LocationBarLayout with dependencies that aren't immediately available at
     * construction time.
     *
     * @param autocompleteCoordinator The coordinator for interacting with the autocomplete
     *         subsystem.
     * @param urlCoordinator The coordinator for interacting with the url bar.
     * @param statusCoordinator The coordinator for interacting with the status icon.
     * @param locationBarDataProvider Provider of LocationBar data, e.g. url and title.
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     */
    @CallSuper
    public void initialize(@NonNull AutocompleteCoordinator autocompleteCoordinator,
            @NonNull UrlBarCoordinator urlCoordinator, @NonNull StatusCoordinator statusCoordinator,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull WindowDelegate windowDelegate, @NonNull WindowAndroid windowAndroid,
            @NonNull VoiceRecognitionHandler voiceRecognitionHandler) {
        mAutocompleteCoordinator = autocompleteCoordinator;
        mUrlCoordinator = urlCoordinator;
        mStatusCoordinator = statusCoordinator;
        mWindowAndroid = windowAndroid;
        mLocationBarDataProvider = locationBarDataProvider;
        mVoiceRecognitionHandler = voiceRecognitionHandler;

        updateButtonVisibility();
        updateShouldAnimateIconChanges();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public AutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    /**
     *  Signals to LocationBarLayout that's it safe to call code that requires native to be loaded.
     */
    public void onFinishNativeInitialization() {
        mNativeInitialized = true;

        updateMicButtonState();
        mDeleteButton.setOnClickListener(this);
        mMicButton.setOnClickListener(this);

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();

        updateMicButtonVisibility();
    }

    /* package */ boolean didFocusUrlFromFakebox() {
        return mUrlFocusedFromFakebox;
    }

    /* package */ void setUrlFocusedFromFakebox(boolean focusedFromFakebox) {
        mUrlFocusedFromFakebox = focusedFromFakebox;
    }

    /* package */ boolean didFocusUrlFromQueryTiles() {
        return mUrlFocusedFromQueryTiles;
    }

    /* package */ void setUrlFocusedFromQueryTiles(boolean focusedFromQueryTiles) {
        mUrlFocusedFromQueryTiles = focusedFromQueryTiles;
    }

    /* package */ void setIsUrlFocusedWithoutAnimations(boolean isUrlFocusedWithoutAnimations) {
        mUrlFocusedWithoutAnimations = isUrlFocusedWithoutAnimations;
    }

    /* package */ void setMicButtonDrawable(Drawable drawable) {
        mMicButton.setImageDrawable(drawable);
    }

    /* package */ void setMicButtonTint(ColorStateList colorStateList) {
        ApiCompatibilityUtils.setImageTintList(mMicButton, colorStateList);
    }

    /* package */ void setDeleteButtonTint(ColorStateList colorStateList) {
        ApiCompatibilityUtils.setImageTintList(mDeleteButton, colorStateList);
    }

    /**
     * Override the default LocationBarDataProvider in tests. Production code should use the
     * {@link #initialize} method instead.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void setLocationBarDataProviderForTesting(
            LocationBarDataProvider locationBarDataProvider) {
        mLocationBarDataProvider = locationBarDataProvider;

        mAutocompleteCoordinator.setLocationBarDataProviderForTesting(locationBarDataProvider);
        mStatusCoordinator.setLocationBarDataProviderForTesting(locationBarDataProvider);
    }

    @Override
    protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        // Don't restore the state of the location bar, it can lead to all kind of bad states with
        // the popup.
        // When we restore tabs, we focus the selected tab so the URL of the page shows.
    }

    @Override
    public void onClick(View v) {
        if (v == mDeleteButton) {
            setUrlBarTextEmpty();
            updateButtonVisibility();

            RecordUserAction.record("MobileOmniboxDeleteUrl");
            return;
        } else if (v == mMicButton) {
            RecordUserAction.record("MobileOmniboxVoiceSearch");
            mVoiceRecognitionHandler.startVoiceRecognition(
                    VoiceRecognitionHandler.VoiceInteractionSource.OMNIBOX);
        }
    }

    /* package */ boolean isUrlBarFocused() {
        return mUrlHasFocus;
    }

    /* package */ boolean isUrlBarFocusedWithoutAnimations() {
        return mUrlFocusedWithoutAnimations;
    }

    protected VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mVoiceRecognitionHandler;
    }

    /* package */ void addUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mUrlFocusChangeListeners.addObserver(listener);
    }

    /* package */ void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mUrlFocusChangeListeners.removeObserver(listener);
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility == View.VISIBLE) updateMicButtonState();
    }

    protected void onNtpStartedLoading() {}

    public View getContainerView() {
        return this;
    }

    public View getSecurityIconView() {
        return mStatusCoordinator.getSecurityIconView();
    }

    protected WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * Call to notify the location bar that the state of the voice search microphone button may
     * need to be updated.
     */
    /* package */ void updateMicButtonState() {
        mVoiceSearchEnabled =
                mVoiceRecognitionHandler != null && mVoiceRecognitionHandler.isVoiceSearchEnabled();
        updateButtonVisibility();
    }

    @CallSuper
    protected void setUrlFocusChangeFraction(float fraction) {
        mUrlFocusChangeFraction = fraction;
    }

    /**
     * Evaluate state and update child components' animations.
     *
     * This call and all overrides should invoke `notifyShouldAnimateIconChanges(boolean)` with a
     * computed boolean value toggling animation support in child components.
     */
    protected void updateShouldAnimateIconChanges() {
        notifyShouldAnimateIconChanges(mUrlHasFocus);
    }

    /**
     * Toggle child components animations.
     * @param shouldAnimate Boolean flag indicating whether animations should be enabled.
     */
    protected void notifyShouldAnimateIconChanges(boolean shouldAnimate) {
        mStatusCoordinator.setShouldAnimateIconChanges(shouldAnimate);
    }

    /**
     * @return Whether the URL focus change is taking place, e.g. a focus animation is running on
     *         a phone device.
     */
    public boolean isUrlFocusChangeInProgress() {
        return mUrlFocusChangeInProgress;
    }

    /**
     * Specify whether location bar should present icons when focused.
     * @param showIcon True if we should show the icons when the url is focused.
     */
    protected void setShowIconsWhenUrlFocused(boolean showIcon) {}

    /**
     * @param inProgress Whether a URL focus change is taking place.
     */
    protected void setUrlFocusChangeInProgress(boolean inProgress) {
        if (mUrlCoordinator == null) return;
        mUrlFocusChangeInProgress = inProgress;
        if (!inProgress) {
            updateButtonVisibility();

            // The accessibility bounding box is not properly updated when focusing the Omnibox
            // from the NTP fakebox.  Clearing/re-requesting focus triggers the bounding box to
            // be recalculated.
            if (didFocusUrlFromFakebox() && mUrlHasFocus
                    && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                String existingText = mUrlCoordinator.getTextWithoutAutocomplete();
                mUrlBar.clearFocus();
                mUrlBar.requestFocus();
                // Existing text (e.g. if the user pasted via the fakebox) from the fake box
                // should be restored after toggling the focus.
                if (!TextUtils.isEmpty(existingText)) {
                    mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(existingText),
                            UrlBar.ScrollType.NO_SCROLL,
                            UrlBarCoordinator.SelectionState.SELECT_END);
                    forceOnTextChanged();
                }
            }

            for (UrlFocusChangeListener listener : mUrlFocusChangeListeners) {
                listener.onUrlAnimationFinished(mUrlHasFocus);
            }
        }
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    protected void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
        updateButtonVisibility();
        updateShouldAnimateIconChanges();

        if (mUrlHasFocus) {
            if (mNativeInitialized) RecordUserAction.record("FocusLocation");
            UrlBarData urlBarData = mLocationBarDataProvider.getUrlBarData();
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
            mUrlFocusedFromQueryTiles = false;
            mUrlFocusedWithoutAnimations = false;

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

        mStatusCoordinator.onUrlFocusChange(mUrlHasFocus);

        if (!mUrlFocusedWithoutAnimations) handleUrlFocusAnimation(mUrlHasFocus);

        if (mUrlHasFocus && mLocationBarDataProvider.hasTab()
                && !mLocationBarDataProvider.isIncognito()) {
            if (mNativeInitialized
                    && TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) {
                GeolocationHeader.primeLocationForGeoHeader();
            } else {
                mDeferredNativeRunnables.add(new Runnable() {
                    @Override
                    public void run() {
                        if (TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) {
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
    }

    /**
     * @return The margin to be applied to the URL bar based on the buttons currently visible next
     *         to it, used to avoid text overlapping the buttons and vice versa.
     */
    private int getUrlContainerMarginEnd() {
        int urlContainerMarginEnd = 0;
        for (View childView : getUrlContainerViewsForMargin()) {
            ViewGroup.MarginLayoutParams childLayoutParams =
                    (ViewGroup.MarginLayoutParams) childView.getLayoutParams();
            urlContainerMarginEnd += childLayoutParams.width
                    + MarginLayoutParamsCompat.getMarginStart(childLayoutParams)
                    + MarginLayoutParamsCompat.getMarginEnd(childLayoutParams);
        }
        if (mUrlActionContainer != null && mUrlActionContainer.getVisibility() == View.VISIBLE) {
            ViewGroup.MarginLayoutParams urlActionContainerLayoutParams =
                    (ViewGroup.MarginLayoutParams) mUrlActionContainer.getLayoutParams();
            urlContainerMarginEnd +=
                    MarginLayoutParamsCompat.getMarginStart(urlActionContainerLayoutParams)
                    + MarginLayoutParamsCompat.getMarginEnd(urlActionContainerLayoutParams);
        }
        // Include the space which the URL bar will be translated post-layout into the end
        // margin so the URL bar doesn't overlap with the URL actions container when focused.
        if (mStatusCoordinator.isSearchEngineStatusIconVisible() && hasFocus()) {
            urlContainerMarginEnd += mStatusCoordinator.getEndPaddingPixelSizeOnFocusDelta();
        }
        return urlContainerMarginEnd;
    }

    /**
     * Updates the layout params for the location bar start aligned views.
     */
    @VisibleForTesting
    void updateLayoutParams() {
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
                    widthMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
                    widthMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
                } else {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.width, MeasureSpec.EXACTLY);
                }
                if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
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
        boolean hasText = mUrlCoordinator != null
                && !TextUtils.isEmpty(mUrlCoordinator.getTextWithAutocomplete());
        return hasText && (mUrlBar.hasFocus() || mUrlFocusChangeInProgress);
    }

    /**
     * Updates the display of the delete URL content button.
     */
    protected void updateDeleteButtonVisibility() {
        mDeleteButton.setVisibility(shouldShowDeleteButton() ? VISIBLE : GONE);
    }

    /**
     * Changes the text on the url bar.  The text update will be applied regardless of the current
     * focus state (comparing to {@link LocationBarMediator#setUrl} which only applies text updates
     * when not focused).
     *
     * @param urlBarData The contents of the URL bar, both for editing and displaying.
     * @param scrollType Specifies how the text should be scrolled in the unfocused state.
     * @param selectionState Specifies how the text should be selected in the focused state.
     * @return Whether the URL was changed as a result of this call.
     */
    /* package */ boolean setUrlBarText(
            UrlBarData urlBarData, @ScrollType int scrollType, @SelectionState int selectionState) {
        return mUrlCoordinator.setUrlBarData(urlBarData, scrollType, selectionState);
    }

    /**
     * Clear any text in the URL bar.
     * @return Whether this changed the existing text.
     */
    /* package */ boolean setUrlBarTextEmpty() {
        boolean textChanged = mUrlCoordinator.setUrlBarData(
                UrlBarData.EMPTY, UrlBar.ScrollType.SCROLL_TO_BEGINNING, SelectionState.SELECT_ALL);
        forceOnTextChanged();
        return textChanged;
    }

    protected void setUnfocusedWidth(int unfocusedWidth) {
        mStatusCoordinator.setUnfocusedLocationBarWidth(unfocusedWidth);
    }

    protected void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        mStatusCoordinator.updateSearchEngineStatusIcon(
                shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);
    }

    /**
     * Call to update the visibility of the buttons inside the location bar.
     */
    protected void updateButtonVisibility() {
        updateDeleteButtonVisibility();
    }

    /**
     * Updates the display of the mic button.
     */
    protected void updateMicButtonVisibility() {
        boolean visible = !shouldShowDeleteButton();
        boolean showMicButton = mVoiceSearchEnabled && visible
                && (mUrlBar.hasFocus() || mUrlFocusChangeInProgress || mUrlFocusChangeFraction > 0f
                        || mShouldShowMicButtonWhenUnfocused);
        mMicButton.setVisibility(showMicButton ? VISIBLE : GONE);
    }

    /**
     * Value determines if mic button should be shown when location bar is not focused. By default
     * mic button is not shown. It is only shown for SearchActivityLocationBarLayout.
     */
    protected void setShouldShowMicButtonWhenUnfocused(boolean shouldShowMicButtonWhenUnfocused) {
        mShouldShowMicButtonWhenUnfocused = shouldShowMicButtonWhenUnfocused;
    }

    @VisibleForTesting
    public StatusCoordinator getStatusCoordinatorForTesting() {
        return mStatusCoordinator;
    }

    /* package */ void forceOnTextChanged() {
        String textWithoutAutocomplete = mUrlCoordinator.getTextWithoutAutocomplete();
        String textWithAutocomplete = mUrlCoordinator.getTextWithAutocomplete();
        mAutocompleteCoordinator.onTextChanged(textWithoutAutocomplete, textWithAutocomplete);
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change. This will be called after any animations are performed to transition from one
     * focus state to the other.
     * @param hasFocus Whether the URL field has gained focus.
     * @param shouldShowKeyboard Whether the keyboard should be shown. This value should be the same
     *         as hasFocus by default.
     */
    protected void finishUrlFocusChange(boolean hasFocus, boolean shouldShowKeyboard) {
        if (mUrlCoordinator == null) return;
        mUrlCoordinator.setKeyboardVisibility(hasFocus && shouldShowKeyboard, true);
        setUrlFocusChangeInProgress(false);
        updateShouldAnimateIconChanges();
    }

    /** Update the status visibility according to the current state held in LocationBar. */
    /* package */ void updateStatusVisibility() {}

    public void setVoiceRecognitionHandlerForTesting(
            VoiceRecognitionHandler voiceRecognitionHandler) {
        mVoiceRecognitionHandler = voiceRecognitionHandler;
    }
}
