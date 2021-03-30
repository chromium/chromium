// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
/**
 * Unit tests for {@link StatusMediator}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_PROMO_EXISTING_DEVICE)
public final class StatusMediatorUnitTest {
    private static final String TEST_SEARCH_URL = "https://www.test.com";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    NewTabPageDelegate mNewTabPageDelegate;
    @Mock
    LocationBarDataProvider mLocationBarDataProvider;
    @Mock
    UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    @Mock
    Runnable mMockForceModelViewReconciliationRunnable;
    @Mock
    SearchEngineLogoUtils mSearchEngineLogoUtils;
    @Mock
    Profile mProfile;
    @Mock
    LibraryLoader mLibraryLoader;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    PermissionDialogController mPermissionDialogController;

    Context mContext;
    Resources mResources;

    PropertyModel mModel;
    StatusMediator mMediator;
    Bitmap mBitmap;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mContext = ContextUtils.getApplicationContext();
        mResources = mContext.getResources();

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        doReturn(true).when(mLibraryLoader).isInitialized();
        LibraryLoader.setLibraryLoaderForTesting(mLibraryLoader);

        // By default return mBitmap, but this behavior is overridden in some tests.
        Answer bitmapAnswer = (invocation) -> {
            ((Callback<Bitmap>) invocation.getArgument(2)).onResult(mBitmap);
            return null;
        };
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        doAnswer(bitmapAnswer)
                .when(mSearchEngineLogoUtils)
                .getSearchEngineLogoFavicon(any(), eq(mResources), any(), any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator = new StatusMediator(mModel, mResources, mContext,
                    mUrlBarEditingTextStateProvider,
                    /* isTablet */ false, mMockForceModelViewReconciliationRunnable,
                    mLocationBarDataProvider, mPermissionDialogController, mSearchEngineLogoUtils,
                    () -> mTemplateUrlService, () -> mProfile, null, null);
        });
        mBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_hideAfterAnimationFinished() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();
        doReturn("chrome://newtab").when(mLocationBarDataProvider).getCurrentUrl();

        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        mMediator.setUrlHasFocus(false);
        mMediator.setUrlFocusChangePercent(0);
        mMediator.setUrlAnimationFinished(true);
        Assert.assertFalse(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_noHideIconAfterAnimationFinishedWhenScrolled() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        mMediator.setUrlFocusChangePercent(1f);
        mMediator.setUrlAnimationFinished(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogoOnNtpScroll() {
        doReturn(true).when(mSearchEngineLogoUtils).currentlyOnNTP(mLocationBarDataProvider);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ false, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        mMediator.setUrlFocusChangePercent(1f);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_whenScrolled() {
        doReturn(false).when(mLocationBarDataProvider).isLoading();
        doReturn(UrlConstants.NTP_URL).when(mLocationBarDataProvider).getCurrentUrl();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();
        doReturn(true).when(mSearchEngineLogoUtils).currentlyOnNTP(mLocationBarDataProvider);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlFocusChangePercent(1f);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_searchloupeEverywhere() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ true);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_showNonGoogleLogo() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ false, /* loupeEverywhere= */ false);

        StatusProperties.StatusIconResource resource =
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE);
        BitmapDrawable bitmapDrawable = (BitmapDrawable) resource.getDrawable(mContext, mResources);
        int tint = resource.getTint();
        Assert.assertEquals(mBitmap, bitmapDrawable.getBitmap());
        Assert.assertEquals("All search engine logos should be untinted.", 0, tint);
        Mockito.verify(mSearchEngineLogoUtils, Mockito.times(1))
                .getSearchEngineLogoFavicon(any(), eq(mResources), any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_showNonGoogleLogo_defaultsToLoupeWhenFaviconIsNull() {
        doAnswer(invocation -> {
            ((Callback<Bitmap>) invocation.getArgument(2)).onResult(null);
            return null;
        })
                .when(mSearchEngineLogoUtils)
                .getSearchEngineLogoFavicon(any(), eq(mResources), any(), any());

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ false, /* loupeEverywhere= */ false);

        StatusProperties.StatusIconResource resource =
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE);
        int tint = resource.getTint();
        Assert.assertEquals(R.drawable.ic_search,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
        Assert.assertEquals("Search loupes should have non-zero tints applied.",
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search), tint);
        Mockito.verify(mSearchEngineLogoUtils, Mockito.times(1))
                .getSearchEngineLogoFavicon(any(), eq(mResources), any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_showNonGoogleLogo_searchloupeEverywhere() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ false, /* loupeEverywhere= */ true);

        Assert.assertEquals(R.drawable.ic_search,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
        Mockito.verify(mSearchEngineLogoUtils, Mockito.times(0))
                .getSearchEngineLogoFavicon(any(), eq(mResources), any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenAutocompleteSiteContainsText() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextDoesNotMatch() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextIsEmpty() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        // Setup a valid url to prevent the default "" from matching the url.
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_incognitoNoIcon() {
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setSecurityIconResource(0);
        mMediator.updateSearchEngineStatusIcon(false, TEST_SEARCH_URL);

        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconChanges() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setSecurityIconResource(0);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        Assert.assertTrue(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconNoChanges() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(false);
        mMediator.setSecurityIconResource(0);
        mMediator.updateSearchEngineStatusIcon(true, TEST_SEARCH_URL);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        Assert.assertFalse(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void setSecurityIconTintForSearchEngineIcon_zeroForGoogleAndNoIcon() {
        mMediator.setUseDarkColors(false);
        Assert.assertEquals(0, mMediator.getSecurityIconTintForSearchEngineIcon(0));
        Assert.assertEquals(0,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_logo_googleg_20dp));
        mMediator.setUseDarkColors(true);
        Assert.assertEquals(0, mMediator.getSecurityIconTintForSearchEngineIcon(0));
        Assert.assertEquals(0,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_logo_googleg_20dp));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void setSecurityIconTintForSearchEngineIcon_correctForDarkColors() {
        mMediator.setUseDarkColors(true);
        Assert.assertEquals(R.color.default_icon_color_secondary_tint_list,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_globe_24dp));
        Assert.assertEquals(R.color.default_icon_color_secondary_tint_list,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void setSecurityIconTintForSearchEngineIcon_correctForLightColors() {
        mMediator.setUseDarkColors(false);
        Assert.assertEquals(R.color.default_icon_color_light_tint_list,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_globe_24dp));
        Assert.assertEquals(R.color.default_icon_color_light_tint_list,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextEmpty() {
        Assert.assertEquals("Empty urlBarText should resolve to empty urlBarTextWithAutocomplete",
                "", mMediator.resolveUrlBarTextWithAutocomplete(""));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextMismatchesAutocompleteText() {
        doReturn("https://foo.com").when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        String msg = "The urlBarText should only resolve to the autocomplete text if it's a "
                + "substring of the autocomplete text.";
        Assert.assertEquals(
                msg, "https://foo.com", mMediator.resolveUrlBarTextWithAutocomplete("foo.com"));
        Assert.assertEquals(msg, "bar.com", mMediator.resolveUrlBarTextWithAutocomplete("bar.com"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void testIncognitoStateChange_goingToIncognito() {
        mMediator.setShowIconsWhenUrlFocused(true);

        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        verify(mMockForceModelViewReconciliationRunnable, times(0)).run();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void testIncognitoStateChange_backFromIncognito() {
        mMediator.setShowIconsWhenUrlFocused(true);

        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        verify(mMockForceModelViewReconciliationRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    @UiThreadTest
    public void testIncognitoStateChange_shouldShowStatusIcon() {
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        verify(mMockForceModelViewReconciliationRunnable, times(0)).run();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testStatusText() {
        mMediator.setUnfocusedLocationBarWidth(10);
        mMediator.setPageIsOffline(true);
        mMediator.setPageIsPreview(true);
        mMediator.setPageIsPaintPreview(true);
        // When all 3 states, offline, preview, and paint preview are enabled, paint preview has
        // the highest priority.
        Assert.assertEquals("Incorrect text for paint preview status text",
                R.string.location_bar_paint_preview_page_status,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for paint preview status text",
                R.color.locationbar_status_preview_color_light,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));
        mMediator.setUseDarkColors(true);
        Assert.assertEquals("Incorrect color for paint preview status text",
                R.color.locationbar_status_preview_color,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));

        // When offline and preview are enabled, preview has higher priority.
        mMediator.setUseDarkColors(false);
        mMediator.setPageIsPaintPreview(false);
        Assert.assertEquals("Incorrect text for preview status text",
                R.string.location_bar_preview_lite_page_status,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for preview status text",
                R.color.locationbar_status_preview_color_light,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));
        mMediator.setUseDarkColors(true);
        Assert.assertEquals("Incorrect color for preview status text",
                R.color.locationbar_status_preview_color,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));

        // When only offline is enabled, it should be shown.
        mMediator.setUseDarkColors(false);
        mMediator.setPageIsPreview(false);
        Assert.assertEquals("Incorrect text for offline page status text",
                R.string.location_bar_verbose_status_offline,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for offline page status text",
                R.color.locationbar_status_offline_color_light,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));
        mMediator.setUseDarkColors(true);
        Assert.assertEquals("Incorrect color for offline page status text",
                R.color.locationbar_status_offline_color,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));
    }

    /**
     * @param showLogo Whether the search engine logo should be shown.
     * @param isGoogle Whether the search engine is Google.
     * @param loupeEverywhere Whether to show the loupe everywhere.
     */
    private void setupSearchEngineLogoForTesting(
            boolean showLogo, boolean isGoogle, boolean loupeEverywhere) {
        doReturn(showLogo).when(mSearchEngineLogoUtils).shouldShowSearchEngineLogo(false);
        doReturn(false).when(mSearchEngineLogoUtils).shouldShowSearchEngineLogo(true);
        doReturn(loupeEverywhere)
                .when(mSearchEngineLogoUtils)
                .shouldShowSearchLoupeEverywhere(anyBoolean());

        mMediator.updateSearchEngineStatusIcon(isGoogle, TEST_SEARCH_URL);
    }
}
