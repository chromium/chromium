// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.graphics.Bitmap;
import android.os.Handler;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManagerJni;
import org.chromium.chrome.browser.preloading.PreloadingFeatureMap;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileVisualType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

/** Tests for events around the loading of a New Tab Page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
// Tests the Java native NTP, which is not used on desktop (it uses the WebUI NTP).
@Restriction(DeviceFormFactor.PHONE_OR_TABLET)
public class NewTabPageLoadTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SuggestionsDependenciesRule mSuggestionDeps = new SuggestionsDependenciesRule();

    @Mock private AndroidPrerenderManager.Natives mPrerenderManagerJniMock;

    private AutoVerifyingMostVisitedSites mMostVisitedSites;

    @Before
    public void setUp() throws Exception {
        mMostVisitedSites = new AutoVerifyingMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(
                mActivityTestRule.getTestServer().getURLs("/site1", "/site2"));
        mSuggestionDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mSuggestionDeps.getFactory().largeIconBridge = new AsyncMockLargeIconBridge();
    }

    @Test
    @SmallTest
    public void testTilesTypeInitialisedWhenPageLoaded() {
        mActivityTestRule
                .startOnBlankPage()
                .loadPageProgrammatically(
                        getOriginalNativeNtpUrl(), RegularNewTabPageStation.newBuilder());
        assertTrue(mMostVisitedSites.pageImpressionRecorded);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        PreloadingFeatureMap.PREWARM,
        PreloadingFeatureMap.PREWARM_PAGE_LOAD_AND_TAB_SWITCH_AFTER_PAINT_TRIGGER
    })
    @DisableFeatures(PreloadingFeatureMap.PREWARM_ZERO_SUGGEST_TRIGGER)
    public void testPrewarmTriggeredOnLoadAndTabSwitch() {
        AndroidPrerenderManagerJni.setInstanceForTesting(mPrerenderManagerJniMock);

        RegularNewTabPageStation ntpStation =
                mActivityTestRule
                        .startOnBlankPage()
                        .loadPageProgrammatically(
                                getOriginalNativeNtpUrl(), RegularNewTabPageStation.newBuilder());
        Tab tab = ntpStation.getTab();
        verify(mPrerenderManagerJniMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .startPrewarm(tab.getWebContents());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clearInvocations(mPrerenderManagerJniMock);
                    tab.hide(TabHidingType.CHANGED_TABS);
                    verify(mPrerenderManagerJniMock, never()).startPrewarm(any());
                    tab.show(TabSelectionType.FROM_USER);
                    verify(mPrerenderManagerJniMock).startPrewarm(tab.getWebContents());
                });
    }

    private static class AutoVerifyingMostVisitedSites extends FakeMostVisitedSites {
        public boolean pageImpressionRecorded;

        @Override
        public void recordPageImpression(int tilesCount) {
            pageImpressionRecorded = true;
        }

        @Override
        public void recordTileImpression(Tile tile) {
            assertNotEquals(TileVisualType.NONE, tile.getType());
        }
    }

    private static class AsyncMockLargeIconBridge extends LargeIconBridge {
        @Override
        public boolean getLargeIconForUrl(
                GURL pageUrl, int desiredSizePx, final LargeIconBridge.LargeIconCallback callback) {
            new Handler()
                    .postDelayed(
                            () -> {
                                callback.onLargeIconAvailable(
                                        Bitmap.createBitmap(148, 148, Bitmap.Config.ALPHA_8),
                                        0,
                                        false,
                                        IconType.INVALID);
                            },
                            0);

            return true;
        }

        @Override
        public void destroy() {
            // Empty to avoid calling nativeDestroy inappropriately.
        }
    }
}
