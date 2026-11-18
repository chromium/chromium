// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_load_metrics;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.page_load_metrics.AppIntentMetricsHelper.AndroidExternalAppIntentCategory;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AppIntentMetricsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppIntentMetricsHelperTest {
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Tab mTab;

    private Activity mActivity;
    private WeakReference<Activity> mActivityRef;
    private UserDataHost mUserDataHost;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(ApplicationProvider.getApplicationContext());
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivityRef = new WeakReference<>(mActivity);
        mUserDataHost = new UserDataHost();
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
    }

    @Test
    public void testCameraApps() {
        for (String pkg :
                new String[] {
                    "com.google.android.GoogleCamera",
                    "com.sec.android.app.camera",
                    "com.oplus.camera",
                    "com.motorola.camera3",
                    "net.sourceforge.opencamera"
                }) {
            TabAssociatedApp.from(mTab).setAppId(pkg);
            Assert.assertEquals(
                    pkg,
                    AndroidExternalAppIntentCategory.CAMERA,
                    AppIntentMetricsHelper.getAppIntentCategory(
                            mWindowAndroid, mTab, /* isFirstNavigation= */ true));
        }
    }

    @Test
    public void testCustomTabAndChooserReferrerFallback() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(
                IntentHandler.EXTRA_ACTIVITY_REFERRER, "android-app://com.android.intentresolver");
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse("android-app://com.google.android.gm"));
        CustomTabActivity customTabActivity =
                Robolectric.buildActivity(CustomTabActivity.class, intent).get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(customTabActivity));

        Assert.assertEquals(
                AndroidExternalAppIntentCategory.EMAIL,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ true));
    }

    @Test
    public void testTabbedActivityReferrerWithUnknownAppId() {
        TabAssociatedApp.from(mTab).setAppId("com.google.android.apps.chrome.unknown_app");
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, "android-app://com.oplus.camera");
        RedirectHandlerTabHelper.updateIntentInTab(mTab, intent, /* isCustomTab= */ false);

        Assert.assertEquals(
                AndroidExternalAppIntentCategory.CAMERA,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ true));
    }

    @Test
    public void testFallsThroughWhenEarlierPackageHasNoCategory() {
        // The tab's app ID and the intent's application ID both name an app outside the five
        // categories, so the later referrer signal decides.
        TabAssociatedApp.from(mTab).setAppId("com.example.unrelated");
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.example.unrelated");
        intent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, "android-app://com.oplus.camera");
        RedirectHandlerTabHelper.updateIntentInTab(mTab, intent, /* isCustomTab= */ false);

        Assert.assertEquals(
                AndroidExternalAppIntentCategory.CAMERA,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ true));
    }

    @Test
    public void testEmailNavigationGalleryAndMessagingApps() {
        Object[][] cases = {
            {"com.google.android.gm", AndroidExternalAppIntentCategory.EMAIL},
            {"com.microsoft.office.outlook", AndroidExternalAppIntentCategory.EMAIL},
            {"com.yahoo.mobile.client.android.mail", AndroidExternalAppIntentCategory.EMAIL},
            {"com.samsung.android.email.provider", AndroidExternalAppIntentCategory.EMAIL},
            {"com.google.android.apps.maps", AndroidExternalAppIntentCategory.NAVIGATION},
            {"com.waze", AndroidExternalAppIntentCategory.NAVIGATION},
            {"net.daum.android.map", AndroidExternalAppIntentCategory.NAVIGATION},
            {"com.google.android.apps.photos", AndroidExternalAppIntentCategory.GALLERY},
            {"com.sec.android.gallery3d", AndroidExternalAppIntentCategory.GALLERY},
            {"com.miui.gallery", AndroidExternalAppIntentCategory.GALLERY},
            {"com.whatsapp", AndroidExternalAppIntentCategory.MESSAGING},
            {"com.google.android.apps.messaging", AndroidExternalAppIntentCategory.MESSAGING},
            {"com.samsung.android.messaging", AndroidExternalAppIntentCategory.MESSAGING},
            {"org.telegram.messenger", AndroidExternalAppIntentCategory.MESSAGING},
            {"jp.naver.line.android", AndroidExternalAppIntentCategory.MESSAGING},
            {"com.Slack", AndroidExternalAppIntentCategory.MESSAGING},
        };
        for (Object[] entry : cases) {
            String pkg = (String) entry[0];
            int expected = (Integer) entry[1];
            TabAssociatedApp.from(mTab).setAppId(pkg);
            Assert.assertEquals(
                    pkg,
                    expected,
                    AppIntentMetricsHelper.getAppIntentCategory(
                            mWindowAndroid, mTab, /* isFirstNavigation= */ true));
        }
    }

    @Test
    public void testSubstringAndWebUrlFalsePositives_Ignored() {
        for (String nonTargetPkg :
                new String[] {
                    "com.example.unrelated",
                    "com.naviapp",
                    "com.naver.linewebtoon",
                    "com.navyfederal.android",
                    "com.dailymail.online",
                    "ru.mail.cloud",
                    "com.youmail.android.vvm",
                    "com.camerasideas.instashot",
                    "com.ants360.yicamera",
                    "com.kvadgroup.photostudio",
                    "com.google.android.apps.camera.services"
                }) {
            TabAssociatedApp.from(mTab).setAppId(nonTargetPkg);
            Assert.assertEquals(
                    nonTargetPkg,
                    AndroidExternalAppIntentCategory.NONE,
                    AppIntentMetricsHelper.getAppIntentCategory(
                            mWindowAndroid, mTab, /* isFirstNavigation= */ true));
        }

        Intent webRefIntent = new Intent(Intent.ACTION_VIEW);
        webRefIntent.putExtra(
                IntentHandler.EXTRA_ACTIVITY_REFERRER, "https://www.example.com/deals/camera");
        CustomTabActivity customTabActivity =
                Robolectric.buildActivity(CustomTabActivity.class, webRefIntent).get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(customTabActivity));
        Assert.assertEquals(
                AndroidExternalAppIntentCategory.NONE,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ true));
    }

    @Test
    public void testInternalSubsequentNullAndDestroyedTabs_Ignored() {
        TabAssociatedApp.from(mTab).setAppId("com.google.android.gm");
        Assert.assertEquals(
                AndroidExternalAppIntentCategory.NONE,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ false));

        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_CHROME_UI);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.google.android.gm");
        mActivity.setIntent(intent);
        Assert.assertEquals(
                AndroidExternalAppIntentCategory.NONE,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ true));

        Assert.assertEquals(
                AndroidExternalAppIntentCategory.NONE,
                AppIntentMetricsHelper.getAppIntentCategory(
                        null, null, /* isFirstNavigation= */ true));

        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        when(mTab.isDestroyed()).thenReturn(true);
        Assert.assertEquals(
                AndroidExternalAppIntentCategory.NONE,
                AppIntentMetricsHelper.getAppIntentCategory(
                        mWindowAndroid, mTab, /* isFirstNavigation= */ true));
    }
}
