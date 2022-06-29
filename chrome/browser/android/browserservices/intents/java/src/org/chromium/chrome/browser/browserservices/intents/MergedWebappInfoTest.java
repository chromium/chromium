// Copyright 2022 The Chromium Authors. All rights reserved.
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

/**
 * Tests the WebappInfo class's ability to parse various URLs.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MergedWebappInfoTest {
    @Test
    public void testOverrideValues() {
        String id = "webapp id";
        String url = "about:blank";

        String nameOld = "nameOld";
        String nameNew = "nameNew";
        String shortNameOld = "shortNameOld";
        String shortNameNew = "shortNameNew";

        {
            Intent intentOld = new Intent();
            intentOld.putExtra(WebappConstants.EXTRA_ID, id);
            intentOld.putExtra(WebappConstants.EXTRA_URL, url);
            intentOld.putExtra(WebappConstants.EXTRA_NAME, nameOld);
            intentOld.putExtra(WebappConstants.EXTRA_SHORT_NAME, shortNameOld);
            intentOld.putExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, false);
            intentOld.putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, false);
            WebappInfo oldInfo = createWebappInfo(intentOld);

            Intent intentNew = new Intent();
            intentNew.putExtra(WebappConstants.EXTRA_ID, id);
            intentNew.putExtra(WebappConstants.EXTRA_URL, url);
            intentNew.putExtra(WebappConstants.EXTRA_NAME, nameNew);
            intentNew.putExtra(WebappConstants.EXTRA_SHORT_NAME, shortNameNew);
            intentNew.putExtra(WebappConstants.EXTRA_IS_ICON_ADAPTIVE, true);
            intentNew.putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, true);
            MergedWebappInfo newInfo = createMergedWebappInfo(oldInfo, createWebappInfo(intentNew));

            Assert.assertEquals(nameOld, oldInfo.name());
            Assert.assertEquals(nameNew, newInfo.name());
            Assert.assertEquals(shortNameOld, oldInfo.shortName());
            Assert.assertEquals(shortNameNew, newInfo.shortName());
            Assert.assertTrue(newInfo.icon() != oldInfo.icon());
            Assert.assertTrue(
                    newInfo.iconUrlToMurmur2HashMap() != oldInfo.iconUrlToMurmur2HashMap());
            Assert.assertFalse(oldInfo.isIconAdaptive());
            Assert.assertTrue(newInfo.isIconAdaptive());
            Assert.assertFalse(oldInfo.isIconGenerated());
            Assert.assertTrue(newInfo.isIconGenerated());

            // Make the new WebappInfo pretend old names are still in use.
            newInfo.setUseOldName(true);
            Assert.assertEquals(nameOld, newInfo.name());
            Assert.assertEquals(shortNameOld, newInfo.shortName());
            // But the icon stuff should be unchanged.
            Assert.assertTrue(newInfo.icon() != oldInfo.icon());
            Assert.assertTrue(
                    newInfo.iconUrlToMurmur2HashMap() != oldInfo.iconUrlToMurmur2HashMap());
            Assert.assertTrue(newInfo.isIconAdaptive());
            Assert.assertTrue(newInfo.isIconGenerated());

            // Now pretend both the old icons and old names are still in use.
            newInfo.setUseOldIcon(true);
            Assert.assertEquals(nameOld, newInfo.name());
            Assert.assertEquals(shortNameOld, newInfo.shortName());
            Assert.assertTrue(newInfo.icon() == oldInfo.icon());
            Assert.assertEquals(
                    newInfo.iconUrlToMurmur2HashMap(), oldInfo.iconUrlToMurmur2HashMap());
            Assert.assertFalse(newInfo.isIconAdaptive());
            Assert.assertFalse(newInfo.isIconGenerated());

            // Make the new WebappInfo pretend only old icons are still in use.
            newInfo.setUseOldName(false);
            Assert.assertEquals(nameNew, newInfo.name());
            Assert.assertEquals(shortNameNew, newInfo.shortName());
            // But the icon stuff should be changed.
            Assert.assertTrue(newInfo.icon() == oldInfo.icon());
            Assert.assertEquals(
                    newInfo.iconUrlToMurmur2HashMap(), oldInfo.iconUrlToMurmur2HashMap());
            Assert.assertFalse(newInfo.isIconAdaptive());
            Assert.assertFalse(newInfo.isIconGenerated());

            // Now revert back to no override.
            newInfo.setUseOldIcon(false);
            Assert.assertEquals(nameOld, oldInfo.name());
            Assert.assertEquals(nameNew, newInfo.name());
            Assert.assertEquals(shortNameOld, oldInfo.shortName());
            Assert.assertEquals(shortNameNew, newInfo.shortName());
            Assert.assertTrue(newInfo.icon() != oldInfo.icon());
            Assert.assertTrue(
                    newInfo.iconUrlToMurmur2HashMap() != oldInfo.iconUrlToMurmur2HashMap());
            Assert.assertFalse(oldInfo.isIconAdaptive());
            Assert.assertTrue(newInfo.isIconAdaptive());
            Assert.assertFalse(oldInfo.isIconGenerated());
            Assert.assertTrue(newInfo.isIconGenerated());
        }
    }

    private WebappInfo createWebappInfo(Intent intent) {
        return WebappInfo.create(WebappIntentDataProviderFactory.create(intent));
    }

    private MergedWebappInfo createMergedWebappInfo(WebappInfo oldInfo, WebappInfo newInfo) {
        return MergedWebappInfo.createForTesting(oldInfo, newInfo);
    }

    /**
     * Creates intent with url and id. If the url or id are not set createWebappInfo() returns
     * null.
     */
    private Intent createIntentWithUrlAndId() {
        Intent intent = new Intent();
        intent.putExtra(WebappConstants.EXTRA_ID, "web app id");
        intent.putExtra(WebappConstants.EXTRA_URL, "about:blank");
        return intent;
    }
}
