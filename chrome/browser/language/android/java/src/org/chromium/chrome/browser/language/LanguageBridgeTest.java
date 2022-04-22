// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
// import org.chromium.browser.language.FakeLanguageBridgeJni;
// import org.chromium.browser.language.LanguageBridge;
// import org.chromium.browser.language.LanguageBridgeJni;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for {@link LanguageBridge} which gets language lists from native
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LanguageBridgeTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private FakeLanguageBridgeJni mFakeLanguageBridge;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // Setup fake language bridge JNI interface
        mFakeLanguageBridge = new FakeLanguageBridgeJni();
        String[] ulpLanguages = {"pt-BR", "en-US"};
        mFakeLanguageBridge.setULPLanguages(ulpLanguages);
        mJniMocker.mock(LanguageBridgeJni.TEST_HOOKS, mFakeLanguageBridge);
    }

    @Test
    @SmallTest
    public void testIsTopULPBaseLanguage() {
        Assert.assertTrue(LanguageBridge.isTopULPBaseLanguage("pt"));
        Assert.assertTrue(LanguageBridge.isTopULPBaseLanguage("pt-PT"));
        Assert.assertTrue(LanguageBridge.isTopULPBaseLanguage("pt-BR"));
        Assert.assertFalse(LanguageBridge.isTopULPBaseLanguage("en"));
        Assert.assertFalse(LanguageBridge.isTopULPBaseLanguage("en-US"));
    }
}
