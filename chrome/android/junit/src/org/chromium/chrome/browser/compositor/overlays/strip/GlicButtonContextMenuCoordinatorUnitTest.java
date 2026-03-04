// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.widget.ListView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/** Tests for {@link GlicButtonContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.GLIC)
public class GlicButtonContextMenuCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    @Mock private RectProvider mRectProvider;

    private GlicButtonContextMenuCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mRectProvider.getRect())
                .thenReturn(new Rect(10, 10, mActivity.getWindow().getDecorView().getWidth(), 50));
        mCoordinator = new GlicButtonContextMenuCoordinator(mActivity);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_BUTTON_PINNED);
    }

    @Test
    public void testShowAndDismiss() {
        mCoordinator.showMenu(mRectProvider, mActivity);
        assertTrue("Menu should be showing", mCoordinator.isShowing());

        mCoordinator.dismiss();
        assertFalse("Menu should be dismissed", mCoordinator.isShowing());
    }

    @Test
    public void testClickUnpin() {
        // Set initial state for SharedPreferences
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_BUTTON_PINNED, true);
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_BUTTON_PINNED, false));

        // Show menu
        mCoordinator.showMenu(mRectProvider, mActivity);

        assertNotNull(mCoordinator.getPopupWindow());
        View contentView = mCoordinator.getPopupWindow().getContentView();
        ListView listView = contentView.findViewById(R.id.menu_list);
        ModelListAdapter adapter = (ModelListAdapter) listView.getAdapter();

        assertEquals(1, adapter.getCount());
        PropertyModel model = ((ListItem) adapter.getItem(0)).model;
        assertEquals(R.string.menu_unpin_glic_button, model.get(ListMenuItemProperties.TITLE_ID));

        // Click "Unpin".
        mCoordinator.getListMenuDelegate().onItemSelected(model, listView);

        // Verify the menu dismissed and the unpin state updated
        assertFalse("Menu should be dismissed after clicking an item.", mCoordinator.isShowing());
        assertFalse(
                "Glic button pinned state should be false after clicking unpin.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_BUTTON_PINNED, false));
    }
}
