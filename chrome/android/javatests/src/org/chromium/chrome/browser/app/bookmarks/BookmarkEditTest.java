// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.MenuItem;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.core.IsInstanceOf;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.bookmarks.BookmarkEditMetrics.BookmarkEditOutcome;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkModelTest;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests functionality in BookmarkEditActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@DisableIf.Device(DeviceFormFactor.AUTOMOTIVE) // crbug.com/462590301
// The suite exercises the mobile edit flow, which saves on onStop().
@DisableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG})
public class BookmarkEditTest {

    private static final String TITLE_A = "a";
    private static final String TITLE_B = "b";
    private static final String URL_A = "http://a.com/";
    private static final String URL_B = "http://b.com/";
    private static final String FOLDER_A = "FolderA";
    private static BookmarkModel sBookmarkModel;
    private static BookmarkModelObserver sModelObserver;
    private static BookmarkId sBookmarkId;
    private static BookmarkId sMobileNode;
    private static BookmarkId sOtherNode;
    private static BookmarkEditActivity sBookmarkEditActivity;

    private static final CallbackHelper sDestroyedCallback = new CallbackHelper();
    private static final ActivityStateListener sActivityStateListener =
            new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    if (newState == ActivityState.DESTROYED) sDestroyedCallback.notifyCalled();
                }
            };
    private static final CallbackHelper sModelChangedCallback = new CallbackHelper();

    @BeforeClass
    public static void setUpClass() throws TimeoutException {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        // Desktop-Android builds default to COMPACT rows, which append child counts to folder
        // titles. Force VISUAL rows so the folder row title matches the folder name exactly.
        // The test runner resets shared prefs between test classes.
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF, BookmarkRowDisplayPref.VISUAL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel =
                            BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
                    sBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sMobileNode = sBookmarkModel.getMobileFolderId();
                    sOtherNode = sBookmarkModel.getOtherFolderId();
                });
        sBookmarkId =
                BookmarkModelTest.addBookmark(
                        sBookmarkModel, sMobileNode, 0, TITLE_A, new GURL(URL_A));

        sModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        sModelChangedCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.addObserver(sModelObserver));

        startEditActivity(sBookmarkId);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            sActivityStateListener, sBookmarkEditActivity);
                });
    }

    @After
    public void resetBookmark() throws ExecutionException {
        if (getBookmarkItem(sBookmarkId) != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        sBookmarkModel.setBookmarkTitle(sBookmarkId, TITLE_A);
                        sBookmarkModel.setBookmarkUrl(sBookmarkId, new GURL(URL_A));
                        sBookmarkModel.moveBookmark(sBookmarkId, sMobileNode, 0);
                    });
        }
        if (sBookmarkEditActivity != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        sBookmarkEditActivity.getTitleEditText().getEditText().setText(TITLE_A);
                        sBookmarkEditActivity.getUrlEditText().getEditText().setText(URL_A);
                    });
        }
    }

    @AfterClass
    public static void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel.removeObserver(sModelObserver);
                    sBookmarkModel.removeAllUserBookmarks();
                    ApplicationStatus.unregisterActivityStateListener(sActivityStateListener);
                });
        // Drop the static reference so the class-level leak check doesn't see the destroyed
        // activity.
        sBookmarkEditActivity = null;
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditTitleAndUrl() throws ExecutionException, TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.SAVED)
                        .build();

        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        // The outcome is only recorded when the activity is finishing, so go through a real
        // finish instead of calling onStop() directly.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.getTitleEditText().getEditText().setText(TITLE_B);
                    sBookmarkEditActivity.getUrlEditText().getEditText().setText(URL_B);
                    sBookmarkEditActivity.finish();
                });
        sDestroyedCallback.waitForCallback(0);

        histogramWatcher.assertExpected();

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_B, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_B, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testOnStopSavesWithoutReinflatingRowOrNoOpModelChanges()
            throws ExecutionException, TimeoutException {
        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        View oldFolderSelectRow = sBookmarkEditActivity.getFolderSelectRowForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.getTitleEditText().getEditText().setText(TITLE_B);
                    sBookmarkEditActivity.getUrlEditText().getEditText().setText(URL_B);
                    sBookmarkEditActivity.onStop();
                });
        // Checks that onStop() did not synchronously re-inflate the folder row; the coalesced
        // model-changed refresh is drained separately below.
        Assert.assertSame(
                "Folder row should not be re-inflated on save.",
                oldFolderSelectRow,
                sBookmarkEditActivity.getFolderSelectRowForTesting());
        // The coalesced refresh re-inflates the row once, after onStop() has returned.
        CriteriaHelper.pollUiThread(
                () -> sBookmarkEditActivity.getFolderSelectRowForTesting() != oldFolderSelectRow,
                "Timed out waiting for the posted refresh to rebuild the folder row");

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_B, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_B, bookmarkItem.getUrl().getSpec());

        int modelChangedCount = sModelChangedCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkEditActivity.onStop());
        Assert.assertEquals(
                "Unchanged bookmark should not trigger model changes on stop.",
                modelChangedCount,
                sModelChangedCallback.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditEmptyInputRejected() throws ExecutionException, TimeoutException {
        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.getTitleEditText().getEditText().setText("");
                    sBookmarkEditActivity.getUrlEditText().getEditText().setText("");
                    sBookmarkEditActivity.onStop();
                });

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_A, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testInvalidUrlRejected() throws ExecutionException, TimeoutException {
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity
                            .getUrlEditText()
                            .getEditText()
                            .setText("http:://?foo=bar");
                    sBookmarkEditActivity.onStop();
                });

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect url after edit.", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditActivityDeleteButton() throws ExecutionException, TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.DELETED)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.onOptionsItemSelected(
                            sBookmarkEditActivity.getDeleteButton());
                });
        sDestroyedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertNull("Bookmark item should have been deleted.", bookmarkItem);
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditActivityHomeButton() throws ExecutionException, TimeoutException {
        MenuItem item = Mockito.mock(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(android.R.id.home);
        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkEditActivity.onOptionsItemSelected(item));

        Assert.assertTrue(
                "BookmarkActivity should be finishing or destroyed.",
                sBookmarkEditActivity.isFinishing() || sBookmarkEditActivity.isDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditActivityReflectsModelChanges() throws TimeoutException, ExecutionException {
        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect folder.",
                getBookmarkItem(sMobileNode).getTitle(),
                sBookmarkEditActivity
                        .getFolderSelectRowPropertyModelForTesting()
                        .get(ImprovedBookmarkRowProperties.TITLE));

        int currentModelChangedCount = sModelChangedCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel.setBookmarkTitle(sBookmarkId, TITLE_B);
                    sBookmarkModel.moveBookmark(sBookmarkId, sOtherNode, 0);
                });
        sModelChangedCallback.waitForCallback(currentModelChangedCount);

        Assert.assertEquals(
                "Title shouldn't change after model update.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Folder should change after model update.",
                getBookmarkItem(sOtherNode).getTitle(),
                sBookmarkEditActivity
                        .getFolderSelectRowPropertyModelForTesting()
                        .get(ImprovedBookmarkRowProperties.TITLE));
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditActivityFinishesWhenBookmarkDeleted() throws TimeoutException {
        int currentModelChangedCount = sModelChangedCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.deleteBookmark(sBookmarkId));
        sModelChangedCallback.waitForCallback(currentModelChangedCount);

        CriteriaHelper.pollUiThread(
                () -> sBookmarkEditActivity.isFinishing() || sBookmarkEditActivity.isDestroyed(),
                "Timed out waiting for BookmarkEditActivity to finish");
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditFolderLocation()
            throws ExecutionException, TimeoutException, InterruptedException {
        var userActionTester = new UserActionTester();
        addFolder(sMobileNode, 0, FOLDER_A);
        ThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkEditActivity.getFolderSelectRowForTesting().performClick());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getLastTrackedFocusedActivity(),
                            IsInstanceOf.instanceOf(BookmarkFolderPickerActivity.class));
                });
        Assert.assertTrue(
                userActionTester.getActions().contains("BookmarkEdit.FolderPickerOpened"));
        userActionTester.tearDown();

        // This sleep allows the espresso events to actually go through. Without them, the test is
        // very flaky.
        Thread.sleep(200);
        onView(withText("Mobile bookmarks")).perform(click());
        onView(withText(FOLDER_A)).perform(click());
        onView(withText("Move here")).perform(click());
        waitForEditActivity();
        Assert.assertEquals(
                "Folder should change after folder activity finishes.",
                FOLDER_A,
                sBookmarkEditActivity
                        .getFolderSelectRowPropertyModelForTesting()
                        .get(ImprovedBookmarkRowProperties.TITLE));
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testChangeFolderWhenBookmarkRemoved() throws ExecutionException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkEditActivity.getFolderSelectRowForTesting().performClick());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getLastTrackedFocusedActivity(),
                            IsInstanceOf.instanceOf(BookmarkFolderPickerActivity.class));
                });

        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.deleteBookmark(sBookmarkId));
        CriteriaHelper.pollUiThread(
                () ->
                        !(ApplicationStatus.getLastTrackedFocusedActivity()
                                instanceof BookmarkFolderPickerActivity),
                "Timed out waiting for BookmarkFolderSelectActivity to close");
    }

    private BookmarkItem getBookmarkItem(BookmarkId bookmarkId) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.getBookmarkById(bookmarkId));
    }

    private static void startEditActivity(BookmarkId bookmarkId) {
        sBookmarkEditActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        BookmarkEditActivity.class,
                        Stage.RESUMED,
                        () -> {
                            // There is no foreground activity to launch from, so build the
                            // intent directly and add FLAG_ACTIVITY_NEW_TASK, which
                            // BookmarkManagerOpenerImpl does not set.
                            Context context = ContextUtils.getApplicationContext();
                            Intent intent = new Intent(context, BookmarkEditActivity.class);
                            intent.putExtra(
                                    BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
                            ProfileIntentUtils.addProfileToIntent(
                                    ProfileManager.getLastUsedRegularProfile(), intent);
                            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                            context.startActivity(intent);
                        });
    }

    private BookmarkId addFolder(BookmarkId parent, int index, String title)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.addFolder(parent, index, title));
    }

    private void waitForEditActivity() {
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getLastTrackedFocusedActivity()
                                instanceof BookmarkEditActivity,
                "Timed out waiting for BookmarkEditActivity");
        sBookmarkEditActivity =
                (BookmarkEditActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
