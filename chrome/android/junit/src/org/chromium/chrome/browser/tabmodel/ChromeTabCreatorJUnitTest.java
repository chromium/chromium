// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Unit tests for {@link ChromeTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabCreatorJUnitTest {
    private ChromeTabCreator mTabCreator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabCreator =
                new ChromeTabCreator(
                        activity,
                        mock(WindowAndroid.class),
                        () -> null,
                        new OneshotSupplierImpl<ProfileProvider>(),
                        /* incognito= */ false,
                        AsyncTabParamsManagerSingleton.getInstance(),
                        () -> null,
                        () -> null);
    }

    @Test
    public void testCreateNewTab_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTab(
                        new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_CHROME_UI,
                        /* parent= */ null));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PRECONNECT_ON_TAB_CREATION)
    public void testCreateNewTab_ReparentingNullParams_AbortsCleanly() {
        mTabCreator.setTabModel(mock(TabModel.class));
        mTabCreator.setTabModelOrderController(mock(TabModelOrderController.class));
        int assignedTabId = 123;
        Intent intent = new Intent();
        IntentHandler.setTabId(intent, assignedTabId);

        assertNull(
                mTabCreator.createNewTab(
                        new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_REPARENTING,
                        /* parent= */ null,
                        intent));
        assertNull(
                mTabCreator.createNewTab(
                        new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_REPARENTING_BACKGROUND,
                        /* parent= */ null,
                        intent));
    }

    @Test
    public void testCreateNewTabs_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTabs(
                        new LoadUrlParams("https://primary.com"),
                        List.of("https://url1.com", "https://url2.com"),
                        TabLaunchType.FROM_CHROME_UI,
                        null,
                        false,
                        null));
    }

    @Test
    public void testCreateNewTabs_WithTabGroup_NullTabModel() {
        // mTabModel is null by default.
        assertNull(
                mTabCreator.createNewTabs(
                        new LoadUrlParams("https://primary.com"),
                        List.of("https://url1.com", "https://url2.com"),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        null,
                        true,
                        null));
    }
}
