// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
public class SearchActivityUtilsUnitTest {
    private Activity mActivity = Robolectric.buildActivity(Activity.class).setup().get();

    @Test
    public void buildTrustedIntent_appliesExpectedAction() {
        var intent = SearchActivityUtils.buildTrustedIntent(mActivity, "abcd");
        assertEquals("abcd", intent.getAction());

        intent = SearchActivityUtils.buildTrustedIntent(mActivity, "1234");
        assertEquals("1234", intent.getAction());
    }

    @Test
    public void buildTrustedIntent_addressesSearchActivity() {
        var intent = SearchActivityUtils.buildTrustedIntent(mActivity, "a");
        assertEquals(
                intent.getComponent().getClassName().toString(), SearchActivity.class.getName());
    }

    @Test
    public void buildTrustedIntent_intentIsTrusted() {
        var intent = SearchActivityUtils.buildTrustedIntent(mActivity, "a");
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));
    }

    @Test
    public void requestOmniboxForResult_noActionWhenActivityIsNull() {
        var url = GURL.emptyGURL();
        SearchActivityUtils.requestOmniboxForResult(null, url);
    }

    @Test
    public void requestOmniboxForResult_propagatesCurrentUrl() {
        var url = new GURL("https://abc.xyz");
        SearchActivityUtils.requestOmniboxForResult(mActivity, url);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertEquals(
                IntentUtils.safeGetStringExtra(
                        intentForResult.intent, SearchActivityUtils.EXTRA_CURRENT_URL),
                url.getSpec());
        assertEquals(SearchActivityUtils.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
    }

    @Test
    public void requestOmniboxForResult_acceptsEmptyUrl() {
        // This is technically an invalid case. The test verifies we still do the right thing.
        var url = GURL.emptyGURL();
        SearchActivityUtils.requestOmniboxForResult(mActivity, url);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertTrue(
                IntentUtils.safeHasExtra(
                        intentForResult.intent, SearchActivityUtils.EXTRA_CURRENT_URL));
        assertTrue(
                TextUtils.isEmpty(
                        IntentUtils.safeGetStringExtra(
                                intentForResult.intent, SearchActivityUtils.EXTRA_CURRENT_URL)));
        assertEquals(SearchActivityUtils.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
    }
}
