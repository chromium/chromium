// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentSender;
import android.content.IntentSender.SendIntentException;
import android.net.Uri;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.share.ShareHelper.TargetChosenReceiver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for static functions in {@link ShareHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPendingIntent.class})
@Features.DisableFeatures(ChromeFeatureList.CHROME_SHARING_HUB_LAUNCH_ADJACENT)
public class ShareHelperUnitTest {
    private static final String IMAGE_URI = "file://path/to/image.png";
    private static final ComponentName TEST_COMPONENT_NAME_1 =
            new ComponentName("test.package.one", "test.class.name.one");
    private static final ComponentName TEST_COMPONENT_NAME_2 =
            new ComponentName("test.package.two", "test.class.name.two");

    private WindowAndroid mWindow;
    private Activity mActivity;
    private Uri mImageUri;

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mWindow = new ActivityWindowAndroid(
                mActivity, false, IntentRequestTracker.createFromActivity(mActivity));
        mImageUri = Uri.parse(IMAGE_URI);
    }

    @After
    public void tearDown() {
        TargetChosenReceiver.resetForTesting();
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME);
        mWindow.destroy();
        mActivity.finish();
    }

    @Test
    public void shareImageWithChooser() throws SendIntentException {
        ShareHelper.shareImage(mWindow, null, null, mImageUri, JUnitTestGURLs.BLUE_1);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());

        // Verify sharing intent has the right image.
        Intent sharingIntent = nextIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, sharingIntent.getAction());
        assertEquals("Text URL not set correctly.", JUnitTestGURLs.BLUE_1,
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));
        assertEquals("Image URI not set correctly.", mImageUri,
                sharingIntent.getParcelableExtra(Intent.EXTRA_STREAM));
        assertNotNull("Shared image does not have preview set.", sharingIntent.getClipData());

        // Last shared component not recorded before chooser is finished.
        assertLastComponentNameRecorded(null);

        // Fire back a chosen intent, the selected target should be recorded.
        selectComponentFromChooserIntent(nextIntent, TEST_COMPONENT_NAME_1);
        assertLastComponentNameRecorded(TEST_COMPONENT_NAME_1);
    }

    @Test
    public void shareImageWithComponentName() {
        ShareHelper.shareImage(
                mWindow, null, TEST_COMPONENT_NAME_1, mImageUri, JUnitTestGURLs.BLUE_1);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Next fired intent should be a SEND intent when direct sharing with component.",
                Intent.ACTION_SEND, nextIntent.getAction());
    }

    @Test
    public void shareWithChooser() throws SendIntentException {
        ShareParams params = new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ShareHelper.shareWithSystemShareSheetUi(params, null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());

        // Verify the intent has the right Url.
        Intent sharingIntent = nextIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, sharingIntent.getAction());
        assertEquals("Text URL not set correctly.", JUnitTestGURLs.EXAMPLE_URL,
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));

        // Fire back a chosen intent, the selected target should be recorded.
        selectComponentFromChooserIntent(nextIntent, TEST_COMPONENT_NAME_1);
        assertLastComponentNameRecorded(TEST_COMPONENT_NAME_1);
    }

    @Test
    public void shareDirectly() {
        ShareParams params = new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ShareHelper.shareDirectly(params, TEST_COMPONENT_NAME_1, null, false);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, nextIntent.getAction());
        assertEquals("Intent component name does not match.", TEST_COMPONENT_NAME_1,
                nextIntent.getComponent());
        assertEquals("Text URL not set correctly.", JUnitTestGURLs.EXAMPLE_URL,
                nextIntent.getStringExtra(Intent.EXTRA_TEXT));

        assertLastComponentNameRecorded(null);
    }

    @Test
    public void shareDirectlyAndSaveLastUsed() {
        // Set a last shared component and verify direct share overwrite such.
        ShareHelper.setLastShareComponentName(null, TEST_COMPONENT_NAME_1);
        ShareParams params = new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ShareHelper.shareDirectly(params, TEST_COMPONENT_NAME_2, null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, nextIntent.getAction());
        assertEquals("Intent component name does not match.", TEST_COMPONENT_NAME_2,
                nextIntent.getComponent());
        assertEquals("Text URL not set correctly.", JUnitTestGURLs.EXAMPLE_URL,
                nextIntent.getStringExtra(Intent.EXTRA_TEXT));

        assertLastComponentNameRecorded(TEST_COMPONENT_NAME_2);
    }

    @Test
    public void shareWithLastSharedComponent() {
        ShareHelper.setLastShareComponentName(null, TEST_COMPONENT_NAME_1);
        ShareHelper.shareWithLastUsedComponent(emptyShareParams());

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, nextIntent.getAction());
        assertEquals("Intent component name does not match.", TEST_COMPONENT_NAME_1,
                nextIntent.getComponent());
    }

    private void selectComponentFromChooserIntent(Intent chooserIntent, ComponentName componentName)
            throws SendIntentException {
        Intent sendBackIntent = new Intent().putExtra(Intent.EXTRA_CHOSEN_COMPONENT, componentName);
        IntentSender sender =
                chooserIntent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT_INTENT_SENDER);
        sender.sendIntent(ContextUtils.getApplicationContext(), Activity.RESULT_OK, sendBackIntent,
                null, null);
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    private void assertLastComponentNameRecorded(ComponentName name) {
        assertThat("Last shared component name not match.", ShareHelper.getLastShareComponentName(),
                Matchers.is(name));
    }

    private ShareParams emptyShareParams() {
        return new ShareParams.Builder(mWindow, "", "").build();
    }
}
