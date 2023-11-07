// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentSender;
import android.content.IntentSender.SendIntentException;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.os.Looper;
import android.os.Parcelable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.share.ShareHelper.TargetChosenReceiver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for static functions in {@link ShareHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPendingIntent.class})
public class ShareHelperUnitTest {
    private static final String INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS =
            "android.intent.extra.CHOOSER_CUSTOM_ACTIONS";
    private static final String KEY_CHOOSER_ACTION_ICON = "icon";
    private static final String KEY_CHOOSER_ACTION_NAME = "name";
    private static final String KEY_CHOOSER_ACTION_ACTION = "action";
    private static final String IMAGE_URI = "file://path/to/image.png";
    private static final ComponentName TEST_COMPONENT_NAME_1 =
            new ComponentName("test.package.one", "test.class.name.one");
    private static final ComponentName TEST_COMPONENT_NAME_2 =
            new ComponentName("test.package.two", "test.class.name.two");

    private WindowAndroid mWindow;
    private Activity mActivity;
    private Uri mImageUri;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mWindow =
                new ActivityWindowAndroid(
                        mActivity, false, IntentRequestTracker.createFromActivity(mActivity));
        mImageUri = Uri.parse(IMAGE_URI);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME);
        mWindow.destroy();
        mActivity.finish();
    }

    @Test
    public void shareImageWithChooser() throws SendIntentException {
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.BLUE_1.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(mImageUri)
                        .build();
        ShareHelper.shareWithSystemShareSheetUi(params, null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());

        // Verify sharing intent has the right image.
        Intent sharingIntent = nextIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, sharingIntent.getAction());
        assertEquals(
                "Text URL not set correctly.",
                JUnitTestGURLs.BLUE_1.getSpec(),
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));
        assertEquals(
                "Image URI not set correctly.",
                mImageUri,
                sharingIntent.getParcelableExtra(Intent.EXTRA_STREAM));
        assertNotNull("Shared image does not have preview set.", sharingIntent.getClipData());

        // Last shared component not recorded before chooser is finished.
        assertLastComponentNameRecorded(null);

        // Fire back a chosen intent, the selected target should be recorded.
        selectComponentFromChooserIntent(nextIntent, TEST_COMPONENT_NAME_1);
        assertLastComponentNameRecorded(TEST_COMPONENT_NAME_1);
    }

    @Test
    public void shareImageDirectly() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.BLUE_1.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(mImageUri)
                        .build();
        ShareHelper.shareDirectly(params, TEST_COMPONENT_NAME_1, null, false);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Next fired intent should be a SEND intent when direct sharing with component.",
                Intent.ACTION_SEND,
                nextIntent.getAction());
    }

    @Test
    public void shareWithChooser() throws SendIntentException {
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
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
        assertEquals(
                "Text URL not set correctly.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));

        // Fire back a chosen intent, the selected target should be recorded.
        selectComponentFromChooserIntent(nextIntent, TEST_COMPONENT_NAME_1);
        assertLastComponentNameRecorded(TEST_COMPONENT_NAME_1);
    }

    @Test
    public void shareDirectly() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ShareHelper.shareDirectly(params, TEST_COMPONENT_NAME_1, null, false);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, nextIntent.getAction());
        assertEquals(
                "Intent component name does not match.",
                TEST_COMPONENT_NAME_1,
                nextIntent.getComponent());
        assertEquals(
                "Text URL not set correctly.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                nextIntent.getStringExtra(Intent.EXTRA_TEXT));

        assertLastComponentNameRecorded(null);
    }

    @Test
    public void shareDirectlyAndSaveLastUsed() {
        // Set a last shared component and verify direct share overwrite such.
        ShareHelper.setLastShareComponentName(null, TEST_COMPONENT_NAME_1);
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ShareHelper.shareDirectly(params, TEST_COMPONENT_NAME_2, null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, nextIntent.getAction());
        assertEquals(
                "Intent component name does not match.",
                TEST_COMPONENT_NAME_2,
                nextIntent.getComponent());
        assertEquals(
                "Text URL not set correctly.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
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
        assertEquals(
                "Intent component name does not match.",
                TEST_COMPONENT_NAME_1,
                nextIntent.getComponent());
    }

    @Test
    public void doNotTrustIntentWithoutTrustedExtra() throws CanceledException {
        ShareHelper.shareWithSystemShareSheetUi(emptyShareParams(), null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);

        String packageName = ContextUtils.getApplicationContext().getPackageName();
        Intent untrustedIntent = new Intent();
        untrustedIntent.setPackage(packageName);
        untrustedIntent.setAction(
                packageName
                        + "/"
                        + TargetChosenReceiver.class.getName()
                        + mActivity.getTaskId()
                        + "_ACTION");
        untrustedIntent.putExtra(Intent.EXTRA_CHOSEN_COMPONENT, TEST_COMPONENT_NAME_2);

        PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        0,
                        untrustedIntent,
                        PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_CANCEL_CURRENT)
                .send();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertLastComponentNameRecorded(null);

        Intent trustedIntent = new Intent(untrustedIntent);
        IntentUtils.addTrustedIntentExtras(trustedIntent);
        PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        1,
                        trustedIntent,
                        PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_CANCEL_CURRENT)
                .send();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertLastComponentNameRecorded(TEST_COMPONENT_NAME_2);
    }

    @Test
    @Config(shadows = {ShadowChooserActionHelper.class})
    public void shareWithCustomActions() throws SendIntentException {
        String actionKey = "key";
        CallbackHelper callbackHelper = new CallbackHelper();
        ChromeCustomShareAction.Provider provider =
                new SingleCustomActionProvider(actionKey, callbackHelper);

        ShareHelper.shareWithSystemShareSheetUi(emptyShareParams(), null, false, provider);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());
        assertNotNull(
                "Custom actions are not attached.",
                nextIntent.getParcelableArrayExtra("android.intent.extra.CHOOSER_CUSTOM_ACTIONS"));

        selectCustomActionFromChooserIntent(nextIntent, actionKey);
        assertEquals("Custom action callback not called.", 1, callbackHelper.getCallCount());
    }

    @Test
    public void shareWithPreviewUri() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setPreviewImageUri(mImageUri)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ShareHelper.shareWithSystemShareSheetUi(params, null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());

        // Verify the intent has the right preview Uri.
        Intent sharingIntent = nextIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, sharingIntent.getAction());
        assertEquals(
                "Preview image Uri not set correctly.",
                mImageUri,
                sharingIntent.getClipData().getItemAt(0).getUri());
    }

    @Test
    public void shareMultipleImage() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setFileUris(new ArrayList<>(List.of(mImageUri, mImageUri)))
                        .setFileContentType("image/png")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ShareHelper.shareWithSystemShareSheetUi(params, null, true);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());

        // Verify sharing intent has the right image.
        Intent sharingIntent = nextIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals(
                "Intent is not a SEND_MULTIPLE intent.",
                Intent.ACTION_SEND_MULTIPLE,
                sharingIntent.getAction());
        assertNotNull(
                "Images should be shared as file list.",
                sharingIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));
    }

    private void selectComponentFromChooserIntent(Intent chooserIntent, ComponentName componentName)
            throws SendIntentException {
        Intent sendBackIntent = new Intent().putExtra(Intent.EXTRA_CHOSEN_COMPONENT, componentName);
        IntentSender sender =
                chooserIntent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT_INTENT_SENDER);
        sender.sendIntent(
                ContextUtils.getApplicationContext(),
                Activity.RESULT_OK,
                sendBackIntent,
                null,
                null);
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    private void selectCustomActionFromChooserIntent(Intent chooserIntent, String action)
            throws SendIntentException {
        Intent sendBackIntent =
                new Intent().putExtra(ShareHelper.EXTRA_SHARE_CUSTOM_ACTION, action);
        IntentSender sender =
                chooserIntent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT_INTENT_SENDER);
        sender.sendIntent(
                ContextUtils.getApplicationContext(),
                Activity.RESULT_OK,
                sendBackIntent,
                null,
                null);
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    private void assertLastComponentNameRecorded(ComponentName name) {
        assertThat(
                "Last shared component name not match.",
                ShareHelper.getLastShareComponentName(),
                Matchers.is(name));
    }

    private ShareParams emptyShareParams() {
        return new ShareParams.Builder(mWindow, "", "").build();
    }

    private static class SingleCustomActionProvider implements ChromeCustomShareAction.Provider {
        private final CallbackHelper mCallbackHelper;
        private final String mActionKey;

        SingleCustomActionProvider(String actionKey, CallbackHelper callbackHelper) {
            mCallbackHelper = callbackHelper;
            mActionKey = actionKey;
        }

        @Override
        public List<ChromeCustomShareAction> getCustomActions() {
            return List.of(
                    new ChromeCustomShareAction(
                            mActionKey,
                            Icon.createWithBitmap(
                                    Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)),
                            "label",
                            mCallbackHelper::notifyCalled));
        }
    }

    /** Test implementation to build a ChooserAction. */
    @Implements(ShareHelper.ChooserActionHelper.class)
    static class ShadowChooserActionHelper {
        @Implementation
        protected static boolean isSupported() {
            return true;
        }

        @Implementation
        protected static Parcelable newChooserAction(Icon icon, String name, PendingIntent action) {
            Bundle bundle = new Bundle();
            bundle.putParcelable(KEY_CHOOSER_ACTION_ICON, icon);
            bundle.putString(KEY_CHOOSER_ACTION_NAME, name);
            bundle.putParcelable(KEY_CHOOSER_ACTION_ACTION, action);
            return bundle;
        }
    }
}
