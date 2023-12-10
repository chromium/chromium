// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.components.permissions.PermissionDialogDelegate.getRequestTypeEnumSize;

import android.Manifest;
import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.PageInfoIPHController;
import org.chromium.chrome.browser.omnibox.status.StatusMediator;
import org.chromium.chrome.browser.omnibox.status.StatusProperties;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Testing the interactions with permissions on a site and how it affects page info. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(PER_CLASS)
public class PageInfoDiscoverabilityTest {
    @ClassRule
    public static final PermissionTestRule sPermissionTestRule = new PermissionTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sPermissionTestRule, false);

    private static final String GEOLOCATION_TEST =
            "/chrome/test/data/geolocation/geolocation_on_load.html";

    /**
     * Parameter provider for testing the different |RequestType|s that affect discoverability. The
     * RequestType enum values are defined in components/permissions/request_type.h.
     */
    public static class RequestTypeTestParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> list = new ArrayList<>();
            list.addAll(getPermissionRequestParameters());
            list.addAll(getChooserParameters());
            return list;
        }

        public List<ParameterSet> getPermissionRequestParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            // ParameterSet.value = {ContentSettingsType, isInSiteSettings}
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kAccessibilityEvents")
                            .value(ContentSettingsType.ACCESSIBILITY_EVENTS, false));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kArSession")
                            .value(ContentSettingsType.AR, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kCameraStream")
                            .value(ContentSettingsType.MEDIASTREAM_CAMERA, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kClipboard")
                            .value(ContentSettingsType.CLIPBOARD_READ_WRITE, true));
            // No associated ContentSettingsType
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kDiskQuota")
                            .value(ContentSettingsType.DEFAULT, false));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kGeolocation")
                            .value(ContentSettingsType.GEOLOCATION, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kIdleDetection")
                            .value(ContentSettingsType.IDLE_DETECTION, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kMicStream")
                            .value(ContentSettingsType.MEDIASTREAM_MIC, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kMidi")
                            .value(ContentSettingsType.MIDI, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kMidiSysex")
                            .value(ContentSettingsType.MIDI_SYSEX, true));
            // No associated ContentSettingsType
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kMultipleDownloads")
                            .value(ContentSettingsType.DEFAULT, false));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kNfcDevice")
                            .value(ContentSettingsType.NFC, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kNotifications")
                            .value(ContentSettingsType.NOTIFICATIONS, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kProtectedMediaIdentifier")
                            .value(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kStorageAccess")
                            .value(ContentSettingsType.STORAGE_ACCESS, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kVrSession")
                            .value(ContentSettingsType.VR, true));
            parameters.add(
                    new ParameterSet()
                            .name("RequestType.kTopLevelStorageAccess")
                            .value(ContentSettingsType.TOP_LEVEL_STORAGE_ACCESS, false));

            return parameters;
        }

        public List<ParameterSet> getChooserParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            // ParameterSet.value = {ContentSettingsType, isInSiteSettings}
            parameters.add(
                    new ParameterSet()
                            .name("Chooser.USB")
                            .value(ContentSettingsType.USB_CHOOSER_DATA, true));
            parameters.add(
                    new ParameterSet()
                            .name("Chooser.Bluetooth")
                            .value(ContentSettingsType.BLUETOOTH_CHOOSER_DATA, true));
            parameters.add(
                    new ParameterSet()
                            .name("Chooser.HID")
                            .value(ContentSettingsType.HID_CHOOSER_DATA, false));
            parameters.add(
                    new ParameterSet()
                            .name("Chooser.Serial")
                            .value(ContentSettingsType.SERIAL_CHOOSER_DATA, false));

            return parameters;
        }
    }

    @Mock LocationBarDataProvider mLocationBarDataProvider;
    @Mock UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    @Mock Profile mProfile;
    @Mock TemplateUrlService mTemplateUrlService;
    @Mock PageInfoIPHController mPageInfoIPHController;

    Context mContext;
    Resources mResources;
    PropertyModel mModel;
    PermissionDialogController mPermissionDialogController;
    StatusMediator mMediator;
    OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mContext = sPermissionTestRule.getActivity();
        mResources = mContext.getResources();
        mPermissionDialogController = PermissionDialogController.getInstance();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = new PropertyModel(StatusProperties.ALL_KEYS);
                    mTemplateUrlServiceSupplier = new OneshotSupplierImpl<>();
                    mMediator =
                            new StatusMediator(
                                    mModel,
                                    mResources,
                                    mContext,
                                    mUrlBarEditingTextStateProvider,
                                    /* isTablet= */ false,
                                    mLocationBarDataProvider,
                                    mPermissionDialogController,
                                    mTemplateUrlServiceSupplier,
                                    () -> mProfile,
                                    mPageInfoIPHController,
                                    sPermissionTestRule.getActivity().getWindowAndroid(),
                                    null);
                    mTemplateUrlServiceSupplier.set(mTemplateUrlService);
                });
    }

    @After
    public void tearDown() throws Exception {
        LocationUtils.setFactory(null);
        LocationProviderOverrider.setLocationProviderImpl(null);

        // Reset content settings.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getInstance()
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }

    /** Tests omnibox permission when permission is allowed by the user. */
    @Test
    @MediumTest
    @EnableFeatures(PermissionsAndroidFeatureList.BLOCK_MIDI_BY_DEFAULT)
    @Feature({"PageInfoDiscoverability"})
    public void testPageInfoDiscoverabilityAllowPrompt() throws Exception {
        Assert.assertEquals(ContentSettingsType.DEFAULT, mMediator.getLastPermission());
        // Prompt for location and accept it.
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testAndroidPermissionDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(
                        requestablePermission,
                        RuntimePermissionTestUtils.RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                sPermissionTestRule,
                testAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                /* missingPermissionPromptTextId= */ 0);

        Assert.assertEquals(ContentSettingsType.GEOLOCATION, mMediator.getLastPermission());
    }

    /** Tests omnibox permission when permission is blocked by the user. */
    @Test
    @MediumTest
    @EnableFeatures(PermissionsAndroidFeatureList.BLOCK_MIDI_BY_DEFAULT)
    @Feature({"PageInfoDiscoverability"})
    public void testPageInfoDiscoverabilityBlockPrompt() throws Exception {
        Assert.assertEquals(ContentSettingsType.DEFAULT, mMediator.getLastPermission());

        // Prompt for location and deny it.
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testAndroidPermissionDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(
                        requestablePermission,
                        RuntimePermissionTestUtils.RuntimePromptResponse.DENY);
        RuntimePermissionTestUtils.runTest(
                sPermissionTestRule,
                testAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ false,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                /* missingPermissionPromptTextId= */ 0);

        Assert.assertEquals(ContentSettingsType.GEOLOCATION, mMediator.getLastPermission());
    }

    @Test
    @MediumTest
    @EnableFeatures(PermissionsAndroidFeatureList.BLOCK_MIDI_BY_DEFAULT)
    @Feature({"PageInfoDiscoverability"})
    public void testPermissionRequestTypeEnumSize() {
        Assert.assertEquals(
                new RequestTypeTestParams().getPermissionRequestParameters().size(),
                getRequestTypeEnumSize());
    }

    @Test
    @MediumTest
    @EnableFeatures(PermissionsAndroidFeatureList.BLOCK_MIDI_BY_DEFAULT)
    @Feature({"PageInfoDiscoverability"})
    @ParameterAnnotations.UseMethodParameter(RequestTypeTestParams.class)
    public void testPermissionRequestTypes(
            @ContentSettingsType int contentSettingsType, boolean isInSiteSettings) {
        if (contentSettingsType == ContentSettingsType.BLUETOOTH_CHOOSER_DATA) {
            isInSiteSettings =
                    ContentFeatureMap.isEnabled(
                            ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND);
        }
        Assert.assertEquals(ContentSettingsType.DEFAULT, mMediator.getLastPermission());
        @ContentSettingsType int[] permissions = {contentSettingsType};
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMediator.onDialogResult(
                            sPermissionTestRule.getActivity().getWindowAndroid(),
                            permissions,
                            ContentSettingValues.ALLOW);
                });
        Assert.assertEquals(
                isInSiteSettings ? contentSettingsType : ContentSettingsType.DEFAULT,
                mMediator.getLastPermission());
    }
}
