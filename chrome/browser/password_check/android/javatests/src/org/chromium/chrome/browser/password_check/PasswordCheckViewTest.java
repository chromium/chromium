// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.HAS_MANUAL_CHANGE_BUTTON;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_CONFIRMATION_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_ORIGIN;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_TIMESTAMP;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.COMPROMISED_CREDENTIALS_COUNT;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.LAUNCH_ACCOUNT_CHECKUP_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.RESTART_BUTTON_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.SHOW_CHECK_SUBTITLE;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.UNKNOWN_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.VIEW_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.VIEW_DIALOG_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_NO_PASSWORDS;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_OFFLINE;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_QUOTA_LIMIT;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_SIGNED_OUT;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_UNKNOWN;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.IDLE;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.RUNNING;
import static org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager.VALID_REAUTHENTICATION_TIME_INTERVAL_MILLIS;

import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.Pair;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager.ReauthScope;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * View tests for the Password Check component ensure model changes are reflected in the check UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordCheckViewTest {
    private static final CompromisedCredential ANA =
            new CompromisedCredential(
                    "https://some-url.com/signin",
                    new GURL("https://some-url.com/"),
                    "Ana",
                    "some-url.com",
                    "Ana",
                    "password",
                    "https://some-url.com/.well-known/change-password",
                    "",
                    1,
                    1,
                    true,
                    false);
    private static final CompromisedCredential PHISHED =
            new CompromisedCredential(
                    "http://example.com/signin",
                    new GURL("http://example.com/"),
                    "",
                    "http://example.com",
                    "(No username)",
                    "DoSomething",
                    "http://example.com/.well-known/change-password",
                    "",
                    1,
                    1,
                    false,
                    true);
    private static final CompromisedCredential LEAKED =
            new CompromisedCredential(
                    "https://some-other-url.com/signin",
                    new GURL("https://some-other-url.com/"),
                    "AZiegler",
                    "some-other-url.com",
                    "AZiegler",
                    "N0M3rcy",
                    "",
                    "com.other.package",
                    1,
                    1,
                    true,
                    false);
    private static final CompromisedCredential LEAKED_AND_PHISHED =
            new CompromisedCredential(
                    "https://super-important.com/signin",
                    new GURL("https://super-important.com/"),
                    "HSong",
                    "super-important.com",
                    "HSong",
                    "N3rfTh1s",
                    "",
                    "com.important.super",
                    1,
                    1,
                    true,
                    true);

    private static final int LEAKS_COUNT = 2;

    private static final long S_TO_MS = 1000;
    private static final long MIN_TO_MS = 60 * S_TO_MS;
    private static final long H_TO_MS = 60 * MIN_TO_MS;
    private static final long DAY_TO_MS = 24 * H_TO_MS;

    private PropertyModel mModel;
    private PasswordCheckFragmentView mPasswordCheckView;

    @Mock private PasswordCheckComponentUi mComponentUi;
    @Mock private PasswordCheckCoordinator.CredentialEventHandler mMockHandler;
    @Mock private Runnable mMockLaunchCheckupInAccount;
    @Mock private Runnable mMockStartButtonCallback;

    @Rule
    public SettingsActivityTestRule<PasswordCheckFragmentView> mTestRule =
            new SettingsActivityTestRule<>(PasswordCheckFragmentView.class);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        PasswordCheckComponentUiFactory.setCreationStrategy(
                (fragmentView, customTabIntentHelper, trustedIntentHelper, profile) -> {
                    mPasswordCheckView = (PasswordCheckFragmentView) fragmentView;
                    mPasswordCheckView.setComponentDelegate(mComponentUi);
                    return mComponentUi;
                });
        setUpUiLaunchedFromSettings();
        runOnUiThreadBlocking(
                () -> {
                    mModel = PasswordCheckProperties.createDefaultModel();
                    PasswordCheckCoordinator.setUpModelChangeProcessors(mModel, mPasswordCheckView);
                });
    }

    @Test
    @MediumTest
    public void testDisplaysHeaderAndCredential() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(RUNNING));
                    mModel.get(ITEMS).add(buildCredentialItem(ANA));
                });
        waitForListViewToHaveLength(2);
        // Has a change passwords button.
        assertNotNull(getCredentialChangeButtonAt(1));
        assertThat(getCredentialChangeButtonAt(1).getVisibility(), is(View.VISIBLE));
        assertThat(
                getCredentialChangeButtonAt(1).getText(),
                is(getString(R.string.password_check_credential_row_change_button_caption)));

        // Has a more button.
        assertNotNull(getCredentialMoreButtonAt(1));
        assertThat(getCredentialMoreButtonAt(1).getVisibility(), is(View.VISIBLE));
        assertThat(
                getCredentialMoreButtonAt(1).getContentDescription(), is(getString(R.string.more)));

        // Has a favicon.
        assertNotNull(getCredentialFaviconAt(1));
        assertThat(getCredentialFaviconAt(1).getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusIllustrationPositive() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertIllustration(R.drawable.password_check_positive);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1133604")
    public void testStatusIllustrationWarning() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, LEAKS_COUNT, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertIllustration(R.drawable.password_checkup_warning);
    }

    @Test
    @MediumTest
    public void testStatusIllustrationNeutral() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_OFFLINE));
                });
        waitForListViewToHaveLength(1);
        assertIllustration(R.drawable.password_check_neutral);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysIconOnIdleNoLeaks() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertDisplaysIcon(R.drawable.ic_check_circle_filled_green_24dp);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysIconOnIdleWithLeaks() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, LEAKS_COUNT, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertDisplaysIcon(R.drawable.ic_warning_red_24dp);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysIconOnError() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_OFFLINE));
                });
        waitForListViewToHaveLength(1);
        assertDisplaysIcon(R.drawable.ic_error_grey800_24dp_filled);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysProgressBarOnRunning() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(RUNNING));
                });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderIcon().getVisibility(), is(View.GONE));
        assertThat(getHeaderProgressBar().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusDisplaysClickableRestartAction() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertThat(getActionButton().getVisibility(), is(View.VISIBLE));
        assertTrue(getActionButton().isClickable());
        getActionButton().callOnClick();
        waitForEvent(mMockStartButtonCallback).run();
    }

    @Test
    @MediumTest
    public void testStatusNotDisplaysRestartAction() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(RUNNING));
                });
        waitForListViewToHaveLength(1);
        assertThat(getActionButton().getVisibility(), is(View.GONE));
        assertFalse(getActionButton().isClickable());
    }

    @Test
    @MediumTest
    public void testStatusDisplaysRestartForOffline() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_OFFLINE));
                });
        waitForListViewToHaveLength(1);
        assertThat(getActionButton().getVisibility(), is(View.VISIBLE));
        assertTrue(getActionButton().isClickable());
    }

    @Test
    @MediumTest
    public void testStatusDoesNotDisplayRestartForNoPasswords() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_NO_PASSWORDS));
                });
        waitForListViewToHaveLength(1);
        assertThat(getActionButton().getVisibility(), is(View.GONE));
        assertFalse(getActionButton().isClickable());
    }

    @Test
    @MediumTest
    public void testStatusRunningText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(RUNNING, UNKNOWN_PROGRESS));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_initial_running)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusIdleNoLeaksText() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_idle_no_leaks)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusIdleWithLeaksText() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, LEAKS_COUNT, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(
                        mPasswordCheckView
                                .getContext()
                                .getResources()
                                .getQuantityString(
                                        R.plurals.password_check_status_message_idle_with_leaks,
                                        LEAKS_COUNT,
                                        LEAKS_COUNT)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusErrorOfflineText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_OFFLINE));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_error_offline)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorNoPasswordsText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_NO_PASSWORDS));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_error_no_passwords)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorQuotaLimitText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_QUOTA_LIMIT));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_error_quota_limit)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorQuotaLimitAccountCheckText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_QUOTA_LIMIT_ACCOUNT_CHECK));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(
                        getString(
                                        R.string
                                                .password_check_status_message_error_quota_limit_account_check)
                                .replace("<link>", "")
                                .replace("</link>", "")));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorSignedOutText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_SIGNED_OUT));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_error_signed_out)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorUnknownText() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_UNKNOWN));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderMessageText(),
                is(getString(R.string.password_check_status_message_error_unknown)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusDisplaysSubtitleOnIdleNoLeaks() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderSubtitle().getText(),
                is(getString(R.string.password_check_status_subtitle_no_findings)));
        assertThat(getHeaderSubtitle().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusDisplaysSubtitleOnIdleWithLeaks() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(IDLE, LEAKS_COUNT, checkTimestamp));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderSubtitle().getText(),
                is(
                        getString(
                                R.string
                                        .password_check_status_subtitle_found_compromised_credentials)));
        assertThat(getHeaderSubtitle().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusDisplaysSubtitle() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_UNKNOWN, true));
                });
        waitForListViewToHaveLength(1);
        assertThat(
                getHeaderSubtitle().getText(),
                is(
                        getString(
                                R.string
                                        .password_check_status_subtitle_found_compromised_credentials)));
        assertThat(getHeaderSubtitle().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusNotDisplaysSubtitle() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildHeader(ERROR_UNKNOWN, false));
                });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderSubtitle().getVisibility(), is(View.GONE));
    }

    @Test
    @SmallTest
    public void testGetTimestampStrings() {
        Resources res = mPasswordCheckView.getContext().getResources();
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 10 * S_TO_MS), is("Just now"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, MIN_TO_MS), is("1 minute ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 17 * MIN_TO_MS), is("17 minutes ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, H_TO_MS), is("1 hour ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 13 * H_TO_MS), is("13 hours ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, DAY_TO_MS), is("1 day ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 2 * DAY_TO_MS), is("2 days ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 315 * DAY_TO_MS), is("315 days ago"));
    }

    @Test
    @MediumTest
    public void testCredentialDisplaysNameOriginAndReason() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildCredentialItem(PHISHED));
                    mModel.get(ITEMS).add(buildCredentialItem(LEAKED));
                    mModel.get(ITEMS).add(buildCredentialItem(LEAKED_AND_PHISHED));
                });
        waitForListViewToHaveLength(3);

        // The phished credential is rendered first:
        assertThat(getCredentialOriginAt(0).getText(), is(PHISHED.getDisplayOrigin()));
        assertThat(getCredentialUserAt(0).getText(), is(PHISHED.getDisplayUsername()));
        assertThat(
                getCredentialReasonAt(0).getText(),
                is(getString(R.string.password_check_credential_row_reason_phished)));
        assertThat(getCredentialChangeButtonAt(0).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialChangeHintAt(0).getVisibility(), is(View.GONE));

        // The leaked credential is rendered second:
        assertThat(getCredentialOriginAt(1).getText(), is(LEAKED.getDisplayOrigin()));
        assertThat(getCredentialUserAt(1).getText(), is(LEAKED.getDisplayUsername()));
        assertThat(
                getCredentialReasonAt(1).getText(),
                is(getString(R.string.password_check_credential_row_reason_leaked)));
        assertThat(getCredentialChangeButtonAt(1).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialChangeHintAt(1).getVisibility(), is(View.GONE));

        // The leaked and phished credential is rendered third:
        assertThat(getCredentialOriginAt(2).getText(), is(LEAKED_AND_PHISHED.getDisplayOrigin()));
        assertThat(getCredentialUserAt(2).getText(), is(LEAKED_AND_PHISHED.getDisplayUsername()));
        assertThat(
                getCredentialReasonAt(2).getText(),
                is(getString(R.string.password_check_credential_row_reason_leaked_and_phished)));
        assertThat(getCredentialChangeButtonAt(2).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialChangeHintAt(2).getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testHidesCredentialChangeButtonWithoutValidEntryPoint() {
        runOnUiThreadBlocking(
                () ->
                        mModel.get(ITEMS)
                                .add(
                                        new MVCListAdapter.ListItem(
                                                PasswordCheckProperties.ItemType
                                                        .COMPROMISED_CREDENTIAL,
                                                new PropertyModel.Builder(
                                                                PasswordCheckProperties
                                                                        .CompromisedCredentialProperties
                                                                        .ALL_KEYS)
                                                        .with(COMPROMISED_CREDENTIAL, ANA)
                                                        .with(HAS_MANUAL_CHANGE_BUTTON, false)
                                                        .with(CREDENTIAL_HANDLER, mMockHandler)
                                                        .build())));
        waitForListViewToHaveLength(1);

        // The credential has no change button:
        assertThat(getCredentialOriginAt(0).getText(), is(ANA.getDisplayOrigin()));
        assertThat(getCredentialUserAt(0).getText(), is(ANA.getDisplayUsername()));
        assertThat(getCredentialChangeButtonAt(0).getVisibility(), is(View.GONE));
        assertThat(getCredentialChangeHintAt(0).getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testCredentialDisplays() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(ITEMS).add(buildCredentialItem(LEAKED));
                });
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(1)));

        // Origin and username.
        assertThat(getCredentialOriginAt(0).getText(), is(LEAKED.getDisplayOrigin()));
        assertThat(getCredentialUserAt(0).getText(), is(LEAKED.getDisplayUsername()));

        // Reason to show credential.
        assertThat(
                getCredentialReasonAt(0).getText(),
                is(getString(R.string.password_check_credential_row_reason_leaked)));

        // Change button without script.
        assertNotNull(getCredentialChangeButtonAt(0));
        assertThat(
                getCredentialChangeButtonAt(0).getText(),
                is(getString(R.string.password_check_credential_row_change_button_caption)));
        assertThat(getCredentialChangeHintAt(0).getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testClickingChangePasswordTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialChangeButtonAt(0));

        waitForEvent(mMockHandler).onChangePasswordButtonClick(eq(ANA));
    }

    @Test
    @MediumTest
    public void testClickingEditInMoreMenuTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialMoreButtonAt(0));

        onView(withText(R.string.password_check_credential_menu_item_edit_button_caption))
                .inRoot(
                        withDecorView(
                                not(
                                        is(
                                                mPasswordCheckView
                                                        .getActivity()
                                                        .getWindow()
                                                        .getDecorView()))))
                .perform(click());

        waitForEvent(mMockHandler).onEdit(eq(ANA), eq(mPasswordCheckView.getContext()));
    }

    @Test
    @MediumTest
    public void testClickingDeleteInMoreMenuTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialMoreButtonAt(0));

        onView(withText(R.string.password_check_credential_menu_item_remove_button_caption))
                .inRoot(
                        withDecorView(
                                not(
                                        is(
                                                mPasswordCheckView
                                                        .getActivity()
                                                        .getWindow()
                                                        .getDecorView()))))
                .perform(click());

        waitForEvent(mMockHandler).onRemove(eq(ANA));
    }

    @Test
    @MediumTest
    public void testClickingViewInMoreMenuTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialMoreButtonAt(0));

        onView(withText(R.string.password_check_credential_menu_item_view_button_caption))
                .inRoot(
                        withDecorView(
                                not(
                                        is(
                                                mPasswordCheckView
                                                        .getActivity()
                                                        .getWindow()
                                                        .getDecorView()))))
                .perform(click());

        waitForEvent(mMockHandler).onView(eq(ANA));
    }

    @Test
    @MediumTest
    public void testConfirmingDeletionDialogTriggersHandler() {
        final AtomicInteger recordedConfirmation = new AtomicInteger(0);
        PasswordCheckDeletionDialogFragment.Handler fakeHandler =
                new PasswordCheckDeletionDialogFragment.Handler() {
                    @Override
                    public void onDismiss() {}

                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        recordedConfirmation.incrementAndGet();
                    }
                };

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(DELETION_ORIGIN, ANA.getDisplayOrigin());
                    mModel.set(DELETION_CONFIRMATION_HANDLER, fakeHandler);
                });

        onView(withText(R.string.password_entry_edit_delete_credential_dialog_confirm))
                .inRoot(
                        withDecorView(
                                not(
                                        is(
                                                mPasswordCheckView
                                                        .getActivity()
                                                        .getWindow()
                                                        .getDecorView()))))
                .perform(click());

        assertThat(recordedConfirmation.get(), is(1));
    }

    @Test
    @MediumTest
    public void testCopyPasswordViewDialog() {
        PasswordCheckDeletionDialogFragment.Handler fakeHandler =
                new PasswordCheckDeletionDialogFragment.Handler() {
                    @Override
                    public void onDismiss() {}

                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {}
                };
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthScope.ONE_AT_A_TIME);

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(VIEW_CREDENTIAL, ANA);
                    mModel.set(VIEW_DIALOG_HANDLER, fakeHandler);
                });
        onView(withId(R.id.view_dialog_copy_button)).inRoot(isDialog()).perform(click());

        ClipboardManager clipboard =
                (ClipboardManager)
                        mPasswordCheckView
                                .getActivity()
                                .getApplicationContext()
                                .getSystemService(Context.CLIPBOARD_SERVICE);
        assertThat(
                clipboard.getPrimaryClip().getItemAt(0).getText().toString(),
                is(ANA.getPassword()));
    }

    @Test
    @MediumTest
    public void testCloseViewDialogTriggersHandler() {
        final AtomicInteger recordedClosure = new AtomicInteger(0);
        PasswordCheckDeletionDialogFragment.Handler fakeHandler =
                new PasswordCheckDeletionDialogFragment.Handler() {
                    @Override
                    public void onDismiss() {}

                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        recordedClosure.incrementAndGet();
                    }
                };
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthScope.ONE_AT_A_TIME);

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(VIEW_CREDENTIAL, ANA);
                    mModel.set(VIEW_DIALOG_HANDLER, fakeHandler);
                });

        onView(withText(R.string.close))
                .inRoot(
                        withDecorView(
                                not(
                                        is(
                                                mPasswordCheckView
                                                        .getActivity()
                                                        .getWindow()
                                                        .getDecorView()))))
                .perform(click());

        assertThat(recordedClosure.get(), is(1));
    }

    @Test
    @MediumTest
    public void testOnResumeViewDialogReauthenticationNeeded() {
        final AtomicInteger recordedDismiss = new AtomicInteger(0);
        PasswordCheckDeletionDialogFragment.Handler fakeHandler =
                new PasswordCheckDeletionDialogFragment.Handler() {
                    @Override
                    public void onDismiss() {
                        recordedDismiss.incrementAndGet();
                    }

                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {}
                };
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthScope.ONE_AT_A_TIME);

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(VIEW_CREDENTIAL, ANA);
                    mModel.set(VIEW_DIALOG_HANDLER, fakeHandler);
                });

        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis() - VALID_REAUTHENTICATION_TIME_INTERVAL_MILLIS,
                ReauthScope.ONE_AT_A_TIME);

        mTestRule.getFragment().onStop();
        mTestRule.getFragment().onResume();

        CriteriaHelper.pollInstrumentationThread(() -> recordedDismiss.get() == 1);
    }

    @Test
    @SmallTest
    public void testHelpHandlerCalled() {
        when(mComponentUi.handleHelp(any())).thenReturn(true);
        onView(withId(R.id.menu_id_targeted_help)).perform(click());
        verify(mComponentUi).handleHelp(any());
    }

    private MVCListAdapter.ListItem buildHeader(
            @PasswordCheckUIStatus int status,
            Integer compromisedCredentialsCount,
            Long checkTimestamp) {
        return buildHeader(status, compromisedCredentialsCount, checkTimestamp, null, true);
    }

    private MVCListAdapter.ListItem buildHeader(
            @PasswordCheckUIStatus int status, Pair<Integer, Integer> progress) {
        return buildHeader(status, null, null, progress, false);
    }

    private MVCListAdapter.ListItem buildHeader(
            @PasswordCheckUIStatus int status, boolean showStatusSubtitle) {
        return buildHeader(status, null, null, null, showStatusSubtitle);
    }

    private MVCListAdapter.ListItem buildHeader(@PasswordCheckUIStatus int status) {
        return buildHeader(status, null, null, null, false);
    }

    private MVCListAdapter.ListItem buildHeader(
            @PasswordCheckUIStatus int status,
            Integer compromisedCredentialsCount,
            Long checkTimestamp,
            Pair<Integer, Integer> progress,
            boolean showStatusSubtitle) {
        return new MVCListAdapter.ListItem(
                PasswordCheckProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(CHECK_PROGRESS, progress)
                        .with(CHECK_STATUS, status)
                        .with(CHECK_TIMESTAMP, checkTimestamp)
                        .with(COMPROMISED_CREDENTIALS_COUNT, compromisedCredentialsCount)
                        .with(LAUNCH_ACCOUNT_CHECKUP_ACTION, mMockLaunchCheckupInAccount)
                        .with(RESTART_BUTTON_ACTION, mMockStartButtonCallback)
                        .with(SHOW_CHECK_SUBTITLE, showStatusSubtitle)
                        .build());
    }

    private MVCListAdapter.ListItem buildCredentialItem(CompromisedCredential credential) {
        return new MVCListAdapter.ListItem(
                PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                new PropertyModel.Builder(
                                PasswordCheckProperties.CompromisedCredentialProperties.ALL_KEYS)
                        .with(COMPROMISED_CREDENTIAL, credential)
                        .with(HAS_MANUAL_CHANGE_BUTTON, true)
                        .with(CREDENTIAL_HANDLER, mMockHandler)
                        .build());
    }

    private void setUpUiLaunchedFromSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PasswordCheckFragmentView.PASSWORD_CHECK_REFERRER,
                PasswordCheckReferrer.PASSWORD_SETTINGS);
        mTestRule.startSettingsActivity(fragmentArgs);
    }

    private void waitForListViewToHaveLength(int length) {
        pollUiThread(
                () -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(length)));
    }

    private void assertDisplaysIcon(int resourceId) {
        assertThat(getHeaderIcon().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderProgressBar().getVisibility(), is(View.GONE));
        Drawable icon = getHeaderIcon().getDrawable();
        int widthPx = icon.getIntrinsicWidth();
        int heightPx = icon.getIntrinsicHeight();
        assertTrue(
                getBitmap(
                                AppCompatResources.getDrawable(
                                        mPasswordCheckView.getContext(), resourceId),
                                widthPx,
                                heightPx)
                        .sameAs(getBitmap(icon, widthPx, heightPx)));
    }

    private void assertIllustration(int resourceId) {
        Drawable illustration =
                ((ImageView) getStatus().findViewById(R.id.check_status_illustration))
                        .getDrawable();
        int widthPx = illustration.getIntrinsicWidth();
        int heightPx = illustration.getIntrinsicHeight();
        assertTrue(
                getBitmap(
                                AppCompatResources.getDrawable(
                                        mPasswordCheckView.getContext(), resourceId),
                                widthPx,
                                heightPx)
                        .sameAs(getBitmap(illustration, widthPx, heightPx)));
    }

    private View getStatus() {
        return mPasswordCheckView.getListView().getChildAt(0);
    }

    private ImageView getHeaderIcon() {
        return getStatus().findViewById(R.id.check_status_icon);
    }

    private ProgressBar getHeaderProgressBar() {
        return getStatus().findViewById(R.id.check_status_progress);
    }

    private TextView getHeaderDescription() {
        return getStatus().findViewById(R.id.check_status_description);
    }

    private TextView getHeaderMessage() {
        return getStatus().findViewById(R.id.check_status_message);
    }

    private String getHeaderMessageText() {
        return getHeaderMessage().getText().toString();
    }

    private TextView getHeaderSubtitle() {
        return getStatus().findViewById(R.id.check_status_subtitle);
    }

    private ImageButton getActionButton() {
        return getStatus().findViewById(R.id.check_status_restart_button);
    }

    private RecyclerView getPasswordCheckViewList() {
        return mPasswordCheckView.getListView();
    }

    private TextView getCredentialOriginAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.credential_origin);
    }

    private TextView getCredentialUserAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.compromised_username);
    }

    private TextView getCredentialReasonAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.compromised_reason);
    }

    private ButtonCompat getCredentialChangeButtonAt(int index) {
        return getPasswordCheckViewList()
                .getChildAt(index)
                .findViewById(R.id.credential_change_button);
    }

    private TextView getCredentialChangeHintAt(int index) {
        return getPasswordCheckViewList()
                .getChildAt(index)
                .findViewById(R.id.credential_change_hint);
    }

    private ListMenuButton getCredentialMoreButtonAt(int index) {
        return getPasswordCheckViewList()
                .getChildAt(index)
                .findViewById(R.id.credential_menu_button);
    }

    private ImageView getCredentialFaviconAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.credential_favicon);
    }

    private String getString(@IdRes int stringResource) {
        return mTestRule.getActivity().getString(stringResource);
    }

    private static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private Bitmap getBitmap(Drawable drawable, int widthPx, int heightPx) {
        Bitmap bitmap = Bitmap.createBitmap(widthPx, heightPx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, widthPx, heightPx);
        drawable.draw(canvas);
        return bitmap;
    }
}
