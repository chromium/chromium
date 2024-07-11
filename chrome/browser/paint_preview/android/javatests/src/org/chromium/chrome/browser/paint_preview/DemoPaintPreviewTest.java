// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.chrome.browser.paint_preview.TabbedPaintPreviewTest.assertAttachedAndShown;

import androidx.test.filters.MediumTest;
import androidx.test.uiautomator.UiObjectNotFoundException;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.paintpreview.player.PlayerManager;

import java.util.concurrent.ExecutionException;

/** Tests for the {@link DemoPaintPreview} class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.PAINT_PREVIEW_DEMO})
@Batch(PER_CLASS)
public class DemoPaintPreviewTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String TEST_URL = "/chrome/test/data/android/about.html";

    // @Mock to tell R8 not to break the ability to mock the class.
    @Mock private static PaintPreviewTabService sMockService;

    @BeforeClass
    public static void setUp() {
        sMockService = Mockito.mock(PaintPreviewTabService.class);
        TabbedPaintPreview.overridePaintPreviewTabServiceForTesting(sMockService);
        PlayerManager.overrideCompositorDelegateFactoryForTesting(
                new TabbedPaintPreviewTest.TestCompositorDelegateFactory());
    }

    @AfterClass
    public static void tearDown() {
        PlayerManager.overrideCompositorDelegateFactoryForTesting(null);
        TabbedPaintPreview.overridePaintPreviewTabServiceForTesting(null);
    }

    @Before
    public void setup() {
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TEST_URL));
    }

    /**
     * Tests the demo mode is accessible from app menu and works successfully when the page has not
     * been captured before.
     */
    @Test
    @MediumTest
    public void testWithNoExistingCapture() throws UiObjectNotFoundException, ExecutionException {
        // Return false for PaintPreviewTabService#hasCaptureForTab initially.
        Mockito.doReturn(false).when(sMockService).hasCaptureForTab(Mockito.anyInt());

        // When PaintPreviewTabService#captureTab is called, return true for future calls to
        // PaintPreviewTabService#hasCaptureForTab and call the success callback with true.
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        Mockito.doAnswer(
                        invocation -> {
                            Mockito.doReturn(true)
                                    .when(sMockService)
                                    .hasCaptureForTab(Mockito.anyInt());
                            callbackCaptor.getValue().onResult(true);
                            return null;
                        })
                .when(sMockService)
                .captureTab(Mockito.any(Tab.class), callbackCaptor.capture());

        AppMenuCoordinator coordinator = sActivityTestRule.getAppMenuCoordinator();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(coordinator, null, false);
                });
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        coordinator, R.id.paint_preview_show_id));
        ThreadUtils.runOnUiThreadBlocking(
                () -> AppMenuTestSupport.callOnItemClick(coordinator, R.id.paint_preview_show_id));

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        TabbedPaintPreview tabbedPaintPreview =
                ThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        assertAttachedAndShown(tabbedPaintPreview, true, true);
    }
}
