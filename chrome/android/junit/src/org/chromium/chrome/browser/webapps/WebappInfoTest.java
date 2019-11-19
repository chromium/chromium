// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Tests the WebappInfo class's ability to parse various URLs.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappInfoTest {
    @Test
    public void testAbout() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "about:blank";

        Intent intent = WebappTestHelper.createMinimalWebappIntent(id, url);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertNotNull(info);
    }

    @Test
    public void testRandomUrl() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "http://google.com";

        Intent intent = WebappTestHelper.createMinimalWebappIntent(id, url);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertNotNull(info);
    }

    @Test
    public void testSpacesInUrl() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String bustedUrl = "http://money.cnn.com/?category=Latest News";

        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, bustedUrl);

        WebappInfo info = WebappInfo.create(intent);
        Assert.assertNotNull(info);
    }

    @Test
    public void testIntentTitleFallBack() {
        String title = "webapp title";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);

        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(title, info.name());
        Assert.assertEquals(title, info.shortName());
    }

    @Test
    public void testIntentNameBlankNoTitle() {
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);

        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals("", info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    @Test
    public void testIntentShortNameFallBack() {
        String title = "webapp title";
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);

        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(title, info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    @Test
    public void testIntentNameShortname() {
        String name = "longName";
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);

        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(name, info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    @Test
    public void testDisplayModeAndOrientationAndSource() {
        String name = "longName";
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.FULLSCREEN);
        intent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(WebDisplayMode.FULLSCREEN, info.displayMode());
        Assert.assertEquals(ScreenOrientationValues.DEFAULT, info.orientation());
        Assert.assertEquals(ShortcutSource.UNKNOWN, info.source());
    }

    @Test
    public void testNormalColors() {
        String name = "longName";
        String shortName = "name";
        long toolbarColor = Color.argb(0xff, 0, 0xff, 0);
        long backgroundColor = Color.argb(0xff, 0, 0, 0xff);

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, toolbarColor);
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, backgroundColor);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(toolbarColor, info.toolbarColor());
        Assert.assertEquals(backgroundColor, info.backgroundColor());
    }

    @Test
    public void testInvalidOrMissingColors() {
        String name = "longName";
        String shortName = "name";

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, info.toolbarColor());
        Assert.assertEquals(
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, info.backgroundColor());
    }

    @Test
    public void testColorsIntentCreation() {
        long toolbarColor = Color.argb(0xff, 0, 0xff, 0);
        long backgroundColor = Color.argb(0xff, 0, 0, 0xff);

        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, toolbarColor);
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, backgroundColor);

        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(toolbarColor, info.toolbarColor());
        Assert.assertEquals(backgroundColor, info.backgroundColor());
    }

    @Test
    public void testScopeIntentCreation() {
        String scope = "https://www.foo.com";
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_SCOPE, scope);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(scope, info.scopeUrl());
    }

    @Test
    public void testIntentScopeFallback() {
        String url = "https://www.foo.com/homepage.html";
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(ShortcutHelper.getScopeFromUrl(url), info.scopeUrl());
    }

    @Test
    public void testIntentDisplayMode() {
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MINIMAL_UI);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(WebDisplayMode.MINIMAL_UI, info.displayMode());
    }

    @Test
    public void testIntentOrientation() {
        Intent intent = createIntentWithUrlAndId();
        intent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.LANDSCAPE);
        WebappInfo info = WebappInfo.create(intent);
        Assert.assertEquals(ScreenOrientationValues.LANDSCAPE, info.orientation());
    }

    @Test
    public void testIntentGeneratedIcon() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "about:blank";

        // Default value.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);

            Assert.assertFalse(name, WebappInfo.create(intent).isIconGenerated());
        }

        // Set to true.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, true);

            Assert.assertTrue(name, WebappInfo.create(intent).isIconGenerated());
        }

        // Set to false.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, false);

            Assert.assertFalse(name, WebappInfo.create(intent).isIconGenerated());
        }

        // Set to something else than a boolean.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, "true");

            Assert.assertFalse(name, WebappInfo.create(intent).isIconGenerated());
        }
    }

    @Test
    public void testIntentAdaptiveIcon() {
        String id = "webapp id";
        String name = "longName";
        String shortName = "name";
        String url = "about:blank";

        // Default value.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);

            Assert.assertFalse(name, WebappInfo.create(intent).isIconAdaptive());
        }

        // Set to true.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, true);

            Assert.assertTrue(name, WebappInfo.create(intent).isIconAdaptive());
        }

        // Set to false.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, false);

            Assert.assertFalse(name, WebappInfo.create(intent).isIconAdaptive());
        }

        // Set to something else than a boolean.
        {
            Intent intent = new Intent();
            intent.putExtra(ShortcutHelper.EXTRA_ID, id);
            intent.putExtra(ShortcutHelper.EXTRA_NAME, name);
            intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, "true");

            Assert.assertFalse(name, WebappInfo.create(intent).isIconAdaptive());
        }
    }

    /**
     * Test that {@link WebappInfo#shouldForceNavigation()} defaults to false when the
     * {@link ShortcutHelper#EXTRA_FORCE_NAVIGATION} intent extra is not specified.
     */
    @Test
    public void testForceNavigationNotSpecified() {
        Intent intent = createIntentWithUrlAndId();
        Assert.assertFalse(WebappInfo.create(intent).shouldForceNavigation());
    }

    /**
     * Creates intent with url and id. If the url or id are not set WebappInfo#create() returns
     * null.
     */
    private Intent createIntentWithUrlAndId() {
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, "web app id");
        intent.putExtra(ShortcutHelper.EXTRA_URL, "about:blank");
        return intent;
    }
}
