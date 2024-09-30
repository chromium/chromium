// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabCardViewBinderUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCardViewBinderUtilsUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Context mContext;
    private TabGroupColorViewProvider mTabGroupColorViewProvider;
    private FrameLayout mContainerView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mContext = activity;
        mTabGroupColorViewProvider =
                new TabGroupColorViewProvider(
                        activity, new Token(2L, 1L), /* isIncognito= */ false, TabGroupColorId.RED);
        mContainerView = new FrameLayout(activity);
        activity.setContentView(mContainerView);
    }

    @Test
    public void testUpdateTabGroupColorView() {
        TabCardViewBinderUtils.updateTabGroupColorView(mContainerView, /* viewProvider= */ null);
        assertEquals(View.GONE, mContainerView.getVisibility());
        assertEquals(0, mContainerView.getChildCount());

        TabCardViewBinderUtils.updateTabGroupColorView(mContainerView, mTabGroupColorViewProvider);
        assertEquals(View.VISIBLE, mContainerView.getVisibility());
        assertEquals(mTabGroupColorViewProvider.getLazyView(), mContainerView.getChildAt(0));

        TabCardViewBinderUtils.updateTabGroupColorView(mContainerView, /* viewProvider= */ null);
        assertEquals(View.GONE, mContainerView.getVisibility());
        assertEquals(0, mContainerView.getChildCount());
    }
}
