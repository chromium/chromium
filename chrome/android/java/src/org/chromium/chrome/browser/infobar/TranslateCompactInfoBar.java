// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar.PrefObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;
import org.chromium.components.translate.TranslateFeatureMap;
import org.chromium.components.translate.TranslateMenu;
import org.chromium.components.translate.TranslateMenuHelper;
import org.chromium.components.translate.TranslateOption;
import org.chromium.components.translate.TranslateOptions;
import org.chromium.components.translate.TranslateTabLayout;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/**
 * Java version of the compact translate infobar.
 */
public class TranslateCompactInfoBar
        extends InfoBar implements TabLayout.OnTabSelectedListener,
                                   TranslateMenuHelper.TranslateMenuListener, PrefObserver {
    public static final int TRANSLATING_INFOBAR = 1;
    public static final int AFTER_TRANSLATING_INFOBAR = 2;

    private static final int SOURCE_TAB_INDEX = 0;
    private static final int TARGET_TAB_INDEX = 1;

    // Action ID for Snackbar.
    // Actions performed by clicking on on the overflow menu.
    public static final int ACTION_OVERFLOW_ALWAYS_TRANSLATE = 0;
    public static final int ACTION_OVERFLOW_NEVER_SITE = 1;
    public static final int ACTION_OVERFLOW_NEVER_LANGUAGE = 2;
    // Actions triggered automatically. (when translation or denied count reaches the threshold.)
    public static final int ACTION_AUTO_ALWAYS_TRANSLATE = 3;
    public static final int ACTION_AUTO_NEVER_LANGUAGE = 4;

    private final int mInitialStep;
    private final int mDefaultTextColor;
    private final TranslateOptions mOptions;

    private long mNativeTranslateInfoBarPtr;
    private TranslateTabLayout mTabLayout;

    private static final String INFOBAR_HISTOGRAM = "Translate.CompactInfobar.Event";

    // Need 2 instances of TranslateMenuHelper to prevent a race condition bug which happens when
    // showing language menu after dismissing overflow menu.
    private TranslateMenuHelper mOverflowMenuHelper;
    private TranslateMenuHelper mLanguageMenuHelper;

    private ImageButton mMenuButton;
    private InfoBarCompactLayout mParent;

    private final WindowAndroid mWindowAndroid;
    private TranslateSnackbarController mSnackbarController;

    private boolean mMenuExpanded;
    private boolean mIsFirstLayout = true;
    private boolean mUserInteracted;

    private final PrefChangeRegistrar mPrefChangeRegistrar;

    /**
     * The controller for translate UI snackbars. The snackbars show when a translate preference has
     * been enabled. The |onAction| method cancels the action, while onDismissNoAction toggles the
     * preference.
     */
    class TranslateSnackbarController implements SnackbarController {
        private final int mActionId;

        /** @param actionId Overflow menu action id for translate preference to enable.*/
        public TranslateSnackbarController(int actionId) {
            mActionId = actionId;
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            mSnackbarController = null;
            handleTranslateOverflowOption(mActionId);
        }

        @Override
        public void onAction(Object actionData) {
            mSnackbarController = null;
            switch (mActionId) {
                case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                    recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_CANCEL_ALWAYS);
                    return;
                case ACTION_AUTO_ALWAYS_TRANSLATE:
                    recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS);
                    return;
                case ACTION_OVERFLOW_NEVER_LANGUAGE:
                    recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_CANCEL_NEVER);
                    return;
                case ACTION_AUTO_NEVER_LANGUAGE:
                    recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER);
                    // This snackbar is triggered automatically after a close button click.  Need to
                    // dismiss the infobar even if the user cancels the "Never Translate".
                    closeInfobar(false);
                    return;
                case ACTION_OVERFLOW_NEVER_SITE:
                    recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_CANCEL_NEVER_SITE);
                    return;
                default:
                    assert false : "Unsupported Menu Item Id, when handling snackbar action";
                    return;
            }
        }
    };

    @CalledByNative
    private static InfoBar create(Tab tab, int initialStep, String sourceLanguageCode,
            String targetLanguageCode,
            boolean neverLanguage /* Never automatically translate the source language */,
            boolean neverDomain /* Never automatically translate the domain */,
            boolean alwaysTranslate /* Always automatically translate the source language */,
            boolean triggeredFromMenu, String[] allLanguages, String[] allLanguagesCodes,
            int[] allLanguagesHashCodes, String[] contentLanguagesCodes, int tabTextColor) {
        recordInfobarAction(InfobarEvent.INFOBAR_IMPRESSION);

        return new TranslateCompactInfoBar(tab.getWindowAndroid(), initialStep, sourceLanguageCode,
                targetLanguageCode, neverLanguage, neverDomain, alwaysTranslate, triggeredFromMenu,
                allLanguages, allLanguagesCodes, allLanguagesHashCodes, contentLanguagesCodes,
                tabTextColor);
    }

    TranslateCompactInfoBar(WindowAndroid windowAndroid, int initialStep, String sourceLanguageCode,
            String targetLanguageCode, boolean neverLanguage, boolean neverDomain,
            boolean alwaysTranslate, boolean triggeredFromMenu, String[] allLanguages,
            String[] allLanguagesCodes, int[] allLanguagesHashCodes, String[] contentLanguagesCodes,
            int tabTextColor) {
        super(R.drawable.infobar_translate_compact, 0, null, null);
        mWindowAndroid = windowAndroid;

        if (TranslateFeatureMap.isEnabled(TranslateFeatureMap.CONTENT_LANGUAGES_IN_LANGUAGE_PICKER)
                && !TranslateFeatureMap.getInstance().getFieldTrialParamByFeatureAsBoolean(
                        TranslateFeatureMap.CONTENT_LANGUAGES_IN_LANGUAGE_PICKER,
                        TranslateFeatureMap.CONTENT_LANGUAGES_DISABLE_OBSERVERS_PARAM, false)) {
            mPrefChangeRegistrar = new PrefChangeRegistrar();
            mPrefChangeRegistrar.addObserver(Pref.ACCEPT_LANGUAGES, this);
        } else {
            mPrefChangeRegistrar = null;
        }
        mInitialStep = initialStep;
        mDefaultTextColor = tabTextColor;
        mOptions = TranslateOptions.create(sourceLanguageCode, targetLanguageCode, allLanguages,
                allLanguagesCodes, neverLanguage, neverDomain, alwaysTranslate, triggeredFromMenu,
                allLanguagesHashCodes, contentLanguagesCodes);
    }

    @Override
    public void onPreferenceChange() {
        if (mNativeTranslateInfoBarPtr != 0) {
            String[] currentContentLanguages =
                    TranslateCompactInfoBarJni.get().getContentLanguagesCodes(
                            mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this);
            mOptions.updateContentLanguages(currentContentLanguages);
            if (mLanguageMenuHelper != null) {
                mLanguageMenuHelper.onContentLanguagesChanged(currentContentLanguages);
            }
        }
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout parent) {
        LinearLayout content =
                (LinearLayout) LayoutInflater.from(getContext())
                        .inflate(R.layout.infobar_translate_compact_content, parent, false);

        // When parent tab is being switched out (view detached), dismiss all menus and snackbars.
        content.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                dismissMenusAndSnackbars();
            }
        });

        mTabLayout = (TranslateTabLayout) content.findViewById(R.id.translate_infobar_tabs);
        if (mDefaultTextColor > 0) {
            mTabLayout.setTabTextColors(SemanticColorUtils.getDefaultTextColor(getContext()),
                    SemanticColorUtils.getDefaultTextColorAccent1(
                            getContext()) /*tab_layout_selected_tab_color*/);
        }
        mTabLayout.addTabs(mOptions.sourceLanguageName(), mOptions.targetLanguageName());

        if (mInitialStep == TRANSLATING_INFOBAR) {
            // Set translating status in the beginning for pages translated automatically.
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            if (mOptions.triggeredFromMenu()) {
                mUserInteracted = true;
            }
        } else if (mInitialStep == AFTER_TRANSLATING_INFOBAR) {
            // Focus on target tab since we are after translation.
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        }

        mTabLayout.addOnTabSelectedListener(this);

        // Dismiss all menus and end scrolling animation when there is layout changed.
        mTabLayout.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (left != oldLeft || top != oldTop || right != oldRight || bottom != oldBottom) {
                    // Dismiss all menus to prevent menu misplacement.
                    dismissMenus();

                    if (mIsFirstLayout) {
                        // Scrolls to the end to make sure the target language tab is visible when
                        // language tabs is too long.
                        mTabLayout.startScrollingAnimationIfNeeded();
                        mIsFirstLayout = false;
                        return;
                    }

                    // End scrolling animation when layout changed.
                    mTabLayout.endScrollingAnimationIfPlaying();
                }
            }
        });

        mMenuButton = content.findViewById(R.id.translate_infobar_menu_button);
        mMenuButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mTabLayout.endScrollingAnimationIfPlaying();
                recordInfobarAction(InfobarEvent.INFOBAR_OPTIONS);
                initMenuHelper(TranslateMenu.MENU_OVERFLOW);
                mOverflowMenuHelper.show(TranslateMenu.MENU_OVERFLOW, getParentWidth());
                mMenuExpanded = true;
            }
        });

        parent.addContent(content, 1.0f);
        mParent = parent;
    }

    private void initMenuHelper(int menuType) {
        boolean isIncognito = TranslateCompactInfoBarJni.get().isIncognito(
                mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this);
        boolean isSourceLangUnknown = mOptions.sourceLanguageCode().equals("und");
        switch (menuType) {
            case TranslateMenu.MENU_OVERFLOW:
                mOverflowMenuHelper = new TranslateMenuHelper(getContext(), mMenuButton, mOptions,
                        this, isIncognito, isSourceLangUnknown);
                return;
            case TranslateMenu.MENU_TARGET_LANGUAGE:
            case TranslateMenu.MENU_SOURCE_LANGUAGE:
                if (mLanguageMenuHelper == null) {
                    mLanguageMenuHelper = new TranslateMenuHelper(getContext(), mMenuButton,
                            mOptions, this, isIncognito, isSourceLangUnknown);
                }
                return;
            default:
                assert false : "Unsupported Menu Item Id";
        }
    }

    /**
     * Selects the tab at tabIndex without triggering onTabSelected. This avoids treating the
     * selection as a user interaction and prevents a potential loop where a translate state change
     * updates the UI, which then updates the translate state.
     */
    private void silentlySelectTabAt(int tabIndex) {
        if (mTabLayout == null) {
            return;
        }

        mTabLayout.removeOnTabSelectedListener(this);
        mTabLayout.getTabAt(tabIndex).select();
        mTabLayout.addOnTabSelectedListener(this);
    }

    /**
     * Begins the translation process and marks it as initiated by the user.
     */
    private void startUserInitiatedTranslation() {
        mUserInteracted = true;
        onButtonClicked(ActionType.TRANSLATE);
    }

    @CalledByNative
    private void onTranslating() {
        if (mTabLayout != null) {
            silentlySelectTabAt(TARGET_TAB_INDEX);
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
        }
    }

    @CalledByNative
    private boolean onPageTranslated(int errorType) {
        boolean errorUIShown = false;
        if (mTabLayout != null) {
            mTabLayout.hideProgressBar();
            if (errorType != 0) {
                Toast toast = Toast.makeText(
                        getContext(), R.string.translate_infobar_error, Toast.LENGTH_SHORT);
                int[] location = new int[2];
                mTabLayout.getLocationOnScreen(location);
                int yOffset = location[1] - mTabLayout.getHeight()
                        - getContext().getResources().getDimensionPixelSize(
                                R.dimen.translate_toast_y_offset);
                toast.setGravity(Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, yOffset);
                toast.show();
                errorUIShown = true;
                silentlySelectTabAt(SOURCE_TAB_INDEX);
            }
        }
        return errorUIShown;
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        mNativeTranslateInfoBarPtr = nativePtr;
    }

    @CalledByNative
    private void setAutoAlwaysTranslate() {
        createAndShowSnackbar(getContext().getString(R.string.translate_snackbar_always_translate,
                                      mOptions.sourceLanguageName(), mOptions.targetLanguageName()),
                Snackbar.UMA_TRANSLATE_ALWAYS, ACTION_AUTO_ALWAYS_TRANSLATE);
    }

    private boolean updateTargetLanguage(String languageCode) {
        // Set the target code in TranslateOptions.
        if (!mOptions.setTargetLanguage(languageCode)) {
            return false;
        }

        // Adjust UI if options were updated successfully.
        mTabLayout.replaceTabTitle(
                TARGET_TAB_INDEX, mOptions.getRepresentationFromCode(languageCode));
        return true;
    }

    @CalledByNative
    public void onTargetLanguageChanged(String languageCode) {
        updateTargetLanguage(languageCode);
    }

    @Override
    protected void resetNativeInfoBar() {
        mNativeTranslateInfoBarPtr = 0;
        super.resetNativeInfoBar();
    }

    private void closeInfobar(boolean explicitly) {
        if (isDismissed()) return;

        if (!mUserInteracted) {
            recordInfobarAction(InfobarEvent.INFOBAR_DECLINE);
        }

        // Check if we should trigger the auto "never translate" if infobar is closed explicitly.
        if (explicitly && mNativeTranslateInfoBarPtr != 0
                && TranslateCompactInfoBarJni.get().shouldAutoNeverTranslate(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this, mMenuExpanded)) {
            createAndShowSnackbar(getContext().getString(R.string.translate_snackbar_language_never,
                                          mOptions.sourceLanguageName()),
                    Snackbar.UMA_TRANSLATE_NEVER, ACTION_AUTO_NEVER_LANGUAGE);
            // Postpone the infobar dismiss until the snackbar finished showing.  Otherwise, the
            // reference to the native infobar is killed and there is no way for the snackbar to
            // perform the action.
            return;
        }
        // This line will dismiss this infobar.
        super.onCloseButtonClicked();
    }

    @Override
    public void onCloseButtonClicked() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
        }
        mTabLayout.endScrollingAnimationIfPlaying();
        closeInfobar(true);
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        switch (tab.getPosition()) {
            case SOURCE_TAB_INDEX:
                recordInfobarAction(InfobarEvent.INFOBAR_REVERT);
                mUserInteracted = true;
                onButtonClicked(ActionType.TRANSLATE_SHOW_ORIGINAL);
                return;
            case TARGET_TAB_INDEX:
                recordInfobarAction(InfobarEvent.INFOBAR_TARGET_TAB_TRANSLATE);
                startUserInitiatedTranslation();
                return;
            default:
                assert false : "Unexpected Tab Index";
        }
    }

    @Override
    public void onTabUnselected(TabLayout.Tab tab) {}

    @Override
    public void onTabReselected(TabLayout.Tab tab) {}

    @Override
    public void onOverflowMenuItemClicked(int itemId) {
        switch (itemId) {
            case TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE:
                recordInfobarAction(InfobarEvent.INFOBAR_MORE_LANGUAGES);
                initMenuHelper(TranslateMenu.MENU_TARGET_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_TARGET_LANGUAGE, getParentWidth());
                return;
            case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                // Only show snackbar when "Always Translate" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)) {
                    recordInfobarAction(InfobarEvent.INFOBAR_ALWAYS_TRANSLATE);
                    createAndShowSnackbar(
                            getContext().getString(R.string.translate_snackbar_always_translate,
                                    mOptions.sourceLanguageName(), mOptions.targetLanguageName()),
                            Snackbar.UMA_TRANSLATE_ALWAYS, ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                } else {
                    recordInfobarAction(InfobarEvent.INFOBAR_ALWAYS_TRANSLATE_UNDO);
                    handleTranslateOverflowOption(ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                // Only show snackbar when "Never Translate" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE)) {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE);
                    createAndShowSnackbar(
                            getContext().getString(R.string.translate_snackbar_language_never,
                                    mOptions.sourceLanguageName()),
                            Snackbar.UMA_TRANSLATE_NEVER, ACTION_OVERFLOW_NEVER_LANGUAGE);
                } else {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE_UNDO);
                    handleTranslateOverflowOption(ACTION_OVERFLOW_NEVER_LANGUAGE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                // Only show snackbar when "Never Translate" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN)) {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE_SITE);
                    createAndShowSnackbar(
                            getContext().getString(R.string.translate_snackbar_site_never),
                            Snackbar.UMA_TRANSLATE_NEVER_SITE, ACTION_OVERFLOW_NEVER_SITE);
                } else {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE_SITE_UNDO);
                    handleTranslateOverflowOption(ACTION_OVERFLOW_NEVER_SITE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                recordInfobarAction(InfobarEvent.INFOBAR_PAGE_NOT_IN);
                initMenuHelper(TranslateMenu.MENU_SOURCE_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_SOURCE_LANGUAGE, getParentWidth());
                return;
            default:
                assert false : "Unexpected overflow menu code";
        }
    }

    @Override
    public void onTargetMenuItemClicked(String languageCode) {
        // Set the target code in both UI and native.
        if (mNativeTranslateInfoBarPtr != 0 && updateTargetLanguage(languageCode)) {
            recordInfobarAction(InfobarEvent.INFOBAR_MORE_LANGUAGES_TRANSLATE);
            // Update the target language in the backend.
            TranslateCompactInfoBarJni.get().applyStringTranslateOption(mNativeTranslateInfoBarPtr,
                    TranslateCompactInfoBar.this, TranslateOption.TARGET_CODE, languageCode);
            startUserInitiatedTranslation();
        }
    }

    @Override
    public void onSourceMenuItemClicked(String languageCode) {
        // Set the source code in both UI and native.
        if (mNativeTranslateInfoBarPtr != 0 && mOptions.setSourceLanguage(languageCode)) {
            TranslateCompactInfoBarJni.get().applyStringTranslateOption(mNativeTranslateInfoBarPtr,
                    TranslateCompactInfoBar.this, TranslateOption.SOURCE_CODE, languageCode);
            // Adjust UI.
            mTabLayout.replaceTabTitle(
                    SOURCE_TAB_INDEX, mOptions.getRepresentationFromCode(languageCode));
            startUserInitiatedTranslation();
        }
    }

    // Dismiss all overflow menus that remains open.
    // This is called when infobar started hiding or layout changed.
    private void dismissMenus() {
        if (mOverflowMenuHelper != null) mOverflowMenuHelper.dismiss();
        if (mLanguageMenuHelper != null) mLanguageMenuHelper.dismiss();
    }

    // Dismiss all overflow menus and snackbars that belong to this infobar and remain open.
    private void dismissMenusAndSnackbars() {
        dismissMenus();
        if (getSnackbarManager() != null && mSnackbarController != null) {
            getSnackbarManager().dismissSnackbars(mSnackbarController);
        }
    }

    @Override
    protected void onStartedHiding() {
        dismissMenusAndSnackbars();
    }

    @Override
    protected CharSequence getAccessibilityMessage(CharSequence defaultMessage) {
        return getContext().getString(R.string.translate_button);
    }

    /**
     * Returns true if overflow menu is showing.  This is only used for automation testing.
     */
    public boolean isShowingOverflowMenuForTesting() {
        if (mOverflowMenuHelper == null) return false;
        return mOverflowMenuHelper.isShowing();
    }

    /**
     * Returns true if language menu is showing.  This is only used for automation testing.
     */
    public boolean isShowingLanguageMenuForTesting() {
        if (mLanguageMenuHelper == null) return false;
        return mLanguageMenuHelper.isShowing();
    }

    /**
     * Returns true if the tab at the given |tabIndex| is selected. This is only used for automation
     * testing.
     */
    private boolean isTabSelectedForTesting(int tabIndex) {
        return mTabLayout.getTabAt(tabIndex).isSelected();
    }

    /**
     * Returns true if the target tab is selected. This is only used for automation testing.
     */
    public boolean isSourceTabSelectedForTesting() {
        return this.isTabSelectedForTesting(SOURCE_TAB_INDEX);
    }

    /**
     * Returns true if the target tab is selected. This is only used for automation testing.
     */
    public boolean isTargetTabSelectedForTesting() {
        return this.isTabSelectedForTesting(TARGET_TAB_INDEX);
    }

    private void createAndShowSnackbar(String title, int umaType, int actionId) {
        if (getSnackbarManager() == null) {
            // Directly apply menu option, if snackbar system is not working.
            handleTranslateOverflowOption(actionId);
            return;
        }
        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION);
                break;
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION);
                break;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
                recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION);
                break;
            case ACTION_AUTO_NEVER_LANGUAGE:
                recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION);
                break;
            case ACTION_OVERFLOW_NEVER_SITE:
                recordInfobarAction(InfobarEvent.INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION);
                break;
            default:
                assert false : "Unsupported Menu Item Id, to show snackbar.";
        }

        mSnackbarController = new TranslateSnackbarController(actionId);
        getSnackbarManager().showSnackbar(
                Snackbar.make(title, mSnackbarController, Snackbar.TYPE_NOTIFICATION, umaType)
                        .setSingleLine(false)
                        .setAction(
                                getContext().getString(R.string.translate_snackbar_cancel), null));
    }

    private SnackbarManager getSnackbarManager() {
        return SnackbarManagerProvider.from(mWindowAndroid);
    }

    private void handleTranslateOverflowOption(int actionId) {
        // Quit if native is destroyed.
        if (mNativeTranslateInfoBarPtr == 0) return;

        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                mUserInteracted = true;
                toggleAlwaysTranslate();
                // Start translating if always translate is selected and if page is not already
                // translated to the target language.
                if (mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)
                        && mTabLayout.getSelectedTabPosition() == SOURCE_TAB_INDEX) {
                    startUserInitiatedTranslation();
                }
                return;
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                toggleAlwaysTranslate();
                return;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
                mUserInteracted = true;
                // Fallthrough intentional.
            case ACTION_AUTO_NEVER_LANGUAGE:
                mOptions.toggleNeverTranslateLanguageState(
                        !mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
                // If toggling never translate to true, after applying this option the translation
                // will revert and the infobar will dismiss.
                TranslateCompactInfoBarJni.get().applyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this,
                        TranslateOption.NEVER_TRANSLATE,
                        mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
                return;
            case ACTION_OVERFLOW_NEVER_SITE:
                mOptions.toggleNeverTranslateDomainState(
                        !mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
                mUserInteracted = true;
                // If toggling never translate to true, after applying this option the translation
                // will revert and the infobar will dismiss.
                TranslateCompactInfoBarJni.get().applyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this,
                        TranslateOption.NEVER_TRANSLATE_SITE,
                        mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
                return;
            default:
                assert false : "Unsupported Menu Item Id, in handle post snackbar";
        }
    }

    private void toggleAlwaysTranslate() {
        mOptions.toggleAlwaysTranslateLanguageState(
                !mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        TranslateCompactInfoBarJni.get().applyBoolTranslateOption(mNativeTranslateInfoBarPtr,
                TranslateCompactInfoBar.this, TranslateOption.ALWAYS_TRANSLATE,
                mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
    }

    private static void recordInfobarAction(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                INFOBAR_HISTOGRAM, action, InfobarEvent.INFOBAR_HISTOGRAM_BOUNDARY);
    }

    // Return the width of parent in pixels.  Return 0 if there is no parent.
    private int getParentWidth() {
        return mParent != null ? mParent.getWidth() : 0;
    }

    @NativeMethods
    interface Natives {
        void applyStringTranslateOption(long nativeTranslateCompactInfoBar,
                TranslateCompactInfoBar caller, int option, String value);
        void applyBoolTranslateOption(long nativeTranslateCompactInfoBar,
                TranslateCompactInfoBar caller, int option, boolean value);
        boolean shouldAutoNeverTranslate(long nativeTranslateCompactInfoBar,
                TranslateCompactInfoBar caller, boolean menuExpanded);
        boolean isIncognito(long nativeTranslateCompactInfoBar, TranslateCompactInfoBar caller);
        String[] getContentLanguagesCodes(
                long nativeTranslateCompactInfoBar, TranslateCompactInfoBar caller);
    }
}
