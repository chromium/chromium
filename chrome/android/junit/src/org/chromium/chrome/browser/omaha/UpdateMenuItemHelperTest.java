// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import com.google.android.play.core.appupdate.testing.FakeAppUpdateManager;
import com.google.android.play.core.install.model.AppUpdateType;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.chrome.browser.omaha.inline.InAppUpdateController;
import org.chromium.chrome.browser.omaha.inline.InAppUpdateController.InAppUpdateManageable;
import org.chromium.chrome.browser.omaha.inline.InAppUpdatePolicy;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.components.variations.VariationsAssociatedDataJni;
import org.chromium.gms.ChromiumPlayServicesAvailability;

/** Unit tests for {@link UpdateMenuItemHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UpdateMenuItemHelperTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VariationsAssociatedData.Natives mMockVariations;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;

    private FakeAppUpdateManager mFakeAppUpdateManager;

    private static class ManageableTestActivity extends Activity implements InAppUpdateManageable {
        private @Nullable InAppUpdateController mController;
        private boolean mWasGetInAppUpdateControllerCalled;

        void setInAppUpdateController(@Nullable InAppUpdateController controller) {
            mController = controller;
        }

        @Override
        public @Nullable InAppUpdateController getInAppUpdateController() {
            mWasGetInAppUpdateControllerCalled = true;
            return mController;
        }

        boolean wasGetInAppUpdateControllerCalled() {
            return mWasGetInAppUpdateControllerCalled;
        }
    }

    @Before
    public void setUp() {
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(true);
        InAppUpdatePolicy.setIsOfficialBuildForTesting(true);

        VariationsAssociatedDataJni.setInstanceForTesting(mMockVariations);
        ResettersForTesting.register(() -> VariationsAssociatedDataJni.setInstanceForTesting(null));
        lenient().when(mMockVariations.getVariationParamValue(any(), any())).thenReturn("");

        UserPrefs.setPrefServiceForTesting(mPrefService);
    }

    @After
    public void tearDown() {
        InAppUpdateController.setAppUpdateManagerForTesting(null);
    }

    private InAppUpdateController createController(Activity activity) {
        mFakeAppUpdateManager = new FakeAppUpdateManager(activity);
        mFakeAppUpdateManager.setUpdateAvailable(2);
        InAppUpdateController.setAppUpdateManagerForTesting(mFakeAppUpdateManager);
        return new InAppUpdateController(activity, () -> null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_nullStatus_doesNothing() {
        ActivityController<ManageableTestActivity> activityController =
                Robolectric.buildActivity(ManageableTestActivity.class).setup();
        try {
            ManageableTestActivity activity = activityController.get();
            InAppUpdateController controller = createController(activity);
            activity.setInAppUpdateController(controller);
            UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
            helper.setUpdateStatusForTesting(null);

            helper.onMenuItemClicked(activity);

            assertFalse(activity.wasGetInAppUpdateControllerCalled());
        } finally {
            activityController.destroy();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_eligible_delegatesToController() {
        ActivityController<ManageableTestActivity> activityController =
                Robolectric.buildActivity(ManageableTestActivity.class).setup();
        try {
            ManageableTestActivity activity = activityController.get();
            InAppUpdateController controller = createController(activity);
            activity.setInAppUpdateController(controller);

            UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
            UpdateStatus status = new UpdateStatus();
            status.updateState = UpdateState.UPDATE_AVAILABLE;
            status.latestVersion = "130.0.0.0";
            status.updateUrl = "https://play.google.com/store/apps/details?id=com.android.chrome";
            helper.setUpdateStatusForTesting(status);

            var watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                            InAppUpdateController.AppMenuAction.TRIGGER_DOWNLOAD);

            helper.onMenuItemClicked(activity);
            ShadowLooper.idleMainLooper();

            assertTrue(activity.wasGetInAppUpdateControllerCalled());
            watcher.assertExpected();
            assertEquals(
                    Integer.valueOf(AppUpdateType.FLEXIBLE),
                    mFakeAppUpdateManager.getTypeForUpdateInProgress());
            verify(mPrefService)
                    .setString(
                            eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
        } finally {
            activityController.destroy();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_nonChromeActivity_runsFallback() {
        ActivityController<Activity> nonChromeController =
                Robolectric.buildActivity(Activity.class).setup();
        try {
            Activity activity = nonChromeController.get();
            UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
            UpdateStatus status = new UpdateStatus();
            status.updateState = UpdateState.UPDATE_AVAILABLE;
            status.latestVersion = "130.0.0.0";
            status.updateUrl = "https://play.google.com/store/apps/details?id=com.android.chrome";
            helper.setUpdateStatusForTesting(status);

            helper.onMenuItemClicked(activity);
            ShadowLooper.idleMainLooper();

            verify(mPrefService)
                    .setString(
                            eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
        } finally {
            nonChromeController.destroy();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_emptyUpdateUrl_doesNotSetPrefOrUpdateState() {
        ActivityController<Activity> nonChromeController =
                Robolectric.buildActivity(Activity.class).setup();
        try {
            Activity activity = nonChromeController.get();
            UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
            UpdateStatus status = new UpdateStatus();
            status.updateState = UpdateState.UPDATE_AVAILABLE;
            status.latestVersion = "130.0.0.0";
            status.updateUrl = "";
            helper.setUpdateStatusForTesting(status);

            helper.onMenuItemClicked(activity);
            ShadowLooper.idleMainLooper();

            verify(mPrefService, never())
                    .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), any());
        } finally {
            nonChromeController.destroy();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_manageableWithNullController_fallsBackToIntent() {
        ActivityController<ManageableTestActivity> activityController =
                Robolectric.buildActivity(ManageableTestActivity.class).setup();
        try {
            ManageableTestActivity activity = activityController.get();
            activity.setInAppUpdateController(null);

            UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
            UpdateStatus status = new UpdateStatus();
            status.updateState = UpdateState.UPDATE_AVAILABLE;
            status.latestVersion = "130.0.0.0";
            status.updateUrl = "https://play.google.com/store/apps/details?id=com.android.chrome";
            helper.setUpdateStatusForTesting(status);

            helper.onMenuItemClicked(activity);
            ShadowLooper.idleMainLooper();

            assertTrue(activity.wasGetInAppUpdateControllerCalled());
            verify(mPrefService)
                    .setString(
                            eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
        } finally {
            activityController.destroy();
        }
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_featureDisabled_fallsBackToIntentWithoutQueryingController() {
        ActivityController<ManageableTestActivity> activityController =
                Robolectric.buildActivity(ManageableTestActivity.class).setup();
        try {
            ManageableTestActivity activity = activityController.get();
            InAppUpdateController controller = createController(activity);
            activity.setInAppUpdateController(controller);

            UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
            UpdateStatus status = new UpdateStatus();
            status.updateState = UpdateState.UPDATE_AVAILABLE;
            status.latestVersion = "130.0.0.0";
            status.updateUrl = "https://play.google.com/store/apps/details?id=com.android.chrome";
            helper.setUpdateStatusForTesting(status);

            helper.onMenuItemClicked(activity);
            ShadowLooper.idleMainLooper();

            assertFalse(activity.wasGetInAppUpdateControllerCalled());
            verify(mPrefService)
                    .setString(
                            eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
        } finally {
            activityController.destroy();
        }
    }
}
