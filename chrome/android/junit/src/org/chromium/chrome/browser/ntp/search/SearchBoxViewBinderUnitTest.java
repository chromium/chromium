// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import com.airbnb.lottie.LottieAnimationView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link SearchBoxViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchBoxViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View.OnClickListener mOnClickListener;

    @Mock private SearchBoxContainerView mParentView;
    @Mock private LottieAnimationView mComposeplateButtonView;

    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mPropertyModel = new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mPropertyModel, mParentView, new SearchBoxViewBinder());
        when(mParentView.findViewById(R.id.composeplate_button))
                .thenReturn(mComposeplateButtonView);
    }

    @Test
    public void testSetComposeplateButtonClickListener() {
        mPropertyModel.set(
                SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK, mOnClickListener);
        verify(mComposeplateButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetComposeplateButtonVisibility() {
        mPropertyModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY, true);
        verify(mParentView).setComposeplateButtonVisibility(eq(true));

        mPropertyModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY, false);
        verify(mParentView).setComposeplateButtonVisibility(eq(false));
    }

    @Test
    public void testSetComposeplateButtonAnimation() {
        int iconRawResId = 10;
        mPropertyModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID, iconRawResId);
        verify(mComposeplateButtonView).setAnimation(eq(iconRawResId));
    }
}
