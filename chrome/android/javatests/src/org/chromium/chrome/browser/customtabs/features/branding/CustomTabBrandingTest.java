// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Integration test for CCT Branding. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
public class CustomTabBrandingTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    // Keep consistent with the key in SharedPreferencesBrandingTimeStorage.
    private static final String BRANDING_SHARED_PREF_KEY = "pref_cct_brand_show_time";
    private static final String TOOLBAR_BRANDING_RENDER_IMAGE_ID = "cct_branding_toolbar";

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_CUSTOM_TABS)
                    .build();

    @Rule
    public IncognitoCustomTabActivityTestRule mCctActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @ClassRule
    public static EmbeddedTestServerRule sEmbeddedTestServerRule = new EmbeddedTestServerRule();

    private SharedPreferences mBrandingSharedPref;
    private Intent mIntent;

    @Before
    public void setup() {
        Context appContext = ContextUtils.getApplicationContext();
        mBrandingSharedPref =
                appContext.getSharedPreferences(BRANDING_SHARED_PREF_KEY, Context.MODE_PRIVATE);
        String url = sEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        mIntent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), url);

        // Set the referrer so branding controller can identify the client app.
        mIntent.putExtra(
                Intent.EXTRA_REFERRER,
                new Uri.Builder()
                        .scheme(UrlConstants.APP_INTENT_SCHEME)
                        .authority(appContext.getPackageName())
                        .build());
    }

    @After
    public void tearDown() {
        mBrandingSharedPref.edit().clear().apply();
    }

    @Test
    @SmallTest
    public void showToastBrandingAndStoreBrandingInfo() {
        // TODO(wenyufu): Verify the toast is shown on screen.
        mCctActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        Assert.assertEquals(
                "Branding launch time should get recorded.",
                1,
                mBrandingSharedPref.getAll().size());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderToolbarBranding() throws IOException {
        markBrandingLaunchedForPackage(ContextUtils.getApplicationContext().getPackageName());
        mCctActivityTestRule.startCustomTabActivityWithIntent(mIntent);

        CustomTabToolbar toolbar = mCctActivityTestRule.getActivity().findViewById(R.id.toolbar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> toolbar.getBrandingDelegate().showBrandingLocationBar());
        mRenderTestRule.render(toolbar, TOOLBAR_BRANDING_RENDER_IMAGE_ID);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY)
    public void doesntStoreBrandingInfoForIncognito() {
        mIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        mCctActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        Assert.assertEquals(
                "Branding info should not be stored in Incognito mode.",
                0,
                mBrandingSharedPref.getAll().size());
    }

    /**
     * Populate a random value for package in branding storage, as if the branding has launched
     * before for the given client app.
     */
    private void markBrandingLaunchedForPackage(String packageName) {
        SharedPreferencesBrandingTimeStorage.getInstance().put(packageName, 1L);
    }
}
