// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.HOVER_LISTENER;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ICON;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE_CONDENSED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.IMMERIVE_MODE_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.IMMERIVE_MODE_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.ZOOM_PERCENT_TEXT;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.components.browser_ui.accessibility.PageZoomProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link PageZoomMenuItemView} and {@link PageZoomMenuItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomMenuItemViewUnitTest {
    private static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    AppMenuItemProperties.ALL_KEYS, PageZoomProperties.ALL_KEYS_FOR_MENU_ITEM);

    private PageZoomMenuItemView mView;
    private PropertyModel mModel;
    private boolean mZoomInClicked;
    private boolean mZoomOutClicked;
    private boolean mImmersiveClicked;
    private boolean mHovered;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mView =
                (PageZoomMenuItemView)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.page_zoom_menu_item,
                                        /* root= */ null,
                                        /* attachToRoot= */ false);

        mModel = new PropertyModel(ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, PageZoomMenuItemViewBinder::bind);
    }

    @Test
    public void testInflationAndChildViewCaching() {
        assertNotNull(mView.getTitleViewForTesting());
        assertNotNull(mView.getZoomOutButtonForTesting());
        assertNotNull(mView.getZoomPercentageTextForTesting());
        assertNotNull(mView.getZoomInButtonForTesting());
        assertNotNull(mView.getFullscreenContainerForTesting());
        assertNotNull(mView.getFullscreenButtonForTesting());
    }

    @Test
    public void testTitleAndTitleCondensed() {
        mModel.set(TITLE, "Zoom");
        mModel.set(TITLE_CONDENSED, "Zoom 100%");
        assertEquals("Zoom", mView.getTitleViewForTesting().getText().toString());
        assertEquals("Zoom 100%", mView.getTitleViewForTesting().getContentDescription());

        mModel.set(TITLE, null);
        assertEquals("Zoom 100%", mView.getTitleViewForTesting().getText().toString());
    }

    @Test
    public void testIcon() {
        Drawable drawable = new ColorDrawable(Color.BLUE);
        mModel.set(ICON, drawable);
        Drawable[] compoundDrawables =
                mView.getTitleViewForTesting().getCompoundDrawablesRelative();
        assertEquals(drawable, compoundDrawables[0]);
    }

    @Test
    public void testZoomButtonsCallbacks() {
        mModel.set(INCREASE_ZOOM_CALLBACK, () -> mZoomInClicked = true);
        mModel.set(DECREASE_ZOOM_CALLBACK, () -> mZoomOutClicked = true);

        mView.getZoomInButtonForTesting().performClick();
        assertTrue(mZoomInClicked);

        mView.getZoomOutButtonForTesting().performClick();
        assertTrue(mZoomOutClicked);
    }

    @Test
    public void testZoomButtonsEnabled() {
        mModel.set(INCREASE_ZOOM_ENABLED, true);
        assertTrue(mView.getZoomInButtonForTesting().isEnabled());

        mModel.set(INCREASE_ZOOM_ENABLED, false);
        assertFalse(mView.getZoomInButtonForTesting().isEnabled());

        mModel.set(DECREASE_ZOOM_ENABLED, true);
        assertTrue(mView.getZoomOutButtonForTesting().isEnabled());

        mModel.set(DECREASE_ZOOM_ENABLED, false);
        assertFalse(mView.getZoomOutButtonForTesting().isEnabled());
    }

    @Test
    public void testZoomPercentText() {
        mModel.set(ZOOM_PERCENT_TEXT, "125%");
        assertEquals("125%", mView.getZoomPercentageTextForTesting().getText().toString());
    }

    @Test
    public void testImmersiveModeCallback() {
        mModel.set(IMMERIVE_MODE_CALLBACK, () -> mImmersiveClicked = true);
        mView.getFullscreenButtonForTesting().performClick();
        assertTrue(mImmersiveClicked);
    }

    @Test
    public void testImmersiveModeEnabled() {
        // IMMERIVE_MODE_ENABLED is a ReadableBooleanPropertyKey, which is immutable
        // post-construction and cannot be updated dynamically via mModel.set().
        PropertyModel enabledModel =
                new PropertyModel.Builder(ALL_KEYS).with(IMMERIVE_MODE_ENABLED, true).build();
        PageZoomMenuItemViewBinder.bind(enabledModel, mView, IMMERIVE_MODE_ENABLED);
        assertEquals(View.VISIBLE, mView.getFullscreenContainerForTesting().getVisibility());

        PropertyModel disabledModel =
                new PropertyModel.Builder(ALL_KEYS).with(IMMERIVE_MODE_ENABLED, false).build();
        PageZoomMenuItemViewBinder.bind(disabledModel, mView, IMMERIVE_MODE_ENABLED);
        assertEquals(View.GONE, mView.getFullscreenContainerForTesting().getVisibility());
    }

    @Test
    public void testMenuItemId() {
        mModel.set(MENU_ITEM_ID, R.id.page_zoom_id);
        assertEquals(R.id.page_zoom_id, mView.getId());
    }

    @Test
    public void testHoverListener() {
        mModel.set(
                HOVER_LISTENER,
                (_, _) -> {
                    mHovered = true;
                    return true;
                });

        MotionEvent hoverEnter =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnter.setSource(InputDevice.SOURCE_CLASS_POINTER);
        assertTrue(mView.dispatchGenericMotionEvent(hoverEnter));
        assertTrue(mHovered);
        assertFalse(mView.isHovered());
        hoverEnter.recycle();
    }
}
