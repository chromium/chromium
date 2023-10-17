// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Intent;
import android.net.Uri;

import androidx.core.util.Pair;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for the ShareUtils class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShareUtilsTest {
    @Test
    public void testNoContent() {
        Assert.assertNull(ShareUtils.createIntent(Collections.emptyList()));
        Assert.assertNull(
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(null, "text/plain", "", null),
                                createItem("", "text/plain", "", ""))));
    }

    @Test
    public void testAction() {
        Intent intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null)));
        Assert.assertEquals(Intent.ACTION_SEND, intent.getAction());

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(
                                        JUnitTestGURLs.URL_2.getSpec(), "text/plain", "", null)));
        Assert.assertEquals(Intent.ACTION_SEND, intent.getAction());

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_2.getSpec())));
        Assert.assertEquals(Intent.ACTION_SEND_MULTIPLE, intent.getAction());

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_1.getSpec()),
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_2.getSpec())));
        Assert.assertEquals(Intent.ACTION_SEND_MULTIPLE, intent.getAction());
    }

    @Test
    public void testFlags() {
        Intent intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null)));
        Assert.assertNotEquals(0, intent.getFlags() | Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    @Test
    public void testExtraText() {
        Intent intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null)));
        Assert.assertEquals(
                JUnitTestGURLs.URL_1.getSpec(), intent.getStringExtra(Intent.EXTRA_TEXT));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(
                                        JUnitTestGURLs.URL_2.getSpec(),
                                        "text/plain",
                                        "",
                                        JUnitTestGURLs.URL_2.getSpec())));
        Assert.assertEquals(
                JUnitTestGURLs.URL_1.getSpec(), intent.getStringExtra(Intent.EXTRA_TEXT));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(
                                        JUnitTestGURLs.URL_2.getSpec(), "text/plain", "", null)));
        Assert.assertEquals(
                JUnitTestGURLs.URL_1.getSpec() + "\n" + JUnitTestGURLs.URL_2.getSpec(),
                intent.getStringExtra(Intent.EXTRA_TEXT));

        intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_1.getSpec())));
        Assert.assertFalse(intent.hasExtra(Intent.EXTRA_TEXT));
    }

    @Test
    public void testExtraSubject() {
        Intent intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        "text/plain",
                                        "title",
                                        null)));
        Assert.assertEquals("title", intent.getStringExtra(Intent.EXTRA_SUBJECT));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        "text/plain",
                                        "title1",
                                        null),
                                createItem(
                                        JUnitTestGURLs.URL_2.getSpec(),
                                        "text/plain",
                                        "title2",
                                        JUnitTestGURLs.URL_2.getSpec())));
        Assert.assertFalse(intent.hasExtra(Intent.EXTRA_SUBJECT));
    }

    @Test
    public void testExtraStream() {
        Intent intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null)));
        Assert.assertFalse(intent.hasExtra(Intent.EXTRA_STREAM));
        Assert.assertNull(intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_1.getSpec())));
        Assert.assertEquals(
                Uri.parse(JUnitTestGURLs.URL_1.getSpec()),
                intent.getParcelableExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        "text/plain",
                                        "",
                                        JUnitTestGURLs.URL_1.getSpec())));
        Assert.assertEquals(
                Uri.parse(JUnitTestGURLs.URL_1.getSpec()),
                intent.getParcelableExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_1.getSpec()),
                                createItem(JUnitTestGURLs.URL_2.getSpec(), "text/plain", "", "")));
        Assert.assertEquals(
                Uri.parse(JUnitTestGURLs.URL_1.getSpec()),
                intent.getParcelableExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_1.getSpec()),
                                createItem(
                                        JUnitTestGURLs.URL_2.getSpec(),
                                        "text/plain",
                                        "",
                                        JUnitTestGURLs.URL_2.getSpec())));
        Assert.assertEquals(
                Arrays.asList(
                        Uri.parse(JUnitTestGURLs.URL_1.getSpec()),
                        Uri.parse(JUnitTestGURLs.URL_2.getSpec())),
                intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_1.getSpec()),
                                createItem("", "text/plain", "", JUnitTestGURLs.URL_2.getSpec())));
        Assert.assertEquals(
                Arrays.asList(
                        Uri.parse(JUnitTestGURLs.URL_1.getSpec()),
                        Uri.parse(JUnitTestGURLs.URL_2.getSpec())),
                intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null)));
        Assert.assertFalse(intent.hasExtra(Intent.EXTRA_STREAM));
        Assert.assertNull(intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(
                                        JUnitTestGURLs.URL_2.getSpec(), "text/plain", "", null)));
        Assert.assertFalse(intent.hasExtra(Intent.EXTRA_STREAM));
        Assert.assertNull(intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));
    }

    @Test
    public void testType() {
        Intent intent =
                ShareUtils.createIntent(
                        Collections.singletonList(
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null)));
        Assert.assertEquals(Intent.normalizeMimeType("text/plain"), intent.getType());

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/html", "", null),
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/html", "", null)));
        Assert.assertEquals(Intent.normalizeMimeType("text/*"), intent.getType());

        intent =
                ShareUtils.createIntent(
                        Arrays.asList(
                                createItem(JUnitTestGURLs.URL_1.getSpec(), "text/plain", "", null),
                                createItem(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        "application/octet-stream",
                                        "",
                                        null)));
        Assert.assertEquals(Intent.normalizeMimeType("*/*"), intent.getType());
    }

    private static Pair<OfflineItem, OfflineItemShareInfo> createItem(
            String url, String mimeType, String title, String uri) {
        OfflineItem item = new OfflineItem();
        if (url == null) {
            item.url = null;
        } else if (url.isEmpty()) {
            item.url = GURL.emptyGURL();
        } else {
            item.url = new GURL(url);
        }
        item.mimeType = mimeType;
        item.title = title;

        OfflineItemShareInfo info = new OfflineItemShareInfo();
        if (uri != null) info.uri = Uri.parse(uri);

        return Pair.create(item, info);
    }
}
