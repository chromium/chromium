// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render tests for {@link ReaderModePrefsView}. */
// TODO(crbug.com/480242750): Support dark mode and sepia for this test suite.
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ReaderModePrefsRenderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_READER_MODE)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DistilledPagePrefs mDistilledPagePrefs;

    private ReaderModePrefsView mReaderModePrefsView;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    private void createView(boolean linksEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mReaderModePrefsView =
                            ReaderModePrefsView.create(
                                    mActivityTestRule.getActivity(), mDistilledPagePrefs);
                    mReaderModePrefsView.onChangeLinksEnabled(linksEnabled);
                    mActivityTestRule.getActivity().setContentView(mReaderModePrefsView);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(DomDistillerFeatures.READER_MODE_TOGGLE_LINKS)
    public void testRender_LinksEnabled() throws IOException {
        createView(true);
        mRenderTestRule.render(mReaderModePrefsView, "reader_mode_prefs_links_enabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(DomDistillerFeatures.READER_MODE_TOGGLE_LINKS)
    public void testRender_LinksDisabled() throws IOException {
        createView(false);
        mRenderTestRule.render(mReaderModePrefsView, "reader_mode_prefs_links_disabled");
    }
}
