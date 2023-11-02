// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import static org.mockito.Mockito.mock;

import android.app.Activity;

import androidx.appcompat.widget.Toolbar;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link LaunchpadCoordinator}.
 *
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LaunchpadCoordinatorTest {
    private static final String APP_PACKAGE_NAME_1 = "package.name.1";
    private static final String APP_NAME_1 = "App Name 1";
    private static final String APP_SHORT_NAME_1 = "App 1";
    private static final String APP_URL_1 = "https://example.com/1";
    private static final String APP_PACKAGE_NAME_2 = "package.name.2";
    private static final String APP_NAME_2 = "App Name 2";
    private static final String APP_SHORT_NAME_2 = "App 2 with long short name";
    private static final String APP_URL_2 = "https://example.com/2";

    private static final List<LaunchpadItem> MOCK_APP_LIST =
            new ArrayList<>(Arrays.asList(new LaunchpadItem(APP_PACKAGE_NAME_1, APP_SHORT_NAME_1,
                                                  APP_NAME_1, APP_URL_1, null, null),
                    new LaunchpadItem(APP_PACKAGE_NAME_2, APP_SHORT_NAME_2, APP_NAME_2, APP_URL_2,
                            null, null)));

    private Activity mActivity;

    @Mock
    private ModalDialogManager mModalDialogManager;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_MaterialComponents);
    }

    private LaunchpadCoordinator createCoordinator(boolean isSeparateActivity) {
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        modalDialogManagerSupplier.set(mModalDialogManager);

        return new LaunchpadCoordinator(mActivity, modalDialogManagerSupplier,
                mock(SettingsLauncher.class), MOCK_APP_LIST, isSeparateActivity);
    }

    @Test
    public void testLaunchpadToolbar() {
        LaunchpadCoordinator coordinator = createCoordinator(true /* isSeparateActivity */);
        Toolbar toolbar = (Toolbar) coordinator.getView().findViewById(R.id.toolbar);
        Assert.assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.launchpad_title),
                toolbar.getTitle());
        Assert.assertNotNull(toolbar.getMenu().findItem(R.id.close_menu_id));

        coordinator = createCoordinator(false /* isSeparateActivity */);
        toolbar = (Toolbar) coordinator.getView().findViewById(R.id.toolbar);
        Assert.assertNull(toolbar.getMenu().findItem(R.id.close_menu_id));
    }
}
