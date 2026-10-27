// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.style.ClickableSpan;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.multiwindow.SessionStartupPref;
import org.chromium.chrome.browser.multiwindow.TabbedStartupCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.containment.ContainedLinearLayout;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Instrumentation tests for {@link OnStartupSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ON_STARTUP_SETTINGS_UI)
public class OnStartupSettingsFragmentTest {
    private static final String URL_ESPN = "https://www.espn.com/";
    private static final String URL_WIKIPEDIA = "https://www.wikipedia.org/";
    private static final String URL_DOCS = "https://docs.google.com/document/u/0/";
    private static final String URL_NTP = "chrome-native://newtab/";
    private static final String URL_SETTINGS = "chrome://settings/";

    @Rule
    public final SettingsTestRule<OnStartupSettingsFragment> mSettingsTestRule =
            new SettingsTestRule<>(OnStartupSettingsFragment.class);

    private Profile mProfile;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mProfile = ProfileManager.getLastUsedRegularProfile());
        setRestoreOnStartupUrls(List.of());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                TabWindowManagerSingleton::resetTabModelSelectorFactoryForTesting);
        setRestoreOnStartupUrls(List.of());
        setRestoreOnStartupPref(SessionStartupPref.LAST);
    }

    @Test
    @SmallTest
    public void testInitialSelection_matchesStoredStartupPref() {
        // Setup stored startup preference as URLS.
        setRestoreOnStartupPref(SessionStartupPref.URLS);

        // Act.
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();

        // Verify initial selection matches URLS and the other radio buttons are unchecked.
        assertFalse(pref.getNewTabPrefForTesting().isChecked());
        assertFalse(pref.getLastPrefForTesting().isChecked());
        assertTrue(pref.getUrlsPrefForTesting().isChecked());
    }

    @Test
    @SmallTest
    public void testSelectingRadioButtons_updatesPrefService() {
        // Setup stored startup preference as NEW_TAB.
        setRestoreOnStartupPref(SessionStartupPref.NEW_TAB);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        assertTrue(pref.getNewTabPrefForTesting().isChecked());
        assertEquals(View.GONE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertTrue(pref.getUrlsPrefForTesting().getParent() instanceof ContainedLinearLayout);
        assertEquals(
                pref.getUrlsPrefForTesting().getParent(),
                pref.getStartupPagesSectionForTesting().getParent());

        // Act & Verify selecting LAST updates PrefService and hides startup pages section.
        ThreadUtils.runOnUiThreadBlocking(() -> pref.getLastPrefForTesting().performClick());
        assertEquals(View.GONE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertEquals(SessionStartupPref.LAST, getRestoreOnStartupPref());

        // Act & Verify selecting URLS updates PrefService and reveals startup pages section.
        ThreadUtils.runOnUiThreadBlocking(() -> pref.getUrlsPrefForTesting().performClick());
        assertEquals(View.VISIBLE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertEquals(SessionStartupPref.URLS, getRestoreOnStartupPref());
    }

    @Test
    @SmallTest
    public void testNewTabPref_displaysSummaryWithHomepageSettingsLink() {
        // Setup fragment and mock SettingsNavigation.
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        SettingsNavigation settingsNavigation = mock(SettingsNavigation.class);
        SettingsNavigationFactory.setInstanceForTesting(settingsNavigation);
        TextViewWithClickableSpans descriptionView =
                pref.getNewTabPrefForTesting().findViewById(R.id.description);

        // Act: Collect keyboard-focusable views in forward TAB order, focus the description view,
        // and press Enter on the focused span.
        ArrayList<View> focusables = new ArrayList<>();
        boolean handled =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            pref.getNewTabPrefForTesting()
                                    .addFocusables(
                                            focusables, View.FOCUS_FORWARD, View.FOCUSABLES_ALL);
                            descriptionView
                                    .getOnFocusChangeListener()
                                    .onFocusChange(descriptionView, /* hasFocus= */ true);
                            descriptionView.dispatchKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
                            return descriptionView.dispatchKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
                        });

        // Verify summary text, keyboard TAB focusability, clickable span, and navigation to
        // HomepageSettings.
        assertNotNull(descriptionView);
        assertEquals(View.VISIBLE, descriptionView.getVisibility());
        assertTrue(descriptionView.isFocusable());
        assertEquals(List.of(pref.getNewTabPrefForTesting(), descriptionView), focusables);
        assertEquals(
                "You can customize your Homepage in settings. Defaults to the New Tab page if none"
                        + " is set.",
                pref.getNewTabPrefForTesting().getDescriptionText().toString());
        ClickableSpan[] spans = descriptionView.getClickableSpans();
        assertNotNull(spans);
        assertEquals(1, spans.length);
        assertTrue(spans[0] instanceof ChromeClickableSpan);
        assertTrue(handled);
        verify(settingsNavigation).startSettings(any(), eq(HomepageSettings.class));
    }

    @Test
    @SmallTest
    public void testStartupPagesList_displaysConfiguredUrls() {
        // Setup configured startup URLs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN, URL_WIKIPEDIA, URL_DOCS));

        // Act.
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();

        // Verify fallback display origin when unvisited, full URL with scheme and path, and menu
        // content description.
        assertNotNull(container);
        assertEquals(3, container.getChildCount());
        View firstItem = container.getChildAt(0);
        TextView firstTitle = firstItem.findViewById(R.id.startup_page_title);
        TextView firstUrl = firstItem.findViewById(R.id.startup_page_url);
        ListMenuButton firstMoreButton = firstItem.findViewById(R.id.startup_page_more_button);
        assertEquals("https://www.espn.com", firstTitle.getText().toString());
        assertEquals("https://www.espn.com", firstUrl.getText().toString());
        assertTrue(
                firstMoreButton
                        .getContentDescription()
                        .toString()
                        .contains("https://www.espn.com"));

        View secondItem = container.getChildAt(1);
        TextView secondTitle = secondItem.findViewById(R.id.startup_page_title);
        TextView secondUrl = secondItem.findViewById(R.id.startup_page_url);
        assertEquals("https://www.wikipedia.org", secondTitle.getText().toString());
        assertEquals("https://www.wikipedia.org", secondUrl.getText().toString());

        View thirdItem = container.getChildAt(2);
        TextView thirdTitle = thirdItem.findViewById(R.id.startup_page_title);
        TextView thirdUrl = thirdItem.findViewById(R.id.startup_page_url);
        assertEquals("https://docs.google.com", thirdTitle.getText().toString());
        assertEquals("https://docs.google.com/document/u/0/", thirdUrl.getText().toString());
    }

    @Test
    @SmallTest
    public void testStartupPagesList_displaysWebsiteTitlesAndFavicons() {
        // Setup preference in URLS mode with custom title and favicon loaders.
        Drawable espnFavicon = new ColorDrawable(Color.RED);
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        pref.initialize(
                                SessionStartupPref.URLS,
                                List.of(URL_ESPN, URL_WIKIPEDIA),
                                startupType -> {},
                                new RadioButtonGroupOnStartupPreference
                                        .StartupPageItemActionListener() {
                                    @Override
                                    public void onAddNewStartupPage() {}

                                    @Override
                                    public void onEditStartupPage(int index, String url) {}

                                    @Override
                                    public void onRemoveStartupPage(int index, String url) {}

                                    @Override
                                    public void onUseCurrentPages() {}
                                },
                                (url, callback) -> {
                                    if (URL_ESPN.equals(url)) {
                                        callback.onResult(espnFavicon);
                                    }
                                },
                                (url, callback) -> {
                                    callback.onResult(
                                            URL_ESPN.equals(url)
                                                    ? "ESPN - Serving Sports Fans"
                                                    : "");
                                }));
        LinearLayout container = pref.getStartupPagesContainerForTesting();

        // Verify resolved website title and favicon on first row and origin fallback on second row.
        assertNotNull(container);
        assertEquals(2, container.getChildCount());
        View firstItem = container.getChildAt(0);
        ImageView firstFavicon = firstItem.findViewById(R.id.startup_page_favicon);
        TextView firstTitle = firstItem.findViewById(R.id.startup_page_title);
        TextView firstUrl = firstItem.findViewById(R.id.startup_page_url);
        assertEquals(espnFavicon, firstFavicon.getDrawable());
        assertEquals("ESPN - Serving Sports Fans", firstTitle.getText().toString());
        assertEquals("https://www.espn.com", firstUrl.getText().toString());

        View secondItem = container.getChildAt(1);
        TextView secondTitle = secondItem.findViewById(R.id.startup_page_title);
        TextView secondUrl = secondItem.findViewById(R.id.startup_page_url);
        assertEquals("https://www.wikipedia.org", secondTitle.getText().toString());
        assertEquals("https://www.wikipedia.org", secondUrl.getText().toString());
    }

    @Test
    @SmallTest
    public void testRemoveStartupPage_removesRowAndUpdatesPrefService() {
        // Setup two configured startup URLs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN, URL_WIKIPEDIA));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);
        assertEquals(2, container.getChildCount());

        // Act: Open the overflow menu on the first row and click "Remove".
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListMenuButton moreButton =
                            container.getChildAt(0).findViewById(R.id.startup_page_more_button);
                    moreButton.callOnClick();
                });
        onView(withText(R.string.remove)).perform(click());

        // Verify the first URL is removed from both the UI list and PrefService.
        assertEquals(1, container.getChildCount());
        TextView remainingUrl = container.getChildAt(0).findViewById(R.id.startup_page_url);
        assertEquals("https://www.wikipedia.org", remainingUrl.getText().toString());
        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(URL_WIKIPEDIA), savedUrls);
    }

    @Test
    @SmallTest
    public void testExternalPrefChanges_updatesRadioSelectionAndStartupPagesList() {
        // Setup initial startup preference as LAST.
        setRestoreOnStartupPref(SessionStartupPref.LAST);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertTrue(pref.getLastPrefForTesting().isChecked());
        assertEquals(View.GONE, pref.getStartupPagesSectionForTesting().getVisibility());

        // Act: Update startup type to URLS and populate startup URLs after launch.
        setRestoreOnStartupUrls(List.of(URL_DOCS));
        setRestoreOnStartupPref(SessionStartupPref.URLS);

        // Verify radio selection and startup pages list update dynamically.
        assertTrue(pref.getUrlsPrefForTesting().isChecked());
        assertEquals(View.VISIBLE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertNotNull(container);
        assertEquals(1, container.getChildCount());
        TextView urlView = container.getChildAt(0).findViewById(R.id.startup_page_url);
        assertEquals("https://docs.google.com/document/u/0/", urlView.getText().toString());
    }

    @Test
    @SmallTest
    public void testAddNewStartupUrl_savesValidatedUrlWithModalDialog() {
        // Setup URLS startup mode with one existing URL.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);
        assertEquals(1, container.getChildCount());

        // Act: Open the tab-modal dialog via "Add a new page" and enter an unnormalized domain.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    pref.getAddNewPageButtonForTesting().performClick();
                    ModalDialogManager modalDialogManager =
                            mSettingsTestRule.getActivity().getModalDialogManager();
                    PropertyModel dialogModel = modalDialogManager.getCurrentDialogForTest();
                    assertNotNull(dialogModel);
                    assertEquals(ModalDialogType.TAB, modalDialogManager.getCurrentType());
                    assertTrue(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

                    EditText urlEditText =
                            dialogModel
                                    .get(ModalDialogProperties.CUSTOM_VIEW)
                                    .findViewById(R.id.url_edit_text);
                    urlEditText.setText("  wikipedia.org  ");
                    assertFalse(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
                    dialogModel
                            .get(ModalDialogProperties.CONTROLLER)
                            .onClick(dialogModel, ButtonType.POSITIVE);
                    assertNull(modalDialogManager.getCurrentDialogForTest());
                });

        // Verify the normalized URL is appended to the UI list and PrefService.
        assertEquals(2, container.getChildCount());
        TextView addedUrlView = container.getChildAt(1).findViewById(R.id.startup_page_url);
        assertEquals("http://wikipedia.org", addedUrlView.getText().toString());
        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(URL_ESPN, "http://wikipedia.org/"), savedUrls);
    }

    @Test
    @SmallTest
    public void testEditStartupUrl_updatesExistingUrlInPlace() {
        // Setup URLS startup mode with two existing URLs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN, URL_WIKIPEDIA));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);
        assertEquals(2, container.getChildCount());

        // Act: Open the overflow menu on the first row and click "Edit".
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListMenuButton moreButton =
                            container.getChildAt(0).findViewById(R.id.startup_page_more_button);
                    moreButton.callOnClick();
                });
        onView(withText(R.string.edit)).perform(click());

        // Verify the Edit dialog is pre-populated and Save is enabled, then update the URL and
        // submit via the keyboard Done action.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModalDialogManager modalDialogManager =
                            mSettingsTestRule.getActivity().getModalDialogManager();
                    PropertyModel dialogModel = modalDialogManager.getCurrentDialogForTest();
                    assertNotNull(dialogModel);
                    assertEquals(
                            mSettingsTestRule
                                    .getFragment()
                                    .getString(R.string.on_startup_edit_page),
                            dialogModel.get(ModalDialogProperties.TITLE));
                    assertFalse(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

                    EditText urlEditText =
                            dialogModel
                                    .get(ModalDialogProperties.CUSTOM_VIEW)
                                    .findViewById(R.id.url_edit_text);
                    assertEquals(URL_ESPN, urlEditText.getText().toString());
                    urlEditText.setText(URL_DOCS);
                    assertFalse(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
                    urlEditText.onEditorAction(EditorInfo.IME_ACTION_DONE);
                    assertNull(modalDialogManager.getCurrentDialogForTest());
                });

        // Verify row 0 is updated in place while row 1 remains unchanged.
        assertEquals(2, container.getChildCount());
        TextView firstUrl = container.getChildAt(0).findViewById(R.id.startup_page_url);
        TextView secondUrl = container.getChildAt(1).findViewById(R.id.startup_page_url);
        assertEquals("https://docs.google.com/document/u/0/", firstUrl.getText().toString());
        assertEquals("https://www.wikipedia.org", secondUrl.getText().toString());
        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(URL_DOCS, URL_WIKIPEDIA), savedUrls);
    }

    @Test
    @SmallTest
    public void testUrlDialog_validationAndDismissal_leavesStartupUrlsUnchanged() {
        // Setup URLS startup mode with one existing URL.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);

        // Act & Verify: Invalid inputs keep positive button disabled and ignore the keyboard Done
        // action; Cancel and Back dismissals leave stored URLs unchanged.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModalDialogManager modalDialogManager =
                            mSettingsTestRule.getActivity().getModalDialogManager();
                    pref.getAddNewPageButtonForTesting().performClick();
                    PropertyModel dialogModel = modalDialogManager.getCurrentDialogForTest();
                    assertNotNull(dialogModel);
                    assertEquals(ModalDialogType.TAB, modalDialogManager.getCurrentType());
                    EditText urlEditText =
                            dialogModel
                                    .get(ModalDialogProperties.CUSTOM_VIEW)
                                    .findViewById(R.id.url_edit_text);

                    for (String invalidInput :
                            List.of("", "   ", "chrome://flags", "about:blank")) {
                        urlEditText.setText(invalidInput);
                        assertTrue(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
                        urlEditText.onEditorAction(EditorInfo.IME_ACTION_DONE);
                        assertNotNull(modalDialogManager.getCurrentDialogForTest());
                    }

                    urlEditText.setText(URL_WIKIPEDIA);
                    assertFalse(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
                    dialogModel
                            .get(ModalDialogProperties.CONTROLLER)
                            .onClick(dialogModel, ButtonType.NEGATIVE);
                    assertNull(modalDialogManager.getCurrentDialogForTest());

                    pref.getAddNewPageButtonForTesting().performClick();
                    PropertyModel secondDialogModel = modalDialogManager.getCurrentDialogForTest();
                    assertNotNull(secondDialogModel);
                    EditText secondEditText =
                            secondDialogModel
                                    .get(ModalDialogProperties.CUSTOM_VIEW)
                                    .findViewById(R.id.url_edit_text);
                    secondEditText.setText(URL_WIKIPEDIA);
                    modalDialogManager.dismissDialog(
                            secondDialogModel, DialogDismissalCause.NAVIGATE_BACK);
                    assertNull(modalDialogManager.getCurrentDialogForTest());
                });

        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(URL_ESPN), savedUrls);

        // Act & Verify: External startup URLs pref update while Edit dialog is open dismisses it.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListMenuButton moreButton =
                            container.getChildAt(0).findViewById(R.id.startup_page_more_button);
                    moreButton.callOnClick();
                });
        onView(withText(R.string.edit)).perform(click());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertNotNull(
                                mSettingsTestRule
                                        .getActivity()
                                        .getModalDialogManager()
                                        .getCurrentDialogForTest()));
        setRestoreOnStartupUrls(List.of(URL_DOCS));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertNull(
                                mSettingsTestRule
                                        .getActivity()
                                        .getModalDialogManager()
                                        .getCurrentDialogForTest()));
    }

    @Test
    @SmallTest
    public void testUrlDialog_inSettingsActivity_usesAppModalType() {
        // Setup SettingsActivity hosting instead of SettingsInTab.
        SettingsInTab.setShouldOpenSettingsInTabForTesting(false);
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () -> pref.getAddNewPageButtonForTesting().performClick());

        // Verify opening the URL dialog in SettingsActivity uses ModalDialogType.APP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModalDialogManager modalDialogManager =
                            mSettingsTestRule.getActivity().getModalDialogManager();
                    PropertyModel dialogModel = modalDialogManager.getCurrentDialogForTest();
                    assertNotNull(dialogModel);
                    assertEquals(ModalDialogType.APP, modalDialogManager.getCurrentType());
                    modalDialogManager.dismissDialog(
                            dialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
                    assertNull(modalDialogManager.getCurrentDialogForTest());
                });
    }

    @Test
    @SmallTest
    public void testUseCurrentPages_savesHttpAndHttpsTabsFromCurrentWindowOnly() {
        // Setup normal TabModel in the active window with HTTP/HTTPS and internal scheme tabs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setupMockTabWindowManagerWithUrls(List.of(URL_ESPN, URL_NTP, URL_WIKIPEDIA, URL_SETTINGS));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () -> pref.getUseCurrentPagesButtonForTesting().performClick());

        // Verify only valid HTTP/HTTPS tab URLs from the active window's normal TabModel are saved
        // and rendered.
        assertEquals(2, container.getChildCount());
        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(URL_ESPN, URL_WIKIPEDIA), savedUrls);
    }

    @Test
    @SmallTest
    public void testUseCurrentPages_withNoHttpOrHttpsTabs_clearsStartupUrls() {
        // Setup normal TabModel containing only non-HTTP/HTTPS internal tabs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN));
        setupMockTabWindowManagerWithUrls(List.of(URL_NTP));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);
        assertEquals(1, container.getChildCount());

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () -> pref.getUseCurrentPagesButtonForTesting().performClick());

        // Verify empty list is written to clear startup URLs and fall back to NEW_TAB.
        assertEquals(0, container.getChildCount());
        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(), savedUrls);
        assertEquals(SessionStartupPref.NEW_TAB, getRestoreOnStartupPref());
    }

    private void setupMockTabWindowManagerWithUrls(List<String> currentWindowTabUrls) {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabModelSelector currentSelector = createMockTabModelSelector(currentWindowTabUrls);
        TabModelSelector otherSelector = createMockTabModelSelector(List.of(URL_DOCS));
        when(tabWindowManager.getTabModelSelectorById(1)).thenReturn(currentSelector);
        when(tabWindowManager.getAllTabModelSelectors())
                .thenReturn(List.of(otherSelector, currentSelector));
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager));
    }

    private static TabModelSelector createMockTabModelSelector(List<String> tabUrls) {
        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        TabModel normalTabModel = mock(TabModel.class);
        when(tabModelSelector.getModel(/* incognito= */ false)).thenReturn(normalTabModel);
        when(normalTabModel.getCount()).thenReturn(tabUrls.size());
        for (int i = 0; i < tabUrls.size(); i++) {
            Tab tab = mock(Tab.class);
            when(tab.getUrl()).thenReturn(new GURL(tabUrls.get(i)));
            when(normalTabModel.getTabAt(i)).thenReturn(tab);
        }
        return tabModelSelector;
    }

    private RadioButtonGroupOnStartupPreference launchAndGetRadioGroupPreference() {
        mSettingsTestRule.startSettingsActivity();
        mSettingsTestRule.getActivity().getIntent().putExtra(IntentHandler.EXTRA_WINDOW_ID, 1);
        OnStartupSettingsFragment fragment = mSettingsTestRule.getFragment();
        RadioButtonGroupOnStartupPreference pref =
                fragment.findPreference(OnStartupSettingsFragment.PREF_RADIO_BUTTON_GROUP);
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout container = pref.getStartupPagesContainerForTesting();
                    return fragment.getListView() != null
                            && fragment.getListView().getItemDecorationCount() > 0
                            && container != null
                            && container.isAttachedToWindow();
                });
        return pref;
    }

    private void setRestoreOnStartupPref(@SessionStartupPref int startupType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabbedStartupCoordinator.setSessionStartupType(mProfile, startupType));
    }

    private void setRestoreOnStartupUrls(List<String> urls) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabbedStartupCoordinator.setSessionStartupUrls(
                                UserPrefs.get(mProfile), urls));
    }

    private @SessionStartupPref int getRestoreOnStartupPref() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).getInteger(Pref.RESTORE_ON_STARTUP));
    }
}
