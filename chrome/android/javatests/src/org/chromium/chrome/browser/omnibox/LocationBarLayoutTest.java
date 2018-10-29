// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.support.test.filters.SmallTest;
import android.support.v7.widget.AppCompatImageButton;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceTestUtils;
import org.chromium.chrome.browser.toolbar.ToolbarModel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link LocationBarLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarLayoutTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final int SEARCH_ICON_RESOURCE = R.drawable.omnibox_search;

    private static final String SEARCH_TERMS = "machine learning";
    private static final String SEARCH_TERMS_URL = "testing.com";
    private static final String GOOGLE_SRP_URL = "https://www.google.com/search?q=machine+learning";
    private static final String GOOGLE_SRP_URL_LIKE_URL =
            "https://www.google.com/search?q=" + SEARCH_TERMS_URL;
    private static final String BING_SRP_URL = "https://www.bing.com/search?q=machine+learning";

    private static final String VERBOSE_URL = "https://www.suchwowveryyes.edu";
    private static final String TRIMMED_URL = "suchwowveryyes.edu";

    private TestToolbarModel mTestToolbarModel;

    private class TestToolbarModel extends ToolbarModel {
        private String mCurrentUrl;
        private String mEditingText;
        private String mDisplayText;
        private Integer mSecurityLevel;

        public TestToolbarModel() {
            super(ContextUtils.getApplicationContext());
            initializeWithNative();
        }

        void setCurrentUrl(String url) {
            mCurrentUrl = url;
        }

        void setSecurityLevel(@ConnectionSecurityLevel int securityLevel) {
            mSecurityLevel = securityLevel;
        }

        @Override
        public String getCurrentUrl() {
            if (mCurrentUrl == null) return super.getCurrentUrl();
            return mCurrentUrl;
        }

        @Override
        @ConnectionSecurityLevel
        public int getSecurityLevel() {
            if (mSecurityLevel == null) return super.getSecurityLevel();
            return mSecurityLevel;
        }

        @Override
        public UrlBarData getUrlBarData() {
            UrlBarData urlBarData = super.getUrlBarData();
            CharSequence displayText = mDisplayText == null ? urlBarData.displayText : mDisplayText;
            String editingText = mEditingText == null ? urlBarData.editingText : mEditingText;
            return UrlBarData.forUrlAndText(getCurrentUrl(), displayText.toString(), editingText);
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestToolbarModel = new TestToolbarModel();
        mTestToolbarModel.setTab(mActivityTestRule.getActivity().getActivityTab(), false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> getLocationBar().setToolbarDataProvider(mTestToolbarModel));
    }

    private void setUrlToPageUrl(LocationBarLayout locationBar) {
        ThreadUtils.runOnUiThreadBlocking(() -> { getLocationBar().updateLoadingState(true); });
    }

    private String getUrlText(UrlBar urlBar) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> urlBar.getText().toString());
        } catch (ExecutionException ex) {
            throw new RuntimeException(ex);
        }
    }

    private UrlBar getUrlBar() {
        return (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
    }

    private LocationBarLayout getLocationBar() {
        return (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    private AppCompatImageButton getDeleteButton() {
        return (AppCompatImageButton) mActivityTestRule.getActivity().findViewById(
                R.id.delete_button);
    }

    private AppCompatImageButton getMicButton() {
        return (AppCompatImageButton) mActivityTestRule.getActivity().findViewById(R.id.mic_button);
    }

    private AppCompatImageButton getSecurityButton() {
        return (AppCompatImageButton) mActivityTestRule.getActivity().findViewById(
                R.id.security_button);
    }

    private void setUrlBarTextAndFocus(String text)
            throws ExecutionException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() throws InterruptedException {
                getLocationBar().onUrlFocusChange(true);
                mActivityTestRule.typeInOmnibox(text, false);
                return null;
            }
        });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testNotShowingVoiceSearchButtonIfUrlBarContainsTextAndFlagIsDisabled()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("testing");

        Assert.assertEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertNotEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingVoiceSearchButtonIfUrlBarIsEmptyAndFlagIsDisabled()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("");

        Assert.assertNotEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingVoiceAndDeleteButtonsShowingIfUrlBarContainsText()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("testing");

        Assert.assertEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingOnlyVoiceButtonIfUrlBarIsEmpty()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("");

        Assert.assertNotEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsOnlyShowingSearchTermsIfSrpIsGoogle() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertEquals(SEARCH_TERMS, getUrlText(urlBar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsOnlyShowingSearchTermsIfSrpIsBing()
            throws ExecutionException, InterruptedException, TimeoutException {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        TemplateUrlServiceTestUtils.setSearchEngine("bing.com");
        mTestToolbarModel.setCurrentUrl(BING_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertEquals(SEARCH_TERMS, getUrlText(urlBar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsNotShowingSearchTermsIfSrpIsBingAndSrpUrlIsGoogle()
            throws ExecutionException, InterruptedException, TimeoutException {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        TemplateUrlServiceTestUtils.setSearchEngine("bing.com");
        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertNotEquals(SEARCH_TERMS, getUrlText(urlBar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testNotShowingSearchIconOnMixedContent() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.NONE);
        setUrlToPageUrl(locationBar);

        AppCompatImageButton securityButton = getSecurityButton();
        Assert.assertNotEquals(SEARCH_TERMS, urlBar.getText().toString());
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotEquals(mTestToolbarModel.getSecurityIconResource(
                                           mActivityTestRule.getActivity().isTablet()),
                    SEARCH_ICON_RESOURCE);
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsShowingSearchIconSecureContent() {
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        AppCompatImageButton securityButton = getSecurityButton();
        Assert.assertEquals(securityButton.getVisibility(), View.VISIBLE);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(mTestToolbarModel.getSecurityIconResource(
                                        mActivityTestRule.getActivity().isTablet()),
                    SEARCH_ICON_RESOURCE);
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testNotShowingSearchTermsIfLooksLikeUrl() throws ExecutionException {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL_LIKE_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertNotEquals(SEARCH_TERMS_URL, getUrlText(urlBar));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testNotShowingSearchTermsIfSrpIsGoogleAndFlagIsDisabled()
            throws ExecutionException {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertNotEquals(SEARCH_TERMS, getUrlText(urlBar));
    }

    @Test
    @SmallTest
    public void testEditingTextShownOnFocus() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(VERBOSE_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        mTestToolbarModel.mDisplayText = TRIMMED_URL;
        mTestToolbarModel.mEditingText = VERBOSE_URL;
        setUrlToPageUrl(locationBar);

        Assert.assertEquals(TRIMMED_URL, getUrlText(urlBar));

        ThreadUtils.runOnUiThreadBlocking(() -> { urlBar.requestFocus(); });

        Assert.assertEquals(VERBOSE_URL, getUrlText(urlBar));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, urlBar.getSelectionStart());
            Assert.assertEquals(VERBOSE_URL.length(), urlBar.getSelectionEnd());
        });
    }
}
