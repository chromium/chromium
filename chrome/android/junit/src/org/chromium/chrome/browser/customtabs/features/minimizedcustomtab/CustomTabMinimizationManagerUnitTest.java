// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManager.KEY_CCT_MINIMIZATION_SYSTEM_TIME;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManager.KEY_IS_CCT_MINIMIZED;
import static org.chromium.chrome.browser.tab.TabLoadIfNeededCaller.ON_ACTIVITY_SHOWN_THEN_SHOW;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.app.PictureInPictureParams;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.PictureInPictureModeChangedInfo;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;

/** Unit tests for {@link CustomTabMinimizationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class CustomTabMinimizationManagerUnitTest {
    @Rule
    public ActivityScenarioRule<CustomTabActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(CustomTabActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final String TITLE = "Google";
    private static final String HOST = JUnitTestGURLs.SEARCH_URL.getHost();

    @Spy private AppCompatActivity mActivity;
    @Mock private ActivityTabProvider mTabProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private MinimizedCustomTabFeatureEngagementDelegate mFeatureEngagementDelegate;
    @Mock private BrowserServicesIntentDataProvider mIntentData;
    @Mock private CustomTabsConnection mConnection;
    @Mock private Runnable mCloseTabRunnable;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private CustomTabMinimizeDelegate.Observer mMinimizationObserver;
    @Mock private CustomTabMinimizeDelegate mOtherMinimizeDelegate;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private Supplier<Bundle> mSavedInstanceStateSupplier;

    private CustomTabMinimizationManager mManager;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = spy(activity));
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);

        CustomTabsConnection.setInstanceForTesting(mConnection);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mTab.getTitle()).thenReturn(TITLE);
        when(mTabProvider.hasValue()).thenReturn(true);
        when(mTabProvider.get()).thenReturn(mTab);
        when(mActivity.enterPictureInPictureMode(any(PictureInPictureParams.class)))
                .thenReturn(true);
        mManager =
                new CustomTabMinimizationManager(
                        mActivity,
                        mTabProvider,
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
        var minimizationEventsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.MinimizedEvents",
                        CustomTabMinimizationManager.MinimizationEvents.MINIMIZE);
        mManager.minimize();
        verify(mActivity).enterPictureInPictureMode(any(PictureInPictureParams.class));
        verify(mFeatureEngagementDelegate).notifyUserEngaged();
        verify(mMinimizationObserver).onMinimizationChanged(true);

        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        verify(mConnection).onMinimized(any());
        verify(mTab).stopLoading();
        verify(mTab).hide(eq(TabHidingType.ACTIVITY_HIDDEN));
        verify(mWebContents).suspendAllMediaPlayers();
        verify(mWebContents).setAudioMuted(eq(true));

        minimizationEventsWatcher.assertExpected(
                "CustomTabs.MinimizedEvents.MINIMIZE should be recorded once");

        assertEquals(TITLE, ((TextView) mActivity.findViewById(R.id.title)).getText());
        assertEquals(HOST, ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testRestore() {
        mManager.minimize();
        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // Now, simulate Activity exiting PiP.
        var minimizationEventsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.MinimizedEvents",
                        CustomTabMinimizationManager.MinimizationEvents.MAXIMIZE);
        var timeElapsedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.TimeElapsedSinceMinimized.Maximized");
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        verify(mTab).show(eq(FROM_USER), eq(ON_ACTIVITY_SHOWN_THEN_SHOW));
        verify(mWebContents).setAudioMuted(false);
        verify(mConnection).onUnminimized(any());
        verify(mMinimizationObserver).onMinimizationChanged(false);
        minimizationEventsWatcher.assertExpected(
                "CustomTabs.MinimizedEvents.MAXIMIZE should be recorded once");
        timeElapsedWatcher.assertExpected(
                "CustomTabs.TimeElapsedSinceMinimized.Maximized should be recorded once");
    }

    @Test
    public void testDismissByUser() {
        mManager.minimize();
        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // Now, simulate PiP being dismissed by the user.
        var minimizationEventsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.MinimizedEvents",
                        CustomTabMinimizationManager.MinimizationEvents.DESTROY);
        var timeElapsedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.TimeElapsedSinceMinimized.Destroyed");
        mActivityScenarioRule.getScenario().moveToState(State.CREATED);
        mManager.accept(new PictureInPictureModeChangedInfo(false));

        verify(mTab, never()).show(anyInt(), anyInt());
        verify(mCloseTabRunnable).run();

        minimizationEventsWatcher.assertExpected(
                "CustomTabs.MinimizedEvents.DESTROY should be recorded once");
        timeElapsedWatcher.assertExpected(
                "CustomTabs.TimeElapsedSinceMinimized.Destroyed should be recorded once");
    }

    @Test
    public void testDistilledPage() {
        String distillerUrl = "chrome-distiller://url/";
        // Simulate having a distiller URL in the tab.
        when(mTab.getUrl()).thenReturn(new GURL(distillerUrl));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(distillerUrl)).thenReturn(true);
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        mManager.minimize();
        mManager.accept(new PictureInPictureModeChangedInfo(true));

        assertEquals(HOST, ((TextView) mActivity.findViewById(R.id.url)).getText());
    }

    @Test
    public void testAboutBlank() {
        // Simulate having about:blank URL in the tab.
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
    public void testDismissByCode() {
        mManager.minimize();
        // Simulate Activity entering PiP.
        mManager.accept(new PictureInPictureModeChangedInfo(true));
        // Dismiss using #dismiss().
        mManager.dismiss();
        verify(mTab, never()).show(anyInt(), anyInt());
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
        assertThat(outBundle.getLong(KEY_CCT_MINIMIZATION_SYSTEM_TIME), greaterThan(0L));
        assertEquals(TITLE, outBundle.getString(MinimizedCardProperties.TITLE.toString()));
        assertEquals(HOST, outBundle.getString(MinimizedCardProperties.URL.toString()));
    }

    @Test
    public void testInitWithInstanceState() {
        var bundle = new Bundle();
        bundle.putBoolean(KEY_IS_CCT_MINIMIZED, true);
        bundle.putLong(KEY_CCT_MINIMIZATION_SYSTEM_TIME, 999999L);
        bundle.putString(MinimizedCardProperties.TITLE.toString(), TITLE);
        bundle.putString(MinimizedCardProperties.URL.toString(), HOST);

        when(mSavedInstanceStateSupplier.hasValue()).thenReturn(true);
        when(mSavedInstanceStateSupplier.get()).thenReturn(bundle);
        var manager =
                new CustomTabMinimizationManager(
                        mActivity,
                        mTabProvider,
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
}
