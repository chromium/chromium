// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.animation.Animator;
import android.annotation.SuppressLint;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Vibrator;
import android.provider.Settings;
import android.text.InputType;
import android.text.Selection;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.accessibility.AccessibilityEventCompat;
import androidx.core.view.inputmethod.EditorInfoCompat;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.text.VerticallyFixedEditText;
import org.chromium.components.find_in_page.FindInPageBridge;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindNotificationDetails;
import org.chromium.components.find_in_page.FindResultBar;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A toolbar providing find in page functionality. */
public class FindToolbar extends LinearLayout implements BackPressHandler {
    private static final long ACCESSIBLE_ANNOUNCEMENT_DELAY_MILLIS = 500;

    @IntDef({
        FindLocationBarState.SHOWN,
        FindLocationBarState.SHOWING,
        FindLocationBarState.HIDDEN,
        FindLocationBarState.HIDING
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface FindLocationBarState {
        int SHOWN = 0;
        int SHOWING = 1;
        int HIDDEN = 2;
        int HIDING = 3;
    }

    // Toolbar UI
    private TextView mFindStatus;
    protected FindQuery mFindQuery;
    protected ImageButton mCloseFindButton;
    protected ImageButton mFindPrevButton;
    protected ImageButton mFindNextButton;
    protected View mDivider;

    private FindResultBar mResultBar;

    private TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final TabModelObserver mTabModelObserver;
    private Tab mCurrentTab;
    private final TabObserver mTabObserver;
    private WindowAndroid mWindowAndroid;
    private FindInPageBridge mFindInPageBridge;
    private FindToolbarObserver mObserver;

    /** Most recently entered search text (globally, in non-incognito tabs). */
    private String mLastUserSearch = "";

    /** Whether toolbar text is being set automatically (not typed by user). */
    private boolean mSettingFindTextProgrammatically;

    /** Whether the search key should trigger a new search. */
    private boolean mSearchKeyShouldTriggerSearch;

    private @FindLocationBarState int mCurrentState = FindLocationBarState.HIDDEN;
    private @FindLocationBarState int mDesiredState = FindLocationBarState.HIDDEN;

    private Handler mHandler = new Handler();
    private Runnable mAccessibleAnnouncementRunnable;
    private boolean mAccessibilityDidActivateResult;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    /** Subclasses EditText in order to intercept BACK key presses. */
    @SuppressLint("Instantiatable")
    static class FindQuery extends VerticallyFixedEditText {
        private FindToolbar mFindToolbar;

        public FindQuery(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        void setFindToolbar(FindToolbar findToolbar) {
            mFindToolbar = findToolbar;
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_ENTER
                    || keyCode == KeyEvent.KEYCODE_F3
                    || (keyCode == KeyEvent.KEYCODE_G && event.isCtrlPressed())) {
                mFindToolbar.hideKeyboardAndStartFinding(!event.isShiftPressed());
                return true;
            }
            if (keyCode == KeyEvent.KEYCODE_ESCAPE && event.hasNoModifiers()) {
                mFindToolbar.deactivate();
                return true;
            }
            return super.onKeyDown(keyCode, event);
        }

        @Override
        public boolean onTextContextMenuItem(int id) {
            if (id == android.R.id.paste) {
                ClipboardManager clipboard =
                        (ClipboardManager) getContext().getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clipData = clipboard.getPrimaryClip();
                if (clipData != null) {
                    // Convert the clip data to a simple string
                    StringBuilder builder = new StringBuilder();
                    for (int i = 0; i < clipData.getItemCount(); i++) {
                        builder.append(clipData.getItemAt(i).coerceToText(getContext()));
                    }

                    // Identify how much of the original text should be replaced
                    int min = 0;
                    int max = getText().length();

                    if (isFocused()) {
                        final int selStart = getSelectionStart();
                        final int selEnd = getSelectionEnd();

                        min = Math.max(0, Math.min(selStart, selEnd));
                        max = Math.max(0, Math.max(selStart, selEnd));
                    }

                    Selection.setSelection(getText(), max);
                    getText().replace(min, max, builder.toString());
                    return true;
                }
            }
            return super.onTextContextMenuItem(id);
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            InputConnection connection = super.onCreateInputConnection(outAttrs);
            if (mFindToolbar.isIncognito()) {
                outAttrs.imeOptions |= EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING;
            }
            return connection;
        }
    }

    public FindToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (window == null && getVisibility() == View.VISIBLE) {
                            deactivate(/* clearSelection= */ true);
                        }
                    }

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        deactivate();
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        deactivate();
                    }

