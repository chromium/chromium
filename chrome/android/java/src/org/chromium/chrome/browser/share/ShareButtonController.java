// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.content.res.Configuration;
import android.view.View.OnClickListener;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Handles displaying share button on toolbar depending on several conditions (e.g.,device width,
 * whether NTP is shown).
 */
public class ShareButtonController implements ButtonDataProvider, ConfigurationChangedObserver {
    /**
     * Default minimum width to show the share button.
     */
    public static final int MIN_WIDTH_DP = 360;

    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;

    private final ShareUtils mShareUtils;

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // The activity tab provider.
    private ActivityTabProvider mTabProvider;

    private ButtonDataImpl mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private OnClickListener mOnClickListener;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    private Integer mMinimumWidthDp;
    private int mScreenWidthDp;

    private int mCurrentOrientation;

    /**
     * Creates ShareButtonController object.
     * @param context The Context for retrieving resources, etc.
     * @param tabProvider The {@link ActivityTabProvider} used for accessing the tab.
     * @param shareDelegateSupplier The supplier to get a handle on the share delegate.
     * @param shareUtils The share utility functions used by this class.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g.
     * configuration changes.
     * @param modalDialogManager dispatcher for modal lifecycles events
     * @param onShareRunnable A {@link Runnable} to execute when a share event occurs. This object
     *                        does not actually handle sharing, but can provide supplemental
     *                        functionality when the share button is pressed.
     */
    public ShareButtonController(Context context, ActivityTabProvider tabProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier, ShareUtils shareUtils,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ModalDialogManager modalDialogManager, Runnable onShareRunnable) {
        mContext = context;

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mShareUtils = shareUtils;

        mShareDelegateSupplier = shareDelegateSupplier;
        mOnClickListener = ((view) -> {
            ShareDelegate shareDelegate = mShareDelegateSupplier.get();
            assert shareDelegate
                    != null : "Share delegate became null after share button was displayed";
            if (shareDelegate == null) return;
            Tab tab = mTabProvider.get();
            assert tab != null : "Tab became null after share button was displayed";
            if (tab == null) return;
            if (onShareRunnable != null) onShareRunnable.run();
            RecordUserAction.record("MobileTopToolbarShareButton");
            shareDelegate.share(tab, /*shareDirectly=*/false, ShareOrigin.TOP_TOOLBAR);
        });

        mModalDialogManagerObserver = new ModalDialogManagerObserver() {
            @Override
            public void onDialogAdded(PropertyModel model) {
                mButtonData.setEnabled(false);
                notifyObservers(mButtonData.canShow());
            }

            @Override
            public void onLastDialogDismissed() {
                mButtonData.setEnabled(true);
                notifyObservers(mButtonData.canShow());
            }
        };
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.addObserver(mModalDialogManagerObserver);

        mButtonData = new ButtonDataImpl(/*canShow=*/false,
                AppCompatResources.getDrawable(mContext, R.drawable.ic_toolbar_share_offset_24dp),
                mOnClickListener, R.string.share, /*supportsTinting=*/true,
                /*iphCommandBuilder=*/null, /*isEnabled=*/true, AdaptiveToolbarButtonVariant.SHARE);

        mScreenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        if (mScreenWidthDp == configuration.screenWidthDp) {
            return;
        }
        mScreenWidthDp = configuration.screenWidthDp;
        updateButtonVisibility(mTabProvider.get());
        notifyObservers(mButtonData.canShow());
    }

    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
        if (mModalDialogManagerObserver != null && mModalDialogManager != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
            mModalDialogManagerObserver = null;
            mModalDialogManager = null;
        }
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        updateButtonVisibility(tab);
        return mButtonData;
    }

    private void updateButtonVisibility(Tab tab) {
        if (tab == null || tab.getWebContents() == null || mTabProvider == null
                || mTabProvider.get() == null || !isFeatureEnabled()) {
            mButtonData.setCanShow(false);
            return;
        }

        if (mMinimumWidthDp == null) {
            mMinimumWidthDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR, "minimum_width", MIN_WIDTH_DP);
        }

        boolean isDeviceWideEnough = mScreenWidthDp > mMinimumWidthDp;

        if (mShareDelegateSupplier.get() == null || !isDeviceWideEnough) {
            mButtonData.setCanShow(false);
            return;
        }

        mButtonData.setCanShow(mShareUtils.shouldEnableShare(tab));
    }

    private static boolean isFeatureEnabled() {
        if (AdaptiveToolbarFeatures.isEnabled()) {
            return AdaptiveToolbarFeatures.getSingleVariantMode()
                    == AdaptiveToolbarButtonVariant.SHARE;
        } else {
            return ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR);
        }
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }
}
