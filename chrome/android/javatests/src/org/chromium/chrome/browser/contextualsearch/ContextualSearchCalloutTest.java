// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchCalloutControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests the touch to search callout feature of Contextual Search. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.TOUCH_TO_SEARCH_CALLOUT)
@Batch(Batch.PER_CLASS)
public class ContextualSearchCalloutTest extends ContextualSearchInstrumentationBase {

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testCalloutPeekToExpandOpacityWhenNoCustomImage() throws Exception {
        ContextualSearchPanel panel = (ContextualSearchPanel) mManager.getContextualSearchPanel();
        // The view gets inflated immediately so this needs to run on the UI thread.
        ThreadUtils.runOnUiThreadBlocking(() -> panel.getSearchBarControl());
        ContextualSearchCalloutControl calloutControl =
                panel.getSearchBarControl().getCalloutControl();

        // Mark custom image as invisible
        calloutControl.onUpdateCustomImageVisibility(
                /* customImageIsVisible= */ false, /* visibilityPercentage= */ 0);

        calloutControl.onUpdateFromPeekToExpand(0);
        Assert.assertEquals(0, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateFromPeekToExpand(0.5f);
        Assert.assertEquals(0, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateFromPeekToExpand(1);
        Assert.assertEquals(0, calloutControl.getOpacity(), 0.01f);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testCalloutPeekToExpandOpacityWhenVisibleCustomImage() throws Exception {
        ContextualSearchPanel panel = (ContextualSearchPanel) mManager.getContextualSearchPanel();
        // The view gets inflated immediately so this needs to run on the UI thread.
        ThreadUtils.runOnUiThreadBlocking(() -> panel.getSearchBarControl());
        ContextualSearchCalloutControl calloutControl =
                panel.getSearchBarControl().getCalloutControl();

        // Mark custom image as visible.
        calloutControl.onUpdateCustomImageVisibility(
                /* customImageIsVisible= */ true, /* visibilityPercentage= */ 1);

        calloutControl.onUpdateFromPeekToExpand(0);
        Assert.assertEquals(1, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateFromPeekToExpand(0.25f);
        Assert.assertEquals(0.5f, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateFromPeekToExpand(0.5f);
        Assert.assertEquals(0, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateFromPeekToExpand(1);
        Assert.assertEquals(0, calloutControl.getOpacity(), 0.01f);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testCalloutCustomImageOpacity() throws Exception {
        ContextualSearchPanel panel = (ContextualSearchPanel) mManager.getContextualSearchPanel();
        // The view gets inflated immediately so this needs to run on the UI thread.
        ThreadUtils.runOnUiThreadBlocking(() -> panel.getSearchBarControl());
        ContextualSearchCalloutControl calloutControl =
                panel.getSearchBarControl().getCalloutControl();

        calloutControl.onUpdateCustomImageVisibility(
                /* customImageIsVisible= */ false, /* visibilityPercentage= */ 0);
        Assert.assertEquals(0, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateCustomImageVisibility(
                /* customImageIsVisible= */ true, /* visibilityPercentage= */ 0.5f);
        Assert.assertEquals(0.5f, calloutControl.getOpacity(), 0.01f);

        calloutControl.onUpdateCustomImageVisibility(
                /* customImageIsVisible= */ true, /* visibilityPercentage= */ 1);
        Assert.assertEquals(1, calloutControl.getOpacity(), 0.01f);
    }
}
