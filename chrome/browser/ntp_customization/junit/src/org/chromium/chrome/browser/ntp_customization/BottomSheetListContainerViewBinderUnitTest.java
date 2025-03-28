// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BottomSheetListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListContainerViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.LIST_CONTAINER_KEYS);
    }

    @Test
    @SmallTest
    public void testBind() {
        BottomSheetListContainerView containerView = mock(BottomSheetListContainerView.class);
        PropertyModelChangeProcessor.create(
                mPropertyModel, containerView, BottomSheetListContainerViewBinder::bind);

        // Verifies if the delegate is not null, it should be bound to the containerView.
        ListContainerViewDelegate delegate = mock(ListContainerViewDelegate.class);
        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, delegate);
        verify(containerView).renderAllListItems(eq(delegate));

        // Verifies the delegate is null, the containerView should be destroyed.
        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, null);
        verify(containerView).destroy();
    }
}
