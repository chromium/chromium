// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.ServiceStatus;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabCardViewBinderUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabCardViewBinderUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private ServiceStatus mServiceStatus;

    private Context mContext;
    private TabGroupColorViewProvider mTabGroupColorViewProvider;
    private FrameLayout mContainerView;

    @Before
    public void setUp() {
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mDataSharingService.getServiceStatus()).thenReturn(mServiceStatus);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mContext = activity;
        mTabGroupColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        new Token(2L, 1L),
                        /* isIncognito= */ false,
                        TabGroupColorId.RED,
                        mTabGroupSyncService,
                        mDataSharingService);
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
