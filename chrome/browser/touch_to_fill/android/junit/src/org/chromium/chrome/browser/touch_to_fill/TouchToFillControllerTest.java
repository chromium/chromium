// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.ON_CLICK_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.SHOW_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SHOW_SUBMIT_SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.ON_WEBAUTHN_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.SHOW_WEBAUTHN_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_CREDENTIAL;

import android.content.Context;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FaviconOrFallback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.MorePasskeysProperties;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/**
 * Controller tests verify that the Touch To Fill controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TouchToFillControllerTest {
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final String TEST_URL_FORMATTED =
            UrlFormatter.formatUrlForSecurityDisplay(TEST_URL, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    private static final String TEST_SUBDOMAIN_URL = "https://subdomain.example.xyz";
    private static final Credential ANA = new Credential(
            "Ana", "S3cr3t", "Ana", "https://m.a.xyz/", "m.a.xyz", GetLoginMatchType.PSL, 0);
    private static final Credential BOB = new Credential("Bob", "*****", "Bob", TEST_SUBDOMAIN_URL,
            "subdomain.example.xyz", GetLoginMatchType.PSL, 0);
    private static final Credential CARL = new Credential("Carl", "G3h3!m", "Carl",
            TEST_URL.getSpec(), "example.xyz", GetLoginMatchType.EXACT, 0);
    private static final WebAuthnCredential DINO =
            new WebAuthnCredential("dinos.com", new byte[] {1}, new byte[] {2}, "dino@example.com");
    private static final @Px int DESIRED_FAVICON_SIZE = 64;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private TouchToFillComponent.Delegate mMockDelegate;
    @Mock
    private LargeIconBridge mMockIconBridge;
    @Mock
    private BottomSheetFocusHelper mMockFocusHelper;

    // Can't be local, as it has to be initialized by initMocks.
    @Captor
    private ArgumentCaptor<LargeIconBridge.LargeIconCallback> mCallbackArgumentCaptor;

    private final Context mContext = ContextUtils.getApplicationContext();
    private final TouchToFillMediator mMediator = new TouchToFillMediator();
    private final PropertyModel mModel =
            TouchToFillProperties.createDefaultModel(mMediator::onDismissed);

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);

        mMediator.initialize(mContext, mMockDelegate, mModel, mMockIconBridge, DESIRED_FAVICON_SIZE,
                mMockFocusHelper);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(SHEET_ITEMS));
        assertNotNull(mModel.get(DISMISS_HANDLER));
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testShowCredentialsWithMultipleEntries() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA, CARL),
                /*showMorePasskeys=*/false, /*submitCredential=*/true,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + 2 credentials + footer.
        assertThat(itemList.get(itemList.size() - 1).model.get(MANAGE_BUTTON_TEXT),
                is(mContext.getString(R.string.manage_passwords)));

        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(TITLE),
                is(mContext.getString(R.string.touch_to_fill_sheet_uniform_title)));
        assertThat(itemList.get(0).model.get(FORMATTED_URL), is(TEST_URL_FORMATTED));
        assertThat(itemList.get(0).model.get(ORIGIN_SECURE), is(true));
        assertThat(itemList.get(0).model.get(SHOW_SUBMIT_SUBTITLE), is(true));

        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(ANA));
        assertNotNull(itemList.get(1).model.get(ON_CLICK_LISTENER));
        assertThat(itemList.get(1).model.get(FORMATTED_ORIGIN), is(ANA.getDisplayName()));

        assertThat(itemList.get(2).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(2).model.get(CREDENTIAL), is(CARL));
        assertNotNull(itemList.get(2).model.get(ON_CLICK_LISTENER));
        assertThat(itemList.get(2).model.get(FORMATTED_ORIGIN), is(CARL.getDisplayName()));
        assertThat(itemList.get(0).model.get(IMAGE_DRAWABLE_ID), is(R.drawable.ic_vpn_key_blue));
    }

    @Test
    public void testShowCredentialsWithSingleEntry() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + 1 credential + Button + Footer.
        assertThat(itemList.get(itemList.size() - 1).model.get(MANAGE_BUTTON_TEXT),
                is(mContext.getString(R.string.manage_passwords)));

        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(TITLE),
                is(mContext.getString(R.string.touch_to_fill_sheet_uniform_title)));
        assertThat(itemList.get(0).model.get(FORMATTED_URL), is(TEST_URL_FORMATTED));
        assertThat(itemList.get(0).model.get(ORIGIN_SECURE), is(true));

        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(ANA));
        assertNotNull(itemList.get(1).model.get(ON_CLICK_LISTENER));
        assertThat(itemList.get(1).model.get(FORMATTED_ORIGIN), is(ANA.getDisplayName()));

        assertThat(itemList.get(2).type, is(ItemType.FILL_BUTTON));
        assertThat(itemList.get(2).model.get(SHOW_SUBMIT_BUTTON), is(false));
    }

    @Test
    public void testShowCredentialsWithSingleWebAuthnEntry() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(DINO), Collections.emptyList(),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + 1 credential + Button + Footer.
        assertThat(itemList.get(itemList.size() - 1).model.get(MANAGE_BUTTON_TEXT),
                is(mContext.getString(R.string.manage_passkeys)));

        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(TITLE),
                is(mContext.getString(R.string.touch_to_fill_sheet_title_passkey)));

        assertThat(itemList.get(1).type, is(ItemType.WEBAUTHN_CREDENTIAL));
        assertThat(itemList.get(1).model.get(WEBAUTHN_CREDENTIAL), is(DINO));
        assertNotNull(itemList.get(1).model.get(ON_WEBAUTHN_CLICK_LISTENER));

        assertThat(itemList.get(2).type, is(ItemType.FILL_BUTTON));
        assertThat(itemList.get(2).model.get(SHOW_WEBAUTHN_SUBMIT_BUTTON), is(false));
    }

    @Test
    public void testShowCredentialsWithWebAuthnAndPasswordEntries() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(DINO), Arrays.asList(ANA),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        // Header + 1 webauthn credential + 1 password credential + Footer.
        assertThat(itemList.size(), is(4));
        assertThat(itemList.get(itemList.size() - 1).model.get(MANAGE_BUTTON_TEXT),
                is(mContext.getString(R.string.manage_passwords_and_passkeys)));

        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(TITLE),
                is(mContext.getString(R.string.touch_to_fill_sheet_title_password_or_passkey)));

        assertThat(itemList.get(1).type, is(ItemType.WEBAUTHN_CREDENTIAL));
        assertThat(itemList.get(1).model.get(WEBAUTHN_CREDENTIAL), is(DINO));
        assertNotNull(itemList.get(1).model.get(ON_WEBAUTHN_CLICK_LISTENER));

        assertThat(itemList.get(2).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(2).model.get(CREDENTIAL), is(ANA));
        assertNotNull(itemList.get(2).model.get(ON_CLICK_LISTENER));
        assertThat(itemList.get(2).model.get(FORMATTED_ORIGIN), is(ANA.getDisplayName()));
    }

    @Test
    public void testShowCredentialsToSubmit() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA),
                /*showMorePasskeys=*/false, /*submitCredential=*/true,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + 1 credential + Button + Footer.
        assertThat(itemList.get(itemList.size() - 1).model.get(MANAGE_BUTTON_TEXT),
                is(mContext.getString(R.string.manage_passwords)));

        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(SHOW_SUBMIT_SUBTITLE), is(true));

        assertThat(itemList.get(2).type, is(ItemType.FILL_BUTTON));
        assertThat(itemList.get(2).model.get(SHOW_SUBMIT_BUTTON), is(true));
    }

    @Test
    public void testShowCredentialsSetsCredentialListAndRequestsFavicons() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Arrays.asList(ANA, CARL, BOB), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(5)); // Header + 3 Credentials + Footer.
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
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Collections.singletonList(CARL), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + Credential + Continue Button + Footer.
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(CARL));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));

        // ANA and CARL both have TEST_URL as their origin URL
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(TEST_URL.getSpec()), eq(DESIRED_FAVICON_SIZE),
                        mCallbackArgumentCaptor.capture());
        LargeIconBridge.LargeIconCallback callback = mCallbackArgumentCaptor.getValue();
        Bitmap bitmap = Bitmap.createBitmap(
                DESIRED_FAVICON_SIZE, DESIRED_FAVICON_SIZE, Bitmap.Config.ARGB_8888);
        callback.onLargeIconAvailable(bitmap, 333, true, IconType.FAVICON);
        FaviconOrFallback iconData = itemList.get(1).model.get(FAVICON_OR_FALLBACK);
        assertThat(iconData.mIcon, is(bitmap));
        assertThat(iconData.mUrl, is(TEST_URL.getSpec()));
        assertThat(iconData.mIconSize, is(DESIRED_FAVICON_SIZE));
        assertThat(iconData.mFallbackColor, is(333));
        assertThat(iconData.mIsFallbackColorDefault, is(true));
        assertThat(iconData.mIconType, is(IconType.FAVICON));
    }

    @Test
    public void testShowCredentialsFormatPslOrigins() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA, BOB),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        assertThat(mModel.get(SHEET_ITEMS).size(), is(4)); // Header + 2 Credentials + Footer.
        assertThat(mModel.get(SHEET_ITEMS).get(1).type, is(ItemType.CREDENTIAL));
        assertThat(mModel.get(SHEET_ITEMS).get(1).model.get(FORMATTED_ORIGIN),
                is(ANA.getDisplayName()));
        assertThat(mModel.get(SHEET_ITEMS).get(2).type, is(ItemType.CREDENTIAL));
        assertThat(mModel.get(SHEET_ITEMS).get(2).model.get(FORMATTED_ORIGIN),
                is(BOB.getDisplayName()));
    }

    @Test
    public void testClearsCredentialListWhenShowingAgain() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Collections.singletonList(ANA), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + Credential + Continue Button + Footer.
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(ANA));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));

        // Showing the sheet a second time should replace all changed credentials.
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Collections.singletonList(BOB), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/false);
        itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(4)); // Header + Credential + Continue Button + Footer.
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(BOB));
        assertThat(itemList.get(1).model.get(FAVICON_OR_FALLBACK), is(nullValue()));
    }

    @Test
    public void testShowCredentialsSetsVisibile() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Arrays.asList(ANA, CARL, BOB), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/false);
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleCredential() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        assertThat(mModel.get(VISIBLE), is(true));
        assertNotNull(mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER));

        mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER).onResult(ANA);
        verify(mMockDelegate).onCredentialSelected(ANA);
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX),
                is(0));
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA, CARL),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        assertThat(mModel.get(VISIBLE), is(true));
        assertNotNull(mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER));

        mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER).onResult(CARL);
        verify(mMockDelegate).onCredentialSelected(CARL);
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX, 1),
                is(1));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA, CARL),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false,
                /*showHybridPasskeyOption=*/false);
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        verify(mMockDelegate).onDismissed();
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           TouchToFillMediator.UMA_TOUCH_TO_FILL_DISMISSAL_REASON,
                           BottomSheetController.StateChangeReason.BACK_PRESS),
                is(1));
    }

    @Test
    public void testHidesWhenSelectingManagePasswords() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Arrays.asList(ANA, CARL, BOB), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(
                itemList.get(itemList.size() - 1).model.get(ON_CLICK_MANAGE), is(notNullValue()));
        itemList.get(itemList.size() - 1).model.get(ON_CLICK_MANAGE).run();
        verify(mMockDelegate).onManagePasswordsSelected(/*passkeysShown=*/false);
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testManagePasswordsWithPasskeysShown() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(DINO), Collections.emptyList(),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/true,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(
                itemList.get(itemList.size() - 1).model.get(ON_CLICK_MANAGE), is(notNullValue()));
        itemList.get(itemList.size() - 1).model.get(ON_CLICK_MANAGE).run();
        verify(mMockDelegate).onManagePasswordsSelected(/*passkeysShown=*/true);
    }

    @Test
    public void testManagePasskeysButtonTitleWhenPasswordsHidden() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(DINO), Arrays.asList(ANA),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/true,
                /*showHybridPasskeyOption=*/false);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        // Header + 1 webauthn credential + 1 password credential + Footer.
        assertThat(itemList.size(), is(4));
        assertThat(itemList.get(itemList.size() - 1).model.get(MANAGE_BUTTON_TEXT),
                is(mContext.getString(R.string.manage_passkeys)));
    }

    @Test
    public void testAddsTheBottomSheetHelperToObserveTheSheet() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(DINO), Arrays.asList(ANA),
                /*showMorePasskeys=*/false, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/true,
                /*showHybridPasskeyOption=*/false);

        verify(mMockFocusHelper).registerForOneTimeUse();
    }

    @Test
    public void testHybridPasskeysShown() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Arrays.asList(ANA, CARL, BOB), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/true);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.get(itemList.size() - 1).model.get(SHOW_HYBRID), is(true));
    }

    @Test
    public void testHidesWhenSelectingHybridSignin() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(),
                Arrays.asList(ANA, CARL, BOB), /*showMorePasskeys=*/false,
                /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/true);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(
                itemList.get(itemList.size() - 1).model.get(ON_CLICK_HYBRID), is(notNullValue()));
        itemList.get(itemList.size() - 1).model.get(ON_CLICK_HYBRID).run();
        verify(mMockDelegate).onHybridSignInSelected();
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testMorePasskeysShown() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Collections.emptyList(),
                /*showMorePasskeys=*/true, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/true);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.get(1).type, is(ItemType.MORE_PASSKEYS));
        Runnable onMorePasskeysItemRunnable =
                itemList.get(1).model.get(MorePasskeysProperties.ON_CLICK);
        assertThat(onMorePasskeysItemRunnable, is(notNullValue()));

        onMorePasskeysItemRunnable.run();
        verify(mMockDelegate).onShowMorePasskeysSelected();
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testHidesConfirmationWhenMorePasskeysShown() {
        mMediator.showCredentials(TEST_URL, true, Collections.emptyList(), Arrays.asList(ANA),
                /*showMorePasskeys=*/true, /*submitCredential=*/false,
                /*managePasskeysHidesPasswords=*/false, /*showHybridPasskeyOption=*/true);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(StreamSupport.stream(itemList.spliterator(), false)
                           .map((item) -> item.type)
                           .collect(Collectors.toList()),
                not(hasItem(ItemType.FILL_BUTTON)));
    }
}
