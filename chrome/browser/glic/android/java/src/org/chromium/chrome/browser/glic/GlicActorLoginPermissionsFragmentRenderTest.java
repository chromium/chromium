// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Render tests for {@link GlicActorLoginPermissionsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class GlicActorLoginPermissionsFragmentRenderTest {
    public final @Rule BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    public final @Rule ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .setRevision(1)
                    .build();

    public final @Rule MockitoRule mMocks = MockitoJUnit.rule();

    private @Mock Profile mProfile;
    private @Mock GlicActorLoginBridge.Natives mGlicActorLoginBridgeJniMock;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeNatives;
    private @Mock PrefService mPrefService;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!NetworkChangeNotifier.isInitialized()) {
                        NetworkChangeNotifier.init();
                    }
                    NetworkChangeNotifier.forceConnectivityState(true);
                });
        GlicActorLoginBridgeJni.setInstanceForTesting(mGlicActorLoginBridgeJniMock);
        when(mGlicActorLoginBridgeJniMock.init(any(), any())).thenReturn(1L);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNatives);
        when(mLargeIconBridgeNatives.init()).thenReturn(1L);
        UserPrefs.setPrefServiceForTesting(mPrefService);
        doAnswer(
                        invocation -> {
                            LargeIconBridge.LargeIconCallback callback = invocation.getArgument(5);
                            callback.onLargeIconAvailable(null, 0, false, 0);
                            return true;
                        })
                .when(mLargeIconBridgeNatives)
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), anyInt(), any());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFragment() throws Exception {
        mSettingsTestRule.launchPreference(
                GlicActorLoginPermissionsFragment.class,
                /* fragmentArgs= */ null,
                fragment -> {
                    mSettingsTestRule.getActivity().setTheme(R.style.Theme_Chromium_Settings);
                    ((GlicActorLoginPermissionsFragment) fragment).setProfile(mProfile);
                });

        GlicActorLoginPermissionsFragment fragment =
                (GlicActorLoginPermissionsFragment) mSettingsTestRule.getPreferenceFragment();

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.populatePermissions(getTestPermissions()));
        applyContainment(fragment);

        // Wait for RecyclerView to perform a layout pass and attach child views.
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = fragment.getListView();
                    Criteria.checkThat(recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(recyclerView.getChildCount(), Matchers.greaterThan(1));
                });

        mRenderTestRule.render(fragment.getView(), "glic_actor_login_permissions_fragment");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFragment_Offline() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(false);
                });

        mSettingsTestRule.launchPreference(
                GlicActorLoginPermissionsFragment.class,
                /* fragmentArgs= */ null,
                fragment -> {
                    mSettingsTestRule.getActivity().setTheme(R.style.Theme_Chromium_Settings);
                    ((GlicActorLoginPermissionsFragment) fragment).setProfile(mProfile);
                });

        GlicActorLoginPermissionsFragment fragment =
                (GlicActorLoginPermissionsFragment) mSettingsTestRule.getPreferenceFragment();

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.populatePermissions(getTestPermissions()));
        applyContainment(fragment);

        // Wait for RecyclerView to perform a layout pass and attach child views.
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = fragment.getListView();
                    Criteria.checkThat(recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(recyclerView.getChildCount(), Matchers.greaterThan(1));
                });

        mRenderTestRule.render(fragment.getView(), "glic_actor_login_permissions_fragment_offline");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFragment_Empty() throws Exception {
        mSettingsTestRule.launchPreference(
                GlicActorLoginPermissionsFragment.class,
                /* fragmentArgs= */ null,
                fragment -> {
                    mSettingsTestRule.getActivity().setTheme(R.style.Theme_Chromium_Settings);
                    ((GlicActorLoginPermissionsFragment) fragment).setProfile(mProfile);
                });

        GlicActorLoginPermissionsFragment fragment =
                (GlicActorLoginPermissionsFragment) mSettingsTestRule.getPreferenceFragment();

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.populatePermissions(new ArrayList<>()));
        applyContainment(fragment);

        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = fragment.getListView();
                    Criteria.checkThat(recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(recyclerView.getChildCount(), Matchers.greaterThan(1));
                });

        mRenderTestRule.render(fragment.getView(), "glic_actor_login_permissions_fragment_empty");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderFragment_Managed() throws Exception {
        when(mPrefService.isManagedPreference(GlicPrefNames.GLIC_ACTUATION_ON_WEB))
                .thenReturn(true);

        mSettingsTestRule.launchPreference(
                GlicActorLoginPermissionsFragment.class,
                /* fragmentArgs= */ null,
                fragment -> {
                    mSettingsTestRule.getActivity().setTheme(R.style.Theme_Chromium_Settings);
                    ((GlicActorLoginPermissionsFragment) fragment).setProfile(mProfile);
                });

        GlicActorLoginPermissionsFragment fragment =
                (GlicActorLoginPermissionsFragment) mSettingsTestRule.getPreferenceFragment();

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.populatePermissions(getTestPermissions()));
        applyContainment(fragment);

        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = fragment.getListView();
                    Criteria.checkThat(recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(recyclerView.getChildCount(), Matchers.greaterThan(1));
                });

        mRenderTestRule.render(fragment.getView(), "glic_actor_login_permissions_fragment_managed");
    }

    private void applyContainment(GlicActorLoginPermissionsFragment fragment) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fragment.requireContext()
                            .getTheme()
                            .applyStyle(R.style.ThemeOverlay_Chromium_Settings_Containment, true);

                    RecyclerView recyclerView = fragment.getListView();
                    ContainmentItemController controller =
                            new ContainmentItemController(mSettingsTestRule.getActivity());
                    ContainmentItemDecoration itemDecoration =
                            new ContainmentItemDecoration(controller);
                    recyclerView.addItemDecoration(itemDecoration);
                    itemDecoration.updatePreferenceStyles(
                            controller.generatePreferenceStyles(
                                    SettingsUtils.getVisiblePreferences(
                                            fragment.getPreferenceScreen())));

                    // Force re-inflation of views to pick up the new theme
                    var adapter = recyclerView.getAdapter();
                    recyclerView.setAdapter(null);
                    recyclerView.setAdapter(adapter);

                    recyclerView.invalidateItemDecorations();
                });
    }

    private List<ActorLoginPermission> getTestPermissions() {
        List<ActorLoginPermission> permissions = new ArrayList<>();
        permissions.add(
                new ActorLoginPermission(
                        "yahoo.com",
                        new GURL("https://yahoo.com"),
                        "https://yahoo.com",
                        "testusername",
                        new GURL("https://yahoo.com/favicon.ico")));
        permissions.add(
                new ActorLoginPermission(
                        "instacart.com",
                        new GURL("https://instacart.com"),
                        "https://instacart.com",
                        "testusername",
                        new GURL("https://instacart.com/favicon.ico")));
        return permissions;
    }
}
