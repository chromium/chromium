// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.HeaderProperties;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.ItemType;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.SourceCardProperties;
import org.chromium.components.autofill.autofill_ai.AutofillAiSourceAttributionInfo;
import org.chromium.components.autofill.autofill_ai.SourceType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link AutofillAiSourceAttributionMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiSourceAttributionMediatorTest {
    private Activity mActivity;
    private ModelList mModelList;
    private AutofillAiSourceAttributionMediator mMediator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModelList = new ModelList();
        List<AutofillAiSourceAttributionInfo> sources =
                List.of(
                        new AutofillAiSourceAttributionInfo(
                                SourceType.GMAIL,
                                new GURL("https://mail.google.com/mail/u/0"),
                                "Flight reservation"),
                        new AutofillAiSourceAttributionInfo(
                                SourceType.PHOTOS,
                                new GURL("https://photos.google.com/albums"),
                                "Passport photo"));
        mMediator =
                new AutofillAiSourceAttributionMediator(
                        mActivity, mModelList, sources, "Vehicle · AN-147338");
    }

    @Test
    public void testModelPopulation() {
        assertEquals(3, mModelList.size());

        ListItem headerItem = mModelList.get(0);
        assertEquals(ItemType.HEADER, headerItem.type);
        assertEquals("Vehicle · AN-147338", headerItem.model.get(HeaderProperties.SUBTITLE));

        ListItem gmailItem = mModelList.get(1);
        assertEquals(ItemType.SOURCE_CARD, gmailItem.type);
        PropertyModel gmailModel = gmailItem.model;
        assertEquals(
                R.drawable.autofill_ai_gmail_24dp,
                gmailModel.get(SourceCardProperties.ICON_RES_ID));
        assertEquals("Flight reservation", gmailModel.get(SourceCardProperties.TITLE));
        assertEquals(
                mActivity.getString(
                        R.string.autofill_ai_attribution_open_source_description,
                        mActivity.getString(R.string.autofill_ai_source_app_gmail),
                        "Flight reservation"),
                gmailModel.get(SourceCardProperties.CONTENT_DESCRIPTION));

        ListItem photosItem = mModelList.get(2);
        assertEquals(ItemType.SOURCE_CARD, photosItem.type);
        PropertyModel photosModel = photosItem.model;
        assertEquals(
                R.drawable.autofill_ai_photos_24dp,
                photosModel.get(SourceCardProperties.ICON_RES_ID));
        assertEquals("Passport photo", photosModel.get(SourceCardProperties.TITLE));
        assertEquals(
                mActivity.getString(
                        R.string.autofill_ai_attribution_open_source_description,
                        mActivity.getString(R.string.autofill_ai_source_app_photos),
                        "Passport photo"),
                photosModel.get(SourceCardProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testSourceCardItemClickListener_launchesCustomTab() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.Ai.AttributionSheet.SourceClicked", SourceType.GMAIL);
        ListItem item = mModelList.get(1);
        item.model.get(SourceCardProperties.ON_CLICK_LISTENER).run();
        histogramWatcher.assertExpected();

        Intent intent = Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(Uri.parse("https://mail.google.com/mail/u/0"), intent.getData());
        assertEquals(mActivity.getPackageName(), intent.getPackage());
        assertEquals(
                CustomTabsIntent.SHOW_PAGE_TITLE,
                intent.getExtras().get(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE));
    }

    @Test
    public void testSourceCardClick_activityFinishing_ignored() {
        mActivity.finish();
        ListItem item = mModelList.get(1);
        item.model.get(SourceCardProperties.ON_CLICK_LISTENER).run();
        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    public void testDestroy() {
        ListItem item = mModelList.get(1);
        Runnable clickListener = item.model.get(SourceCardProperties.ON_CLICK_LISTENER);

        mMediator.destroy();
        assertEquals(0, mModelList.size());

        clickListener.run();
        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivity());

        // Idempotent destroy call
        mMediator.destroy();
        assertEquals(0, mModelList.size());
    }
}
