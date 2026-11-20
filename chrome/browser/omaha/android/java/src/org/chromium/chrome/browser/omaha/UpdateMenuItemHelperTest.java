// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import static org.junit.Assert.assertEquals;
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
import org.mockito.Mockito;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.chrome.browser.omaha.inline.InAppUpdateController;
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

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private FakeAppUpdateManager mFakeAppUpdateManager;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivity = mActivityController.get();
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(true);
        InAppUpdatePolicy.setIsOfficialBuildForTesting(true);

        VariationsAssociatedDataJni.setInstanceForTesting(mMockVariations);
        ResettersForTesting.register(() -> VariationsAssociatedDataJni.setInstanceForTesting(null));
        lenient().when(mMockVariations.getVariationParamValue(any(), any())).thenReturn("");

        UserPrefs.setPrefServiceForTesting(mPrefService);
    }

    @After
    public void tearDown() {
        if (mActivityController != null && !mActivity.isDestroyed()) {
            mActivityController.destroy();
        }
        InAppUpdateController.setAppUpdateManagerForTesting(null);
    }

    private InAppUpdateController createController() {
        mFakeAppUpdateManager = new FakeAppUpdateManager(mActivity);
        mFakeAppUpdateManager.setUpdateAvailable(2);
        InAppUpdateController.setAppUpdateManagerForTesting(mFakeAppUpdateManager);
        return new InAppUpdateController(mActivity, () -> null);
    }

    private UpdateMenuItemHelper setAvailableUpdateStatus(String updateUrl) {
        UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
        UpdateStatus status = new UpdateStatus();
        status.updateState = UpdateState.UPDATE_AVAILABLE;
        status.latestVersion = "130.0.0.0";
        status.updateUrl = updateUrl;
        helper.setUpdateStatusForTesting(status);
        return helper;
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_nullStatus_doesNothing() {
        InAppUpdateController controller = createController();
        UpdateMenuItemHelper helper = UpdateMenuItemHelper.getInstance(mProfile);
        helper.setUpdateStatusForTesting(null);

        helper.onMenuItemClicked(mActivity, controller);

        verify(mPrefService, never())
                .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_eligible_delegatesToController() {
        InAppUpdateController controller = createController();
        UpdateMenuItemHelper helper =
                setAvailableUpdateStatus(
                        "https://play.google.com/store/apps/details?id=com.android.chrome");

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                        InAppUpdateController.AppMenuAction.TRIGGER_DOWNLOAD);

        helper.onMenuItemClicked(mActivity, controller);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertEquals(
                Integer.valueOf(AppUpdateType.FLEXIBLE),
                mFakeAppUpdateManager.getTypeForUpdateInProgress());
        verify(mPrefService)
                .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_nonChromeActivity_runsFallback() {
        UpdateMenuItemHelper helper =
                setAvailableUpdateStatus(
                        "https://play.google.com/store/apps/details?id=com.android.chrome");

        helper.onMenuItemClicked(mActivity);
        ShadowLooper.idleMainLooper();

        verify(mPrefService)
                .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_emptyUpdateUrl_doesNotSetPrefOrUpdateState() {
        UpdateMenuItemHelper helper = setAvailableUpdateStatus("");

        helper.onMenuItemClicked(mActivity);
        ShadowLooper.idleMainLooper();

        verify(mPrefService, never())
                .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_nullController_fallsBackToIntent() {
        UpdateMenuItemHelper helper =
                setAvailableUpdateStatus(
                        "https://play.google.com/store/apps/details?id=com.android.chrome");

        helper.onMenuItemClicked(mActivity, null);
        ShadowLooper.idleMainLooper();

        verify(mPrefService)
                .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_featureDisabled_fallsBackToIntent() {
        InAppUpdateController controller = Mockito.mock(InAppUpdateController.class);
        UpdateMenuItemHelper helper =
                setAvailableUpdateStatus(
                        "https://play.google.com/store/apps/details?id=com.android.chrome");

        helper.onMenuItemClicked(mActivity, controller);
        ShadowLooper.idleMainLooper();

        Mockito.verifyNoInteractions(controller);
        verify(mPrefService)
                .setString(eq(Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM), eq("130.0.0.0"));
    }
}
