// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.app.Activity;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Entry point to show the choice screen associated with {@link SearchEnginePromoType#SHOW_WAFFLE}
 * as a modal dialog.
 */
public class ChoiceDialogCoordinator {
    @VisibleForTesting
    static final @DialogDismissalCause int SUCCESS_DISMISSAL_CAUSE =
            DialogDismissalCause.ACTION_ON_CONTENT;

    private final ChoiceScreenCoordinator mContentCoordinator;
    private final ModalDialogManager mModalDialogManager;
    private final @Nullable Callback<Boolean> mOnClosedCallback;
    private final ChoiceScreenDelegate mDelegate;

    private @Nullable PropertyModel mDialogModel;

    public static ChoiceDialogCoordinator maybeShow(Activity activity) {
        return maybeShowInternal(
                () ->
                        new ChoiceDialogCoordinator(
                                activity, new PlaceholderDelegate(), unused -> {}));
    }

    @VisibleForTesting
    static ChoiceDialogCoordinator maybeShowInternal(
            Supplier<ChoiceDialogCoordinator> coordinatorSupplier) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.CLAY_BLOCKING);

        var searchEngineChoiceService = SearchEngineChoiceService.getInstance();
        if (searchEngineChoiceService == null
                || !searchEngineChoiceService.isDeviceChoiceDialogEligible()) {
            return null;
        }

        var coordinator = coordinatorSupplier.get();
        withUiThreadTimeout(searchEngineChoiceService.shouldShowDeviceChoiceDialog(), 1000)
                .then(
                        shouldShow -> {
                            if (shouldShow) coordinator.show();
                        },
                        unused -> {
                            /* timeout*/
                        });

        return coordinator;
    }

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
    public ChoiceDialogCoordinator(
            Activity activity,
            DefaultSearchEngineDialogHelper.Delegate delegate,
            @Nullable Callback<Boolean> onClosedCallback) {
        mModalDialogManager = ((ModalDialogManagerHolder) activity).getModalDialogManager();
        mOnClosedCallback = onClosedCallback;
        mDelegate =
                new ChoiceScreenDelegate(delegate, onClosedCallback) {
                    @Override
                    void onChoiceMade(String keyword) {
                        super.onChoiceMade(keyword);
                        ChoiceDialogCoordinator.this.dismissDialog();
                    }
                };
        mContentCoordinator = buildContentCoordinator(activity, mDelegate);
    }

    /** Constructs and shows the dialog. */
    public void show() {
        assert mDialogModel == null;
        mDialogModel = createDialogPropertyModel(mContentCoordinator.getContentView());
        mModalDialogManager.showDialog(
                mDialogModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    @VisibleForTesting
    ChoiceScreenCoordinator buildContentCoordinator(
            Activity activity, ChoiceScreenDelegate delegate) {
        return new ChoiceScreenCoordinator(activity, delegate);
    }

    private void dismissDialog() {
        assert mDialogModel != null;

        // For non-reentry verification.
        PropertyModel model = mDialogModel;
        mDialogModel = null;

        // Will no-op if the dialog was already dismissed.
        mModalDialogManager.dismissDialog(model, SUCCESS_DISMISSAL_CAUSE);
    }

    private PropertyModel createDialogPropertyModel(View contentView) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CUSTOM_VIEW, contentView)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                .with(
                        ModalDialogProperties.DIALOG_STYLES,
                        ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG)
                .with(
                        ModalDialogProperties.CONTROLLER,
                        new ModalDialogProperties.Controller() {
                            @Override
                            public void onClick(PropertyModel model, @ButtonType int buttonType) {}

                            @Override
                            public void onDismiss(
                                    PropertyModel model, @DialogDismissalCause int dismissalCause) {
                                if (dismissalCause != SUCCESS_DISMISSAL_CAUSE) {
                                    mDelegate.onExitWithoutChoice();
                                }
                            }
                        })
                .with(
                        ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
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

    private static <T> Promise<T> withUiThreadTimeout(Promise<T> promise, long delayMillis) {
        if (!promise.isPending()) return promise;

        Promise<T> timeoutPromise = new Promise<>();
        promise.then(timeoutPromise::fulfill, timeoutPromise::reject);
        ThreadUtils.postOnUiThreadDelayed(
                () -> {
                    if (timeoutPromise.isPending()) {
                        timeoutPromise.reject(new TimeoutException());
                    }
                },
                delayMillis);
        return timeoutPromise;
    }

    // TODO(b/355054464): Remove when we update the coordinator.
    private static class PlaceholderDelegate implements DefaultSearchEngineDialogHelper.Delegate {
        @Override
        public List<TemplateUrl> getSearchEnginesForPromoDialog(int type) {
            return List.of(
                    new TemplateUrl(0) {
                        @Override
                        public String getShortName() {
                            return "Placeholder Engine";
                        }

                        @Override
                        public int getPrepopulatedId() {
                            return 999;
                        }

                        @Override
                        public boolean getIsPrepopulated() {
                            return true;
                        }

                        @Override
                        public String getKeyword() {
                            return "placeholder";
                        }

                        @Override
                        public long getLastVisitedTime() {
                            return 0;
                        }
                    });
        }

        @Override
        public void onUserSearchEngineChoice(int type, List<String> keywords, String keyword) {}
    }
}
