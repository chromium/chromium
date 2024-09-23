// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.ENABLED;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;

import android.app.Activity;
import android.net.Uri;
import android.util.Pair;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuMode;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for the context menu logic of Chrome. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ChromeContextMenuPopulatorTest {
    private static final String PAGE_URL = "http://www.blah.com/page_url";
    private static final String LINK_URL = "http://www.blah.com/other_blah";
    private static final String LINK_TEXT = "BLAH!";
    private static final String IMAGE_SRC_URL = "http://www.blah.com/image.jpg";
    private static final String IMAGE_TITLE_TEXT = "IMAGE!";
    private static final String RETRIEVED_IMAGE_URL = "http://www.blah.com/retrieved_image.jpg";

    @Rule public JniMocker jniMocker = new JniMocker();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveRule =
            new AutomotiveContextWrapperTestRule();

    @Mock private Activity mActivity;
    @Mock private TabContextMenuItemDelegate mItemDelegate;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private ContextMenuNativeDelegate mNativeDelegate;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;
    @Mock private Profile.Natives mProfileNatives;

    // Despite this being a spy, we add the @Mock annotation so that proguard doesn't strip the
    // spied class.
    @Mock private ChromeContextMenuPopulator mPopulator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mAutomotiveRule.setIsAutomotive(false);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(false);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        GURL pageUrl = new GURL(PAGE_URL);
        when(mItemDelegate.getPageUrl()).thenReturn(pageUrl);
        when(mItemDelegate.isIncognitoSupported()).thenReturn(true);
        when(mItemDelegate.supportsCall()).thenReturn(true);
        when(mItemDelegate.supportsSendEmailMessage()).thenReturn(true);
        when(mItemDelegate.supportsSendTextMessage()).thenReturn(true);
        when(mItemDelegate.supportsAddToContacts()).thenReturn(true);
        when(mItemDelegate.getWebContents()).thenReturn(mWebContents);

        jniMocker.mock(ProfileJni.TEST_HOOKS, mProfileNatives);
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
        mPopulator =
                Mockito.spy(
                        new ChromeContextMenuPopulator(
                                mItemDelegate,
                                () -> mShareDelegate,
                                mode,
                                mExternalAuthUtils,
                                ContextUtils.getApplicationContext(),
                                params,
                                mNativeDelegate));
        doReturn(mTemplateUrlService).when(mPopulator).getTemplateUrlService();
        doReturn(false).when(mPopulator).shouldTriggerEphemeralTabHelpUi();
        doReturn(false).when(mPopulator).shouldTriggerReadLaterHelpUi();
        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);
    }

    private void checkMenuOptions(List<Integer> disabled, int[]... groups) {
        List<Pair<Integer, ModelList>> contextMenuState = mPopulator.buildContextMenu();

        assertEquals(
                "Number of groups doesn't match",
                groups[0] == null ? 0 : groups.length,
                contextMenuState.size());

        for (int i = 0; i < contextMenuState.size(); i++) {
            int[] availableInTab = new int[contextMenuState.get(i).second.size()];
            for (int j = 0; j < contextMenuState.get(i).second.size(); j++) {
                PropertyModel model = contextMenuState.get(i).second.get(j).model;
                if (disabled.contains(model.get(MENU_ID))) {
                    assertFalse(model.get(ENABLED));
                } else {
                    assertTrue(model.get(ENABLED));
                }
                availableInTab[j] = model.get(MENU_ID);
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
                StringBuilder info = new StringBuilder();
                for (int j = 0; j < contextMenuState.get(i).second.size(); j++) {
                    info.append("'");
                    info.append(contextMenuState.get(i).second.get(j).model.get(TEXT));
                    info.append("' ");
                }
                fail("Items in group " + i + " don't match, generated: " + info.toString());
            }
        }
    }

    private void checkMenuOptions(int[]... groups) {
        checkMenuOptions(/* disabled= */ new ArrayList<Integer>(), groups);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLinkWithDownloadBlockedByPolicy() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLinkWithPreviewTabEnabled() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected1 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
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
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testVideoLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL sourceUrl = new GURL("http://www.blah.com/");
        GURL url = new GURL(sourceUrl.getSpec() + "I_love_mouse_video.avi");
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.VIDEO,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        int[] expectedTab1 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expectedTab1);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testVideoLinkWithDownloadBlockedByPolicy() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        DownloadUtils.setIsDownloadRestrictedByPolicyForTesting(true);
        GURL sourceUrl = new GURL("http://www.blah.com/");
        GURL url = new GURL(sourceUrl.getSpec() + "I_love_mouse_video.avi");
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.VIDEO,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testImageHiFi() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        int[] expected = null;
        checkMenuOptions(expected);

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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLinkWithImageHiFi() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        int[] expected = {R.id.contextmenu_copy_link_address};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
                        ContextMenuDataMediaType.IMAGE,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testReadLater() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        // HTTP scheme should include read later context menu item.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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

        // Non-http scheme should not include read later context menu item.
        params =
                new ContextMenuParams(
                        0,
                        0,
                        new GURL("chrome://flags"),
                        new GURL(LINK_URL),
                        LINK_TEXT,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected4 = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testIncognito() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        when(mItemDelegate.isIncognito()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expectedIncognito = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expectedIncognito);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOpenInOtherWindow() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        when(mItemDelegate.isOpenInOtherWindowSupported()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expectedMultiWindow = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
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
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOpenInNewWindow() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        when(mItemDelegate.canEnterMultiWindowMode()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        doReturn(true).when(mPopulator).isTabletScreen();
        int[] expectedMultiWindow = {
            R.id.contextmenu_open_in_new_tab_in_group,
            R.id.contextmenu_open_in_new_tab,
            R.id.contextmenu_open_in_incognito_tab,
            R.id.contextmenu_open_in_new_window,
            R.id.contextmenu_open_in_ephemeral_tab,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_save_link_as,
            R.id.contextmenu_read_later,
            R.id.contextmenu_share_link
        };
        checkMenuOptions(expectedMultiWindow);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetLensIntentParams() {
        when(mItemDelegate.isIncognito()).thenReturn(true);
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
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
                        /* mediaType= */ 0,
                        /* pageUrl= */ GURL.emptyGURL(),
                        /* linkUrl= */ GURL.emptyGURL(),
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ GURL.emptyGURL(),
                        /* srcUrl= */ GURL.emptyGURL(),
                        /* titleText= */ "",
                        /* referrer= */ null,
                        /* canSaveMedia= */ false,
                        /* triggeringTouchXDp= */ 0,
                        /* triggeringTouchXDp= */ 0,
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        /* openedFromHighlight= */ true,
                        /* additionalNavigationParams= */ null);

        // In normal mode, there should be three options: share, remove and learn more.
        int[] normal_expected = {
            R.id.contextmenu_share_highlight,
            R.id.contextmenu_remove_highlight,
            R.id.contextmenu_learn_more
        };
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(normal_expected);

        // In custom tab or web app mode, only the remove option should be present.
        int[] other_expected = {R.id.contextmenu_remove_highlight};
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(other_expected);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
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
                        0,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
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

        ContextMenuParams imageParams =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
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
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
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
    }
}
