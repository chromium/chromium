// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.content.Intent;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.webapps.WebappIntentDataProviderFactory;

/** Tests the WebappInfo class's ability to parse various URLs. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MergedWebappInfoTest {
    private static final String APP_ID = "webapp id";
    private static final String APP_URL = "about:blank";
    private static final String APP_NAME_OLD = "nameOld";
    private static final String APP_NAME_NEW = "nameNew";
    private static final String APP_SHORTNAME_OLD = "shortNameOld";
    private static final String APP_SHORTNAME_NEW = "shortNameNew";

    @Test
    public void testNullProvider() {
        Intent intentOld = new Intent();
        intentOld.putExtra(WebappConstants.EXTRA_ID, APP_ID);
        intentOld.putExtra(WebappConstants.EXTRA_URL, APP_URL);
        intentOld.putExtra(WebappConstants.EXTRA_NAME, APP_NAME_OLD);
        intentOld.putExtra(WebappConstants.EXTRA_SHORT_NAME, APP_SHORTNAME_OLD);
        intentOld.putExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, false);
        intentOld.putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, false);
        WebappInfo oldInfo = createWebappInfo(intentOld);

        // Test for issue https://crbug.com/1341149. Make sure we don't get a valid Merged object if
        // provider is null.
        Assert.assertEquals(null, MergedWebappInfo.create(oldInfo, /* provider= */ null));
        Assert.assertEquals(null, MergedWebappInfo.create(null, /* provider= */ null));
    }

    @Test
    public void testOverrideValues() {
        Intent intentOld = new Intent();
        intentOld.putExtra(WebappConstants.EXTRA_ID, APP_ID);
        intentOld.putExtra(WebappConstants.EXTRA_URL, APP_URL);
        intentOld.putExtra(WebappConstants.EXTRA_NAME, APP_NAME_OLD);
        intentOld.putExtra(WebappConstants.EXTRA_SHORT_NAME, APP_SHORTNAME_OLD);
        intentOld.putExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, false);
        intentOld.putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, false);
        WebappInfo oldInfo = createWebappInfo(intentOld);

        Intent intentNew = new Intent();
        intentNew.putExtra(WebappConstants.EXTRA_ID, APP_ID);
        intentNew.putExtra(WebappConstants.EXTRA_URL, APP_URL);
        intentNew.putExtra(WebappConstants.EXTRA_NAME, APP_NAME_NEW);
        intentNew.putExtra(WebappConstants.EXTRA_SHORT_NAME, APP_SHORTNAME_NEW);
        intentNew.putExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, true);
        intentNew.putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, true);
        MergedWebappInfo newInfo = createMergedWebappInfo(oldInfo, createWebappInfo(intentNew));

        Assert.assertEquals(APP_NAME_OLD, oldInfo.name());
        Assert.assertEquals(APP_NAME_NEW, newInfo.name());
        Assert.assertEquals(APP_SHORTNAME_OLD, oldInfo.shortName());
        Assert.assertEquals(APP_SHORTNAME_NEW, newInfo.shortName());
        Assert.assertTrue(newInfo.icon() != oldInfo.icon());
        Assert.assertTrue(newInfo.iconUrlToMurmur2HashMap() != oldInfo.iconUrlToMurmur2HashMap());
        Assert.assertFalse(oldInfo.isIconAdaptive());
        Assert.assertTrue(newInfo.isIconAdaptive());
        Assert.assertFalse(oldInfo.isIconGenerated());
        Assert.assertTrue(newInfo.isIconGenerated());

        // Make the new WebappInfo pretend old names are still in use.
        newInfo.setUseOldName(true);
        Assert.assertEquals(APP_NAME_OLD, newInfo.name());
        Assert.assertEquals(APP_SHORTNAME_OLD, newInfo.shortName());
        // But the icon stuff should be unchanged.
        Assert.assertTrue(newInfo.icon() != oldInfo.icon());
        Assert.assertTrue(newInfo.iconUrlToMurmur2HashMap() != oldInfo.iconUrlToMurmur2HashMap());
        Assert.assertTrue(newInfo.isIconAdaptive());
        Assert.assertTrue(newInfo.isIconGenerated());

        // Now pretend both the old icons and old names are still in use.
        newInfo.setUseOldIcon(true);
        Assert.assertEquals(APP_NAME_OLD, newInfo.name());
        Assert.assertEquals(APP_SHORTNAME_OLD, newInfo.shortName());
        Assert.assertTrue(newInfo.icon() == oldInfo.icon());
        Assert.assertEquals(newInfo.iconUrlToMurmur2HashMap(), oldInfo.iconUrlToMurmur2HashMap());
        Assert.assertFalse(newInfo.isIconAdaptive());
        Assert.assertFalse(newInfo.isIconGenerated());

        // Make the new WebappInfo pretend only old icons are still in use.
        newInfo.setUseOldName(false);
        Assert.assertEquals(APP_NAME_NEW, newInfo.name());
        Assert.assertEquals(APP_SHORTNAME_NEW, newInfo.shortName());
        // But the icon stuff should be changed.
        Assert.assertTrue(newInfo.icon() == oldInfo.icon());
        Assert.assertEquals(newInfo.iconUrlToMurmur2HashMap(), oldInfo.iconUrlToMurmur2HashMap());
        Assert.assertFalse(newInfo.isIconAdaptive());
        Assert.assertFalse(newInfo.isIconGenerated());

        // Now revert back to no override.
        newInfo.setUseOldIcon(false);
        Assert.assertEquals(APP_NAME_OLD, oldInfo.name());
        Assert.assertEquals(APP_NAME_NEW, newInfo.name());
        Assert.assertEquals(APP_SHORTNAME_OLD, oldInfo.shortName());
        Assert.assertEquals(APP_SHORTNAME_NEW, newInfo.shortName());
        Assert.assertTrue(newInfo.icon() != oldInfo.icon());
        Assert.assertTrue(newInfo.iconUrlToMurmur2HashMap() != oldInfo.iconUrlToMurmur2HashMap());
        Assert.assertFalse(oldInfo.isIconAdaptive());
        Assert.assertTrue(newInfo.isIconAdaptive());
        Assert.assertFalse(oldInfo.isIconGenerated());
        Assert.assertTrue(newInfo.isIconGenerated());
    }

    private WebappInfo createWebappInfo(Intent intent) {
        return WebappInfo.create(WebappIntentDataProviderFactory.create(intent));
    }

    private MergedWebappInfo createMergedWebappInfo(WebappInfo oldInfo, WebappInfo newInfo) {
        return MergedWebappInfo.createForTesting(oldInfo, newInfo);
    }

    /**
     * Creates intent with url and id. If the url or id are not set createWebappInfo() returns null.
     */
    private Intent createIntentWithUrlAndId() {
        Intent intent = new Intent();
        intent.putExtra(WebappConstants.EXTRA_ID, "web app id");
        intent.putExtra(WebappConstants.EXTRA_URL, "about:blank");
        return intent;
    }
}