                    @Override
                    public void onClosingStateChanged(Tab tab, boolean closing) {
                        if (closing) deactivate();
                    }

                    @Override
                    public void onFindResultAvailable(FindNotificationDetails result) {
                        onFindResult(result);
                    }

                    @Override
                    public void onFindMatchRectsAvailable(FindMatchRectsDetails result) {
                        onFindMatchRects(result);
                    }
                };

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        deactivate();
                        updateVisualsForTabModel(isIncognito());
                    }
                };

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (tab != mCurrentTab) deactivate();
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        if (tab != mCurrentTab) return;
                        deactivate();
                    }
                };
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);

        mFindQuery = findViewById(R.id.find_query);
        mFindQuery.setFindToolbar(this);
        mFindQuery.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_FILTER);
        mFindQuery.setSelectAllOnFocus(true);
        mFindQuery.setOnFocusChangeListener(
                new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        mAccessibilityDidActivateResult = false;
                        if (!hasFocus) {
                            if (mFindQuery.getText().length() > 0) {
                                mSearchKeyShouldTriggerSearch = true;
                            }
                            mWindowAndroid.getKeyboardDelegate().hideKeyboard(mFindQuery);
                        }
                    }
                });
        mFindQuery.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        if (mFindInPageBridge == null) return;

                        mAccessibilityDidActivateResult = false;

                        if (mSettingFindTextProgrammatically) return;

                        // If we're called during onRestoreInstanceState() the current
                        // view won't have been set yet. TODO(husky): Find a better fix.
                        assert mCurrentTab != null;
                        assert mCurrentTab.getWebContents() != null;
                        if (mCurrentTab.getWebContents() == null) return;

                        if (s.length() > 0) {
                            // Don't clearResults() as that would cause flicker.
                            // Just wait until onFindResultReceived updates it.
                            mSearchKeyShouldTriggerSearch = false;
                            mFindInPageBridge.startFinding(s.toString(), true, false);
                        } else {
                            clearResults();
                            mFindInPageBridge.stopFinding(true);
                            setPrevNextEnabled(false);
                        }

                        if (!isIncognito()) {
                            mLastUserSearch = s.toString();
                        }
                    }
                });
        mFindQuery.setOnEditorActionListener(
                new TextView.OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        if (event != null && event.getAction() == KeyEvent.ACTION_UP) return false;

                        if (mFindInPageBridge == null) return false;

                        // Only trigger a new find if the text was set programmatically.
                        // Otherwise just revisit the current active match.
                        if (mSearchKeyShouldTriggerSearch) {
                            mSearchKeyShouldTriggerSearch = false;
                            hideKeyboardAndStartFinding(true);
                        } else {
                            mWindowAndroid.getKeyboardDelegate().hideKeyboard(mFindQuery);
                            mFindInPageBridge.activateFindInPageResultForAccessibility();
                            mAccessibilityDidActivateResult = true;
                        }
                        return true;
                    }
                });

        mFindStatus = findViewById(R.id.find_status);
        setStatus("", false);

        mFindPrevButton = findViewById(R.id.find_prev_button);
        mFindPrevButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        hideKeyboardAndStartFinding(false);
                    }
                });

        mFindNextButton = findViewById(R.id.find_next_button);
        mFindNextButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        hideKeyboardAndStartFinding(true);
                    }
                });

        setPrevNextEnabled(false);

        mCloseFindButton = findViewById(R.id.close_find_button);
        mCloseFindButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        deactivate();
                    }
                });

        mDivider = findViewById(R.id.find_separator);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        int result =
                shouldDeactivateByBackPress() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        deactivate();
        return result;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // Overridden by subclasses.
    protected void findResultSelected(Rect rect) {}

    private void hideKeyboardAndStartFinding(boolean forward) {
        if (mFindInPageBridge == null) return;

        final String findQuery = mFindQuery.getText().toString();
        if (findQuery.length() == 0) return;

        mWindowAndroid.getKeyboardDelegate().hideKeyboard(mFindQuery);
        mFindInPageBridge.startFinding(findQuery, forward, false);
        mFindInPageBridge.activateFindInPageResultForAccessibility();
        mAccessibilityDidActivateResult = true;
    }

    private boolean mShowKeyboardOnceWindowIsFocused;

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (mShowKeyboardOnceWindowIsFocused) {
            mShowKeyboardOnceWindowIsFocused = false;
            // See showKeyboard() for explanation.
            // By this point we've already waited till the window regains focus
            // from the options menu, but we still need to use postDelayed with
            // a zero wait time to delay until all the side-effects are complete
            // (e.g. becoming the target of the Input Method).
            mHandler.postDelayed(
                    new Runnable() {
                        @Override
                        public void run() {
                            showKeyboard();

                            // This is also a great time to set accessibility focus to the query box
                            // -
                            // this also fails if we don't wait until the window regains focus.
                            // Sending a HOVER_ENTER event before the ACCESSIBILITY_FOCUSED event
                            // is a widely-used hack to force TalkBack to move accessibility focus
                            // to a view, which is discouraged in general but reasonable in this
                            // case.
                            mFindQuery.sendAccessibilityEvent(
                                    AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER);
                            mFindQuery.sendAccessibilityEvent(
                                    AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
                        }
                    },
                    0);
        }
    }

    private void onFindMatchRects(FindMatchRectsDetails matchRects) {
        if (mResultBar == null) return;
        if (mFindQuery.getText().length() > 0) {
            mResultBar.setMatchRects(matchRects.version, matchRects.rects, matchRects.activeRect);
        } else {
            // Since we don't issue a request for an empty string we never get a 'no rects' response
            // in that case. This could cause us to display stale state if the user is deleting the
            // search string. If the response for the last character comes in after we've issued a
            // clearReslts in TextChangedListener that response will be accepted and we will end up
            // showing stale results for an empty query.
            // Sending an empty string message seems a bit wasteful, so instead we simply ignore all
            // results that come in if the query is empty.
            mResultBar.clearMatchRects();
        }
    }

    private void onFindResult(FindNotificationDetails result) {
        if (mResultBar != null) mResultBar.onFindResult();

        assert mFindInPageBridge != null;

        if ((result.activeMatchOrdinal == -1 || result.numberOfMatches == 1)
                && !result.finalUpdate) {
            // Wait until activeMatchOrdinal has been determined (is no longer
            // -1) before showing counts. Additionally, to reduce flicker,
            // ignore short-lived interim notifications with numberOfMatches set
            // to 1, which are sent as soon as something has been found (see bug
            // 894389 and FindBarController::UpdateFindBarForCurrentResult).
            // Instead wait until the scoping effort starts returning real
            // match counts (or the search actually finishes with 1 result).
            // This also protects against receiving bogus rendererSelectionRects
            // at the start (see below for why we can't filter them out).
            return;
        }

        if (result.finalUpdate) {
            if (result.numberOfMatches > 0) {
                // TODO(johnme): Don't wait till end of find, stream rects live!
                mFindInPageBridge.requestFindMatchRects(
                        mResultBar != null ? mResultBar.getRectsVersion() : -1);
            } else {
                clearResults();
            }

            findResultSelected(result.rendererSelectionRect);
        }

        // Even though we wait above until activeMatchOrdinal is no longer -1,
        // it's possible for it to still be -1 (unknown) in the final find
        // notification. This happens very rarely, e.g. if the m_activeMatch
        // found by WebFrameImpl::find has been removed from the DOM by the time
        // WebFrameImpl::scopeStringMatches tries to find the ordinal of the
        // active match (while counting the matches), as in b/4147049. In such
        // cases it looks less broken to show 0 instead of -1 (as desktop does).
        Context context = getContext();
        String text =
                context.getResources()
                        .getString(
                                R.string.find_in_page_count,
                                Math.max(result.activeMatchOrdinal, 0),
                                result.numberOfMatches);
        setStatus(text, result.numberOfMatches == 0);

        setPrevNextEnabled(result.numberOfMatches > 0);

        // The accessible version will be something like "Result 1 of 9".
        String accessibleText =
                getAccessibleStatusText(
                        Math.max(result.activeMatchOrdinal, 0), result.numberOfMatches);
        mFindStatus.setContentDescription(accessibleText);
        announceStatusForAccessibility(accessibleText);

        // Vibrate when no results are found, unless you're just deleting chars.
        if (result.numberOfMatches == 0
                && result.finalUpdate
                && !mFindInPageBridge
                        .getPreviousFindText()
                        .startsWith(mFindQuery.getText().toString())) {
            final boolean hapticFeedbackEnabled =
                    Settings.System.getInt(
                                    context.getContentResolver(),
                                    Settings.System.HAPTIC_FEEDBACK_ENABLED,
                                    1)
                            == 1;
            if (hapticFeedbackEnabled) {
                Vibrator v = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
                final long noResultsVibrateDurationMs = 50;
                v.vibrate(noResultsVibrateDurationMs);
            }
        }
    }

    private String getAccessibleStatusText(int activeMatchOrdinal, int numberOfMatches) {
        Context context = getContext();
        return (numberOfMatches > 0)
                ? context.getResources()
                        .getString(
                                R.string.accessible_find_in_page_count,
                                activeMatchOrdinal,
                                numberOfMatches)
                : context.getResources().getString(R.string.accessible_find_in_page_no_results);
    }

    private void announceStatusForAccessibility(final String announcementText) {
        // Don't announce if the user has already activated a result by pressing Enter/Search
        // or clicking on the Next/Previous buttons.
        if (mAccessibilityDidActivateResult) return;

        // Delay the announcement briefly, and if any additional announcements come in,
        // have them preempt the previous queued one. That makes for a better user experience
        // than speaking instantly as you're typing and constantly interrupting itself.

        if (mAccessibleAnnouncementRunnable != null) {
            mHandler.removeCallbacks(mAccessibleAnnouncementRunnable);
        }

        mAccessibleAnnouncementRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        mFindQuery.announceForAccessibility(announcementText);
                    }
                };
        mHandler.postDelayed(mAccessibleAnnouncementRunnable, ACCESSIBLE_ANNOUNCEMENT_DELAY_MILLIS);
    }

    /** The find toolbar's container must provide access to its TabModel. */
    public void setTabModelSelector(TabModelSelector modelSelector) {
        mTabModelSelector = modelSelector;
        updateVisualsForTabModel(isIncognito());
    }

    /** Sets the WindowAndroid in which the find toolbar will be shown. Needed for animations. */
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    /**
     * Handles updating any visual elements of the find toolbar based on changes to the tab model.
     * @param isIncognito Whether the current tab model is incognito or not.
     */
    protected void updateVisualsForTabModel(boolean isIncognito) {}

    /**
     * Sets a custom ActionMode.Callback instance to the FindQuery.  This lets us
     * get notified when the user tries to do copy, paste, etc. on the FindQuery.
     * @param callback The ActionMode.Callback instance to be notified when selection ActionMode
     * is triggered.
     */
    public void setActionModeCallbackForTextEdit(ActionMode.Callback callback) {
        mFindQuery.setCustomSelectionActionModeCallback(callback);
    }

    /** Sets the observer to be notified of changes to the find toolbar. */
    protected void setObserver(FindToolbarObserver observer) {
        mObserver = observer;
    }

    /** Checks to see if a WebContents is available to hook into. */
    protected boolean isWebContentAvailable() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        return currentTab != null
                && currentTab.getWebContents() != null
                && !currentTab.isNativePage();
    }

    /**
     * Initializes the find toolbar. Should be called just after the find toolbar is shown.
     * If the toolbar is already showing, this just focuses the toolbar.
     */
    public final void activate() {
        ThreadUtils.checkUiThread();
        if (!isWebContentAvailable()) return;

        if (mCurrentState == FindLocationBarState.SHOWN) {
            requestQueryFocus();
            return;
        }

        mDesiredState = FindLocationBarState.SHOWN;
        if (mCurrentState != FindLocationBarState.HIDDEN) return;
        setCurrentState(FindLocationBarState.SHOWING);
        handleActivate();
    }

    /** Logic for handling the activation of the find toolbar. */
    protected void handleActivate() {
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        for (TabModel model : mTabModelSelector.getModels()) {
            model.addObserver(mTabModelObserver);
        }
        mCurrentTab = mTabModelSelector.getCurrentTab();
        mCurrentTab.addObserver(mTabObserver);
        mFindInPageBridge = new FindInPageBridge(mCurrentTab.getWebContents());
        initializeFindText();
        mFindQuery.requestFocus();
        // The keyboard doesn't show itself automatically.
        showKeyboard();
        // Always show the bar to make the FindToolbar more distinct from the Omnibox.
        setResultsBarVisibility(true);
        updateVisualsForTabModel(isIncognito());

        setCurrentState(FindLocationBarState.SHOWN);
    }

    /**
     * Call this just before closing the find toolbar. The selection on the page will be cleared.
     */
    public final void deactivate() {
        deactivate(true);
    }

    /**
     * Call this just before closing the find toolbar.
     * @param clearSelection Whether the selection on the page should be cleared.
     */
    public final void deactivate(boolean clearSelection) {
        ThreadUtils.checkUiThread();

        mDesiredState = FindLocationBarState.HIDDEN;
        if (mCurrentState != FindLocationBarState.SHOWN) return;
        setCurrentState(FindLocationBarState.HIDING);
        handleDeactivation(clearSelection);
    }

    /** Logic for handling deactivating the find toolbar. */
    protected void handleDeactivation(boolean clearSelection) {
        setResultsBarVisibility(false);

        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        for (TabModel model : mTabModelSelector.getModels()) {
            model.removeObserver(mTabModelObserver);
        }

        mCurrentTab.removeObserver(mTabObserver);

        mWindowAndroid.getKeyboardDelegate().hideKeyboard(mFindQuery);
        if (mFindQuery.getText().length() > 0) {
            clearResults();
            mFindInPageBridge.stopFinding(clearSelection);
        }

        mFindInPageBridge.destroy();
        mFindInPageBridge = null;
        mCurrentTab = null;

        setCurrentState(FindLocationBarState.HIDDEN);
    }

    private void setCurrentState(@FindLocationBarState int state) {
        mCurrentState = state;
        mBackPressStateSupplier.set(shouldDeactivateByBackPress());

        // Notify the observers if we hit the transition states.
        if (mObserver != null) {
            if (mCurrentState == FindLocationBarState.HIDDEN) {
                mObserver.onFindToolbarHidden();
            } else if (mCurrentState == FindLocationBarState.SHOWN) {
                mObserver.onFindToolbarShown();
            }
        }

        // Ensure the current state reflects the desired state if the state change happened while
        // processing the previous state change.
        assert mDesiredState == FindLocationBarState.HIDDEN
                || mDesiredState == FindLocationBarState.SHOWN;
        if (mCurrentState == FindLocationBarState.HIDDEN
                && mDesiredState == FindLocationBarState.SHOWN) {
            activate();
        } else if (mCurrentState == FindLocationBarState.SHOWN
                && mDesiredState == FindLocationBarState.HIDDEN) {
            deactivate();
        }
    }

    // Whether the find toolbar should be deactivated by back press.
    private boolean shouldDeactivateByBackPress() {
        return mCurrentState == FindLocationBarState.SHOWN;
    }

    /** Requests focus for the query input field and shows the keyboard. */
    public void requestQueryFocus() {
        mFindQuery.requestFocus();
        showKeyboard();
    }

    /** Called by the tablet-specific implementation when the hide animation is about to begin. */
    protected void onHideAnimationStart() {
        // We do this because hiding the bar after the animation ends doesn't look good.
        setResultsBarVisibility(false);
    }

    /**
     * @see WindowAndroid#startAnimationOverContent(Animator)
     */
    protected void startAnimationOverContent(Animator animation) {
        mWindowAndroid.startAnimationOverContent(animation);
    }

    @VisibleForTesting
    public FindResultBar getFindResultBar() {
        return mResultBar;
    }

    /** Returns whether an animation to show/hide the FindToolbar is currently running. */
    @VisibleForTesting
    public boolean isAnimating() {
        return false;
    }

    /** Restores the last text searched in this tab, or the global last search. */
    private void initializeFindText() {
        assert mFindInPageBridge != null;

        mSettingFindTextProgrammatically = true;
        String findText = null;
        if (mSettingFindTextProgrammatically) {
            findText = mFindInPageBridge.getPreviousFindText();
            if (findText.isEmpty() && !isIncognito()) {
                findText = mLastUserSearch;
            }
            mSearchKeyShouldTriggerSearch = true;
        } else {
            mSearchKeyShouldTriggerSearch = false;
        }
        mFindQuery.setText(findText);
        mSettingFindTextProgrammatically = false;
    }

    /** Sets the find query text string. */
    void setFindQuery(String findText) {
        mFindQuery.setText(findText);
    }

    /** Clears the result displays (except in-page match highlighting). */
    protected void clearResults() {
        setStatus("", false);
        if (mResultBar != null) {
            mResultBar.clearMatchRects();
        }
    }

    private void setResultsBarVisibility(boolean visibility) {
        if (visibility
                && mResultBar == null
                && mCurrentTab != null
                && mCurrentTab.getWebContents() != null) {
            assert mFindInPageBridge != null;

            mResultBar =
                    new FindResultBar(
                            getContext(),
                            mCurrentTab.getContentView(),
                            mWindowAndroid,
                            mFindInPageBridge);
        } else if (!visibility) {
            if (mResultBar != null) {
                mResultBar.dismiss();
                mResultBar = null;
            }
        }
    }

    private void setStatus(String text, boolean failed) {
        mFindStatus.setText(text);
        mFindStatus.setContentDescription(null);
        mFindStatus.setTextColor(getStatusColor(failed, isIncognito()));
        mFindStatus.setVisibility(TextUtils.isEmpty(text) ? GONE : VISIBLE);
    }

    /**
     * @param failed    Whether or not the find query had any matching results.
     * @param incognito Whether or not the current tab is incognito.
     * @return          The color of the status text.
     */
    protected int getStatusColor(boolean failed, boolean incognito) {
        return failed
                ? getContext().getColor(R.color.find_in_page_failed_results_status_color)
                : SemanticColorUtils.getDefaultTextColorSecondary(getContext());
    }

    protected void setPrevNextEnabled(boolean enable) {
        mFindPrevButton.setEnabled(enable);
        mFindNextButton.setEnabled(enable);
    }

    private void showKeyboard() {
        if (!mFindQuery.hasWindowFocus()) {
            // HACK: showKeyboard() is normally called from activate() which is
            // triggered by an options menu item. Unfortunately, because the
            // options menu is still focused at this point, that means our
            // window doesn't actually have focus when this first gets called,
            // and hence it isn't the target of the Input Method, and in
            // practice that means the soft keyboard never shows up (whatever
            // flags you pass). So as a workaround we postpone asking for the
            // keyboard to be shown until just after the window gets refocused.
            // See onWindowFocusChanged(boolean hasFocus).
            mShowKeyboardOnceWindowIsFocused = true;
            return;
        }
        mWindowAndroid.getKeyboardDelegate().showKeyboard(mFindQuery);
    }

    protected boolean isIncognito() {
        return mTabModelSelector != null && mTabModelSelector.isIncognitoSelected();
    }
}
