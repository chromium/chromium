// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.os.Build.VERSION_CODES;

import androidx.appsearch.app.GenericDocument;
import androidx.appsearch.app.SearchResult;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@RunWith(ChromeJUnit4ClassRunner.class)
public final class AuxiliarySearchDonorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private AuxiliarySearchHooks mHooks;

    private int[] mIds;
    private String[] mUrls;
    private String[] mTitles;
    private long[] mLastAccessTimestamps;
    private Bitmap[] mBitmap;

    private AuxiliarySearchDonor mAuxiliarySearchDonor;

    @Before
    public void setUp() {
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(mHooks);

        mActivityTestRule.launchActivity(null);
        mAuxiliarySearchDonor = AuxiliarySearchDonor.getInstance();

        mIds = new int[] {1, 2};
        mUrls = new String[] {"Url1", "Url2"};
        mTitles = new String[] {"Title1", "Title2"};
        mLastAccessTimestamps = new long[] {100, 111};
        mBitmap =
                new Bitmap[] {
                    Bitmap.createBitmap(100, 100, Config.RGB_565),
                    Bitmap.createBitmap(100, 100, Config.ARGB_8888)
                };
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mAuxiliarySearchDonor.deleteAll((result) -> {}));
    }

    @Test
    @MediumTest
    @EnableFeatures({
        "AndroidAppIntegrationMultiDataSource:multi_data_source_skip_schema_check/true"
    })
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.S, message = "The donation API is for S+.")
    public void testDonateTabs() {
        testDonateTabsImpl();
    }

    private void testDonateTabsImpl() {
        ThreadUtils.runOnUiThreadBlocking(() -> mAuxiliarySearchDonor.createSessionAndInit());
        CriteriaHelper.pollUiThread(() -> mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        int size = 2;
        List<AuxiliarySearchEntry> entryList = new ArrayList<>();
        for (int i = 0; i < size; i++) {
            AuxiliarySearchEntry entry =
                    AuxiliarySearchEntry.newBuilder()
                            .setId(mIds[i])
                            .setTitle(mTitles[i])
                            .setUrl(mUrls[i])
                            .setLastAccessTimestamp(mLastAccessTimestamps[i])
                            .build();
            entryList.add(entry);
        }

        Map<AuxiliarySearchEntry, Bitmap> map = new HashMap<>();
        map.put(entryList.get(0), mBitmap[0]);
        map.put(entryList.get(1), mBitmap[1]);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAuxiliarySearchDonor.donateFavicons(
                            entryList,
                            map,
                            (success) -> {
                                assertTrue(success);
                                mAuxiliarySearchDonor.searchDonationResultsForTesting(
                                        (searchResults) -> verifyResults(entryList, searchResults));
                            });
                });
    }

    private void verifyResults(
            List<AuxiliarySearchEntry> entries, List<SearchResult> searchResults) {
        ThreadUtils.assertOnUiThread();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(entries.size(), searchResults.size());

                    try {
                        for (int i = 0; i < 2; i++) {
                            SearchResult result = searchResults.get(i);
                            GenericDocument genericDocument = result.getGenericDocument();
                            WebPage webPage = genericDocument.toDocumentClass(WebPage.class);

                            String documentId =
                                    AuxiliarySearchDonor.getDocumentId(
                                            AuxiliarySearchEntryType.TAB, mIds[i]);
                            assertEquals(documentId, genericDocument.getId());
                            assertEquals(
                                    mLastAccessTimestamps[i],
                                    genericDocument.getCreationTimestampMillis());

                            assertEquals(documentId, webPage.getId());
                            assertEquals(mUrls[i], webPage.getUrl());
                            assertEquals(mTitles[i], webPage.getName());
                            assertEquals(
                                    mLastAccessTimestamps[i], webPage.getCreationTimestampMillis());
                            assertEquals(
                                    mAuxiliarySearchDonor.getTabDocumentTtlMs(),
                                    webPage.getDocumentTtlMillis());
                            assertTrue(
                                    Arrays.equals(
                                            AuxiliarySearchUtils.bitmapToBytes(mBitmap[i]),
                                            webPage.getFavicon().getBytes()));
                        }
                    } catch (AppSearchException e) {
                        throw new AssertionError();
                    }
                });
    }
}
