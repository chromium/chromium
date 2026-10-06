// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.android.bars_common.TabSwitcherButtonView;
import org.chromium.chrome.browser.ui.android.bars_common.TabSwitcherDrawable;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabSwitcherActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherActionButtonBinderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSwitcherDrawable.Observer mDrawableObserver;

    private TabSwitcherButtonView mView;
    private TabSwitcherDrawable mDrawable;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = new TabSwitcherButtonView(activity, null);
        mDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        activity,
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawable.TabSwitcherDrawableLocation.TAB_TOOLBAR);
        mView.setDrawableForTesting(mDrawable);
        mDrawable.addTabSwitcherDrawableObserver(mDrawableObserver);

        mModel = new PropertyModel.Builder(TabSwitcherActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mModel,
                mView,
                (model, view, propertyKey) ->
                        TabSwitcherActionButtonBinder.bind(model, (View) view, propertyKey));
    }

    @Test
    public void testTabCount() {
        mModel.set(TabSwitcherActionProperties.TAB_COUNT, 5);
        assertEquals(5, mDrawable.getTabCount());
        assertEquals("5", drawAndGetRenderedText());
    }

    @Test
    public void testIsIncognito() {
        mModel.set(TabSwitcherActionProperties.IS_INCOGNITO, true);
        // Only the incognito state changed, which still updates the drawable.
        verify(mDrawableObserver).onDrawableStateChanged();
        assertEquals(0, mDrawable.getTabCount());
    }

    @Test
    public void testTabCountAndIncognito() {
        // More than 99 tabs renders a different string for incognito and non-incognito.
        mModel.set(TabSwitcherActionProperties.TAB_COUNT, 100);
        assertEquals(":D", drawAndGetRenderedText());

        mModel.set(TabSwitcherActionProperties.IS_INCOGNITO, true);
        assertEquals(100, mDrawable.getTabCount());
        assertEquals(";)", drawAndGetRenderedText());
    }

    @Test
    public void testHasNotificationDot() {
        mModel.set(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT, true);
        assertTrue(mView.isNotificationDotVisible());

        mModel.set(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT, false);
        assertFalse(mView.isNotificationDotVisible());
    }

    @Test
    public void testShowTabSwitcherTrigger() {
        Drawable background = spy(new ColorDrawable(Color.RED));
        mView.setBackground(background);
        clearInvocations(background);

        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        verify(background).jumpToCurrentState();
    }

    @Test
    public void testShowTabSwitcherTrigger_MultipleTimes() {
        Drawable background = spy(new ColorDrawable(Color.RED));
        mView.setBackground(background);
        clearInvocations(background);

        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        verify(background, times(3)).jumpToCurrentState();
    }

    @Test
    public void testFallbackToActionButtonBinder() {
        mModel.set(ActionProperties.CONTENT_DESCRIPTION_RESOLVER, context -> "Tab Switcher");
        assertEquals("Tab Switcher", mView.getContentDescription());
    }

    private String drawAndGetRenderedText() {
        mDrawable.setBounds(0, 0, 100, 100);
        mDrawable.draw(new Canvas(Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)));
        return mDrawable.getTextRenderedForTesting();
    }
}
