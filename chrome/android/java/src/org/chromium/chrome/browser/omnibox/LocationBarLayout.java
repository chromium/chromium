// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.MarginLayoutParamsCompat;
import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * This class represents the location bar where the user types in URLs and
 * search terms.
 */
public class LocationBarLayout
        extends FrameLayout implements OnClickListener, AutocompleteDelegate, FakeboxDelegate,
                                       VoiceRecognitionHandler.Delegate,
                                       AssistantVoiceSearchService.Observer, UrlBarDelegate {
    private static final int KEYBOARD_HIDE_DELAY_MS = 150;
    private static final int KEYBOARD_MODE_CHANGE_DELAY_MS = 300;

    protected ImageButton mDeleteButton;
    protected ImageButton mMicButton;
    private boolean mShouldShowMicButtonWhenUnfocused;
    protected UrlBar mUrlBar;
    private final boolean mIsTablet;

    protected UrlBarCoordinator mUrlCoordinator;
    protected AutocompleteCoordinator mAutocompleteCoordinator;

    protected LocationBarDataProvider mLocationBarDataProvider;
    private final ObserverList<UrlFocusChangeListener> mUrlFocusChangeListeners =
            new ObserverList<>();

    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();

    protected StatusCoordinator mStatusCoordinator;

    private WindowAndroid mWindowAndroid;
    private WindowDelegate mWindowDelegate;

    private String mOriginalUrl = "";

    private boolean mUrlFocusChangeInProgress;
    protected boolean mNativeInitialized;
    private boolean mUrlHasFocus;
    private boolean mUrlFocusedFromFakebox;
    private boolean mUrlFocusedFromQueryTiles;
    private boolean mUrlFocusedWithoutAnimations;
    protected boolean mVoiceSearchEnabled;

    private OmniboxPrerender mOmniboxPrerender;

    protected float mUrlFocusChangeFraction;
    protected LinearLayout mUrlActionContainer;

    private VoiceRecognitionHandler mVoiceRecognitionHandler;

    protected CompositeTouchDelegate mCompositeTouchDelegate;

    private AssistantVoiceSearchService mAssistantVoiceSearchService;
    private Runnable mKeyboardResizeModeTask;
    private Runnable mKeyboardHideTask;
    private ObservableSupplier<Profile> mProfileSupplier;
    private Callback<Profile> mProfileSupplierObserver;
    private CallbackController mCallbackController = new CallbackController();
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;

    /**
     * Class to handle input from a hardware keyboard when the focus is on the URL bar. In
     * particular, handle navigating the suggestions list from the keyboard.
     */
    private final class UrlBarKeyListener implements OnKeyListener {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            boolean isRtl = v.getLayoutDirection() == LAYOUT_DIRECTION_RTL;
            if (mAutocompleteCoordinator.handleKeyEvent(keyCode, event)) {
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_BACK) {
                if (KeyNavigationUtil.isActionDown(event) && event.getRepeatCount() == 0) {
                    // Tell the framework to start tracking this event.
                    getKeyDispatcherState().startTracking(event, this);
                    return true;
                } else if (KeyNavigationUtil.isActionUp(event)) {
                    getKeyDispatcherState().handleUpEvent(event);
                    if (event.isTracking() && !event.isCanceled()) {
                        backKeyPressed();
                        return true;
                    }
                }
            } else if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
                if (KeyNavigationUtil.isActionDown(event) && event.getRepeatCount() == 0) {
                    revertChanges();
                    return true;
                }
            } else if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                    || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
                // Ensures URL bar doesn't lose focus, when RIGHT or LEFT (RTL) key is pressed while
                // the cursor is positioned at the end of the text.
                TextView tv = (TextView) v;
                return tv.getSelectionStart() == tv.getSelectionEnd()
                        && tv.getSelectionEnd() == tv.getText().length();
            }
            return false;
        }
    }

    public LocationBarLayout(Context context, AttributeSet attrs) {
        this(context, attrs, R.layout.location_bar);

        mCompositeTouchDelegate = new CompositeTouchDelegate(this);
        setTouchDelegate(mCompositeTouchDelegate);
    }

    public LocationBarLayout(Context context, AttributeSet attrs, int layoutId) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(layoutId, this, true);

        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        mDeleteButton = findViewById(R.id.delete_button);

        mUrlBar = findViewById(R.id.url_bar);

        mUrlCoordinator = new UrlBarCoordinator((UrlBar) mUrlBar);
        mUrlCoordinator.setDelegate(this);

        OmniboxSuggestionsDropdownEmbedder embedder = new OmniboxSuggestionsDropdownEmbedder() {
            @Override
            public boolean isTablet() {
                return mIsTablet;
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
        mUrlCoordinator.addUrlTextChangeListener(mAutocompleteCoordinator);

        mMicButton = findViewById(R.id.mic_button);

        mUrlActionContainer = (LinearLayout) findViewById(R.id.url_action_container);

        mVoiceRecognitionHandler = new VoiceRecognitionHandler(this);
    }

    /**
     * Called when activity is being destroyed.
     */
    void destroy() {
        mUrlFocusChangeListeners.clear();

        if (mAutocompleteCoordinator != null) {
            removeUrlFocusChangeListener(mAutocompleteCoordinator);
            mAutocompleteCoordinator.destroy();
            mAutocompleteCoordinator = null;
        }

        if (mAssistantVoiceSearchService != null) {
            mAssistantVoiceSearchService.destroy();
            mAssistantVoiceSearchService = null;
        }

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mProfileSupplier != null) {
            mProfileSupplier.removeObserver(mProfileSupplierObserver);
            mProfileSupplier = null;
            mProfileSupplierObserver = null;
        }

        if (mTemplateUrlObserver != null) {
            TemplateUrlServiceFactory.get().removeObserver(mTemplateUrlObserver);
            mTemplateUrlObserver = null;
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setLayoutTransition(null);

        StatusView statusView = findViewById(R.id.location_bar_status);
        statusView.setCompositeTouchDelegate(mCompositeTouchDelegate);
        mStatusCoordinator = new StatusCoordinator(mIsTablet, statusView, mUrlCoordinator);
        mUrlCoordinator.addUrlTextChangeListener(mStatusCoordinator);

        updateShouldAnimateIconChanges();
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
            setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
        }
    }

    public void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate) {
        mWindowDelegate = windowDelegate;
        mWindowAndroid = windowAndroid;

        mUrlCoordinator.setWindowDelegate(windowDelegate);
        mAutocompleteCoordinator.setWindowAndroid(windowAndroid);
        mAutocompleteCoordinator.setActivityTabProvider(activityTabProvider);
        mAutocompleteCoordinator.setShareDelegateSupplier(shareDelegateSupplier);
        mStatusCoordinator.setIncognitoStateProvider(incognitoStateProvider);
        mStatusCoordinator.setModalDialogManagerSupplier(modalDialogManagerSupplier);
        mOverrideUrlLoadingDelegate = overrideUrlLoadingDelegate;
    }

    @Override
    public AutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    /**
     * Runs logic that can't be invoked until after native is initialized but shouldn't be on the
     * critical path, e.g. pre-fetching autocomplete suggestions. Contrast with
     * onFinishNativeInitialization, which is for logic that should be on the critical path and need
     * native to be initialized. This method must be called after onFinishNativeInitialization.
     */
    public void onDeferredStartup() {
        assert mNativeInitialized;
        startPrefetch();
    }

    public void onFinishNativeInitialization() {
        TemplateUrlServiceFactory.get().runWhenLoaded(this::registerTemplateUrlObserver);
        mNativeInitialized = true;

        mAutocompleteCoordinator.onNativeInitialized();
        mStatusCoordinator.onNativeInitialized();
        updateMicButtonState();
        mDeleteButton.setOnClickListener(this);
        mMicButton.setOnClickListener(this);

        mOmniboxPrerender = new OmniboxPrerender();

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();

        updateVisualsForState();

        updateMicButtonVisibility();

        mAssistantVoiceSearchService = new AssistantVoiceSearchService(getContext(),
                AppHooks.get().getExternalAuthUtils(), TemplateUrlServiceFactory.get(),
                GSAState.getInstance(getContext()), this, SharedPreferencesManager.getInstance());
        mVoiceRecognitionHandler.setAssistantVoiceSearchService(mAssistantVoiceSearchService);
        onAssistantVoiceSearchServiceChanged();
        setProfile(mProfileSupplier.get());
    }

    /** Initiates a prefetch of autocomplete suggestions. */
    public void startPrefetch() {
        if (!mNativeInitialized) return;

        mAutocompleteCoordinator.prefetchZeroSuggestResults();
    }

    public void setProfileSupplier(ObservableSupplier<Profile> profileSupplier) {
        assert profileSupplier != null;
        assert mProfileSupplier == null;
        mProfileSupplier = profileSupplier;
        mProfileSupplierObserver = mCallbackController.makeCancelable(this::setProfile);
        mProfileSupplier.addObserver(mProfileSupplierObserver);
    }

    @Override
    public void clearOmniboxFocus() {
        setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
    }

    public void selectAll() {
        mUrlCoordinator.selectAll();
    }

    public void revertChanges() {
        if (!mUrlHasFocus) {
            setUrl(mLocationBarDataProvider.getCurrentUrl());
        } else {
            String currentUrl = mLocationBarDataProvider.getCurrentUrl();
            if (NativePageFactory.isNativePageUrl(
                        currentUrl, mLocationBarDataProvider.isIncognito())) {
                setUrlBarTextEmpty();
            } else {
                setUrlBarText(mLocationBarDataProvider.getUrlBarData(), UrlBar.ScrollType.NO_SCROLL,
                        SelectionState.SELECT_ALL);
            }
            setKeyboardVisibility(false, false);
        }
    }

    @Override
    public void onUrlTextChanged() {
        updateButtonVisibility();
    }

    public void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback) {
        mUrlCoordinator.setActionModeCallback(callback);
    }

    @Override
    public boolean didFocusUrlFromFakebox() {
        return mUrlFocusedFromFakebox;
    }

    @Override
    public boolean didFocusUrlFromQueryTiles() {
        return mUrlFocusedFromQueryTiles;
    }

    public void showUrlBarCursorWithoutFocusAnimations() {
        if (mUrlHasFocus || mUrlFocusedFromFakebox) return;

        mUrlFocusedWithoutAnimations = true;

        // This interface should only be called to devices with a hardware keyboard attached as
        // described in the LocationBar.
        setUrlBarFocus(true, null, OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD);
    }

    /**
     * Sets the toolbar that owns this LocationBar.
     */
    public void setLocationBarDataProvider(LocationBarDataProvider locationBarDataProvider) {
        mLocationBarDataProvider = locationBarDataProvider;

        updateButtonVisibility();

        mAutocompleteCoordinator.setLocationBarDataProvider(locationBarDataProvider);
        mStatusCoordinator.setLocationBarDataProvider(locationBarDataProvider);
        mUrlCoordinator.setOnFocusChangedCallback(this::onUrlFocusChange);
    }

    @Override
    public final LocationBarDataProvider getLocationBarDataProvider() {
        return mLocationBarDataProvider;
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateStatusIcon() {
        mStatusCoordinator.updateStatusIcon();
        // Update the URL in case the scheme change triggers a URL emphasis change.
        setUrl(mLocationBarDataProvider.getCurrentUrl());
    }

    @Override
    public boolean isKeyboardActive() {
        return KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(getContext(), this)
                || (getContext().getResources().getConfiguration().keyboard
                        == Configuration.KEYBOARD_QWERTY);
    }

    @Override
    public void onSuggestionsHidden() {}

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

        if (mNativeInitialized
                && !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_INSTANT)
                && PrivacyPreferencesManager.getInstance().shouldPrerender()
                && mLocationBarDataProvider.hasTab()) {
            mOmniboxPrerender.prerenderMaybe(userText, getOriginalUrl(),
                    mAutocompleteCoordinator.getCurrentNativeAutocompleteResult(),
                    mLocationBarDataProvider.getProfile(), mLocationBarDataProvider.getTab());
        }
    }

    @Override
    protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        // Don't restore the state of the location bar, it can lead to all kind of bad states with
        // the popup.
        // When we restore tabs, we focus the selected tab so the URL of the page shows.
    }

    @Override
    public void performSearchQuery(String query, List<String> searchParams) {
        if (TextUtils.isEmpty(query)) return;

        String queryUrl = TemplateUrlServiceFactory.get().getUrlForSearchQuery(query, searchParams);

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

        // Ensure the UrlBar has focus before entering text. If the UrlBar is not focused,
        // autocomplete text will be updated but the visible text will not.
        setUrlBarFocus(true, null, OmniboxFocusReason.SEARCH_QUERY);
        setUrlBarText(UrlBarData.forNonUrlText(query), UrlBar.ScrollType.NO_SCROLL,
                SelectionState.SELECT_ALL);
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
        } else if (v == mMicButton) {
            RecordUserAction.record("MobileOmniboxVoiceSearch");
            mVoiceRecognitionHandler.startVoiceRecognition(
                    VoiceRecognitionHandler.VoiceInteractionSource.OMNIBOX);
        }
    }

    @Override
    public void backKeyPressed() {
        setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
        // Revert the URL to match the current page.
        setUrl(mLocationBarDataProvider.getCurrentUrl());
        focusCurrentTab();
    }

    @Override
    public void gestureDetected(boolean isLongPress) {
        recordOmniboxFocusReason(isLongPress ? OmniboxFocusReason.OMNIBOX_LONG_PRESS
                                             : OmniboxFocusReason.OMNIBOX_TAP);
    }

    /**
     * Update the location bar visuals based on a loading state change.
     * @param updateUrl Whether to update the URL as a result of this call.
     */
    public void updateLoadingState(boolean updateUrl) {
        if (updateUrl) setUrl(mLocationBarDataProvider.getCurrentUrl());
        mStatusCoordinator.updateStatusIcon();
    }

    @Override
    public View getViewForUrlBackFocus() {
        Tab tab = getCurrentTab();
        if (tab == null) return null;
        return tab.getView();
    }

    @Override
    public boolean allowKeyboardLearning() {
        if (mLocationBarDataProvider == null) return false;
        return !mLocationBarDataProvider.isIncognito();
    }

    @Override
    public void setUrlBarFocus(
            boolean shouldBeFocused, @Nullable String pastedText, @OmniboxFocusReason int reason) {
        if (shouldBeFocused) {
            if (!mUrlHasFocus) recordOmniboxFocusReason(reason);
            if (reason == OmniboxFocusReason.FAKE_BOX_TAP
                    || reason == OmniboxFocusReason.FAKE_BOX_LONG_PRESS
                    || reason == OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS
                    || reason == OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP) {
                mUrlFocusedFromFakebox = true;
            }

            if (reason == OmniboxFocusReason.QUERY_TILES_NTP_TAP) {
                mUrlFocusedFromFakebox = true;
                mUrlFocusedFromQueryTiles = true;
            }

            if (mUrlHasFocus && mUrlFocusedWithoutAnimations) {
                handleUrlFocusAnimation(mUrlHasFocus);
            } else {
                mUrlBar.requestFocus();
            }
        } else {
            assert pastedText == null;
            mUrlBar.clearFocus();
        }

        if (pastedText != null) {
            // This must be happen after requestUrlFocus(), which changes the selection.
            mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(pastedText),
                    UrlBar.ScrollType.NO_SCROLL, UrlBarCoordinator.SelectionState.SELECT_END);
            forceOnTextChanged();
        }
    }

    @Override
    public boolean isUrlBarFocused() {
        return mUrlHasFocus;
    }

    @Override
    public VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mVoiceRecognitionHandler;
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
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility == View.VISIBLE) updateMicButtonState();
    }

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    public void updateVisualsForState() {
        // If the location bar is focused, the toolbar background color would be the default color
        // regardless of whether it is branded or not.
        final int defaultPrimaryColor = ChromeColors.getDefaultThemeColor(
                getResources(), mLocationBarDataProvider.isIncognito());
        final int primaryColor =
                mUrlHasFocus ? defaultPrimaryColor : mLocationBarDataProvider.getPrimaryColor();

        updateAssistantVoiceSearchDrawableAndColors();

        final boolean useDarkColors =
                !ColorUtils.shouldUseLightForegroundOnBackground(primaryColor);
        ColorStateList colorStateList =
                ChromeColors.getPrimaryIconTint(getContext(), !useDarkColors);
        ApiCompatibilityUtils.setImageTintList(mDeleteButton, colorStateList);

        // If the URL changed colors and is not focused, update the URL to account for the new
        // color scheme.
        if (mUrlCoordinator.setUseDarkTextColors(useDarkColors) && !mUrlBar.hasFocus()) {
            setUrl(mLocationBarDataProvider.getCurrentUrl());
        }

        mStatusCoordinator.setUseDarkColors(useDarkColors);
        mStatusCoordinator.setIncognitoBadgeVisibility(
                mLocationBarDataProvider.isIncognito() && !mIsTablet);

        if (mAutocompleteCoordinator != null) {
            mAutocompleteCoordinator.updateVisualsForState(
                    useDarkColors, mLocationBarDataProvider.isIncognito());
        }
    }

    public void onTabLoadingNTP(NewTabPage ntp) {
        ntp.setFakeboxDelegate(this);
    }

    public View getContainerView() {
        return this;
    }

    public View getSecurityIconView() {
        return mStatusCoordinator.getSecurityIconView();
    }

    public void setShowTitle(boolean showTitle) {}

    @Override
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @Override
    public void onAssistantVoiceSearchServiceChanged() {
        updateAssistantVoiceSearchDrawableAndColors();
    }

    private void updateAssistantVoiceSearchDrawableAndColors() {
        if (mAssistantVoiceSearchService == null) return;

        Drawable drawable = mAssistantVoiceSearchService.getCurrentMicDrawable();
        mMicButton.setImageDrawable(drawable);

        final int defaultPrimaryColor = ChromeColors.getDefaultThemeColor(
                getResources(), mLocationBarDataProvider.isIncognito());
        final int primaryColor =
                mUrlHasFocus ? defaultPrimaryColor : mLocationBarDataProvider.getPrimaryColor();
        ColorStateList colorStateList =
                mAssistantVoiceSearchService.getMicButtonColorStateList(primaryColor, getContext());
        ApiCompatibilityUtils.setImageTintList(mMicButton, colorStateList);
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
     * Sets the displayed URL to be the URL of the page currently showing.
     *
     * <p>The URL is converted to the most user friendly format (removing HTTP:// for example).
     *
     * <p>If the current tab is null, the URL text will be cleared.
     */
    protected void setUrl(String currentUrl) {
        // If the URL is currently focused, do not replace the text they have entered with the URL.
        // Once they stop editing the URL, the current tab's URL will automatically be filled in.
        if (mUrlBar.hasFocus()) {
            if (mUrlFocusedWithoutAnimations && !UrlUtilities.isNTPUrl(currentUrl)) {
                // If we did not run the focus animations, then the user has not typed any text.
                // So, clear the focus and accept whatever URL the page is currently attempting to
                // display. If the NTP is showing, the current page's URL should not be displayed.
                setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
            } else {
                return;
            }
        }

        mOriginalUrl = currentUrl;
        setUrlBarText(mLocationBarDataProvider.getUrlBarData(), UrlBar.ScrollType.SCROLL_TO_TLD,
                SelectionState.SELECT_ALL);
        if (!mLocationBarDataProvider.hasTab()) return;

        // Profile may be null if switching to a tab that has not yet been initialized.
        Profile profile = mLocationBarDataProvider.getProfile();
        if (profile != null && mOmniboxPrerender != null) mOmniboxPrerender.clear(profile);
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(text), UrlBar.ScrollType.NO_SCROLL,
                UrlBarCoordinator.SelectionState.SELECT_END);
        updateButtonVisibility();
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
        loadUrlWithPostData(url, transition, inputStart, null, null);
    }

    @Override
    public void loadUrlWithPostData(String url, @PageTransition int transition, long inputStart,
            @Nullable String postDataType, @Nullable byte[] postData) {
        Tab currentTab = getCurrentTab();

        // The code of the rest of this class ensures that this can't be called until the native
        // side is initialized
        assert mNativeInitialized : "Loading URL before native side initialized";

        // TODO(crbug.com/1085812): Should be taking a full loaded LoadUrlParams.
        if (mOverrideUrlLoadingDelegate.willHandleLoadUrlWithPostData(url, transition, postDataType,
                    postData, mLocationBarDataProvider.isIncognito())) {
            return;
        }

        if (currentTab != null
                && (currentTab.isNativePage()
                        || UrlUtilities.isNTPUrl(currentTab.getUrlString()))) {
            NewTabPageUma.recordOmniboxNavigation(url, transition);
            // Passing in an empty string should not do anything unless the user is at the NTP.
            // Since the NTP has no url, pressing enter while clicking on the URL bar should refresh
            // the page as it does when you click and press enter on any other site.
            if (url.isEmpty()) url = currentTab.getUrlString();
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

            if (!TextUtils.isEmpty(postDataType)) {
                StringBuilder headers = new StringBuilder();
                String prevHeader = loadUrlParams.getVerbatimHeaders();
                if (prevHeader != null && !prevHeader.isEmpty()) {
                    headers.append(prevHeader);
                    headers.append("\r\n");
                }
                loadUrlParams.setExtraHeaders(new HashMap<String, String>() {
                    { put("Content-Type", postDataType); }
                });
                headers.append(loadUrlParams.getExtraHttpRequestHeadersString());
                loadUrlParams.setVerbatimHeaders(headers.toString());
            }

            if (postData != null && postData.length != 0) {
                loadUrlParams.setPostData(ResourceRequestBody.createFromBytes(postData));
            }

            currentTab.loadUrl(loadUrlParams);
            RecordUserAction.record("MobileOmniboxUse");
        }
        LocaleManager.getInstance().recordLocaleBasedSearchMetrics(false, url, transition);

        focusCurrentTab();
    }

    /**
     * @param focusable Whether the url bar should be focusable.
     */
    public void setUrlBarFocusable(boolean focusable) {
        if (mUrlCoordinator == null) return;
        mUrlCoordinator.setAllowFocus(focusable);
    }

    @CallSuper
    public void setUrlFocusChangeFraction(float fraction) {
        mUrlFocusChangeFraction = fraction;
    }

    private void registerTemplateUrlObserver() {
        final TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        assert mTemplateUrlObserver == null;
        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            private TemplateUrl mSearchEngine =
                    templateUrlService.getDefaultSearchEngineTemplateUrl();

            @Override
            public void onTemplateURLServiceChanged() {
                TemplateUrl searchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
                if ((mSearchEngine == null && searchEngine == null)
                        || (mSearchEngine != null && mSearchEngine.equals(searchEngine))) {
                    return;
                }

                mSearchEngine = searchEngine;
                updateSearchEngineStatusIcon(SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                                                     mLocationBarDataProvider.isIncognito()),
                        TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                        SearchEngineLogoUtils.getSearchLogoUrl());
            }
        };
        templateUrlService.addObserver(mTemplateUrlObserver);

        // Force an update once to populate initial data.
        updateSearchEngineStatusIcon(SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                                             mLocationBarDataProvider.isIncognito()),
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                SearchEngineLogoUtils.getSearchLogoUrl());
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
     * Updates the profile used by this LocationBar, for, e.g. determining incognito status or
     * generating autocomplete suggestions..
     * @param profile The profile to be used.
     */
    private void setProfile(Profile profile) {
        if (profile == null || !mNativeInitialized) return;
        mAutocompleteCoordinator.setAutocompleteProfile(profile);
        mOmniboxPrerender.initializeForProfile(profile);

        setShowIconsWhenUrlFocused(
                SearchEngineLogoUtils.shouldShowSearchEngineLogo(profile.isOffTheRecord()));
    }

    /** Focuses the current page. */
    private void focusCurrentTab() {
        if (mLocationBarDataProvider.hasTab()) {
            View view = getCurrentTab().getView();
            if (view != null) view.requestFocus();
        }
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

            // Focus change caused by a close-tab may result in an invalid current tab.
            if (mLocationBarDataProvider.hasTab()) {
                setUrl(mLocationBarDataProvider.getCurrentUrl());
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

        if (mLocationBarDataProvider.isUsingBrandColor()) updateVisualsForState();

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
        removeCallbacks(mKeyboardResizeModeTask);
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
        boolean hasText = !TextUtils.isEmpty(mUrlCoordinator.getTextWithAutocomplete());
        return hasText && (mUrlBar.hasFocus() || mUrlFocusChangeInProgress);
    }

    /**
     * Updates the display of the delete URL content button.
     */
    protected void updateDeleteButtonVisibility() {
        mDeleteButton.setVisibility(shouldShowDeleteButton() ? VISIBLE : GONE);
    }

    /**
     * @return Returns the original url of the page.
     */
    public String getOriginalUrl() {
        return mOriginalUrl;
    }

    /**
     * Changes the text on the url bar.  The text update will be applied regardless of the current
     * focus state (comparing to {@link #setUrlToPageUrl(mLocationBarDataProvider.getCurrentUrl())}
     * which only applies text updates when not focused).
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
        forceOnTextChanged();
        return textChanged;
    }

    /** @return The current active {@link Tab}. */
    @Nullable
    private Tab getCurrentTab() {
        if (mLocationBarDataProvider == null) return null;
        return mLocationBarDataProvider.getTab();
    }

    public void setUnfocusedWidth(int unfocusedWidth) {
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

    private void forceOnTextChanged() {
        String textWithoutAutocomplete = mUrlCoordinator.getTextWithoutAutocomplete();
        String textWithAutocomplete = mUrlCoordinator.getTextWithAutocomplete();
        mAutocompleteCoordinator.onTextChanged(textWithoutAutocomplete, textWithAutocomplete);
    }

    private void recordOmniboxFocusReason(@OmniboxFocusReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.OmniboxFocusReason", reason, OmniboxFocusReason.NUM_ENTRIES);
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change.  This will be called after any animations are performed to transition from one
     * focus state to the other.
     * @param hasFocus Whether the URL field has gained focus.
     * @param shouldShowKeyboard Whether the keyboard should be shown. This value should be the same
     *         as hasFocus by default.
     */
    protected void finishUrlFocusChange(boolean hasFocus, boolean shouldShowKeyboard) {
        setKeyboardVisibility(hasFocus && shouldShowKeyboard, true);
        setUrlFocusChangeInProgress(false);
        updateShouldAnimateIconChanges();
    }

    /**
     * Controls keyboard visibility.
     * TODO(https://crbug.com/1060729): This should be relocated to UrlBar component.
     *
     * @param shouldShow Whether the soft keyboard should be shown.
     * @param shouldDelayHiding When true, keyboard hide operation will be delayed slightly to
     *         improve the animation smoothness.
     */
    @Override
    public void setKeyboardVisibility(boolean showKeyboard, boolean shouldDelayHiding) {
        // Cancel pending jobs to prevent any possibility of keyboard flicker.
        if (mKeyboardHideTask != null) {
            removeCallbacks(mKeyboardHideTask);
        }

        // Note: due to nature of this mechanism, we may occasionally experience subsequent requests
        // to show or hide keyboard anyway. This may happen when we schedule keyboard hide, and
        // receive a second request to hide the keyboard instantly.
        if (showKeyboard) {
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN, /* delay */ false);
            getWindowAndroid().getKeyboardDelegate().showKeyboard(mUrlBar);
        } else {
            // The animation rendering may not yet be 100% complete and hiding the keyboard makes
            // the animation quite choppy.
            // clang-format off
            mKeyboardHideTask = () -> {
                getWindowAndroid().getKeyboardDelegate().hideKeyboard(mUrlBar);
                mKeyboardHideTask = null;
            };
            // clang-format on
            postDelayed(mKeyboardHideTask, shouldDelayHiding ? KEYBOARD_HIDE_DELAY_MS : 0);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary amount
            // of time in hopes the keyboard will be completely hidden before making this change).
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE, /* delay */ true);
        }
    }

    /**
     * @param softInputMode The software input resize mode.
     * @param delay Delay the change in input mode.
     */
    private void setSoftInputMode(final int softInputMode, boolean delay) {
        if (mKeyboardResizeModeTask != null) {
            removeCallbacks(mKeyboardResizeModeTask);
            mKeyboardResizeModeTask = null;
        }

        if (mWindowDelegate == null || mWindowDelegate.getWindowSoftInputMode() == softInputMode) {
            return;
        }

        if (delay) {
            mKeyboardResizeModeTask = () -> {
                mWindowDelegate.setWindowSoftInputMode(softInputMode);
                mKeyboardResizeModeTask = null;
            };
            postDelayed(mKeyboardResizeModeTask, KEYBOARD_MODE_CHANGE_DELAY_MS);
        } else {
            mWindowDelegate.setWindowSoftInputMode(softInputMode);
        }
    }

    public void setVoiceRecognitionHandlerForTesting(
            VoiceRecognitionHandler voiceRecognitionHandler) {
        mVoiceRecognitionHandler = voiceRecognitionHandler;
    }
}
