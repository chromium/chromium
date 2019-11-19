// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.toolbar.ToolbarCommonPropertiesModel;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for {@link StatusMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class StatusMediatorUnitTest {
    private static final String TEST_SEARCH_URL = "https://www.test.com";

    @Mock
    NewTabPage mNewTabPage;
    @Mock
    Resources mResources;
    @Mock
    ToolbarCommonPropertiesModel mToolbarCommonPropertiesModel;
    @Mock
    UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    @Mock
    StatusMediator.StatusMediatorDelegate mDelegate;
    @Mock
    Bitmap mBitmap;
    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;
    @Captor
    ArgumentCaptor<String> mUrlCaptor;

    PropertyModel mModel;
    StatusMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);
        mMediator = new StatusMediator(mModel, mResources, mUrlBarEditingTextStateProvider);
        mMediator.setToolbarCommonPropertiesModel(mToolbarCommonPropertiesModel);
        mMediator.setDelegateForTesting(mDelegate);

        when(mDelegate.isUrlValid(mUrlCaptor.capture()))
                .thenAnswer(invocation -> mUrlCaptor.getValue().equals(TEST_SEARCH_URL));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo() {
        setupSearchEngineLogoForTesting(true, false, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo_hideAfterAnimationFinished() {
        setupSearchEngineLogoForTesting(true, false, false);
        doReturn(mNewTabPage).when(mToolbarCommonPropertiesModel).getNewTabPageForCurrentTab();
        doReturn("chrome://newtab").when(mToolbarCommonPropertiesModel).getCurrentUrl();

        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        mMediator.setUrlHasFocus(false);
        mMediator.setUrlFocusChangePercent(0);
        mMediator.setUrlAnimationFinished(true);
        Assert.assertFalse(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo_noHideIconAfterAnimationFinishedWhenScrolled() {
        setupSearchEngineLogoForTesting(true, false, false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        mMediator.setUrlFocusChangePercent(1f);
        mMediator.setUrlAnimationFinished(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogoOnNtpScroll() {
        setupSearchEngineLogoForTesting(true, false, false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        mMediator.setUrlFocusChangePercent(1f);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo_whenScrolled() {
        setupSearchEngineLogoForTesting(true, false, false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlFocusChangePercent(1f);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showGoogleLogo_searchLoupeEverywhere() {
        setupSearchEngineLogoForTesting(true, true, true);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showNonGoogleLogo() {
        setupSearchEngineLogoForTesting(true, false, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);

        // Clear invocations since the setup methods call updateLocationBarIcon.
        Mockito.clearInvocations(mDelegate);
        mMediator.updateSearchEngineStatusIcon(true, false, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search, mModel.get(StatusProperties.STATUS_ICON_RES));
        Mockito.verify(mDelegate, Mockito.times(1)).getSearchEngineLogoFavicon(any(), any());
        mCallbackCaptor.getValue().onResult(mBitmap);
        Assert.assertEquals(mBitmap, mModel.get(StatusProperties.STATUS_ICON));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_showNonGoogleLogo_searchLoupeEverywhere() {
        setupSearchEngineLogoForTesting(true, false, true);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);

        // Clear invocations since the setup methods call updateLocationBarIcon.
        Mockito.clearInvocations(mDelegate);
        mMediator.updateSearchEngineStatusIcon(true, false, TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_search, mModel.get(StatusProperties.STATUS_ICON_RES));
        Mockito.verify(mDelegate, Mockito.times(0)).getSearchEngineLogoFavicon(any(), any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        setupSearchEngineLogoForTesting(true, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.onTextChanged(TEST_SEARCH_URL);
        Assert.assertEquals(R.drawable.ic_globe_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenAutocompleteSiteContainsText() {
        setupSearchEngineLogoForTesting(true, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.onTextChanged(TEST_SEARCH_URL.substring(0, TEST_SEARCH_URL.length() - 1));
        Assert.assertEquals(R.drawable.ic_globe_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextDoesNotMatch() {
        setupSearchEngineLogoForTesting(true, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.onTextChanged("food near me");
        verify(mDelegate).isUrlValid("food near me");
        Assert.assertNotEquals(
                R.drawable.ic_globe_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextIsEmpty() {
        setupSearchEngineLogoForTesting(true, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        // Setup a valid url to prevent the default "" from matching the url.
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        mMediator.onTextChanged(TEST_SEARCH_URL);

        mMediator.onTextChanged("");
        verify(mDelegate).isUrlValid("");
        Assert.assertNotEquals(
                R.drawable.ic_globe_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_incognitoNoIcon() {
        setupSearchEngineLogoForTesting(true, true, false);
        doReturn(true).when(mToolbarCommonPropertiesModel).isIncognito();

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setSecurityIconResource(0);
        mMediator.updateSearchEngineStatusIcon(true, false, TEST_SEARCH_URL);

        Assert.assertEquals(0, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconChanges() {
        setupSearchEngineLogoForTesting(true, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setSecurityIconResource(0);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);

        Assert.assertTrue(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_24dp, mModel.get(StatusProperties.STATUS_ICON_RES));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconNoChanges() {
        setupSearchEngineLogoForTesting(true, true, false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(false);
        mMediator.setSecurityIconResource(0);
        mMediator.updateSearchEngineStatusIcon(true, true, TEST_SEARCH_URL);

        Assert.assertFalse(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void setSecurityIconTintForSearchEngineIcon_zeroForGoogleAndNoIcon() {
        mMediator.setUseDarkColors(false);
        Assert.assertEquals(0, mMediator.getSecurityIconTintForSearchEngineIcon(0));
        Assert.assertEquals(0,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_logo_googleg_24dp));
        mMediator.setUseDarkColors(true);
        Assert.assertEquals(0, mMediator.getSecurityIconTintForSearchEngineIcon(0));
        Assert.assertEquals(0,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_logo_googleg_24dp));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void setSecurityIconTintForSearchEngineIcon_correctForDarkColors() {
        mMediator.setUseDarkColors(true);
        Assert.assertEquals(R.color.default_icon_color_secondary_list,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_globe_24dp));
        Assert.assertEquals(R.color.default_icon_color_secondary_list,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
    public void setSecurityIconTintForSearchEngineIcon_correctForLightColors() {
        mMediator.setUseDarkColors(false);
        Assert.assertEquals(R.color.tint_on_dark_bg,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_globe_24dp));
        Assert.assertEquals(R.color.tint_on_dark_bg,
                mMediator.getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search));
    }

    private void setupSearchEngineLogoForTesting(
            boolean shouldShowLogo, boolean showGoogle, boolean loupeEverywhere) {
        doReturn(shouldShowLogo).when(mDelegate).shouldShowSearchEngineLogo(false);
        doReturn(false).when(mDelegate).shouldShowSearchEngineLogo(true);
        doReturn(loupeEverywhere).when(mDelegate).shouldShowSearchLoupeEverywhere(anyBoolean());
        Mockito.doNothing().when(mDelegate).getSearchEngineLogoFavicon(
                any(), mCallbackCaptor.capture());

        mMediator.updateSearchEngineStatusIcon(shouldShowLogo, showGoogle, TEST_SEARCH_URL);
    }
}
