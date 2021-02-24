// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SINGLE_CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import android.graphics.Bitmap;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordHistogramJni;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillComponent.UserAction;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FaviconOrFallback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collections;

/**
 * Controller tests verify that the Touch To Fill controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class TouchToFillControllerTest {
    private static final String TEST_URL = "https://www.example.xyz";
    private static final String TEST_SUBDOMAIN_URL = "https://subdomain.example.xyz";
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", "https://m.a.xyz/", true, false, 0);
    private static final Credential BOB =
            new Credential("Bob", "*****", "Bob", TEST_SUBDOMAIN_URL, true, false, 0);
    private static final Credential CARL =
            new Credential("Carl", "G3h3!m", "Carl", TEST_URL, false, false, 0);
    private static final @Px int DESIRED_FAVICON_SIZE = 64;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private TouchToFillComponent.Delegate mMockDelegate;
    @Mock
    private LargeIconBridge mMockIconBridge;

    @Mock
    private RecordHistogram.Natives mMockRecordHistogram;

    // Can't be local, as it has to be initialized by initMocks.
    @Captor
    private ArgumentCaptor<LargeIconBridge.LargeIconCallback> mCallbackArgumentCaptor;

    private final TouchToFillMediator mMediator = new TouchToFillMediator();
    private final PropertyModel mModel =
            TouchToFillProperties.createDefaultModel(mMediator::onDismissed);

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        mJniMocker.mock(RecordHistogramJni.TEST_HOOKS, mMockRecordHistogram);
        when(mUrlFormatterJniMock.formatUrlForDisplayOmitScheme(anyString()))
                .then(inv -> format(inv.getArgument(0)));
        when(mUrlFormatterJniMock.formatStringUrlForSecurityDisplay(
                     anyString(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> formatForSecurityDisplay(inv.getArgument(0)));

        mMediator.initialize(mMockDelegate, mModel, mMockIconBridge, DESIRED_FAVICON_SIZE);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(SHEET_ITEMS));
        assertNotNull(mModel.get(DISMISS_HANDLER));
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testShowCredentialsCreatesHeader() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(SINGLE_CREDENTIAL), is(false));
        assertThat(
                itemList.get(0).model.get(FORMATTED_URL), is(formatForSecurityDisplay(TEST_URL)));
        assertThat(itemList.get(0).model.get(ORIGIN_SECURE), is(true));
    }
    @Test
    public void testShowCredentialWithSingleEntryCreatesHeader() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA));
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(SINGLE_CREDENTIAL), is(true));
        assertThat(
                itemList.get(0).model.get(FORMATTED_URL), is(formatForSecurityDisplay(TEST_URL)));
        assertThat(itemList.get(0).model.get(ORIGIN_SECURE), is(true));
    }

    @Test
    public void testShowCredentialsSetsCredentialListAndRequestsFavicons() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + three Credentials
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(ANA));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));
        assertThat(itemList.get(2).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(2).model.get(CREDENTIAL), is(CARL));
        assertThat(itemList.get(2).model.get(FAVICON_OR_FALLBACK), is(nullValue()));
        assertThat(itemList.get(3).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(3).model.get(CREDENTIAL), is(BOB));
        assertThat(itemList.get(3).model.get(FAVICON_OR_FALLBACK), is(nullValue()));

        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(CARL.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(ANA.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(BOB.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
    }

    @Test
    public void testFetchFaviconUpdatesModel() {
        mMediator.showCredentials(TEST_URL, true, Collections.singletonList(CARL));
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(3)); // Header + Credential + Continue Button
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(CARL));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));

        // ANA and CARL both have TEST_URL as their origin URL
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(
                        eq(TEST_URL), eq(DESIRED_FAVICON_SIZE), mCallbackArgumentCaptor.capture());
        LargeIconBridge.LargeIconCallback callback = mCallbackArgumentCaptor.getValue();
        Bitmap bitmap = Bitmap.createBitmap(
                DESIRED_FAVICON_SIZE, DESIRED_FAVICON_SIZE, Bitmap.Config.ARGB_8888);
        callback.onLargeIconAvailable(bitmap, 333, true, IconType.FAVICON);
        FaviconOrFallback iconData = itemList.get(1).model.get(FAVICON_OR_FALLBACK);
        assertThat(iconData.mIcon, is(bitmap));
        assertThat(iconData.mUrl, is(TEST_URL));
        assertThat(iconData.mIconSize, is(DESIRED_FAVICON_SIZE));
        assertThat(iconData.mFallbackColor, is(333));
        assertThat(iconData.mIsFallbackColorDefault, is(true));
        assertThat(iconData.mIconType, is(IconType.FAVICON));
    }

    @Test
    public void testShowCredentialsFormatPslOrigins() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, BOB));
        assertThat(mModel.get(SHEET_ITEMS).size(), is(3)); // Header + two Credentials
        assertThat(mModel.get(SHEET_ITEMS).get(1).type, is(ItemType.CREDENTIAL));
        assertThat(mModel.get(SHEET_ITEMS).get(1).model.get(FORMATTED_ORIGIN),
                is(format(ANA.getOriginUrl())));
        assertThat(mModel.get(SHEET_ITEMS).get(2).type, is(ItemType.CREDENTIAL));
        assertThat(mModel.get(SHEET_ITEMS).get(2).model.get(FORMATTED_ORIGIN),
                is(format(BOB.getOriginUrl())));
    }

    @Test
    public void testClearsCredentialListWhenShowingAgain() {
        mMediator.showCredentials(TEST_URL, true, Collections.singletonList(ANA));
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(3)); // Header + Credential + Continue Button
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(ANA));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));

        // Showing the sheet a second time should replace all changed credentials.
        mMediator.showCredentials(TEST_URL, true, Collections.singletonList(BOB));
        itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(3)); // Header + Credential + Continue Button
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(BOB));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));
    }

    @Test
    public void testShowCredentialsSetsVisibile() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleCredential() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA));
        assertThat(mModel.get(VISIBLE), is(true));
        assertNotNull(mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER));

        mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER).onResult(ANA);
        verify(mMockDelegate).onCredentialSelected(ANA);
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX),
                is(0));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_USER_ACTION,
                           UserAction.SELECT_CREDENTIAL),
                is(1));
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL));
        assertThat(mModel.get(VISIBLE), is(true));
        assertNotNull(mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER));

        mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER).onResult(CARL);
        verify(mMockDelegate).onCredentialSelected(CARL);
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX, 1),
                is(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_USER_ACTION,
                           UserAction.SELECT_CREDENTIAL),
                is(1));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL));
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        verify(mMockDelegate).onDismissed();
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_DISMISSAL_REASON,
                           BottomSheetController.StateChangeReason.BACK_PRESS),
                is(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_USER_ACTION, UserAction.DISMISS),
                is(1));
    }

    @Test
    public void testHidesWhenSelectingManagePasswords() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(ON_CLICK_MANAGE), is(notNullValue()));
        mModel.get(ON_CLICK_MANAGE).run();
        verify(mMockDelegate).onManagePasswordsSelected();
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_USER_ACTION,
                           UserAction.SELECT_MANAGE_PASSWORDS),
                is(1));
    }

    /**
     * Helper to verify formatted URLs. The real implementation calls {@link UrlFormatter}. It's not
     * useful to actually reimplement the formatter, so just modify the string in a trivial way.
     * @param originUrl A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static String format(String originUrl) {
        return "formatted_" + originUrl + "_formatted";
    }

    /**
     * Helper to verify URLs formatted for security display. The real implementation calls
     * {@link UrlFormatter}. It's not useful to actually reimplement the formatter, so just
     * modify the string in a trivial way.
     * @param originUrl A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static String formatForSecurityDisplay(String originUrl) {
        return "formatted_for_security_" + originUrl + "_formatted_for_security";
    }
}
