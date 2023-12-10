// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Handles displaying share button on toolbar depending on several conditions (e.g.,device width,
 * whether NTP is shown).
 */
public class ShareButtonController extends BaseButtonDataProvider {
    private final ShareUtils mShareUtils;

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    private final Supplier<Tracker> mTrackerSupplier;
    private final Runnable mOnShareRunnable;

    /**
     * Creates ShareButtonController object.
     * @param context The context for retrieving string resources.
     * @param buttonDrawable Drawable for the new tab button.
     * @param tabProvider The {@link ActivityTabProvider} used for accessing the tab.
     * @param shareDelegateSupplier The supplier to get a handle on the share delegate.
     * @param trackerSupplier  Supplier for the current profile tracker.
     * @param shareUtils The share utility functions used by this class.
     * @param modalDialogManager dispatcher for modal lifecycles events
     * @param onShareRunnable A {@link Runnable} to execute when a share event occurs. This object
     *                        does not actually handle sharing, but can provide supplemental
     *                        functionality when the share button is pressed.
     */
    public ShareButtonController(
            Context context,
            Drawable buttonDrawable,
            ActivityTabProvider tabProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            Supplier<Tracker> trackerSupplier,
            ShareUtils shareUtils,
            ModalDialogManager modalDialogManager,
            Runnable onShareRunnable) {
        super(
                tabProvider,
                modalDialogManager,
                buttonDrawable,
                context.getString(R.string.share),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.SHARE,
                /* tooltipTextResId= */ R.string.adaptive_toolbar_button_preference_share,
                /* showHoverHighlight= */ true);

        mShareUtils = shareUtils;
        mShareDelegateSupplier = shareDelegateSupplier;
        mTrackerSupplier = trackerSupplier;
        mOnShareRunnable = onShareRunnable;
    }

    @Override
    public void onClick(View view) {
        ShareDelegate shareDelegate = mShareDelegateSupplier.get();
        assert shareDelegate != null
                : "Share delegate became null after share button was displayed";
        if (shareDelegate == null) return;
        Tab tab = mActiveTabSupplier.get();
        assert tab != null : "Tab became null after share button was displayed";
        if (tab == null) return;
        if (mOnShareRunnable != null) mOnShareRunnable.run();
        RecordUserAction.record("MobileTopToolbarShareButton");
        if (tab.getWebContents() != null) {
            new UkmRecorder.Bridge()
                    .recordEventWithBooleanMetric(
                            tab.getWebContents(), "TopToolbar.Share", "HasOccurred");
        }
        shareDelegate.share(tab, /* shareDirectly= */ false, ShareOrigin.TOP_TOOLBAR);

        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SHARE_OPENED);
        }
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        if (!super.shouldShowButton(tab) || mShareDelegateSupplier.get() == null) return false;

        return mShareUtils.shouldEnableShare(tab);
    }

    /**
     * Returns an IPH for this button. Only called once native is initialized and when {@code
     * AdaptiveToolbarFeatures.isCustomizationEnabled()} is true.
     * @param tab Current tab.
     */
    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                                tab.getContext().getResources(),
                                FeatureConstants
                                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_SHARE_FEATURE,
                                /* stringId= */ R.string.adaptive_toolbar_button_share_iph,
                                /* accessibilityStringId= */ R.string
                                        .adaptive_toolbar_button_share_iph)
                        .setHighlightParams(params);
        return iphCommandBuilder;
    }
}
