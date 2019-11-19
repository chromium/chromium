// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.design.widget.TabLayout;
import android.support.v4.content.ContextCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.translate.TranslateMenu;
import org.chromium.chrome.browser.infobar.translate.TranslateMenuHelper;
import org.chromium.chrome.browser.infobar.translate.TranslateTabLayout;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.ui.widget.Toast;

/**
 * Java version of the compact translate infobar.
 */
public class TranslateCompactInfoBar extends InfoBar
        implements TabLayout.OnTabSelectedListener, TranslateMenuHelper.TranslateMenuListener {
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

    // Metric to track the total number of translations in a page, including reverts to original.
    private int mTotalTranslationCount;

    // Histogram names for logging metrics.
    private static final String INFOBAR_HISTOGRAM_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.Translate";
    private static final String INFOBAR_HISTOGRAM_MORE_LANGUAGES_LANGUAGE =
            "Translate.CompactInfobar.Language.MoreLanguages";
    private static final String INFOBAR_HISTOGRAM_PAGE_NOT_IN_LANGUAGE =
            "Translate.CompactInfobar.Language.PageNotIn";
    private static final String INFOBAR_HISTOGRAM_ALWAYS_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.AlwaysTranslate";
    private static final String INFOBAR_HISTOGRAM_NEVER_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.NeverTranslate";
    private static final String INFOBAR_HISTOGRAM = "Translate.CompactInfobar.Event";
    private static final String INFOBAR_HISTOGRAM_TRANSLATION_COUNT =
            "Translate.CompactInfobar.TranslationsPerPage";

    /**
     * This is used to back a UMA histogram, so it should be treated as
     * append-only. The values should not be changed or reused, and
     * INFOBAR_HISTOGRAM_BOUNDARY should be the last.
     */
    private static final int INFOBAR_IMPRESSION = 0;
    private static final int INFOBAR_TARGET_TAB_TRANSLATE = 1;
    private static final int INFOBAR_DECLINE = 2;
    private static final int INFOBAR_OPTIONS = 3;
    private static final int INFOBAR_MORE_LANGUAGES = 4;
    private static final int INFOBAR_MORE_LANGUAGES_TRANSLATE = 5;
    private static final int INFOBAR_PAGE_NOT_IN = 6;
    private static final int INFOBAR_ALWAYS_TRANSLATE = 7;
    private static final int INFOBAR_NEVER_TRANSLATE = 8;
    private static final int INFOBAR_NEVER_TRANSLATE_SITE = 9;
    private static final int INFOBAR_SCROLL_HIDE = 10;
    private static final int INFOBAR_SCROLL_SHOW = 11;
    private static final int INFOBAR_REVERT = 12;
    private static final int INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION = 13;
    private static final int INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION = 14;
    private static final int INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION = 15;
    private static final int INFOBAR_SNACKBAR_CANCEL_ALWAYS = 16;
    private static final int INFOBAR_SNACKBAR_CANCEL_NEVER_SITE = 17;
    private static final int INFOBAR_SNACKBAR_CANCEL_NEVER = 18;
    private static final int INFOBAR_ALWAYS_TRANSLATE_UNDO = 19;
    private static final int INFOBAR_CLOSE_DEPRECATED = 20;
    private static final int INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION = 21;
    private static final int INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION = 22;
    private static final int INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS = 23;
    private static final int INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER = 24;
    private static final int INFOBAR_HISTOGRAM_BOUNDARY = 25;

    // Need 2 instances of TranslateMenuHelper to prevent a race condition bug which happens when
    // showing language menu after dismissing overflow menu.
    private TranslateMenuHelper mOverflowMenuHelper;
    private TranslateMenuHelper mLanguageMenuHelper;

    private ImageButton mMenuButton;
    private InfoBarCompactLayout mParent;

    private TranslateSnackbarController mSnackbarController;

    private boolean mMenuExpanded;
    private boolean mIsFirstLayout = true;
    private boolean mUserInteracted;

    /** The controller for translate UI snackbars. */
    class TranslateSnackbarController implements SnackbarController {
        private final int mActionId;

        public TranslateSnackbarController(int actionId) {
            mActionId = actionId;
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            mSnackbarController = null;
            handleTranslateOptionPostSnackbar(mActionId);
        }

        @Override
        public void onAction(Object actionData) {
            mSnackbarController = null;
            switch (mActionId) {
                case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_ALWAYS);
                    return;
                case ACTION_AUTO_ALWAYS_TRANSLATE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS);
                    return;
                case ACTION_OVERFLOW_NEVER_LANGUAGE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_NEVER);
                    return;
                case ACTION_AUTO_NEVER_LANGUAGE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER);
                    // This snackbar is triggered automatically after a close button click.  Need to
                    // dismiss the infobar even if the user cancels the "Never Translate".
                    closeInfobar(false);
                    return;
                case ACTION_OVERFLOW_NEVER_SITE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_NEVER_SITE);
                    return;
                default:
                    assert false : "Unsupported Menu Item Id, when handling snackbar action";
                    return;
            }
        }
    };

    @CalledByNative
    private static InfoBar create(int initialStep, String sourceLanguageCode,
            String targetLanguageCode, boolean alwaysTranslate, boolean triggeredFromMenu,
            String[] languages, String[] languageCodes, int[] hashCodes, int tabTextColor) {
        recordInfobarAction(INFOBAR_IMPRESSION);
        return new TranslateCompactInfoBar(initialStep, sourceLanguageCode, targetLanguageCode,
                alwaysTranslate, triggeredFromMenu, languages, languageCodes, hashCodes,
                tabTextColor);
    }

    TranslateCompactInfoBar(int initialStep, String sourceLanguageCode, String targetLanguageCode,
            boolean alwaysTranslate, boolean triggeredFromMenu, String[] languages,
            String[] languageCodes, int[] hashCodes, int tabTextColor) {
        super(R.drawable.infobar_translate_compact, 0, null, null);

        mInitialStep = initialStep;
        mDefaultTextColor = tabTextColor;
        mOptions = TranslateOptions.create(sourceLanguageCode, targetLanguageCode, languages,
                languageCodes, alwaysTranslate, triggeredFromMenu, hashCodes);
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
            mTabLayout.setTabTextColors(
                    ContextCompat.getColor(getContext(), R.color.default_text_color),
                    ContextCompat.getColor(getContext(), R.color.tab_layout_selected_tab_color));
        }
        mTabLayout.addTabs(mOptions.sourceLanguageName(), mOptions.targetLanguageName());

        if (mInitialStep == TRANSLATING_INFOBAR) {
            // Set translating status in the beginning for pages translated automatically.
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            mUserInteracted = true;
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
                recordInfobarAction(INFOBAR_OPTIONS);
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
        switch (menuType) {
            case TranslateMenu.MENU_OVERFLOW:
                if (mOverflowMenuHelper == null) {
                    mOverflowMenuHelper = new TranslateMenuHelper(
                            getContext(), mMenuButton, mOptions, this, isIncognito);
                }
                return;
            case TranslateMenu.MENU_TARGET_LANGUAGE:
            case TranslateMenu.MENU_SOURCE_LANGUAGE:
                if (mLanguageMenuHelper == null) {
                    mLanguageMenuHelper = new TranslateMenuHelper(
                            getContext(), mMenuButton, mOptions, this, isIncognito);
                }
                return;
            default:
                assert false : "Unsupported Menu Item Id";
        }
    }

    private void startTranslating(int tabPosition) {
        if (TARGET_TAB_INDEX == tabPosition) {
            // Already on the target tab.
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            onButtonClicked(ActionType.TRANSLATE);
            mUserInteracted = true;
        } else {
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        }
    }

    @CalledByNative
    private void onPageTranslated(int errorType) {
        incrementAndRecordTranslationsPerPageCount();
        if (mTabLayout != null) {
            mTabLayout.hideProgressBar();
            if (errorType != 0) {
                Toast.makeText(getContext(), R.string.translate_infobar_error, Toast.LENGTH_SHORT)
                        .show();
                // Disable OnTabSelectedListener then revert selection.
                mTabLayout.removeOnTabSelectedListener(this);
                mTabLayout.getTabAt(SOURCE_TAB_INDEX).select();
                // Add OnTabSelectedListener back.
                mTabLayout.addOnTabSelectedListener(this);
            }
        }
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

    @Override
    protected void onNativeDestroyed() {
        mNativeTranslateInfoBarPtr = 0;
        super.onNativeDestroyed();
    }

    private void closeInfobar(boolean explicitly) {
        if (isDismissed()) return;

        if (!mUserInteracted) {
            recordInfobarAction(INFOBAR_DECLINE);
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
        mTabLayout.endScrollingAnimationIfPlaying();
        closeInfobar(true);
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        switch (tab.getPosition()) {
            case SOURCE_TAB_INDEX:
                incrementAndRecordTranslationsPerPageCount();
                recordInfobarAction(INFOBAR_REVERT);
                onButtonClicked(ActionType.TRANSLATE_SHOW_ORIGINAL);
                return;
            case TARGET_TAB_INDEX:
                recordInfobarAction(INFOBAR_TARGET_TAB_TRANSLATE);
                recordInfobarLanguageData(
                        INFOBAR_HISTOGRAM_TRANSLATE_LANGUAGE, mOptions.targetLanguageCode());
                startTranslating(TARGET_TAB_INDEX);
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
                recordInfobarAction(INFOBAR_MORE_LANGUAGES);
                initMenuHelper(TranslateMenu.MENU_TARGET_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_TARGET_LANGUAGE, getParentWidth());
                return;
            case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                // Only show snackbar when "Always Translate" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)) {
                    recordInfobarAction(INFOBAR_ALWAYS_TRANSLATE);
                    recordInfobarLanguageData(INFOBAR_HISTOGRAM_ALWAYS_TRANSLATE_LANGUAGE,
                            mOptions.sourceLanguageCode());
                    createAndShowSnackbar(
                            getContext().getString(R.string.translate_snackbar_always_translate,
                                    mOptions.sourceLanguageName(), mOptions.targetLanguageName()),
                            Snackbar.UMA_TRANSLATE_ALWAYS, ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                } else {
                    recordInfobarAction(INFOBAR_ALWAYS_TRANSLATE_UNDO);
                    handleTranslateOptionPostSnackbar(ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                recordInfobarAction(INFOBAR_NEVER_TRANSLATE);
                recordInfobarLanguageData(
                        INFOBAR_HISTOGRAM_NEVER_TRANSLATE_LANGUAGE, mOptions.sourceLanguageCode());
                createAndShowSnackbar(
                        getContext().getString(R.string.translate_snackbar_language_never,
                                mOptions.sourceLanguageName()),
                        Snackbar.UMA_TRANSLATE_NEVER, ACTION_OVERFLOW_NEVER_LANGUAGE);
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                recordInfobarAction(INFOBAR_NEVER_TRANSLATE_SITE);
                createAndShowSnackbar(
                        getContext().getString(R.string.translate_snackbar_site_never),
                        Snackbar.UMA_TRANSLATE_NEVER_SITE, ACTION_OVERFLOW_NEVER_SITE);
                return;
            case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                recordInfobarAction(INFOBAR_PAGE_NOT_IN);
                initMenuHelper(TranslateMenu.MENU_SOURCE_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_SOURCE_LANGUAGE, getParentWidth());
                return;
            default:
                assert false : "Unexpected overflow menu code";
        }
    }

    @Override
    public void onTargetMenuItemClicked(String code) {
        // Reset target code in both UI and native.
        if (mNativeTranslateInfoBarPtr != 0 && mOptions.setTargetLanguage(code)) {
            recordInfobarAction(INFOBAR_MORE_LANGUAGES_TRANSLATE);
            recordInfobarLanguageData(
                    INFOBAR_HISTOGRAM_MORE_LANGUAGES_LANGUAGE, mOptions.targetLanguageCode());
            TranslateCompactInfoBarJni.get().applyStringTranslateOption(mNativeTranslateInfoBarPtr,
                    TranslateCompactInfoBar.this, TranslateOption.TARGET_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(TARGET_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
        }
    }

    @Override
    public void onSourceMenuItemClicked(String code) {
        // If source language is same as target language, the infobar will dismiss and no
        // translation will be done.
        if (mOptions.targetLanguageCode().equals(code)) {
            closeInfobar(true);
            return;
        }
        // Reset source code in both UI and native.
        if (mNativeTranslateInfoBarPtr != 0 && mOptions.setSourceLanguage(code)) {
            recordInfobarLanguageData(
                    INFOBAR_HISTOGRAM_PAGE_NOT_IN_LANGUAGE, mOptions.sourceLanguageCode());
            TranslateCompactInfoBarJni.get().applyStringTranslateOption(mNativeTranslateInfoBarPtr,
                    TranslateCompactInfoBar.this, TranslateOption.SOURCE_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(SOURCE_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
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
            handleTranslateOptionPostSnackbar(actionId);
            return;
        }
        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                recordInfobarAction(INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION);
                break;
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                recordInfobarAction(INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION);
                break;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
                recordInfobarAction(INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION);
                break;
            case ACTION_AUTO_NEVER_LANGUAGE:
                recordInfobarAction(INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION);
                break;
            case ACTION_OVERFLOW_NEVER_SITE:
                recordInfobarAction(INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION);
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

    private void handleTranslateOptionPostSnackbar(int actionId) {
        // Quit if native is destroyed.
        if (mNativeTranslateInfoBarPtr == 0) return;

        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                toggleAlwaysTranslate();
                // Start translating if always translate is selected and if page is not already
                // translated to the target language.
                if (mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)
                        && mTabLayout.getSelectedTabPosition() == SOURCE_TAB_INDEX) {
                    startTranslating(mTabLayout.getSelectedTabPosition());
                }
                return;
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                toggleAlwaysTranslate();
                return;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
            case ACTION_AUTO_NEVER_LANGUAGE:
                mUserInteracted = true;
                // After applying this option, the infobar will dismiss.
                TranslateCompactInfoBarJni.get().applyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this,
                        TranslateOption.NEVER_TRANSLATE, true);
                return;
            case ACTION_OVERFLOW_NEVER_SITE:
                mUserInteracted = true;
                // After applying this option, the infobar will dismiss.
                TranslateCompactInfoBarJni.get().applyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this,
                        TranslateOption.NEVER_TRANSLATE_SITE, true);
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
                INFOBAR_HISTOGRAM, action, INFOBAR_HISTOGRAM_BOUNDARY);
    }

    private void recordInfobarLanguageData(String histogram, String langCode) {
        Integer hashCode = mOptions.getUMAHashCodeFromCode(langCode);
        if (hashCode != null) {
            RecordHistogram.recordSparseHistogram(histogram, hashCode);
        }
    }

    private void incrementAndRecordTranslationsPerPageCount() {
        RecordHistogram.recordCountHistogram(
                INFOBAR_HISTOGRAM_TRANSLATION_COUNT, ++mTotalTranslationCount);
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
    }
}
