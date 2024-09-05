// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.tab_ui.TabGridIphDialogCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Coordinator for the Tab group promo card. */
public class TabGroupPromoCoordinator implements EducationalTipCardProvider {
    private final Context mContext;

    private final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Runnable mShowTabSwitcherRunnable;
    private final Supplier<ViewGroup> mParentViewSupplier;
    private final Runnable mOnClickedRunnable;
    private CallbackController mCallbackController;
    @Nullable private TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;

    /**
     * @param context The Context of the application.
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param modalDialogManagerSupplier The supplier of {@link ModalDialogManager} instance.
     * @param showTabSwitcherRunnable The runnable to open the tab switcher.
     * @param parentViewSupplier The supplier of the parent view.
     */
    public TabGroupPromoCoordinator(
            @NonNull Context context,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Runnable showTabSwitcherRunnable,
            @NonNull Supplier<ViewGroup> parentViewSupplier) {
        mContext = context;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mShowTabSwitcherRunnable = showTabSwitcherRunnable;
        mParentViewSupplier = parentViewSupplier;

        mCallbackController = new CallbackController();
        mOnClickedRunnable =
                mCallbackController.makeCancelable(
                        () -> {
                            if (mTabGridIphDialogCoordinator == null) {
                                mTabGridIphDialogCoordinator =
                                        new TabGridIphDialogCoordinator(
                                                mContext, mModalDialogManagerSupplier.get());
                                mTabGridIphDialogCoordinator.setParentView(
                                        mParentViewSupplier.get());
                            }
                            mShowTabSwitcherRunnable.run();
                            mTabGridIphDialogCoordinator.showIph();
                            onModuleClickedCallback.run();
                        });
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mContext.getString(R.string.educational_tip_tab_group_title);
    }

    @Override
    public String getCardDescription() {
        return mContext.getString(R.string.educational_tip_tab_group_description);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.tab_group_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }

    @Override
    public void destroy() {
        if (mTabGridIphDialogCoordinator != null) {
            mTabGridIphDialogCoordinator.setParentView(null);
        }
        mCallbackController.destroy();
    }

    public void setTabGridIphDialogCoordinatorForTesting(
            TabGridIphDialogCoordinator tabGridIphDialogCoordinator) {
        mTabGridIphDialogCoordinator = tabGridIphDialogCoordinator;
    }
}
