// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.PostNativeFlag;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.List;

/**
 * A class of helper methods that assist in the restore tabs workflow.
 */
public class RestoreTabsFeatureHelper {
    public static final PostNativeFlag RESTORE_TABS_PROMO =
            new PostNativeFlag(ChromeFeatureList.RESTORE_TABS_ON_FRE);
    public static final BooleanCachedFieldTrialParameter
            RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.RESTORE_TABS_ON_FRE, "skip_feature_engagement", false);
    private RestoreTabsController mController;
    private RestoreTabsControllerDelegate mDelegate;
    private RestoreTabsControllerDelegate mDelegateForTesting;
    private Activity mActivity;
    private Profile mProfile;
    private TabCreatorManager mTabCreatorManager;
    private BottomSheetController mBottomSheetController;

    public RestoreTabsFeatureHelper(Activity activity, Profile profile,
            TabCreatorManager tabCreatorManager, BottomSheetController bottomSheetController) {
        mActivity = activity;
        mProfile = profile;
        mTabCreatorManager = tabCreatorManager;
        mBottomSheetController = bottomSheetController;
    }

    public void destroy() {
        if (mController != null) {
            mController.destroy();
            mController = null;
        }

        if (mDelegate != null) {
            mDelegate = null;
        }
    }

    /**
     * Check the criteria for displaying the restore tabs promo.
     */
    public void maybeShowPromo() {
        if (RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.getValue()
                || TrackerFactory.getTrackerForProfile(mProfile).wouldTriggerHelpUI(
                        FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE)) {
            setDelegate();
            ForeignSessionHelper foreignSessionHelper = new ForeignSessionHelper(mProfile);
            List<ForeignSession> sessions =
                    foreignSessionHelper.getMobileAndTabletForeignSessions();

            if (hasValidSyncedDevices(sessions)
                    && (RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.getValue()
                            || TrackerFactory.getTrackerForProfile(mProfile).shouldTriggerHelpUI(
                                    FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE))) {
                mDelegate.showPromo(foreignSessionHelper, sessions);
            } else {
                foreignSessionHelper.destroy();
                foreignSessionHelper = null;
                mDelegate.onDismissed(/*wasPromoShown=*/false);
            }
        }
    }

    private void setDelegate() {
        mDelegate = (mDelegateForTesting != null)
                ? mDelegateForTesting
                : new RestoreTabsControllerDelegate() {
                      private boolean mWasDismissed;

                      @Override
                      public void showPromo(ForeignSessionHelper foreignSessionHelper,
                              List<ForeignSession> sessions) {
                          mController = RestoreTabsControllerFactory.createInstance(
                                  mActivity, mProfile, mTabCreatorManager, mBottomSheetController);
                          mController.showHomeScreen(foreignSessionHelper, sessions, mDelegate);
                      }

                      @Override
                      public void onDismissed(boolean wasPromoShown) {
                          assert !mWasDismissed : "Promo should only be dismissed once.";
                          mWasDismissed = true;

                          if (!RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.getValue()
                                  && wasPromoShown) {
                              TrackerFactory.getTrackerForProfile(mProfile).dismissed(
                                      FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE);
                          }

                          destroy();
                      }

                      @Override
                      public BooleanCachedFieldTrialParameter getSkipFeatureEngagementParam() {
                          return RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT;
                      }
                  };
    }

    private boolean hasValidSyncedDevices(List<ForeignSession> sessions) {
        for (ForeignSession session : sessions) {
            for (ForeignSessionWindow window : session.windows) {
                if (window.tabs.size() != 0) {
                    return true;
                }
            }
        }
        return false;
    }

    @VisibleForTesting
    void setRestoreTabsControllerDelegateForTesting(RestoreTabsControllerDelegate delegate) {
        mDelegateForTesting = delegate;
    }
}
