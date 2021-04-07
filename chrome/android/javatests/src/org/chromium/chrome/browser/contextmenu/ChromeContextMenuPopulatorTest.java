// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.TEXT;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST;

import android.app.Activity;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuMode;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/**
 * Unit tests for the context menu logic of Chrome.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ChromeContextMenuPopulatorTest {
    private static final String PAGE_URL = "http://www.blah.com";
    private static final String LINK_URL = "http://www.blah.com/other_blah";
    private static final String LINK_TEXT = "BLAH!";
    private static final String IMAGE_SRC_URL = "http://www.blah.com/image.jpg";
    private static final String IMAGE_TITLE_TEXT = "IMAGE!";

    @Mock
    private Activity mActivity;
    @Mock
    private ContextMenuItemDelegate mItemDelegate;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private ShareDelegate mShareDelegate;
    @Mock
    private ExternalAuthUtils mExternalAuthUtils;
    @Mock
    private ContextMenuNativeDelegate mNativeDelegate;

    // Despite this being a spy, we add the @Mock annotation so that proguard doesn't strip the
    // spied class.
    @Mock
    private ChromeContextMenuPopulator mPopulator;

    private boolean mSupportsOpenInChromeFromCct = true;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        GURL pageUrl = new GURL(PAGE_URL);
        when(mItemDelegate.getPageUrl()).thenReturn(pageUrl);
        when(mItemDelegate.isIncognitoSupported()).thenReturn(true);
        when(mItemDelegate.isOpenInOtherWindowSupported()).thenReturn(true);
        when(mItemDelegate.supportsCall()).thenReturn(true);
        when(mItemDelegate.supportsSendEmailMessage()).thenReturn(true);
        when(mItemDelegate.supportsSendTextMessage()).thenReturn(true);
        when(mItemDelegate.supportsAddToContacts()).thenReturn(true);
        when(mItemDelegate.supportsOpenInChromeFromCct())
                .thenAnswer((mock) -> mSupportsOpenInChromeFromCct);

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, false);
        features.put(ChromeFeatureList.EPHEMERAL_TAB_USING_BOTTOM_SHEET, false);
        features.put(ChromeFeatureList.READ_LATER, false);

        ChromeFeatureList.setTestFeatures(features);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ApplicationStatus.resetActivitiesForInstrumentationTests(); });
    }

    private void initializePopulator(@ContextMenuMode int mode, ContextMenuParams params) {
        mPopulator = Mockito.spy(new ChromeContextMenuPopulator(mItemDelegate,
                ()
                        -> mShareDelegate,
                mode, mExternalAuthUtils, ContextUtils.getApplicationContext(), params,
                mNativeDelegate));
        doReturn(mTemplateUrlService).when(mPopulator).getTemplateUrlService();
        doReturn(false).when(mPopulator).shouldTriggerEphemeralTabHelpUi();
        doReturn(false).when(mPopulator).shouldTriggerReadLaterHelpUi();
        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);
    }

    private void checkMenuOptions(int[]... tabs) {
        List<Pair<Integer, ModelList>> contextMenuState = mPopulator.buildContextMenu();

        assertEquals("Number of groups doesn't match", tabs[0] == null ? 0 : tabs.length,
                contextMenuState.size());

        for (int i = 0; i < contextMenuState.size(); i++) {
            int[] availableInTab = new int[contextMenuState.get(i).second.size()];
            for (int j = 0; j < contextMenuState.get(i).second.size(); j++) {
                availableInTab[j] = contextMenuState.get(i).second.get(j).model.get(MENU_ID);
            }

            if (!Arrays.equals(tabs[i], availableInTab)) {
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

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL),
                new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false,
                0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        int[] expected = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {R.id.contextmenu_open_in_browser_id, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link,
                R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldShowOpenInChromeMenuItemInContextMenu() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL),
                new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false,
                0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        // If the delegate returns false from supportsOpenInChromeFromCct() then open_in_chrome item
        // should not be present.
        mSupportsOpenInChromeFromCct = false;
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link};
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLinkWithPreviewTabEnabled() {
        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL),
                new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false,
                0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        FirstRunStatus.setFirstRunFlowComplete(true);

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.EPHEMERAL_TAB_USING_BOTTOM_SHEET, true);
        features.put(ChromeFeatureList.READ_LATER, false);
        ChromeFeatureList.setTestFeatures(features);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected1 = {R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_open_in_ephemeral_tab,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link};
        checkMenuOptions(expected1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected2 = {R.id.contextmenu_open_in_browser_id,
                R.id.contextmenu_open_in_ephemeral_tab, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        checkMenuOptions(expected2);

        // Webapp doesn't show preview tab.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected3 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link,
                R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected3);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMailLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL mailto = new GURL("mailto:fake@email.com");
        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL), mailto, "MAIL!",
                GURL.emptyGURL(), new GURL(PAGE_URL), "", null, false, 0, 0,
                MenuSourceType.MENU_SOURCE_TOUCH);

        int[] expected = {R.id.contextmenu_copy};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {R.id.contextmenu_share_link, R.id.contextmenu_send_message,
                R.id.contextmenu_add_to_contacts, R.id.contextmenu_copy};
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {R.id.contextmenu_open_in_browser_id, R.id.contextmenu_share_link,
                R.id.contextmenu_send_message, R.id.contextmenu_add_to_contacts,
                R.id.contextmenu_copy};
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {R.id.contextmenu_share_link, R.id.contextmenu_send_message,
                R.id.contextmenu_add_to_contacts, R.id.contextmenu_copy,
                R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTelLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL tel = new GURL("tel:0048221234567");
        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL), tel, "PHONE!",
                GURL.emptyGURL(), new GURL(PAGE_URL), "", null, false, 0, 0,
                MenuSourceType.MENU_SOURCE_TOUCH);

        int[] expected = {R.id.contextmenu_copy};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {R.id.contextmenu_share_link, R.id.contextmenu_call,
                R.id.contextmenu_send_message, R.id.contextmenu_add_to_contacts,
                R.id.contextmenu_copy};
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {R.id.contextmenu_open_in_browser_id, R.id.contextmenu_share_link,
                R.id.contextmenu_call, R.id.contextmenu_send_message,
                R.id.contextmenu_add_to_contacts, R.id.contextmenu_copy};
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {R.id.contextmenu_share_link, R.id.contextmenu_call,
                R.id.contextmenu_send_message, R.id.contextmenu_add_to_contacts,
                R.id.contextmenu_copy, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testVideoLink() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        GURL sourceUrl = new GURL("http://www.blah.com/");
        GURL url = new GURL(sourceUrl.getSpec() + "I_love_mouse_video.avi");
        ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.VIDEO,
                new GURL(PAGE_URL), url, "VIDEO!", GURL.emptyGURL(), sourceUrl, "", null, true, 0,
                0, MenuSourceType.MENU_SOURCE_TOUCH);

        int[] expectedTab1 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expectedTab1);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expectedTab1);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_incognito_tab, R.id.contextmenu_open_in_other_window,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link};
        int[] expected2Tab2 = {R.id.contextmenu_save_video};
        checkMenuOptions(expected2Tab1, expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3Tab1 = {R.id.contextmenu_open_in_browser_id,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link};
        checkMenuOptions(expected3Tab1, expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4Tab1 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_share_link};
        int[] expected4Tab2 = {R.id.contextmenu_save_video, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4Tab1, expected4Tab2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testImageHiFi() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                new GURL(PAGE_URL), GURL.emptyGURL(), "", GURL.emptyGURL(), new GURL(IMAGE_SRC_URL),
                IMAGE_TITLE_TEXT, null, true, 0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        int[] expected = null;
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_copy_image,
                R.id.contextmenu_save_image, R.id.contextmenu_share_image};
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3 = {R.id.contextmenu_open_in_browser_id, R.id.contextmenu_open_image,
                R.id.contextmenu_copy_image, R.id.contextmenu_save_image,
                R.id.contextmenu_share_image};
        checkMenuOptions(expected3);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4 = {R.id.contextmenu_copy_image, R.id.contextmenu_save_image,
                R.id.contextmenu_share_image, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLinkWithImageHiFi() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        ContextMenuParams params =
                new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE, new GURL(PAGE_URL),
                        new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), new GURL(IMAGE_SRC_URL),
                        IMAGE_TITLE_TEXT, null, true, 0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        int[] expected = {R.id.contextmenu_copy_link_address};

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        checkMenuOptions(expected);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        checkMenuOptions(expected);

        FirstRunStatus.setFirstRunFlowComplete(true);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2Tab1 = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_incognito_tab, R.id.contextmenu_open_in_other_window,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        int[] expected2Tab2 = {R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_copy_image,
                R.id.contextmenu_save_image, R.id.contextmenu_share_image};
        checkMenuOptions(expected2Tab1, expected2Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected3Tab1 = {R.id.contextmenu_open_in_browser_id,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        int[] expected3Tab2 = {R.id.contextmenu_open_image, R.id.contextmenu_copy_image,
                R.id.contextmenu_save_image, R.id.contextmenu_share_image};
        checkMenuOptions(expected3Tab1, expected3Tab2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected4Tab1 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        int[] expected4Tab2 = {R.id.contextmenu_copy_image, R.id.contextmenu_save_image,
                R.id.contextmenu_share_image, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected4Tab1, expected4Tab2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testReadLater() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.EPHEMERAL_TAB_USING_BOTTOM_SHEET, false);
        features.put(ChromeFeatureList.READ_LATER, true);
        ChromeFeatureList.setTestFeatures(features);

        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL),
                new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false,
                0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        // HTTP scheme should include read later context menu item.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected = {R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_read_later, R.id.contextmenu_share_link};
        checkMenuOptions(expected);

        // Custom tab should include read later.
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB, params);
        int[] expected2 = {R.id.contextmenu_open_in_browser_id, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_read_later, R.id.contextmenu_share_link};
        checkMenuOptions(expected2);

        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.WEB_APP, params);
        int[] expected3 = {R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_save_link_as, R.id.contextmenu_read_later,
                R.id.contextmenu_share_link, R.id.contextmenu_open_in_chrome};
        checkMenuOptions(expected3);

        // Non-http scheme should not include read later context menu item.
        params = new ContextMenuParams(0, 0, new GURL("chrome://flags"), new GURL(LINK_URL),
                LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false, 0, 0,
                MenuSourceType.MENU_SOURCE_TOUCH);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected4 = {R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        checkMenuOptions(expected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testIncognito() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.EPHEMERAL_TAB_USING_BOTTOM_SHEET, false);
        features.put(ChromeFeatureList.READ_LATER, true);
        ChromeFeatureList.setTestFeatures(features);

        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL),
                new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false,
                0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        when(mItemDelegate.isIncognito()).thenReturn(true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expectedIncognito = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_read_later,
                R.id.contextmenu_share_link};
        checkMenuOptions(expectedIncognito);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHttpLink_WithoutAutoGroupCreation() {
        TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.setForTesting(false);

        FirstRunStatus.setFirstRunFlowComplete(true);

        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL(PAGE_URL),
                new GURL(LINK_URL), LINK_TEXT, GURL.emptyGURL(), GURL.emptyGURL(), "", null, false,
                0, 0, MenuSourceType.MENU_SOURCE_TOUCH);

        // Show "open in new tab" item first
        SharedPreferencesManager.getInstance().writeBoolean(
                CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST, false);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_new_tab_in_group, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        checkMenuOptions(expected);

        // Show "open in new tab in group" item first
        SharedPreferencesManager.getInstance().writeBoolean(
                CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST, true);
        initializePopulator(ChromeContextMenuPopulator.ContextMenuMode.NORMAL, params);
        int[] expected2 = {R.id.contextmenu_open_in_new_tab_in_group,
                R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_open_in_other_window, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_save_link_as,
                R.id.contextmenu_share_link};
        checkMenuOptions(expected2);

        // Clean up
        SharedPreferencesManager.getInstance().removeKey(
                CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST);
        TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.setForTesting(true);
    }
}
