// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabGroupColorViewProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupColorViewProviderUnitTest {
    private static final Token REGULAR_TAB_GROUP_ID = new Token(3L, 4L);
    private static final Token INCOGNITO_TAB_GROUP_ID = new Token(5L, 6L);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Context mContext;
    private TabGroupColorViewProvider mRegularColorViewProvider;
    private TabGroupColorViewProvider mIncognitoColorViewProvider;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mContext = activity;
        mRegularColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        REGULAR_TAB_GROUP_ID,
                        /* isIncognito= */ false,
                        TabGroupColorId.RED);
        mIncognitoColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        INCOGNITO_TAB_GROUP_ID,
                        /* isIncognito= */ true,
                        TabGroupColorId.BLUE);
    }

    @Test
    public void testGetTabGroupId() {
        assertEquals(REGULAR_TAB_GROUP_ID, mRegularColorViewProvider.getTabGroupId());
        assertEquals(INCOGNITO_TAB_GROUP_ID, mIncognitoColorViewProvider.getTabGroupId());
    }

    @Test
    public void testColorView() {
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.RED,
                TabGroupColorId.CYAN);
        verifyColorView(
                mIncognitoColorViewProvider,
                /* isIncognito= */ true,
                TabGroupColorId.BLUE,
                TabGroupColorId.PURPLE);
    }

    private void verifyColorView(
            TabGroupColorViewProvider viewProvider,
            boolean isIncognito,
            @TabGroupColorId int initialColorId,
            @TabGroupColorId int finalColorId) {
        View colorView = viewProvider.getLazyView();
        GradientDrawable drawable = (GradientDrawable) colorView.getBackground();
        assertNotNull(drawable);

        assertEquals(
                ColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, initialColorId, isIncognito),
                drawable.getColor().getDefaultColor());

        viewProvider.setTabGroupColorId(finalColorId);
        assertEquals(colorView, viewProvider.getLazyView());

        assertEquals(
                ColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, finalColorId, isIncognito),
                drawable.getColor().getDefaultColor());
    }
}
