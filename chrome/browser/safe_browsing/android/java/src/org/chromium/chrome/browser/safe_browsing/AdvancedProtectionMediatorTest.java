// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.fragment.app.Fragment;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;

/** Tests for {@link AdvancedProtectionMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(PermissionsAndroidFeatureList.OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH)
@Config(manifest = Config.NONE)
public class AdvancedProtectionMediatorTest {
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Context mContext;
    private final WeakReference<Context> mWeakContext = new WeakReference<Context>(mContext);
    private final UnownedUserDataHost mWindowUserDataHost = new UnownedUserDataHost();

    @Mock private ManagedMessageDispatcher mMessageDispatcher;

    private static class TestFragment extends Fragment {}

    private static class TestPermissionProvider extends OsAdditionalSecurityPermissionProvider {
        private boolean mIsAdvancedProtectionRequestedByOs;
        private Observer mObserver;

        public TestPermissionProvider(boolean isAdvancedProtectionRequestedByOs) {
            mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionRequestedByOs;
        }

        @Override
        public void addObserver(Observer observer) {
            assert mObserver == null;
            mObserver = observer;
        }

        @Override
        public boolean isAdvancedProtectionRequestedByOs() {
            return mIsAdvancedProtectionRequestedByOs;
        }

        @Override
        public @Nullable PropertyModel buildAdvancedProtectionMessagePropertyModel(
                Context context, Runnable primaryButtonAction) {
            return new PropertyModel();
        }

        public void setAdvancedProtectionRequestedByOs(boolean isAdvancedProtectionRequestedByOs) {
            mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionRequestedByOs;
            if (mObserver != null) {
                mObserver.onAdvancedProtectionOsSettingChanged();
            }
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mWindowUserDataHost);
        when(mWindowAndroid.getContext()).thenReturn(mWeakContext);
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mMessageDispatcher);

        ContextUtils.getAppSharedPreferences().edit().clear();
        OsAdditionalSecurityPermissionUtil.resetForTesting();
    }

    private TestPermissionProvider setPermissionProvider(
            boolean isAdvancedProtectionRequestedByOs) {
        var provider = new TestPermissionProvider(isAdvancedProtectionRequestedByOs);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class, provider);
                });
        return provider;
    }

    private void verifyEnqueuedMessage() {
        verify(mMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    private void verifyDidNotEnqueueMessage() {
        verify(mMessageDispatcher, times(0)).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    /**
     * Test that {@link AdvancedProtectionCoordinator#showMessageOnStartupIfNeeded()} does not show
     * a message if the pref is not stored and advanced-protection-mode is off.
     */
    @Test
    public void testDontShowMessageNoPrefAdvancedProtectionOff() {
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertFalse(coordinator.showMessageOnStartupIfNeeded());
        verifyDidNotEnqueueMessage();

        coordinator.destroy();
    }

    /**
     * Test that {@link AdvancedProtectionCoordinator#showMessageOnStartupIfNeeded()} shows a
     * message if the pref is not stored and advanced-protection-mode is on.
     */
    @Test
    public void testShowMessageNoPrefAdvancedProtectionOn() {
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertTrue(coordinator.showMessageOnStartupIfNeeded());
        verifyEnqueuedMessage();

        coordinator.destroy();
    }

    /**
     * Test that {@link AdvancedProtectionCoordinator#showMessageOnStartupIfNeeded()} does not show
     * a message if a pref is stored and its value matches the current advanced-protection-mode
     * state.
     */
    @Test
    public void testDontShowMessagePrefMatches() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, true);
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertFalse(coordinator.showMessageOnStartupIfNeeded());
        verifyDidNotEnqueueMessage();

        coordinator.destroy();
    }

    /**
     * Test that {@link AdvancedProtectionCoordinator#showMessageOnStartupIfNeeded()} does not show
     * a message if a pref is stored and its value is true and advanced-protection-mode is off.
     */
    @Test
    public void testShowMessagePrefTrueAndDiffers() {
        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, true);
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        coordinator.showMessageOnStartupIfNeeded();
        verifyDidNotEnqueueMessage();

        assertFalse(
                sharedPreferences.readBoolean(
                        ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING,
                        /* defaultValue= */ false));

        coordinator.destroy();
    }

    /**
     * Test that {@link AdvancedProtectionCoordinator#showMessageOnStartupIfNeeded()} shows a
     * message if a pref is stored and its value is false and advanced-protection-mode is on.
     */
    @Test
    public void testShowMessagePrefFalseAndDiffers() {
        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, false);
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertTrue(coordinator.showMessageOnStartupIfNeeded());
        verifyEnqueuedMessage();

        assertTrue(
                sharedPreferences.readBoolean(
                        ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING,
                        /* defaultValue= */ false));

        coordinator.destroy();
    }

    /**
     * Test that message is shown when advanced-protection-state is changed while Chrome is running.
     */
    @Test
    public void testShowMessageOnStateChange() {
        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, true);
        var provider = setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertFalse(coordinator.showMessageOnStartupIfNeeded());
        verifyDidNotEnqueueMessage();
        provider.setAdvancedProtectionRequestedByOs(/* isAdvancedProtectionRequestedByOs= */ true);
        verifyEnqueuedMessage();

        coordinator.destroy();
    }

    /** Test that a message is not shown when the feature-kill-switch is set. */
    @Test
    @EnableFeatures({PermissionsAndroidFeatureList.OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH})
    public void testDontShowMessageKillSwitch() {
        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, true);
        var provider = setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false);

        var coordinator = new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertFalse(coordinator.showMessageOnStartupIfNeeded());
        verifyDidNotEnqueueMessage();
        provider.setAdvancedProtectionRequestedByOs(/* isAdvancedProtectionRequestedByOs= */ true);
        verifyDidNotEnqueueMessage();

        coordinator.destroy();
    }

    /**
     * Test that {@link ChromePreferenceKeys#OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME} is updated
     * when the advanced-protection setting is changed.
     */
    @Test
    public void testUpdateTimestampOnStateChange() {
        long yesterdayTimestamp = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);

        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, true);
        sharedPreferences.writeLong(
                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME,
                yesterdayTimestamp);
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false);

        new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertTrue(
                yesterdayTimestamp
                        < sharedPreferences.readLong(
                                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME,
                                0));
    }

    /**
     * Test that {@link ChromePreferenceKeys#OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME} is not
     * updated if the advanced-protection setting has not changed since the last Chrome run.
     */
    @Test
    public void testDoNotUpdateTimestamp_SameState() {
        long yesterdayTimestamp = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);

        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.writeBoolean(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, true);
        sharedPreferences.writeLong(
                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME,
                yesterdayTimestamp);
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true);

        new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertEquals(
                yesterdayTimestamp,
                sharedPreferences.readLong(
                        ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME, 0));
    }

    /**
     * Test that {@link ChromePreferenceKeys#OS_ADVANCED_PROTECTION_SETTING} and {@link
     * ChromePreferenceKeys#OS_ADVANCED_PROTECTION_SETTING_UPDATE_TIME} are set by the {@link
     * AdvancedProtectionCoordinator} constructor if they are not set.
     */
    @Test
    public void testUpdateTimestamp_FirstRun() {
        var sharedPreferences = ChromeSharedPreferences.getInstance();
        sharedPreferences.removeKey(ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING);
        sharedPreferences.removeKey(
                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME);
        setPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true);

        new AdvancedProtectionCoordinator(mWindowAndroid, TestFragment.class);
        assertTrue(
                sharedPreferences.readBoolean(
                        ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING, false));
        assertTrue(
                sharedPreferences.readLong(
                                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME, 0)
                        != 0);
    }
}
