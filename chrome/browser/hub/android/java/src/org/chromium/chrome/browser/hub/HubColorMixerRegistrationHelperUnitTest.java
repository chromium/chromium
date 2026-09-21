// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link HubColorMixerRegistrationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubColorMixerRegistrationHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HubColorMixer mColorMixer1;
    @Mock private HubColorMixer mColorMixer2;
    @Mock private HubViewColorBlend mBlend1;
    @Mock private HubViewColorBlend mBlend2;
    private HubColorMixerRegistrationHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new HubColorMixerRegistrationHelper();
    }

    @Test
    public void testRegisterBlend_noMixer() {
        mHelper.registerBlend(mBlend1);
        // Should not crash, and not register to anything yet.
        verify(mColorMixer1, never()).registerBlend(mBlend1);
    }

    @Test
    public void testRegisterBlend_withMixer() {
        mHelper.setColorMixer(mColorMixer1);
        mHelper.registerBlend(mBlend1);
        verify(mColorMixer1).registerBlend(mBlend1);
    }

    @Test
    public void testRegisterBlend_duplicateIgnored() {
        mHelper.setColorMixer(mColorMixer1);
        mHelper.registerBlend(mBlend1);
        mHelper.registerBlend(mBlend1);
        verify(mColorMixer1).registerBlend(mBlend1);
    }

    @Test
    public void testSetColorMixer_registersExisting() {
        mHelper.registerBlend(mBlend1);
        mHelper.registerBlend(mBlend2);

        mHelper.setColorMixer(mColorMixer1);
        verify(mColorMixer1).registerBlend(mBlend1);
        verify(mColorMixer1).registerBlend(mBlend2);
    }

    @Test
    public void testSetColorMixer_unregistersOld_registersNew() {
        mHelper.registerBlend(mBlend1);
        mHelper.setColorMixer(mColorMixer1);
        verify(mColorMixer1).registerBlend(mBlend1);

        mHelper.setColorMixer(mColorMixer2);
        verify(mColorMixer1).unregisterBlend(mBlend1);
        verify(mColorMixer2).registerBlend(mBlend1);
    }

    @Test
    public void testSetColorMixer_nullUnregisters() {
        mHelper.registerBlend(mBlend1);
        mHelper.setColorMixer(mColorMixer1);
        verify(mColorMixer1).registerBlend(mBlend1);

        mHelper.setColorMixer(null);
        verify(mColorMixer1).unregisterBlend(mBlend1);
    }

    @Test
    public void testDestroy_unregisters() {
        mHelper.registerBlend(mBlend1);
        mHelper.setColorMixer(mColorMixer1);
        verify(mColorMixer1).registerBlend(mBlend1);

        mHelper.destroy();
        verify(mColorMixer1).unregisterBlend(mBlend1);

        // Registering after destroy should not register to the old mixer.
        mHelper.registerBlend(mBlend2);
        verify(mColorMixer1, never()).registerBlend(mBlend2);
    }
}
