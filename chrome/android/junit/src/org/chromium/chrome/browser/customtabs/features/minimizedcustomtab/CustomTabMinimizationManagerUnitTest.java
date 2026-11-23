// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManager.KEY_IS_CCT_MINIMIZED;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.graphics.Bitmap;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.PictureInPictureModeChangedInfo;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.toolbar.top.ResourceFactory;
import org.chromium.chrome.browser.toolbar.top.ResourceFactoryJni;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.components.web_distiller.core.WebDistillerUrlUtilsJni;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;
import java.util.function.Supplier;

/** Unit tests for {@link CustomTabMinimizationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    BaseSwitches.DISABLE_NATIVE_INITIALIZATION
})
@EnableFeatures(ChromeFeatureList.CCT_REPORT_PRERENDER_EVENTS)
public class CustomTabMinimizationManagerUnitTest {
    @Rule
    public ActivityScenarioRule<CustomTabActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(CustomTabActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TITLE = "Google";
    private static final String HOST = "www.google.com";
    private static final String RTL_POC_URL = "https://xn--mgb.google.com.xn--mgb.yen.comma.sbs/";
    private static final String RTL_POC_PUNYCODE_HOST = "xn--mgb.google.com.xn--mgb.yen.comma.sbs";

    private AppCompatActivity mActivity;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private TabFavicon mTabFavicon;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private MinimizedCustomTabFeatureEngagementDelegate mFeatureEngagementDelegate;
    @Mock private BrowserServicesIntentDataProvider mIntentData;
    @Mock private CustomTabsConnection mConnection;
    @Mock private Runnable mCloseTabRunnable;
    @Mock private WebDistillerUrlUtilsJni mWebDistillerUrlUtilsJni;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock private ResourceFactory.Natives mResourceFactoryNatives;
    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeNatives;
    @Mock private CustomTabMinimizeDelegate.Observer mMinimizationObserver;
    @Mock private CustomTabMinimizeDelegate mOtherMinimizeDelegate;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private Supplier<Bundle> mSavedInstanceStateSupplier;
    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();

    private CustomTabMinimizationManager mManager;

    @Before
    public void setUp() {
        WebDistillerUrlUtilsJni.setInstanceForTesting(mWebDistillerUrlUtilsJni);
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryNatives);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeNatives);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        CustomTabsConnection.setInstanceForTesting(mConnection);
        mActivityTabProvider.setForTesting(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mWebContents.getLastCommittedUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mTab.getTitle()).thenReturn(TITLE);
        mManager =
                new CustomTabMinimizationManager(
                        mActivity,
                        mActivityTabProvider,
                        mFeatureEngagementDelegate,
                        mCloseTabRunnable,
                        mIntentData,
                        mLifecycleDispatcher,
                        mSavedInstanceStateSupplier);
        mManager.addObserver(mMinimizationObserver);
    }

    @After
    public void tearDown() {
        CustomTabMinimizationManager.sLastMinimizeDelegate = null;
    }

    @Test
    public void testMinimize() {
        assertFalse(mActivity.isInPictureInPictureMode());
        mManager.minimize();
        assertTrue(mActivity.isInPictureInPictureMode());
        verify(mFeatureEngagementDelegate).notifyUserEngaged();
        verify(mMinimizationObserver).onMinimizationChanged(true);

        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        verify(mConnection).onMinimized(any());
        verify(mTab).stopLoading();
        verify(mTab).hide(eq(TabHidingType.ACTIVITY_HIDDEN));
        verify(mWebContents).suspendAllMediaPlayers();
        verify(mWebContents).setAudioMuted(eq(true));

        assertEquals(TITLE, ((TextView) mActivity.findViewById(R.id.title)).getText());
        assertEquals(HOST, ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testRestore() {
        mManager.minimize();
        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // Now, simulate Activity exiting PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        verify(mTab).show(eq(FROM_USER));
        verify(mWebContents).setAudioMuted(false);
        verify(mConnection).onUnminimized(any());
        verify(mMinimizationObserver).onMinimizationChanged(false);
    }

    @Test
    public void testDismissByUser() {
        mManager.minimize();
        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // Now, simulate PiP being dismissed by the user.
        mActivityScenarioRule.getScenario().moveToState(State.CREATED);
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        verify(mTab, never()).show(anyInt());
        verify(mCloseTabRunnable).run();
    }

    @Test
    public void testDistilledPage() {
        String distillerUrl = "chrome-distiller://url/";
        // Simulate having a committed distiller URL while tab.getUrl() holds a different pending
        // URL; getOriginalUrlFromDistillerUrl must unwrap the committed distiller URL.
        when(mWebContents.getLastCommittedUrl()).thenReturn(new GURL(distillerUrl));
        when(mTab.getUrl()).thenReturn(new GURL("https://pending.example.com/"));
        when(mWebDistillerUrlUtilsJni.isDistilledPage(distillerUrl)).thenReturn(true);
        when(mWebDistillerUrlUtilsJni.getOriginalUrlFromDistillerUrl(distillerUrl))
                .thenReturn(JUnitTestGURLs.SEARCH_URL);

        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        assertEquals(HOST, ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testAboutBlank() {
        // Simulate having about:blank URL in the tab.
        when(mWebContents.getLastCommittedUrl()).thenReturn(JUnitTestGURLs.ABOUT_BLANK);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.ABOUT_BLANK);
        when(mTab.getTitle()).thenReturn("google.com");

        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        assertEquals(View.GONE, mActivity.findViewById(R.id.title).getVisibility());
        assertEquals(
                JUnitTestGURLs.ABOUT_BLANK.getSpec(),
                ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testAboutBlank_fragment() {
        // Simulate having about:blank#google.com URL in the tab.
        GURL aboutBlankFragment = new GURL("about:blank#google.com");
        when(mWebContents.getLastCommittedUrl()).thenReturn(aboutBlankFragment);
        when(mTab.getUrl()).thenReturn(aboutBlankFragment);
        when(mTab.getTitle()).thenReturn("google.com");

        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        assertEquals(View.GONE, mActivity.findViewById(R.id.title).getVisibility());
        assertEquals(
                JUnitTestGURLs.ABOUT_BLANK.getSpec(),
                ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testDismissByCode() {
        mManager.minimize();
        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // Dismiss using #dismiss().
        mManager.dismiss();
        verify(mTab, never()).show(anyInt());
        verify(mCloseTabRunnable).run();
    }

    @Test
    public void testIsMinimized() {
        // Initially, #isMinimized() should be false.
        assertFalse(mManager.isMinimized());
        mManager.minimize();
        // Now, it should be minimized.
        assertTrue(mManager.isMinimized());
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // It should still be minimized once we actually go in PiP.
        assertTrue(mManager.isMinimized());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testMinimizeWhenThereIsAlreadyPip_S() {
        // Simulate having another Custom Tab in PiP.
        CustomTabMinimizationManager.sLastMinimizeDelegate =
                new WeakReference<>(mOtherMinimizeDelegate);
        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        // This should dismiss the other PiPed Custom Tab.
        verify(mOtherMinimizeDelegate).dismiss();
        // And replace sLastMinimizeDelegate.
        assertEquals(mManager, CustomTabMinimizationManager.sLastMinimizeDelegate.get());

        // Simulate restoring
        mManager.accept(new PictureInPictureModeChangedInfo(false));
        // This should clear the last.
        assertNull(CustomTabMinimizationManager.sLastMinimizeDelegate);
    }

    @Test
    @Config(sdk = VERSION_CODES.TIRAMISU)
    public void testMinimizeWhenThereIsAlreadyPip_T() {
        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        // This shouldn't save the last delegate.
        assertNull(CustomTabMinimizationManager.sLastMinimizeDelegate);

        mManager.accept(new PictureInPictureModeChangedInfo(false));
        // Last delegate should still be null.
        assertNull(CustomTabMinimizationManager.sLastMinimizeDelegate);
    }

    @Test
    public void testMinimizeIsFalseWithoutSavedInstanceState() {
        assertFalse(mManager.isMinimized());
    }

    @Test
    public void testSaveInstanceStateAddsData() {
        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        var outBundle = new Bundle();
        mManager.onSaveInstanceState(outBundle);

        assertTrue(outBundle.getBoolean(KEY_IS_CCT_MINIMIZED));
        assertEquals(TITLE, outBundle.getString(MinimizedCardProperties.TITLE.toString()));
        assertEquals(HOST, outBundle.getString(MinimizedCardProperties.URL.toString()));
    }

    @Test
    public void testInitWithInstanceState() {
        var bundle = new Bundle();
        bundle.putBoolean(KEY_IS_CCT_MINIMIZED, true);
        bundle.putString(MinimizedCardProperties.TITLE.toString(), TITLE);
        bundle.putString(MinimizedCardProperties.URL.toString(), HOST);

        when(mSavedInstanceStateSupplier.get()).thenReturn(bundle);
        var manager =
                new CustomTabMinimizationManager(
                        mActivity,
                        mActivityTabProvider,
                        mFeatureEngagementDelegate,
                        mCloseTabRunnable,
                        mIntentData,
                        mLifecycleDispatcher,
                        mSavedInstanceStateSupplier);

        var captor = ArgumentCaptor.forClass(InflationObserver.class);
        verify(mLifecycleDispatcher).register(captor.capture());
        captor.getValue().onPostInflationStartup();

        assertTrue(manager.isMinimized());
        assertEquals(TITLE, ((TextView) mActivity.findViewById(R.id.title)).getText());
        assertEquals(HOST, ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testMinimize_usesSecurityDisplayForRtlIdnUrl() {
        // Robolectric's host-JVM native library omits ICU UTS #46 xn-- tables (covered end-to-end
        // in MinimizedCustomTabsIntegrationTest); verify delegation to formatUrlForSecurityDisplay.
        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);
        GURL pocUrl = new GURL(RTL_POC_URL);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(
                        pocUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS))
                .thenReturn(RTL_POC_PUNYCODE_HOST);
        when(mWebContents.getLastCommittedUrl()).thenReturn(pocUrl);

        minimizeAndEnterPip();

        verify(mUrlFormatterJniMock)
                .formatUrlForSecurityDisplay(eq(pocUrl), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS));
        verify(mUrlFormatterJniMock, never())
                .formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(any());
        assertEquals(RTL_POC_PUNYCODE_HOST, getDisplayedUrl());
    }

    @Test
    public void testMinimize_pendingNavigationUsesCommittedUrlAfterStopLoading() {
        when(mWebContents.getLastCommittedUrl()).thenReturn(new GURL("https://comma.sbs/page"));
        when(mTab.getUrl()).thenReturn(new GURL("https://www.google.com/"));

        minimizeAndEnterPip();

        InOrder order = inOrder(mTab, mWebContents);
        order.verify(mTab).stopLoading();
        order.verify(mWebContents).getLastCommittedUrl();
        assertEquals("comma.sbs", getDisplayedUrl());
    }

    @Test
    public void testMinimize_blobAndFilesystemUseCommittedOrigin() {
        Origin creatorOrigin = Origin.create(new GURL("https://www.google.com.long.comma.sbs"));
        when(mRenderFrameHost.getLastCommittedOrigin()).thenReturn(creatorOrigin);

        when(mWebContents.getLastCommittedUrl())
                .thenReturn(
                        new GURL(
                                "blob:https://www.google.com.long.comma.sbs/1234#%20%20www.google.com"));
        minimizeAndEnterPip();
        assertEquals("www.google.com.long.comma.sbs", getDisplayedUrl());
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        when(mWebContents.getLastCommittedUrl())
                .thenReturn(
                        new GURL(
                                "filesystem:https://www.google.com.long.comma.sbs/temporary/doc.pdf#www.google.com"));
        minimizeAndEnterPip();
        assertEquals("www.google.com.long.comma.sbs", getDisplayedUrl());
    }

    @Test
    public void testMinimize_opaqueBlobNormalizesToAboutBlankAndSuppressesFavicon() {
        Bitmap pageFavicon = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        TabFavicon.setInstanceForTesting(mTabFavicon);
        when(mTabFavicon.getFavicon(false)).thenReturn(pageFavicon);

        minimizeAndEnterPip();
        ImageView webFaviconView = mActivity.findViewById(R.id.favicon);
        assertEquals(
                pageFavicon, ((RoundedBitmapDrawable) webFaviconView.getDrawable()).getBitmap());
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        when(mWebContents.getLastCommittedUrl())
                .thenReturn(new GURL("blob:null/1234#www.google.com"));
        when(mRenderFrameHost.getLastCommittedOrigin()).thenReturn(Origin.createOpaqueOrigin());
        when(mTab.getTitle()).thenReturn("Google");

        minimizeAndEnterPip();

        assertEquals(View.GONE, mActivity.findViewById(R.id.title).getVisibility());
        assertEquals(JUnitTestGURLs.ABOUT_BLANK.getSpec(), getDisplayedUrl());
        ImageView faviconView = mActivity.findViewById(R.id.favicon);
        assertFalse(faviconView.getDrawable() instanceof RoundedBitmapDrawable);
    }

    @Test
    public void testMinimize_unallowlistedSchemesNormalizeToAboutBlank() {
        assertNormalizesToAboutBlank(
                new GURL("data:text/html,<title>Google</title>#www.google.com"));
        assertNormalizesToAboutBlank(new GURL("about://blank#www.google.com"));
        assertNormalizesToAboutBlank(
                new GURL("content://com.example.provider/doc/1?q=1#www.google.com"));
        assertNormalizesToAboutBlank(GURL.emptyGURL());
        assertNormalizesToAboutBlank(JUnitTestGURLs.INVALID_URL);
    }

    @Test
    public void testMinimize_fileAndChromeSchemesOmitPathAndFragment() {
        when(mWebContents.getLastCommittedUrl())
                .thenReturn(new GURL("file:///sdcard/Download/report.pdf?q=1#www.google.com"));
        minimizeAndEnterPip();
        assertEquals("file://", getDisplayedUrl());
        assertEquals(View.VISIBLE, mActivity.findViewById(R.id.title).getVisibility());
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        when(mWebContents.getLastCommittedUrl())
                .thenReturn(new GURL("chrome://version/path?q=1#www.google.com"));
        minimizeAndEnterPip();
        assertEquals("chrome://version", getDisplayedUrl());
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        when(mWebContents.getLastCommittedUrl())
                .thenReturn(
                        new GURL("chrome-native://pdf/link?url=https://comma.sbs#www.google.com"));
        minimizeAndEnterPip();
        assertEquals("chrome-native://pdf", getDisplayedUrl());
    }

    @Test
    public void testSavedInstanceState_clearsStaleModelAndNormalizesMissingUrl() {
        minimizeAndEnterPip();
        mManager.accept(new PictureInPictureModeChangedInfo(false));
        mManager.minimize();

        var outBundle = new Bundle();
        mManager.onSaveInstanceState(outBundle);
        assertTrue(outBundle.getBoolean(KEY_IS_CCT_MINIMIZED));
        assertNull(outBundle.getString(MinimizedCardProperties.URL.toString()));

        when(mSavedInstanceStateSupplier.get()).thenReturn(outBundle);
        new CustomTabMinimizationManager(
                mActivity,
                mActivityTabProvider,
                mFeatureEngagementDelegate,
                mCloseTabRunnable,
                mIntentData,
                mLifecycleDispatcher,
                mSavedInstanceStateSupplier);
        var captor = ArgumentCaptor.forClass(InflationObserver.class);
        verify(mLifecycleDispatcher).register(captor.capture());
        captor.getValue().onPostInflationStartup();

        assertEquals(View.GONE, mActivity.findViewById(R.id.title).getVisibility());
        assertEquals(JUnitTestGURLs.ABOUT_BLANK.getSpec(), getDisplayedUrl());
    }

    @Test
    public void testRestore_withNullTabStillDismissesCard() {
        var bundle = new Bundle();
        bundle.putBoolean(KEY_IS_CCT_MINIMIZED, true);
        bundle.putString(MinimizedCardProperties.TITLE.toString(), TITLE);
        bundle.putString(MinimizedCardProperties.URL.toString(), HOST);
        when(mSavedInstanceStateSupplier.get()).thenReturn(bundle);

        var restoredManager =
                new CustomTabMinimizationManager(
                        mActivity,
                        mActivityTabProvider,
                        mFeatureEngagementDelegate,
                        mCloseTabRunnable,
                        mIntentData,
                        mLifecycleDispatcher,
                        mSavedInstanceStateSupplier);
        var captor = ArgumentCaptor.forClass(InflationObserver.class);
        verify(mLifecycleDispatcher).register(captor.capture());
        captor.getValue().onPostInflationStartup();
        assertEquals(View.VISIBLE, mActivity.findViewById(R.id.card).getVisibility());

        mActivityTabProvider.setForTesting(null);
        restoredManager.accept(new PictureInPictureModeChangedInfo(false));

        assertNull(mActivity.findViewById(R.id.card));
    }

    @Test
    public void testMinimize_nullWebContentsFallsBackToTabUrl() {
        when(mTab.getWebContents()).thenReturn(null);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        minimizeAndEnterPip();
        assertEquals(HOST, getDisplayedUrl());
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        when(mTab.getUrl()).thenReturn(new GURL("blob:https://comma.sbs/1234"));
        minimizeAndEnterPip();
        assertEquals(JUnitTestGURLs.ABOUT_BLANK.getSpec(), getDisplayedUrl());
    }

    private void assertNormalizesToAboutBlank(GURL url) {
        when(mWebContents.getLastCommittedUrl()).thenReturn(url);
        when(mTab.getTitle()).thenReturn("Google");

        minimizeAndEnterPip();

        assertEquals(View.GONE, mActivity.findViewById(R.id.title).getVisibility());
        assertEquals(JUnitTestGURLs.ABOUT_BLANK.getSpec(), getDisplayedUrl());
        mManager.accept(new PictureInPictureModeChangedInfo(false));
    }

    private void minimizeAndEnterPip() {
        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));
    }

    private String getDisplayedUrl() {
        return ((TextView) mActivity.findViewById(R.id.url)).getText().toString();
    }
}
