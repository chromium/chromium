// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.components.webapps.ShortcutSource;

@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkWidgetProxyTest {
    @Test
    public void testCreateBookmarkProxyLaunchIntent_NotTrusted() {
        Context context = ApplicationProvider.getApplicationContext();
        PendingIntent pendingIntent = BookmarkWidgetProxy.createBookmarkProxyLaunchIntent(context);

        Intent intent = Shadows.shadowOf(pendingIntent).getSavedIntent();
        assertFalse(
                "Bookmark widget intent should not be trusted since it is mutable.",
                IntentUtils.isTrustedIntentFromSelf(intent));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            boolean isMutableFlag =
                    (Shadows.shadowOf(pendingIntent).getFlags() & PendingIntent.FLAG_MUTABLE) != 0;
            assertTrue(isMutableFlag);
        } else {
            boolean isImmutableFlag =
                    (Shadows.shadowOf(pendingIntent).getFlags() & PendingIntent.FLAG_IMMUTABLE)
                            != 0;
            assertFalse(isImmutableFlag);
        }
    }

    @Test
    public void testOnCreate_SafeHttpsUrl_TrustedAndSanitized() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com"));
        intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID, "42");
        intent.putExtra("attacker_extra_key", "attacker_extra_value");

        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertEquals(Intent.ACTION_VIEW, nextIntent.getAction());
        assertEquals(Uri.parse("https://www.google.com"), nextIntent.getData());
        assertTrue(nextIntent.hasCategory(Intent.CATEGORY_BROWSABLE));
        assertEquals(
                ShortcutSource.BOOKMARK_NAVIGATOR_WIDGET,
                nextIntent.getIntExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN));
        assertTrue(
                nextIntent.getBooleanExtra(
                        WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
        assertEquals(
                "42", nextIntent.getStringExtra(IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID));
        assertNull(
                "Attacker injected extras should not be propagated.",
                nextIntent.getStringExtra("attacker_extra_key"));
        assertTrue(
                "Safe bookmark URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_SafeHttpUrl_Trusted() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://example.com/path"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertEquals(Intent.ACTION_VIEW, nextIntent.getAction());
        assertEquals(Uri.parse("http://example.com/path"), nextIntent.getData());
        assertTrue(
                "HTTP URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_EmptyUrl_LaunchesChromeMain() {
        Intent intent = new Intent();
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertEquals(Intent.ACTION_MAIN, nextIntent.getAction());
        assertTrue(nextIntent.hasCategory(Intent.CATEGORY_LAUNCHER));
        assertEquals(
                ShortcutSource.BOOKMARK_NAVIGATOR_WIDGET,
                nextIntent.getIntExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN));
        assertTrue(
                "Empty bookmark launch should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_AboutBlank_Trusted() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("about:blank"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertTrue(
                "about:blank URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_ChromeNativeRecentTabs_Trusted() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome-native://recent-tabs/"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertTrue(
                "chrome-native recent-tabs URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_ChromeNativeBookmarks_Trusted() {
        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse("chrome-native://bookmarks/folder/1"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertTrue(
                "chrome-native bookmarks URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_ChromeNativeDownloads_Trusted() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome-native://downloads/"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertTrue(
                "chrome-native downloads URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_ChromeNativeHistory_Trusted() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome-native://history/"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNotNull(nextIntent);
        assertEquals(
                ChromeLauncherActivity.class.getName(), nextIntent.getComponent().getClassName());
        assertTrue(
                "chrome-native history URL should have trusted extras attached.",
                IntentUtils.isTrustedIntentFromSelf(nextIntent));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeChromeVersionUrl_NotLaunched() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome://version/"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull(
                "Unsafe chrome://version URL should not trigger ChromeLauncherActivity.",
                nextIntent);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeChromeFlagsUrl_NotLaunched() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome://flags/"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull(
                "Unsafe chrome://flags URL should not trigger ChromeLauncherActivity.", nextIntent);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeChromeNetInternalsUrl_NotLaunched() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("chrome://net-internals/"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull(
                "Unsafe chrome://net-internals URL should not trigger ChromeLauncherActivity.",
                nextIntent);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeJavascriptUrl_NotLaunched() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("javascript:alert(1)"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull("Javascript URL scheme should not trigger ChromeLauncherActivity.", nextIntent);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeFileUrl_NotLaunched() {
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse("file:///data/data/com.android.chrome/databases"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull("file:// URL scheme should not trigger ChromeLauncherActivity.", nextIntent);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeContentUrl_NotLaunched() {
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW, Uri.parse("content://media/external/images/media/1"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull("content:// URL scheme should not trigger ChromeLauncherActivity.", nextIntent);
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testOnCreate_UnsafeAboutUrl_NotLaunched() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("about:config"));
        BookmarkWidgetProxy activity =
                Robolectric.buildActivity(BookmarkWidgetProxy.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull("Non-blank about: URLs should not trigger ChromeLauncherActivity.", nextIntent);
        assertTrue(activity.isFinishing());
    }
}
