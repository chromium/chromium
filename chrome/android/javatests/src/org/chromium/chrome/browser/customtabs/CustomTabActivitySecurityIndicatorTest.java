// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.hamcrest.Matchers.is;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * Instrumentation tests for the security indicator in the toolbar of a {@link CustomTabActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabActivitySecurityIndicatorTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        Context appContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(appContext, ServerCertificate.CERT_OK);
        mTestPage = mTestServer.getURL("/chrome/test/data/android/google.html");
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS})
    public void testCustomTabSecurityIndicators() throws Exception {
        Context context =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Check that tab has loaded the expected URL.
        CriteriaHelper.pollUiThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
                });

        // Test that the security indicator is the lock icon.
        final int expectedSecurityIcon = R.drawable.omnibox_https_valid;
        ImageView securityButton =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.security_button);
        Assert.assertEquals(View.VISIBLE, securityButton.getVisibility());

        ColorStateList colorStateList =
                AppCompatResources.getColorStateList(
                        ApplicationProvider.getApplicationContext(),
                        R.color.default_icon_color_light_tint_list);
        ImageView expectedSecurityButton =
                new ImageView(ApplicationProvider.getApplicationContext());
        expectedSecurityButton.setImageResource(expectedSecurityIcon);
        ImageViewCompat.setImageTintList(expectedSecurityButton, colorStateList);

        BitmapDrawable expectedDrawable = (BitmapDrawable) expectedSecurityButton.getDrawable();
        BitmapDrawable actualDrawable = (BitmapDrawable) securityButton.getDrawable();
        Assert.assertTrue(expectedDrawable.getBitmap().sameAs(actualDrawable.getBitmap()));
    }

    // Custom tabs should use the new security indicators.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS})
    public void testCustomTabSecurityIndicator_UpdateEnabled() throws Exception {
        Context context =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Check that tab has loaded the expected URL.
        CriteriaHelper.pollUiThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
                });

        // Test that the security indicator is the tune icon.
        ImageView securityButton =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.security_button);
        Assert.assertEquals(View.VISIBLE, securityButton.getVisibility());

        CustomTabToolbar toolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        CustomTabLocationBar locationBar = (CustomTabLocationBar) toolbar.getLocationBar();
        Assert.assertEquals(locationBar.getSecurityIconResourceForTesting(),
                            R.drawable.omnibox_https_valid_refresh);
    }
}
