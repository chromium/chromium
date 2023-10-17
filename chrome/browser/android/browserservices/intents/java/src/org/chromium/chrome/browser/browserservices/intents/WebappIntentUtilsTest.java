// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;

import android.content.Intent;
import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;

import java.util.ArrayList;

/** Tests for {@link WebappIntentUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappIntentUtilsTest {
    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} does not set intent extras
     * on the destination intent if they are not present in the source intent.
     */
    @Test
    public void testCopyWebappLaunchIntentExtrasMissingKeys() {
        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(new Intent(), toIntent);
        assertFalse(toIntent.hasExtra(WebappConstants.EXTRA_NAME));
        assertFalse(toIntent.hasExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE));
        assertFalse(toIntent.hasExtra(WebappConstants.EXTRA_DISPLAY_MODE));
        assertFalse(toIntent.hasExtra(WebappConstants.EXTRA_BACKGROUND_COLOR));
        assertFalse(toIntent.hasExtra(WebappConstants.EXTRA_DARK_THEME_COLOR));
        assertFalse(toIntent.hasExtra(WebappConstants.EXTRA_DARK_BACKGROUND_COLOR));
        assertFalse(toIntent.hasExtra(Intent.EXTRA_STREAM));
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} properly copies intent
     * extras of each type.
     */
    @Test
    public void testCopyWebappLaunchIntentExtras() {
        Intent fromIntent = new Intent();
        fromIntent.putExtra(WebappConstants.EXTRA_NAME, "name");
        fromIntent.putExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, false);
        fromIntent.putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, true);
        fromIntent.putExtra(WebappConstants.EXTRA_DISPLAY_MODE, 1);
        fromIntent.putExtra(WebappConstants.EXTRA_BACKGROUND_COLOR, 1L);
        fromIntent.putExtra(WebappConstants.EXTRA_DARK_THEME_COLOR, 2L);
        fromIntent.putExtra(WebappConstants.EXTRA_DARK_BACKGROUND_COLOR, 3L);
        BrowserIntentUtils.addStartupTimestampsToIntent(fromIntent);

        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(fromIntent, toIntent);
        assertEquals("name", toIntent.getStringExtra(WebappConstants.EXTRA_NAME));
        assertEquals(false, toIntent.getBooleanExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, true));
        assertEquals(
                true, toIntent.getBooleanExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, false));
        assertEquals(1, toIntent.getIntExtra(WebappConstants.EXTRA_DISPLAY_MODE, 0));
        assertEquals(1L, toIntent.getLongExtra(WebappConstants.EXTRA_BACKGROUND_COLOR, 0L));
        assertEquals(2L, toIntent.getLongExtra(WebappConstants.EXTRA_DARK_THEME_COLOR, 0L));
        assertEquals(3L, toIntent.getLongExtra(WebappConstants.EXTRA_DARK_BACKGROUND_COLOR, 0L));
        assertNotEquals(-1L, BrowserIntentUtils.getStartupRealtimeMillis(toIntent));
        assertNotEquals(-1L, BrowserIntentUtils.getStartupUptimeMillis(toIntent));
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebApkLaunchIntentExtras()} properly copies both of
     * the data types which can be used for Intent.EXTRA_STREAM.
     */
    @Test
    public void testCopyStream() {
        {
            ArrayList fromList = new ArrayList<Uri>();
            fromList.add(Uri.parse("https://www.google.com/"));
            fromList.add(Uri.parse("https://www.blogspot.com/"));
            Intent fromIntent = new Intent();
            fromIntent.putExtra(Intent.EXTRA_STREAM, fromList);

            Intent toIntent = new Intent();
            WebappIntentUtils.copyWebApkLaunchIntentExtras(fromIntent, toIntent);
            assertEquals(fromList, toIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));
        }

        {
            Uri fromUri = Uri.parse("https://www.google.com/");
            Intent fromIntent = new Intent();
            fromIntent.putExtra(Intent.EXTRA_STREAM, fromUri);

            Intent toIntent = new Intent();
            WebappIntentUtils.copyWebApkLaunchIntentExtras(fromIntent, toIntent);
            assertEquals(fromUri, toIntent.getParcelableExtra(Intent.EXTRA_STREAM));
        }
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} does not copy non allow
     * listed intent extras.
     */
    @Test
    public void testCopyWebappLaunchIntentExtrasAllowlist() {
        final String extraKey = "not_in_allowlist";
        Intent fromIntent = new Intent();
        fromIntent.putExtra(extraKey, "random");
        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(fromIntent, toIntent);
        assertNull(toIntent.getStringExtra(extraKey));
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} does not modify the source
     * intent.
     */
    @Test
    public void testCopyWebappLaunchIntentExtrasDoesNotModifyFromIntent() {
        final String notInAllowlistKey = "not_in_allowlist";
        Intent fromIntent = new Intent();
        fromIntent.putExtra(WebappConstants.EXTRA_NAME, "name");
        fromIntent.putExtra(notInAllowlistKey, "random");
        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(fromIntent, toIntent);
        assertEquals("name", fromIntent.getStringExtra(WebappConstants.EXTRA_NAME));
        assertEquals("random", fromIntent.getStringExtra(notInAllowlistKey));
        assertEquals("name", toIntent.getStringExtra(WebappConstants.EXTRA_NAME));
    }
}
