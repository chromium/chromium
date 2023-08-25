// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.app.Activity;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Entry point to show the choice screen associated with {@link SearchEnginePromoType#SHOW_WAFFLE}
 * as a modal dialog.
 */
public class ChoiceDialogCoordinator {
    private final ModalDialogManager mModalDialogManager;
    private final DefaultSearchEngineDialogHelper.Delegate mDelegate;
    private final @Nullable Callback<Boolean> mOnClosedCallback;

    /**
     * Creates the coordinator that will show a search engine choice dialog.
     * Shows a promotion dialog about search engines depending on Locale and other conditions.
     * See {@link org.chromium.chrome.browser.locale.LocaleManager#getSearchEnginePromoShowType} for
     * possible types and logic.
     *
     * @param activity Activity that will show through its {@link ModalDialogManager}.
     * @param delegate Object providing access to the data to display and callbacks to run to act on
     *         the user choice.
     * @param onClosedCallback If provided, should be notified when the search engine choice has
     *         been finalized and the dialog closed.  It should be called with {@code true} if a
     *         search engine was selected, or {@code false} if the dialog was dismissed without a
     *         selection.
     */
    public ChoiceDialogCoordinator(Activity activity,
            DefaultSearchEngineDialogHelper.Delegate delegate,
            @Nullable Callback<Boolean> onClosedCallback) {
        mModalDialogManager = ((ModalDialogManagerHolder) activity).getModalDialogManager();
        mDelegate = delegate;
        mOnClosedCallback = onClosedCallback;
    }

    /** Constructs and shows the dialog. */
    public void show() {
        mModalDialogManager.showDialog(createDialogPropertyModel(),
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    private PropertyModel createDialogPropertyModel() {
        return new PropertyModel
                .Builder(ModalDialogProperties.ALL_KEYS)
                // TODO(b/280753530): Replace the placeholder UI.
                .with(ModalDialogProperties.TITLE, "Waffle Dialog")
                .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                        mDelegate.getSearchEnginesForPromoDialog(SearchEnginePromoType.SHOW_WAFFLE)
                                        .size()
                                + " item(s) to display")
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                .with(ModalDialogProperties.DIALOG_STYLES,
                        ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG)
                .with(ModalDialogProperties.CONTROLLER,
                        new ModalDialogProperties.Controller() {
                            @Override
                            public void onClick(PropertyModel model, @ButtonType int buttonType) {}

                            @Override
                            public void onDismiss(
                                    PropertyModel model, @DialogDismissalCause int dismissalCause) {
                                // TODO(b/280753530): Implement proper triggering and handling.
                                if (mOnClosedCallback != null) {
                                    mOnClosedCallback.onResult(false);
                                }
                            }
                        })
                .with(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                        // Capture back navigations and suppress them. The user must complete the
                        // screen by interacting with the options presented.
                        // TODO(b/280753530): Instead of fully suppressing it, maybe perform back
                        // from Chrome's highest level to go back to the caller?
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {}
                        })
                .build();
    }
}
