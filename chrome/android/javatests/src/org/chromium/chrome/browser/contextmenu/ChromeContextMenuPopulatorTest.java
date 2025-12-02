// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import androidx.browser.customtabs.CustomContentAction;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuMode;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.embedder_support.contextmenu.ContextMenuImageFormat;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for the context menu logic of Chrome. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ChromeContextMenuPopulatorTest {
    private static final String DATA_URL = "data:encodedstringblahblah";
    private static final String PAGE_URL = "http://www.blah.com/page_url";
    private static final String LINK_URL = "http://www.blah.com/other_blah";
    private static final String LINK_TEXT = "BLAH!";
    private static final String IMAGE_SRC_URL = "http://www.blah.com/image.jpg";
    private static final String IMAGE_TITLE_TEXT = "IMAGE!";
    private static final String RETRIEVED_IMAGE_URL = "http://www.blah.com/retrieved_image.jpg";
    private static final Uri RETRIEVED_IMAGE_URI =
            Uri.parse("content://com.my.app.testing/mock/image.png");

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveRule = new OverrideContextWrapperTestRule();

    @Mock private Activity mActivity;
    @Mock private TabContextMenuItemDelegate mItemDelegate;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private ContextMenuNativeDelegate mNativeDelegate;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;
    @Mock private Profile.Natives mProfileNatives;
    @Mock private MenuModelBridge mMenuModelBridge;
    @Mock private ChromeContextMenuPopulator.PendingIntentSender mMockPendingIntentSender;

    private ChromeContextMenuPopulator mPopulator;

    @Before
    public void setUp() {
        mAutomotiveRule.setIsAutomotive(false);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(false);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        when(mMenuModelBridge.populateModelList()).thenReturn(new ModelList());

        GURL pageUrl = new GURL(PAGE_URL);
        when(mItemDelegate.getPageUrl()).thenReturn(pageUrl);
        when(mItemDelegate.isIncognitoSupported()).thenReturn(true);
        when(mItemDelegate.supportsCall()).thenReturn(true);
        when(mItemDelegate.supportsSendEmailMessage()).thenReturn(true);
        when(mItemDelegate.supportsSendTextMessage()).thenReturn(true);
        when(mItemDelegate.supportsAddToContacts()).thenReturn(true);
        when(mItemDelegate.getWebContents()).thenReturn(mWebContents);
        when(mItemDelegate.canCurrentTabGoBack()).thenReturn(true);
        when(mItemDelegate.canCurrentTabGoForward()).thenReturn(true);

        ProfileJni.setInstanceForTesting(mProfileNatives);
        when(mProfileNatives.fromWebContents(eq(mWebContents))).thenReturn(mProfile);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
                });
    }

    @After
    public void tearDown() {
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.resetActivitiesForInstrumentationTests();
                });
    }

    private void initializePopulator(@ContextMenuMode int mode, ContextMenuParams params) {
        initializePopulator(mode, params, false, false, true);
    }

    private void initializePopulator(
            @ContextMenuMode int mode,
            ContextMenuParams params,
            boolean shouldShowDeveloperMenu,
            boolean shouldShowViewPageSourceMenu,
            boolean supportPrint) {
        initializePopulator(
                mode,
                params,
                List.of(),
                shouldShowDeveloperMenu,
                shouldShowViewPageSourceMenu,
                supportPrint);
    }

    private void initializePopulator(
            @ContextMenuMode int mode,
            ContextMenuParams params,
            List<CustomContentAction> actions) {
        initializePopulator(mode, params, actions, false, false, true);
    }

    private void initializePopulator(
            @ContextMenuMode int mode,
            ContextMenuParams params,
            List<CustomContentAction> actions,
            boolean shouldShowDeveloperMenu,
            boolean shouldShowViewPageSourceMenu,
            boolean supportPrint) {
        mPopulator =
                Mockito.spy(
                        new ChromeContextMenuPopulator(
                                mItemDelegate,
                                () -> mShareDelegate,
                                actions,
                                mode,
                                ContextUtils.getApplicationContext(),
                                params,
                                mNativeDelegate));
        doReturn(mTemplateUrlService).when(mPopulator).getTemplateUrlService();
        doReturn(false).when(mPopulator).shouldTriggerEphemeralTabHelpUi();
        doReturn(false).when(mPopulator).shouldTriggerReadLaterHelpUi();
        doReturn(true).when(mPopulator).shouldShowEmptySpaceContextMenu();
        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);
        doReturn(shouldShowDeveloperMenu).when(mPopulator).shouldShowDeveloperMenu();
        doReturn(shouldShowViewPageSourceMenu).when(mPopulator).shouldShowViewPageSourceMenu();
        doReturn(supportPrint).when(mItemDelegate).isPrintSupported();
        doNothing().when(mPopulator).maybeRecordBooleanUkm(anyString(), anyString());
    }

    private void checkMenuOptions(List<Integer> disabled, int[]... groups) {
        List<ModelList> contextMenuState = mPopulator.buildContextMenu();

        assertEquals(
                "Number of groups doesn't match",
                groups[0] == null ? 0 : groups.length,
                contextMenuState.size());

        for (int i = 0; i < contextMenuState.size(); i++) {
            int[] availableInTab = new int[contextMenuState.get(i).size()];
            for (int j = 0; j < contextMenuState.get(i).size(); j++) {
                PropertyModel model = contextMenuState.get(i).get(j).model;
                assertEquals(
                        "'" + model.get(TITLE) + "' has different enablement setting than expected",
                        !disabled.contains(model.get(MENU_ITEM_ID)),
                        model.get(ENABLED));
                availableInTab[j] = model.get(MENU_ITEM_ID);
            }

            int[] expectedItemsInGroup = groups[i];

            // Strip ephemeral tab options if they're not supported.
            if (!EphemeralTabCoordinator.isSupported()) {
                ArrayList<Integer> updatedList = new ArrayList<>();
                for (int initialListIndex = 0;
                        initialListIndex < expectedItemsInGroup.length;
                        initialListIndex++) {
                    if (expectedItemsInGroup[initialListIndex]
                                    != R.id.contextmenu_open_in_ephemeral_tab
                            && expectedItemsInGroup[initialListIndex]
                                    != R.id.contextmenu_open_image_in_ephemeral_tab) {
                        updatedList.add(expectedItemsInGroup[initialListIndex]);
                    }
                }
                expectedItemsInGroup = CollectionUtil.integerCollectionToIntArray(updatedList);
            }

            if (!Arrays.equals(expectedItemsInGroup, availableInTab)) {
                StringBuilder generated_info = new StringBuilder();
                for (int j = 0; j < contextMenuState.get(i).size(); j++) {
                    generated_info.append("'");
                    generated_info.append(contextMenuState.get(i).get(j).model.get(MENU_ITEM_ID));
                    generated_info.append("' ");
                }
                StringBuilder expected_info = new StringBuilder();
                for (int j = 0; j < expectedItemsInGroup.length; j++) {
                    expected_info.append("'");
                    expected_info.append(expectedItemsInGroup[j]);
                    expected_info.append("' ");
                }
                fail(
                        "Items in group "
                                + i
                                + " don't match, expecting: "
                                + expected_info.toString()
                                + ", generated: "
                                + generated_info.toString());
            }
        }
    }

    private void checkMenuOptions(int[]... groups) {
        checkMenuOptions(/* disabled= */ new ArrayList<>(), groups);
    }

    private ContextMenuParams getHttpLinkParams() {
        return new ContextMenuParams(
                0,
                mMenuModelBridge,
                ContextMenuDataMediaType.NONE,
                ContextMenuDataMediaFlags.MEDIA_NONE,
                new GURL(PAGE_URL),
                new GURL(LINK_URL),
                LINK_TEXT,
                GURL.emptyGURL(),
                GURL.emptyGURL(),
                "",
                null,
                false,
                0,
                0,
                MenuSourceType.TOUCH,
                false,
                /* openedFromInterestFor= */ false,
                /* interestForNodeID= */ 0,
                /* additionalNavigationParams= */ null);
    }

    private ContextMenuParams getInterestForLinkParams() {
        return new ContextMenuParams(
                0,
                mMenuModelBridge,
                ContextMenuDataMediaType.NONE,
                ContextMenuDataMediaFlags.MEDIA_NONE,
                new GURL(PAGE_URL),
                new GURL(LINK_URL),
                LINK_TEXT,
                GURL.emptyGURL(),
                GURL.emptyGURL(),
                "",
                null,
                false,
                0,
                0,
                MenuSourceType.TOUCH,
                false,
                /* openedFromInterestFor= */ true,
                /* interestForNodeID= */ 12345,
                /* additionalNavigationParams= */ null);
    }

    private ContextMenuParams createVideoPipParams(@ContextMenuDataMediaFlags int mediaFlags) {
        GURL sourceUrl = new GURL("http://www.blah.com/");
        GURL url = new GURL(sourceUrl.getSpec() + "I_love_mouse_video.avi");
        return new ContextMenuParams(
                0,
                mMenuModelBridge,
                ContextMenuDataMediaType.VIDEO,
                mediaFlags,
                new GURL(PAGE_URL),
                url,
                "VIDEO!",
                GURL.emptyGURL(),
                sourceUrl,
                "",
                null,
                true,
                0,
                0,
                MenuSourceType.TOUCH,
                false,
                /* openedFromInterestFor= */ false,
                /* interestForNodeID= */ 0,
                /* additionalNavigationParams= */ null);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testHttpLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params = getHttpLinkParams();

        int[] expected = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(expected4);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected5);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params, true, false, true);
        int[] expected6Tab1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
        };
        int[] expected6Tab2 = {
            R.id.contextmenu_inspect_element,
        };
        checkMenuOptions(expected6Tab1, expected6Tab2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testShowInterestInElement() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params = getInterestForLinkParams();

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_show_interest_in_element,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testHttpLinkWithDownloadBlockedByPolicy() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_link_as), expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_link_as), expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_link_as), expected4);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_link_as), expected5);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testHttpLinkWithPreviewTabEnabled() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected2);

        // Webapp doesn't show preview tab.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected3 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected4 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected4);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testDataUrlDisablesPreviewTab() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(DATA_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected3 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected4 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected4);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMailLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL mailto = new GURL("mailto:fake@email.com");
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        mailto,
                        "MAIL!",
                        GURL.emptyGURL(),
                        new GURL(PAGE_URL),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected4);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected5);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTelLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL tel = new GURL("tel:0048221234567");
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        tel,
                        "PHONE!",
                        GURL.emptyGURL(),
                        new GURL(PAGE_URL),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_call,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_call,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_call,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected4);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5 = {
            R.id.contextmenu_share_link,
            R.id.contextmenu_call,
            R.id.contextmenu_send_message,
            R.id.contextmenu_add_to_contacts,
            R.id.contextmenu_copy
        };
        checkMenuOptions(expected5);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testVideoLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL sourceUrl = new GURL("http://www.blah.com/");
        GURL url = new GURL(sourceUrl.getSpec() + "I_love_mouse_video.avi");
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.VIDEO,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        url,
                        "VIDEO!",
                        GURL.emptyGURL(),
                        sourceUrl,
                        "",
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        int[] expectedTab1 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expectedTab1);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        int[] expected2Tab2 = {R.id.contextmenu_save_video};
        checkMenuOptions(expected2Tab1, expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3Tab1 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected3Tab1, expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4Tab1 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        int[] expected4Tab2 = {R.id.contextmenu_save_video, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4Tab1, expected4Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5Tab1 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected5Tab1, expected2Tab2);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params, true, false, true);
        int[] expected6Tab1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        int[] expected6Tab2 = {R.id.contextmenu_save_video};
        int[] expected6Tab3 = {R.id.contextmenu_inspect_element};
        checkMenuOptions(expected6Tab1, expected6Tab2, expected6Tab3);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testVideoLinkWithDownloadBlockedByPolicy() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(true);
        GURL sourceUrl = new GURL("http://www.blah.com/");
        GURL url = new GURL(sourceUrl.getSpec() + "I_love_mouse_video.avi");
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.VIDEO,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        url,
                        "VIDEO!",
                        GURL.emptyGURL(),
                        sourceUrl,
                        "",
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        int[] expected2Tab2 = {R.id.contextmenu_save_video};
        checkMenuOptions(
                Arrays.asList(R.id.contextmenu_save_link_as, R.id.contextmenu_save_video),
                expected2Tab1,
                expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3Tab1 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(
                Arrays.asList(R.id.contextmenu_save_link_as, R.id.contextmenu_save_video),
                expected3Tab1,
                expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4Tab1 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        int[] expected4Tab2 = {R.id.contextmenu_save_video, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(
                Arrays.asList(R.id.contextmenu_save_link_as, R.id.contextmenu_save_video),
                expected4Tab1,
                expected4Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5Tab1 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(
                Arrays.asList(R.id.contextmenu_save_link_as, R.id.contextmenu_save_video),
                expected5Tab1,
                expected2Tab2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CONTEXT_MENU_PICTURE_IN_PICTURE_ANDROID)
    public void testVideoPictureInPicture_Enter() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        final String enterPip =
                ContextUtils.getApplicationContext()
                        .getString(R.string.contextmenu_picture_in_picture);
        final String exitPip =
                ContextUtils.getApplicationContext()
                        .getString(R.string.contextmenu_exit_picture_in_picture);

        ContextMenuParams canPipParams =
                createVideoPipParams(ContextMenuDataMediaFlags.MEDIA_CAN_PICTURE_IN_PICTURE);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, canPipParams);
        // Mock this method because it goes into native code to record a histogram.
        doNothing().when(mPopulator).recordContextMenuSelection(anyInt());
        List<ModelList> menuState = mPopulator.buildContextMenu();
        ListItem pipItem = findItemWithTitle(menuState, enterPip);
        assertNotNull("Should have 'Picture in Picture' menu item.", pipItem);
        assertNull(
                "Should not have 'Exit Picture in Picture' menu item.",
                findItemWithTitle(menuState, exitPip));

        assertTrue(
                "Clicking on enter pip should be handled.",
                mPopulator.onItemSelected(R.id.contextmenu_picture_in_picture));
        verify(mNativeDelegate).setPictureInPicture(true);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CONTEXT_MENU_PICTURE_IN_PICTURE_ANDROID)
    public void testVideoPictureInPicture_Exit() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        final String enterPip =
                ContextUtils.getApplicationContext()
                        .getString(R.string.contextmenu_picture_in_picture);
        final String exitPip =
                ContextUtils.getApplicationContext()
                        .getString(R.string.contextmenu_exit_picture_in_picture);

        ContextMenuParams inPipParams =
                createVideoPipParams(
                        ContextMenuDataMediaFlags.MEDIA_CAN_PICTURE_IN_PICTURE
                                | ContextMenuDataMediaFlags.MEDIA_PICTURE_IN_PICTURE);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, inPipParams);
        // Mock this method because it goes into native code to record a histogram.
        doNothing().when(mPopulator).recordContextMenuSelection(anyInt());
        List<ModelList> menuState = mPopulator.buildContextMenu();
        ListItem pipItem = findItemWithTitle(menuState, exitPip);
        assertNotNull("Should have 'Exit Picture in Picture' menu item.", pipItem);
        assertNull(
                "Should not have 'Picture in Picture' menu item.",
                findItemWithTitle(menuState, enterPip));

        assertTrue(
                "Clicking on exit pip should be handled.",
                mPopulator.onItemSelected(R.id.contextmenu_picture_in_picture));
        verify(mNativeDelegate).setPictureInPicture(false);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testImageHiFi() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        int[] expected = null;
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_open_image_in_new_tab,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_image,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(expected4);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5 = {
            R.id.contextmenu_open_image,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(expected5);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params, true, false, true);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_image_in_new_tab,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        int[] expected2Tab2 = {
            R.id.contextmenu_inspect_element,
        };
        checkMenuOptions(expected2Tab1, expected2Tab2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testHttpLinkWithImageHiFi() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy_link_address};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        int[] expected2Tab2 = {
            R.id.contextmenu_open_image_in_new_tab,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(expected2Tab1, expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3Tab1 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        int[] expected3Tab2 = {
            R.id.contextmenu_open_image,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(expected3Tab1, expected3Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4Tab1 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        int[] expected4Tab2 = {
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(expected4Tab1, expected4Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5Tab1 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        int[] expected5Tab2 = {
            R.id.contextmenu_open_image,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(expected5Tab1, expected5Tab2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testImageWithDownloadBlockedByPolicy() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_open_image_in_new_tab,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_image), expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_image,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_image), expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_image), expected4);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected5 = {
            R.id.contextmenu_open_image,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image,
            R.id.contextmenu_share_image
        };
        checkMenuOptions(Arrays.asList(R.id.contextmenu_save_image), expected5);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testReadLater() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        // HTTP scheme should include read later context menu item.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected);

        // Custom tab should include read later.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected3 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link,
            R.id.contextmenu_open_in_chrome
        };
        checkMenuOptions(expected3);

        // Network bound tab should include read later.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected4 = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected4);

        // Non-http scheme should not include read later context menu item.
        params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL("about://blank"), // have an accepted scheme but not HTTP
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected5 = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected5);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testIncognito() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        when(mItemDelegate.isIncognito()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expectedIncognito = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expectedIncognito);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected2Incognito = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected2Incognito);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenInOtherWindow() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        when(mItemDelegate.isOpenInOtherWindowSupported()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expectedMultiWindow = {
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_other_window,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expectedMultiWindow);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        int[] expected2MultiWindow = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected2MultiWindow);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenInNewWindow() {
        checkOpenInNewWindowItems(/* isIncognitoWindowFeatureEnabled= */ false);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenInNewWindow_incognitoWindowEnabled() {
        checkOpenInNewWindowItems(/* isIncognitoWindowFeatureEnabled= */ true);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetLensIntentParams() {
        when(mItemDelegate.isIncognito()).thenReturn(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);

        LensIntentParams lensIntentParams =
                mPopulator.getLensIntentParams(
                        LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                        Uri.parse(RETRIEVED_IMAGE_URL));
        assertEquals(
                "Lens intent parameters has incorrect image URI.",
                RETRIEVED_IMAGE_URL,
                lensIntentParams.getImageUri().toString());
        assertTrue(
                "Lens intent parameters has incorrect incognito value.",
                lensIntentParams.getIsIncognito());
        assertEquals(
                "Lens intent parameters has incorrect src URL.",
                IMAGE_SRC_URL,
                lensIntentParams.getSrcUrl());
        assertEquals(
                "Lens intent parameters has incorrect title or alt text.",
                IMAGE_TITLE_TEXT,
                lensIntentParams.getImageTitleOrAltText());
        assertEquals(
                "Lens intent parameters has incorrect page URL.",
                PAGE_URL,
                lensIntentParams.getPageUrl());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOpenFromHighlight() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        // The setup requires only the openedFromHighlight param.
        ContextMenuParams params =
                new ContextMenuParams(
                        /* nativePtr= */ 0,
                        /* menuModelBridge */ mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        /* pageUrl= */ GURL.emptyGURL(),
                        /* linkUrl= */ GURL.emptyGURL(),
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ GURL.emptyGURL(),
                        /* srcUrl= */ GURL.emptyGURL(),
                        /* titleText= */ "",
                        /* referrer= */ null,
                        /* canSaveMedia= */ false,
                        /* triggeringTouchXDp= */ 0,
                        /* triggeringTouchYDp= */ 0,
                        MenuSourceType.TOUCH,
                        /* openedFromHighlight= */ true,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        // In normal mode, there should be three options: share, remove and learn more.
        int[] normal_expected = {
            R.id.contextmenu_share_highlight,
            R.id.contextmenu_remove_highlight,
            R.id.contextmenu_learn_more
        };
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(normal_expected);

        // In custom tab, network bound tab or web app mode, only the remove option should be
        // present.
        int[] other_expected = {R.id.contextmenu_remove_highlight};
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(other_expected);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(other_expected);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(other_expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSharingLinkWithCctAutomotive() {
        mAutomotiveRule.setIsAutomotive(true);
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams linkParams =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, linkParams);
        int[] linkExpected = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later
        };
        checkMenuOptions(linkExpected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, linkParams);
        int[] link2Expected = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later
        };
        checkMenuOptions(link2Expected);

        ContextMenuParams imageParams =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, imageParams);
        int[] imageExpected = {
            R.id.contextmenu_open_in_browser_id,
            R.id.contextmenu_open_image,
            R.id.contextmenu_open_image_in_ephemeral_tab,
            R.id.contextmenu_copy_image,
            R.id.contextmenu_save_image
        };
        checkMenuOptions(imageExpected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, imageParams);
        int[] image2Expected = {
            R.id.contextmenu_open_image, R.id.contextmenu_copy_image, R.id.contextmenu_save_image
        };
        checkMenuOptions(image2Expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testPage() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        /* additionalNavigationParams= */ null);

        int[][] expected = {
            {R.id.contextmenu_save_page, R.id.contextmenu_share_page, R.id.contextmenu_print_page},
        };

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testPageWithDevMenu() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        /* additionalNavigationParams= */ null);

        int[][] expected = {
            {R.id.contextmenu_save_page, R.id.contextmenu_share_page, R.id.contextmenu_print_page},
            {R.id.contextmenu_view_page_source, R.id.contextmenu_inspect_element},
        };

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params, true, true, true);
        checkMenuOptions(expected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params, true, true, true);
        checkMenuOptions(expected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params, true, true, true);
        checkMenuOptions(expected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB,
                params,
                true,
                true,
                true);
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testPageDownloadRestricted() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        /* additionalNavigationParams= */ null);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(true);

        int[] expectedPage = {
            R.id.contextmenu_save_page, R.id.contextmenu_share_page, R.id.contextmenu_print_page
        };
        List<Integer> expected_disabled = Arrays.asList(R.id.contextmenu_save_page);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected_disabled, expectedPage);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected_disabled, expectedPage);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected_disabled, expectedPage);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB, params);
        checkMenuOptions(expected_disabled, expectedPage);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testPagePrintNotSupported() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        /* additionalNavigationParams= */ null);

        int[][] expected = {
            {R.id.contextmenu_save_page, R.id.contextmenu_share_page},
        };

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params, false, false, false);
        checkMenuOptions(expected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params, false, false, false);
        checkMenuOptions(expected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params, false, false, false);
        checkMenuOptions(expected);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.NETWORK_BOUND_TAB,
                params,
                false,
                false,
                false);
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testIncludeMenuModelBridgeItems() {
        ModelList modelListFromBridge = new ModelList();
        modelListFromBridge.add(
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, "Test title")
                                .build()));
        when(mMenuModelBridge.populateModelList()).thenReturn(modelListFromBridge);
        ContextMenuParams params = getHttpLinkParams();
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        List<ModelList> result = mPopulator.buildContextMenu();
        assertEquals(2, result.size());
        assertEquals(
                "Expected the group of extension-injected items to be the last group",
                modelListFromBridge,
                result.get(result.size() - 1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAddMenuItemWithSubmenu() {
        ContextMenuParams params = getHttpLinkParams();
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);

        ModelList modelList = new ModelList();
        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(
                new ListItem(
                        ListItemType.MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, "Sample submenu item")
                                .build()));
        String menuItemWithSubmenuTitle = "Sample item with Submenu";
        modelList.add(mPopulator.createListItemWithSubmenu(menuItemWithSubmenuTitle, submenuItems));

        when(mMenuModelBridge.populateModelList()).thenReturn(modelList);

        ModelListAdapter adapter = new ModelListAdapter(modelList);
        assertEquals(1, adapter.getCount());
        ListItem menuItemWithSubmenu = (ListItem) adapter.getItem(0);
        assertNotNull("Should find the menu item with submenu", menuItemWithSubmenu);
        assertEquals(
                "Title of the found submenu item should match",
                menuItemWithSubmenuTitle,
                menuItemWithSubmenu.model.get(TITLE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testCustomContentActions_Link() throws PendingIntent.CanceledException {
        var linkHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ChromeContextMenuPopulator
                                        .getContextualCustomActionTypeSelectedHistogramForTesting(),
                                ChromeContextMenuPopulator.ContextualCustomActionType.LINK)
                        .expectIntRecord(
                                ChromeContextMenuPopulator
                                        .getCustomActionTypeDisplayedHistogramForTesting(),
                                ChromeContextMenuPopulator.ContextualCustomActionType.LINK)
                        .build();

        FirstRunStatus.setFirstRunFlowComplete(true);
        final int linkActionId = 101;
        final String linkDescription = "Custom Link Action";
        PendingIntent mockPendingIntent =
                PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        0,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE);
        CustomContentAction linkAction =
                new CustomContentAction.Builder(
                                linkActionId,
                                linkDescription,
                                mockPendingIntent,
                                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK)
                        .build();

        List<CustomContentAction> customActions = List.of(linkAction);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB,
                getHttpLinkParams(),
                customActions);

        mPopulator.setPendingIntentSenderForTesting(mMockPendingIntentSender);

        List<ModelList> menuState = mPopulator.buildContextMenu();
        assertFalse("Menu should contain at least one group", menuState.isEmpty());

        ListItem customItem = findItemWithTitle(menuState, linkDescription);
        assertNotNull(
                "Custom link item with title '" + linkDescription + "' was not found.", customItem);

        int customItemId = customItem.model.get(MENU_ITEM_ID);
        assertTrue(
                "Custom item ID should be == the starting ID",
                customItemId == ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting());

        assertTrue(
                "Clicking custom link item should be handled.",
                mPopulator.onItemSelected(
                        ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting()));
        linkHistogramWatcher.assertExpected();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPendingIntentSender)
                .send(eq(mockPendingIntent), any(Context.class), eq(0), intentCaptor.capture());

        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(
                "The intent extra for the triggered action id should be the same as the link action"
                        + " id ("
                        + linkActionId
                        + ").",
                linkActionId,
                capturedIntent.getIntExtra(
                        CustomTabsIntent.EXTRA_TRIGGERED_CUSTOM_CONTENT_ACTION_ID, -1));
        assertEquals(
                "The intent extra for the clicked content target type should be LINK.",
                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK,
                capturedIntent.getIntExtra(CustomTabsIntent.EXTRA_CLICKED_CONTENT_TARGET_TYPE, -1));
        assertEquals(
                "The intent extra for the context link URL should match the link's URL.",
                LINK_URL,
                capturedIntent.getStringExtra(CustomTabsIntent.EXTRA_CONTEXT_LINK_URL));
        assertEquals(
                "The intent extra for the context link text should match the link's text.",
                LINK_TEXT,
                capturedIntent.getStringExtra(CustomTabsIntent.EXTRA_CONTEXT_LINK_TEXT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testCustomContentActions_Image() throws PendingIntent.CanceledException {
        var imageHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ChromeContextMenuPopulator
                                        .getContextualCustomActionTypeSelectedHistogramForTesting(),
                                ChromeContextMenuPopulator.ContextualCustomActionType.IMAGE)
                        .expectIntRecord(
                                ChromeContextMenuPopulator
                                        .getCustomActionTypeDisplayedHistogramForTesting(),
                                ChromeContextMenuPopulator.ContextualCustomActionType.IMAGE)
                        .build();

        FirstRunStatus.setFirstRunFlowComplete(true);
        final int imageActionId = 202;
        final String imageDescription = "Custom Image Action";
        PendingIntent mockPendingIntent =
                PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        1,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE);
        CustomContentAction imageAction =
                new CustomContentAction.Builder(
                                imageActionId,
                                imageDescription,
                                mockPendingIntent,
                                CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE)
                        .build();

        List<CustomContentAction> customActions = List.of(imageAction);

        ContextMenuParams imageParams =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        null);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, imageParams, customActions);

        mPopulator.setPendingIntentSenderForTesting(mMockPendingIntentSender);

        doAnswer(
                        (invocation) -> {
                            Callback<Uri> callback = invocation.getArgument(1);
                            callback.onResult(RETRIEVED_IMAGE_URI);
                            return null;
                        })
                .when(mNativeDelegate)
                .retrieveImageForShare(eq(ContextMenuImageFormat.ORIGINAL), any());

        List<ModelList> menuState = mPopulator.buildContextMenu();
        assertFalse("Menu should contain at least one group", menuState.isEmpty());

        ListItem customItem = findItemWithTitle(menuState, imageDescription);
        assertNotNull(
                "Custom image item with title '" + imageDescription + "' was not found.",
                customItem);

        int customItemId = customItem.model.get(MENU_ITEM_ID);
        assertTrue(
                "Custom item ID should be == the starting ID",
                customItemId == ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting());

        assertTrue(
                "Clicking custom image item should be handled.",
                mPopulator.onItemSelected(
                        ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting()));
        imageHistogramWatcher.assertExpected();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPendingIntentSender)
                .send(eq(mockPendingIntent), any(Context.class), eq(0), intentCaptor.capture());

        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(
                "The intent extra for the triggered action id should be the same as the image"
                        + " action id ("
                        + imageActionId
                        + ").",
                imageActionId,
                capturedIntent.getIntExtra(
                        CustomTabsIntent.EXTRA_TRIGGERED_CUSTOM_CONTENT_ACTION_ID, -1));
        assertEquals(
                "The intent extra for the clicked content target type should be IMAGE.",
                CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE,
                capturedIntent.getIntExtra(CustomTabsIntent.EXTRA_CLICKED_CONTENT_TARGET_TYPE, -1));
        assertEquals(
                "The intent extra for the context image URL should match the source URL.",
                IMAGE_SRC_URL,
                capturedIntent.getStringExtra(CustomTabsIntent.EXTRA_CONTEXT_IMAGE_URL));
        assertEquals(
                "The intent extra for the context image alt text should match the title text.",
                IMAGE_TITLE_TEXT,
                capturedIntent.getStringExtra(CustomTabsIntent.EXTRA_CONTEXT_IMAGE_ALT_TEXT));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(
                    "The intent extra for the context image data URI should match the retrieved"
                            + " URI.",
                    RETRIEVED_IMAGE_URI,
                    capturedIntent.getParcelableExtra(
                            CustomTabsIntent.EXTRA_CONTEXT_IMAGE_DATA_URI, Uri.class));
        }
        assertEquals(
                "The intent's data URI should match the page URL.",
                PAGE_URL,
                capturedIntent.getData().toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testCustomContentActions_ImageLink_DoesNotSetPageUri()
            throws PendingIntent.CanceledException {
        var imageHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ChromeContextMenuPopulator
                                        .getContextualCustomActionTypeSelectedHistogramForTesting(),
                                ChromeContextMenuPopulator.ContextualCustomActionType.IMAGE)
                        .expectIntRecord(
                                ChromeContextMenuPopulator
                                        .getCustomActionTypeDisplayedHistogramForTesting(),
                                ChromeContextMenuPopulator.ContextualCustomActionType.IMAGE)
                        .build();

        FirstRunStatus.setFirstRunFlowComplete(true);
        final int imageActionId = 202;
        final String imageDescription = "Custom Image Action";
        PendingIntent mockPendingIntent =
                PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        1,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE);
        CustomContentAction imageAction =
                new CustomContentAction.Builder(
                                imageActionId,
                                imageDescription,
                                mockPendingIntent,
                                CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE)
                        .build();

        List<CustomContentAction> customActions = List.of(imageAction);

        ContextMenuParams imageParams =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        "",
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        null);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, imageParams, customActions);

        mPopulator.setPendingIntentSenderForTesting(mMockPendingIntentSender);

        doAnswer(
                        (invocation) -> {
                            Callback<Uri> callback = invocation.getArgument(1);
                            callback.onResult(RETRIEVED_IMAGE_URI);
                            return null;
                        })
                .when(mNativeDelegate)
                .retrieveImageForShare(eq(ContextMenuImageFormat.ORIGINAL), any());

        List<ModelList> menuState = mPopulator.buildContextMenu();
        assertFalse("Menu should contain at least one group", menuState.isEmpty());

        ListItem customItem = findItemWithTitle(menuState, imageDescription);
        assertNotNull(
                "Custom image item with title '" + imageDescription + "' was not found.",
                customItem);

        int customItemId = customItem.model.get(MENU_ITEM_ID);
        assertTrue(
                "Custom item ID should be == the starting ID",
                customItemId == ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting());

        assertTrue(
                "Clicking custom image item should be handled.",
                mPopulator.onItemSelected(
                        ChromeContextMenuPopulator.getCustomMenuItemIdStartForTesting()));
        imageHistogramWatcher.assertExpected();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPendingIntentSender)
                .send(eq(mockPendingIntent), any(Context.class), eq(0), intentCaptor.capture());

        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(
                "The page uri should be set for image-link items.",
                PAGE_URL,
                capturedIntent.getData().toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testCustomContentActions_enforcesLimitWithMixedActionTypes() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        List<CustomContentAction> oversizedActionList = new ArrayList<>();
        // Action 0 (Link) - Expected to be included
        oversizedActionList.add(
                new CustomContentAction.Builder(
                                100,
                                "Link Action 0",
                                PendingIntent.getBroadcast(
                                        ContextUtils.getApplicationContext(),
                                        0,
                                        new Intent(),
                                        PendingIntent.FLAG_IMMUTABLE),
                                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK)
                        .build());
        // Action 1 (Image) - Expected to be included
        oversizedActionList.add(
                new CustomContentAction.Builder(
                                101,
                                "Image Action 1",
                                PendingIntent.getBroadcast(
                                        ContextUtils.getApplicationContext(),
                                        1,
                                        new Intent(),
                                        PendingIntent.FLAG_IMMUTABLE),
                                CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE)
                        .build());
        // Action 2 (Link) - Expected to be included
        oversizedActionList.add(
                new CustomContentAction.Builder(
                                102,
                                "Link Action 2",
                                PendingIntent.getBroadcast(
                                        ContextUtils.getApplicationContext(),
                                        2,
                                        new Intent(),
                                        PendingIntent.FLAG_IMMUTABLE),
                                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK)
                        .build());
        // Action 3 (Image) - Expected to be included
        oversizedActionList.add(
                new CustomContentAction.Builder(
                                103,
                                "Image Action 3",
                                PendingIntent.getBroadcast(
                                        ContextUtils.getApplicationContext(),
                                        3,
                                        new Intent(),
                                        PendingIntent.FLAG_IMMUTABLE),
                                CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE)
                        .build());
        // Action 4 (Link) - Expected to be EXCLUDED
        oversizedActionList.add(
                new CustomContentAction.Builder(
                                104,
                                "Link Action 4",
                                PendingIntent.getBroadcast(
                                        ContextUtils.getApplicationContext(),
                                        4,
                                        new Intent(),
                                        PendingIntent.FLAG_IMMUTABLE),
                                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK)
                        .build());
        // Action 5 (Image) - Expected to be EXCLUDED
        oversizedActionList.add(
                new CustomContentAction.Builder(
                                105,
                                "Image Action 5",
                                PendingIntent.getBroadcast(
                                        ContextUtils.getApplicationContext(),
                                        5,
                                        new Intent(),
                                        PendingIntent.FLAG_IMMUTABLE),
                                CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE)
                        .build());

        ContextMenuParams mixedParams =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        new GURL(PAGE_URL),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT,
                        null,
                        true,
                        0,
                        0,
                        MenuSourceType.TOUCH,
                        false,
                        false,
                        0,
                        null);

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB,
                mixedParams,
                oversizedActionList);

        List<ModelList> menuState = mPopulator.buildContextMenu();

        assertNotNull(
                "Expected 'Link Action 0' to be present.",
                findItemWithTitle(menuState, "Link Action 0"));
        assertNotNull(
                "Expected 'Image Action 1' to be present.",
                findItemWithTitle(menuState, "Image Action 1"));
        assertNotNull(
                "Expected 'Link Action 2' to be present.",
                findItemWithTitle(menuState, "Link Action 2"));
        assertNotNull(
                "Expected 'Image Action 3' to be present.",
                findItemWithTitle(menuState, "Image Action 3"));

        assertNull(
                "'Link Action 4' should have been excluded.",
                findItemWithTitle(menuState, "Link Action 4"));
        assertNull(
                "'Image Action 5' should have been excluded.",
                findItemWithTitle(menuState, "Image Action 5"));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testHasCustomContextItems_DoesHaveWithFlagEnabled() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        List<CustomContentAction> customActions =
                List.of(
                        createSimpleContentAction(
                                /** actionId= */
                                101));

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB,
                getHttpLinkParams(),
                customActions);

        assertTrue("Custom context menu items should be present.", mPopulator.hasCustomItems());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testHasCustomContextItems_HasNoneWithFlagEnabled() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB,
                getHttpLinkParams(),
                /** actions= */
                List.of());

        assertFalse("Custom context menu items should be present.", mPopulator.hasCustomItems());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testHasCustomContextItems_ShouldNotHaveWithFlagDisabled() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        List<CustomContentAction> customActions =
                List.of(
                        createSimpleContentAction(
                                /** actionId= */
                                101));

        initializePopulator(
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB,
                getHttpLinkParams(),
                customActions);

        assertFalse(
                "Custom context menu items should not be present when the flag is disabled.",
                mPopulator.hasCustomItems());
    }

    private CustomContentAction createSimpleContentAction(int actionId) {
        PendingIntent mockPendingIntent =
                PendingIntent.getBroadcast(
                        ContextUtils.getApplicationContext(),
                        0,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE);
        CustomContentAction action =
                new CustomContentAction.Builder(
                                actionId,
                                "Custom Link Action",
                                mockPendingIntent,
                                CustomTabsIntent.CONTENT_TARGET_TYPE_LINK)
                        .build();

        return action;
    }

    /**
     * Searches through all generated menu groups to find a menu item with a specific title.
     *
     * @param menuState The list of ModelLists generated by the populator.
     * @param title The title of the menu item to find.
     * @return The {@link ListItem} if found, otherwise {@code null}.
     */
    private ListItem findItemWithTitle(List<ModelList> menuState, String title) {
        for (ModelList group : menuState) {
            for (ListItem item : group) {
                if (item.type == MENU_ITEM) {
                    if (title.equals(item.model.get(TITLE))) {
                        return item;
                    }
                }
            }
        }
        return null;
    }

    private void checkOpenInNewWindowItems(boolean isIncognitoWindowFeatureEnabled) {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params = getHttpLinkParams();

        when(mItemDelegate.isIncognito()).thenReturn(false);
        when(mItemDelegate.isIncognitoSupported()).thenReturn(true);
        when(mItemDelegate.canEnterMultiWindowMode()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        doReturn(true).when(mPopulator).isTabletScreen();

        List<Integer> expectedItems = new ArrayList<>();
        expectedItems.add(R.id.contextmenu_open_in_new_tab);
        expectedItems.add(R.id.contextmenu_open_in_new_tab_in_group);

        if (isIncognitoWindowFeatureEnabled) {
            expectedItems.add(R.id.contextmenu_open_in_new_window);
            expectedItems.add(R.id.contextmenu_open_in_incognito_window);
        } else {
            expectedItems.add(R.id.contextmenu_open_in_incognito_tab);
            expectedItems.add(R.id.contextmenu_open_in_new_window);
        }

        expectedItems.add(R.id.contextmenu_open_in_ephemeral_tab);
        expectedItems.add(R.id.contextmenu_copy_link_address);
        expectedItems.add(R.id.contextmenu_copy_link_text);
        expectedItems.add(R.id.contextmenu_save_link_as);
        expectedItems.add(R.id.contextmenu_read_later);
        expectedItems.add(R.id.contextmenu_share_link);

        int[] expectedItemsArray = expectedItems.stream().mapToInt(Integer::intValue).toArray();
        checkMenuOptions(expectedItemsArray);
    }
}
