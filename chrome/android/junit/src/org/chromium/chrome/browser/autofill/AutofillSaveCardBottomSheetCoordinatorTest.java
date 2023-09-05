// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.Mockito.verify;
import static org.robolectric.Robolectric.buildActivity;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveCardBottomSheetCoordinatorTest {
    private static final String HTTPS_EXAMPLE_TEST = "https://example.test";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutofillSaveCardBottomSheetCoordinator mCoordinator;

    private ShadowActivity mShadowActivity;

    @Mock
    private BottomSheetController mBottomSheetController;

    @Mock
    private LayoutStateProvider mLayoutStateProvider;

    private MockTabModel mTabModel;

    private AutofillSaveCardUiInfo mUiInfo;

    @Mock
    private AutofillSaveCardBottomSheetBridge mBridge;

    @Mock
    private AutofillSaveCardBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mUiInfo = new AutofillSaveCardUiInfo.Builder()
                          .withCardDetail(new CardDetail(/*iconId*/ 0, "label", "subLabel"))
                          .build();
        Activity activity = buildActivity(Activity.class).create().get();
        mShadowActivity = shadowOf(activity);
        mTabModel = new MockTabModel(/* incognito= */ false, /* delegate= */ null);
        mCoordinator = new AutofillSaveCardBottomSheetCoordinator(activity, mBridge, mMediator);
    }

    @Test
    public void testRequestShowContent_callsMediatorRequestShow() {
        mCoordinator.requestShowContent();

        verify(mMediator).requestShowContent();
    }

    @Test
    public void testConstructor_setsLaunchChromeCallback() {
        mCoordinator.launchCctOnLegalMessageClick(HTTPS_EXAMPLE_TEST);

        Intent intent = mShadowActivity.getNextStartedActivity();
        assertThat(intent.getData(), equalTo(Uri.parse(HTTPS_EXAMPLE_TEST)));
        assertThat(intent.getAction(), equalTo(Intent.ACTION_VIEW));
        assertThat(intent.getExtras().get(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE),
                equalTo(CustomTabsIntent.SHOW_PAGE_TITLE));
    }

    @Test
    public void testDestroy_callsMediatorDestroy() {
        mCoordinator.requestShowContent();

        mCoordinator.destroy();

        verify(mMediator).destroy();
    }
}
