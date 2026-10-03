// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.provider.Settings;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link GlicHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManageable mSnackbarManageableMock;
    @Mock private SnackbarManager mSnackbarManagerMock;
    @Mock private Profile mProfileMock;
    @Mock private Context mContextMock;
    @Mock private ActorKeyedService mActorServiceMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private WindowAndroid mWindowAndroidMock;

    private UserActionTester mUserActionTester;

    /** Activity that implements {@link SnackbarManageable}. */
    private static class SnackbarManageableActivity extends Activity implements SnackbarManageable {
        private SnackbarManager mSnackbarManager;

        @Override
        public SnackbarManager getSnackbarManager() {
            return mSnackbarManager;
        }
    }

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
        when(mSnackbarManageableMock.getSnackbarManager()).thenReturn(mSnackbarManagerMock);
        ActorKeyedServiceFactory.setForTesting(mActorServiceMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
    }

    @After
    public void tearDown() {
        if (mUserActionTester != null) {
            mUserActionTester.tearDown();
        }
    }

    @Test
    public void testMaybeShowSnackbar_WithActiveTasks() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.SETTINGS_ACTIVITY);

        verify(mSnackbarManagerMock).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_NoActiveTasks() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(0);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.SETTINGS_ACTIVITY);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_OffTheRecord() {
        when(mProfileMock.isOffTheRecord()).thenReturn(true);
        // Even if there are active tasks, we shouldn't show snackbar for OTR profiles.
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.SETTINGS_ACTIVITY);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_OncePerInstance() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.SETTINGS_ACTIVITY);
        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.SETTINGS_ACTIVITY);

        verify(mSnackbarManagerMock, times(1)).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_MultipleInstances() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        SnackbarManageable secondInstance = mock(SnackbarManageable.class);
        when(secondInstance.getSnackbarManager()).thenReturn(mSnackbarManagerMock);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.SETTINGS_ACTIVITY);
        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                secondInstance, mProfileMock, mContextMock, GlicHelper.Caller.SETTINGS_ACTIVITY);

        verify(mSnackbarManagerMock, times(2)).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_NewTabPageException() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.NEW_TAB_PAGE);
        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock,
                mProfileMock,
                mContextMock,
                GlicHelper.Caller.NEW_TAB_PAGE);

        verify(mSnackbarManagerMock, times(2)).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testShowUnpinnedSnackbar() {
        // Arrange.
        when(mContextMock.getString(R.string.glic_button_unpinned)).thenReturn("Gemini unpinned");
        when(mContextMock.getString(R.string.undo)).thenReturn("Undo");

        // Act: Show unpinned snackbar.
        GlicHelper.showUnpinnedSnackbar(mSnackbarManagerMock, mContextMock, mProfileMock);

        // Verify.
        ArgumentCaptor<Snackbar> captor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManagerMock).showSnackbar(captor.capture());
        Snackbar snackbar = captor.getValue();
        assertEquals(Snackbar.UMA_GLIC_UNPIN_UNDO, snackbar.getIdentifierForTesting());
        assertEquals("Gemini unpinned", snackbar.getTextForTesting());
        assertEquals("Undo", snackbar.getActionText());

        // Act: Trigger "Undo".
        SnackbarController controller = snackbar.getController();
        assertNotNull(controller);
        controller.onAction(null);

        // Verify.
        verify(mPrefServiceMock).setBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, true);
        assertEquals(
                1, mUserActionTester.getActionCount("Glic.Interaction.TabStripButton.UndoUnpin"));
    }

    @Test
    public void testShowMicDisabledSnackbar_PermissionAlreadyGranted() {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(true);

        GlicHelper.showMicDisabledSnackbar(mWindowAndroidMock);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testShowMicDisabledSnackbar_NullActivity() {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(null));

        GlicHelper.showMicDisabledSnackbar(mWindowAndroidMock);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testShowMicDisabledSnackbar_NonSnackbarManageableActivity() {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        Activity nonManageableActivity = Robolectric.buildActivity(Activity.class).get();
        when(mWindowAndroidMock.getActivity())
                .thenReturn(new WeakReference<>(nonManageableActivity));

        GlicHelper.showMicDisabledSnackbar(mWindowAndroidMock);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testShowMicDisabledSnackbar_NullSnackbarManager() {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        SnackbarManageableActivity activity =
                Robolectric.buildActivity(SnackbarManageableActivity.class).get();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));

        GlicHelper.showMicDisabledSnackbar(mWindowAndroidMock);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testShowMicDisabledSnackbar_Success() {
        // Arrange.
        SnackbarManageableActivity activity =
                Robolectric.buildActivity(SnackbarManageableActivity.class).get();
        activity.mSnackbarManager = mSnackbarManagerMock;
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));

        String expectedText = activity.getString(R.string.glic_mic_disabled_snackbar);
        String expectedActionText = activity.getString(R.string.settings);
        ContextUtils.initApplicationContextForTests(mContextMock);
        when(mContextMock.getPackageName()).thenReturn("org.chromium.chrome");

        // Act: Show mic disabled snackbar.
        GlicHelper.showMicDisabledSnackbar(mWindowAndroidMock);

        // Verify.
        ArgumentCaptor<Snackbar> captor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManagerMock).showSnackbar(captor.capture());
        Snackbar snackbar = captor.getValue();
        assertEquals(Snackbar.UMA_GLIC_MIC_DISABLED, snackbar.getIdentifierForTesting());
        assertEquals(expectedText, snackbar.getTextForTesting());
        assertEquals(expectedActionText, snackbar.getActionText());

        // Act: Trigger "Settings".
        SnackbarController controller = snackbar.getController();
        assertNotNull(controller);
        controller.onAction(null);

        // Verify.
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Glic.Interaction.MicDisabledSnackbar.SettingsClicked"));
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContextMock).startActivity(intentCaptor.capture(), isNull());
        Intent intent = intentCaptor.getValue();
        assertEquals(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, intent.getAction());
        assertEquals(Uri.parse("package:org.chromium.chrome"), intent.getData());
        assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    @Test
    public void testRequestMicOsPermission_AlreadyGranted() {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(true);
        List<Boolean> results = new ArrayList<>();

        GlicHelper.requestMicOsPermission(mWindowAndroidMock, results::add);

        assertEquals(List.of(true), results);
        verify(mWindowAndroidMock, never()).requestPermissions(any(), any());
    }

    @Test
    public void testRequestMicOsPermission_NoActivity() {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(null));
        List<Boolean> results = new ArrayList<>();

        GlicHelper.requestMicOsPermission(mWindowAndroidMock, results::add);

        assertEquals(List.of(false), results);
        verify(mWindowAndroidMock, never()).requestPermissions(any(), any());
    }

    @Test
    public void testRequestMicOsPermission_Granted() {
        assertEquals(
                List.of(true),
                requestMicOsPermissionWithResult(new int[] {PackageManager.PERMISSION_GRANTED}));
    }

    @Test
    public void testRequestMicOsPermission_Denied() {
        assertEquals(
                List.of(false),
                requestMicOsPermissionWithResult(new int[] {PackageManager.PERMISSION_DENIED}));
    }

    @Test
    public void testRequestMicOsPermission_Cancelled() {
        assertEquals(List.of(false), requestMicOsPermissionWithResult(new int[0]));
    }

    @Test
    public void testRequestMicOsPermission_ActivityDestroyed() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class).setup();
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(controller.get()));
        List<Boolean> results = new ArrayList<>();

        GlicHelper.requestMicOsPermission(mWindowAndroidMock, results::add);
        ArgumentCaptor<PermissionCallback> callbackCaptor =
                ArgumentCaptor.forClass(PermissionCallback.class);
        verify(mWindowAndroidMock).requestPermissions(any(), callbackCaptor.capture());
        assertTrue(results.isEmpty());

        controller.pause().stop().destroy();
        assertEquals(List.of(false), results);

        // A late result is ignored.
        callbackCaptor
                .getValue()
                .onRequestPermissionsResult(
                        new String[] {Manifest.permission.RECORD_AUDIO},
                        new int[] {PackageManager.PERMISSION_GRANTED});
        assertEquals(List.of(false), results);
    }

    private List<Boolean> requestMicOsPermissionWithResult(int[] grantResults) {
        when(mWindowAndroidMock.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        when(mWindowAndroidMock.getActivity())
                .thenReturn(
                        new WeakReference<>(
                                Robolectric.buildActivity(Activity.class).setup().get()));
        List<Boolean> results = new ArrayList<>();

        GlicHelper.requestMicOsPermission(mWindowAndroidMock, results::add);

        ArgumentCaptor<PermissionCallback> callbackCaptor =
                ArgumentCaptor.forClass(PermissionCallback.class);
        verify(mWindowAndroidMock)
                .requestPermissions(
                        aryEq(new String[] {Manifest.permission.RECORD_AUDIO}),
                        callbackCaptor.capture());
        assertTrue(results.isEmpty());
        callbackCaptor
                .getValue()
                .onRequestPermissionsResult(
                        new String[] {Manifest.permission.RECORD_AUDIO}, grantResults);
        return results;
    }
}
