// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.CallbackController;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPoint;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Responsible for providing UI resources for showing a reader mode button on toolbar. */
@NullMarked
public class ReaderModeToolbarButtonController extends BaseButtonDataProvider
        implements ReaderModeActionRateLimiter.Observer {
    // The amount of time the Reader Mode contextual page action button is shown before it is
    // automatically hidden.
    private static final long HIDE_CPA_DELAY_MS = TimeUnit.SECONDS.toMillis(5);

    private final Supplier<@Nullable ReaderModeIphController> mReaderModeIphControllerSupplier;
    private final TabSupplierObserver mActivityTabObserver;
    private final ButtonSpec mEntryPointSpec;
    private final ButtonSpec mExitPointSpec;


    private CallbackController mCallbackController = new CallbackController();
    // Only populated when the TabSupplierObserver events fire.
    private @Nullable GURL mTabLastUrlSeen;
    private boolean mShouldShowButtonForCurrentPage;
    // Null until native is initialized.
    private @Nullable ReaderModeActionRateLimiter mReaderModeActionRateLimiter;

    /**
     * Creates a new instance of {@code ReaderModeToolbarButtonController}.
     *
     * @param context The context for retrieving string resources.
     * @param activityTabProvider Supplier for the current active tab.
     * @param modalDialogManager Modal dialog manager, used to disable the button when a dialog is
     *     visible. Can be null to disable this behavior.
     * @param readerModeIphControllerSupplier Supplies the reader mode IPH controller, null for
     *     CCTs.
     */
    public ReaderModeToolbarButtonController(
            Context context,
            ActivityTabProvider activityTabProvider,
            ModalDialogManager modalDialogManager,
            Supplier<@Nullable ReaderModeIphController> readerModeIphControllerSupplier) {
        super(
                activityTabProvider,
                modalDialogManager,
                new ButtonSpec.Builder(
                                AppCompatResources.getDrawable(
                                        context, R.drawable.ic_mobile_friendly_24dp),
                                context.getString(R.string.reader_mode_cpa_button_text),
                                /* supportsTinting= */ true)
                        .setActionChipLabelResId(R.string.reader_mode_cpa_button_text)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.READER_MODE)
                        .setHoverTooltipTextId(R.string.show_reading_mode_text)
                        .build());

        mReaderModeIphControllerSupplier = readerModeIphControllerSupplier;
        mActivityTabObserver =
                new TabSupplierObserver(activityTabProvider.asObservable()) {
                    @Override
                    public void onUrlUpdated(@Nullable Tab tab) {
                        GURL currentUrl = tab == null ? null : tab.getUrl();
                        if (Objects.equals(currentUrl, mTabLastUrlSeen)) return;
                        boolean isExitingReaderMode =
                                DomDistillerUrlUtils.isExitingReaderMode(
                                        mTabLastUrlSeen, currentUrl);
                        mTabLastUrlSeen = currentUrl;

                        maybeRefreshButton(tab, isExitingReaderMode);
                    }

                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        mTabLastUrlSeen = tab == null ? null : tab.getUrl();
                        maybeRefreshButton(tab, /* isExitingReaderMode= */ false);
                    }
                };

        mEntryPointSpec = mButtonData.getButtonSpec();
        mExitPointSpec =
                new ButtonSpec.Builder(
                                AppCompatResources.getDrawable(
                                        context, R.drawable.ic_mobile_friendly_24dp),
                                context.getString(R.string.hide_reading_mode_text),
                                /* supportsTinting= */ true)
                        .setOnClickListener(this)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.READER_MODE)
                        .setHoverTooltipTextId(R.string.hide_reading_mode_text)
                        .setIsChecked(true)
                        .build();
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mReaderModeActionRateLimiter != null) {
            mReaderModeActionRateLimiter.removeObserver(this);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mReaderModeActionRateLimiter = ReaderModeActionRateLimiter.getInstance();
        mReaderModeActionRateLimiter.addObserver(this);
    }

    @Override
    public void onClick(View view) {
        ReaderModeActionRateLimiter.getInstance().onActionClicked();
        Tab currentTab = mActiveTabSupplier.get();
        if (currentTab == null) return;

        ReaderModeManager readerModeManager =
                currentTab.getUserDataHost().getUserData(ReaderModeManager.class);
        if (readerModeManager == null) return;

        if (DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl())) {
            readerModeManager.hideReaderMode();
            return;
        }

        ReaderModeMetrics.recordReaderModeContextualPageActionEvent(
                ReaderModeMetrics.ReaderModeContextualPageActionEvent.CLICKED);
        readerModeManager.activateReaderMode(EntryPoint.TOOLBAR_BUTTON);
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        return mShouldShowButtonForCurrentPage;
    }

    // ReaderModeActionRateLimiter.Observer implementation.

    @Override
    public void onActionShown() {
        Runnable task =
                mCallbackController.makeCancelable(
                        () -> {
                            ReaderModeMetrics.recordReaderModeContextualPageActionEvent(
                                    ReaderModeMetrics.ReaderModeContextualPageActionEvent.TIME_OUT);
                            ReaderModeIphController readerModeIphController =
                                    mReaderModeIphControllerSupplier.get();
                            if (readerModeIphController != null) {
                                readerModeIphController.showIph();
                            }
                            setCanShowButton(false);
                        });
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, task, HIDE_CPA_DELAY_MS);
    }

    // Private methods

    private void setCanShowButton(boolean canShow) {
        mShouldShowButtonForCurrentPage = canShow;
        notifyObservers(mShouldShowButtonForCurrentPage);
    }

    private void maybeRefreshButton(@Nullable Tab tab, boolean isExitingReaderMode) {
        // The callback controller may still have a pending task to hide the button. Destroy it and
        // create a new one to ensure that the button can be shown again.
        mCallbackController.destroy();
        mCallbackController = new CallbackController();

        if (tab != null && DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
            mButtonData.setButtonSpec(mExitPointSpec);
            setCanShowButton(true);
        } else {
            mButtonData.setButtonSpec(mEntryPointSpec);
            setCanShowButton(!isExitingReaderMode);
        }
    }

    // Testing-specific functions

    TabSupplierObserver getTabSupplierObserverForTesting() {
        return mActivityTabObserver;
    }

    ButtonData getButtonDataForTesting() {
        return mButtonData;
    }
}
