// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Assert;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** TestSupport augmented with states. */
public class TestSupportExtended extends TestSupport {
    // Derived classes need to initialize states in setUp().
    @Mock protected TabModel mTabModel;
    @Mock protected ModuleDelegate mModuleDelegate;

    protected Context mContext;
    protected PropertyModel mModel;

    /** Creates a mock local Tab instance, and adds it to `mTabModel`. */
    protected Tab createMockLocalTab(int index) {
        assert index == 0 || index == 1;
        GURL[] urlChoices = {JUnitTestGURLs.GOOGLE_URL_DOG, JUnitTestGURLs.GOOGLE_URL_CAT};
        String[] titleChoices = {"Google Dog", "Google Cat"};
        int[] tabIds = {40, 60};
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(urlChoices[index]);
        when(tab.getTitle()).thenReturn(titleChoices[index]);
        when(tab.getTimestampMillis()).thenReturn(makeTimestamp(16, 0, 0));
        when(tab.getId()).thenReturn(tabIds[index]);
        when(mTabModel.getTabById(tab.getId())).thenReturn(tab);
        return tab;
    }

    /** Verifies basic UI state and `mModuleDelegate` call counts. */
    protected void checkModuleState(
            boolean isVisible,
            int expectOnDataReadyCalls,
            int expectOnDataFetchFailedCalls,
            int expectRemoveModuleCalls) {
        Assert.assertEquals(isVisible, mModel.get(TabResumptionModuleProperties.IS_VISIBLE));
        verify(mModuleDelegate, times(expectOnDataReadyCalls)).onDataReady(anyInt(), any());
        verify(mModuleDelegate, times(expectOnDataFetchFailedCalls)).onDataFetchFailed(anyInt());
        verify(mModuleDelegate, times(expectRemoveModuleCalls)).removeModule(anyInt());
    }
}
