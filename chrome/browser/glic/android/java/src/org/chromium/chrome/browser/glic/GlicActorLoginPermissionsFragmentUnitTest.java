// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.preference.PreferenceCategory;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link GlicActorLoginPermissionsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicActorLoginPermissionsFragmentUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<BlankUiTestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(BlankUiTestActivity.class);

    private BlankUiTestActivity mActivity;
    private ModalDialogManager mModalDialogManager;

    @Mock private Profile mProfileMock;
    @Mock private GlicActorLoginBridge.Natives mGlicActorLoginBridgeJniMock;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeNatives;
    @Captor private ArgumentCaptor<Callback<Boolean>> mCallbackCaptor;

    private List<ActorLoginPermission> mTestPermissions;

    @Before
    public void setUp() {
        GlicActorLoginBridgeJni.setInstanceForTesting(mGlicActorLoginBridgeJniMock);
        when(mGlicActorLoginBridgeJniMock.init(any(), any())).thenReturn(1L);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNatives);
        when(mLargeIconBridgeNatives.init()).thenReturn(1L);

        mTestPermissions = getTestPermissions();

        // Mock getAllPermissions to return test permissions
        doAnswer(
                        invocation -> {
                            Callback<List<ActorLoginPermission>> callback =
                                    invocation.getArgument(1);
                            callback.onResult(mTestPermissions);
                            return null;
                        })
                .when(mGlicActorLoginBridgeJniMock)
                .getAllPermissions(anyLong(), any());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mModalDialogManager = mActivity.getModalDialogManager();
                        });
    }

    private GlicActorLoginPermissionsFragment launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        GlicActorLoginPermissionsFragment fragment =
                (GlicActorLoginPermissionsFragment)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        GlicActorLoginPermissionsFragment.class.getClassLoader(),
                                        GlicActorLoginPermissionsFragment.class.getName());
        fragment.setProfile(mProfileMock);
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit();
        mActivityScenarioRule.getScenario().moveToState(State.STARTED);
        return fragment;
    }

    @Test
    public void testRevokePermission_Confirm() {
        GlicActorLoginPermissionsFragment fragment = launchFragment();

        PreferenceCategory category = fragment.findPreference("actor_login_permissions_category");
        assertNotNull("Category should exist", category);

        // Index 0 is description, index 1 is the first permission preference
        assertEquals(
                "Should have 3 preferences (description + 2 permissions)",
                3,
                category.getPreferenceCount());
        ActorLoginPermissionPreference pref =
                (ActorLoginPermissionPreference) category.getPreference(1);
        assertNotNull("Preference should exist", pref);

        fragment.onRevokeClicked(pref, mTestPermissions.get(0));

        // Verify dialog was shown
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull("Dialog model should not be null", dialogModel);

        // Simulate clicking positive button ("Remove")
        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(dialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        // Verify revokePermission was called and capture callback
        verify(mGlicActorLoginBridgeJniMock)
                .revokePermission(
                        anyLong(),
                        eq(mTestPermissions.get(0).getSignonRealm()),
                        eq(mTestPermissions.get(0).getUsername()),
                        mCallbackCaptor.capture());

        // Simulate successful callback
        mCallbackCaptor.getValue().onResult(true);

        // Verify preference was removed
        assertEquals("Should have 2 preferences after removal", 2, category.getPreferenceCount());
    }

    private List<ActorLoginPermission> getTestPermissions() {
        List<ActorLoginPermission> permissions = new ArrayList<>();
        permissions.add(
                new ActorLoginPermission(
                        "yahoo.com",
                        new GURL("https://yahoo.com"),
                        "https://yahoo.com",
                        "user1",
                        new GURL("https://yahoo.com/favicon.ico")));
        permissions.add(
                new ActorLoginPermission(
                        "instacart.com",
                        new GURL("https://instacart.com"),
                        "https://instacart.com",
                        "user2",
                        new GURL("https://instacart.com/favicon.ico")));
        return permissions;
    }
}
